/*
 *  Mumak Offline Trace Analysis Tool
 */

#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>

#include "logger.hpp"
#include "pin.H"
#include "trace.hpp"
#include "utils.hpp"

using std::cerr;
using std::endl;
using std::hash;
using std::ifstream;
using std::istream_iterator;
using std::istringstream;
using std::map;
using std::ofstream;
using std::pair;
using std::set;
using std::string;
using std::vector;

#define RECORD_OPERATIONS_LIMIT 10000000

string out_path;
ofstream trace_out;
ifstream detected_bugs_in;
ofstream bugs_out;

bool collecting_backtraces, non_deterministic;
string latest_trace_symbols;

map<uint32_t, bool> logged_addresses;

uint32_t logged_instructions = 0, actual_bugs = 0;

uint32_t current_instruction_id;
trace::Bug next_bug(trace::BugType::NONE, 1);

void *addresses[BACKTRACE_ADDRESSES_LIMIT];
int num_addresses;

RecordOperation *recorded_operations;
int fd;
uint32_t inserted_since_increase = 0, total_inserted = 0;

// DETERMINISTIC COLLECT TRACE BEGIN
void RegisterRecordOperation(trace::Instruction type, uint64_t address = 0,
                             uint32_t size = 0) {
    switch (type) {
        case trace::Instruction::STORE:
        case trace::Instruction::NON_TEMPORAL_STORE:
        case trace::Instruction::RMW:
            trace_out << type << " " << address << " " << size << " "
                      << current_instruction_id++ << "\n";
            break;
        case trace::Instruction::CLFLUSH:
        case trace::Instruction::CLFLUSHOPT:
        case trace::Instruction::CLWB:
            trace_out << type << " " << address << " "
                      << current_instruction_id++ << "\n";
            break;
        case trace::Instruction::FENCE:
            trace_out << type << " " << current_instruction_id++ << "\n";
            break;
        default:
            trace_out << type << "\n";
    }

    logged_instructions++;
}

void TraceWrite(ADDRINT address, uint32_t size, trace::Instruction type) {
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        RegisterRecordOperation(type, address, size);
    }
}

void TraceFlush(ADDRINT address, trace::Instruction flush_type) {
    if (IsPMAddress(address, 0)) {  // only the start matters in this case
        COUNT_FLUSH;
        RegisterRecordOperation(flush_type, address);
    }
}

void TraceFence() {
    COUNT_FENCE;
    RegisterRecordOperation(trace::Instruction::FENCE);
}

void TraceRMW(ADDRINT address, uint32_t size) {
    COUNT_RMW;
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        RegisterRecordOperation(trace::Instruction::RMW, address, size);
    } else {
        RegisterRecordOperation(trace::Instruction::RMW, 0, 0);
    }
}

VOID TraceInstructions(INS ins, VOID *v) {
    int opcode = INS_Opcode(ins);

    if (INS_IsAtomicUpdate(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceRMW,
                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
    } else if (INS_IsMemoryWrite(ins)) {
        trace::Instruction inst = IsMovnt(opcode)
                                      ? trace::Instruction::NON_TEMPORAL_STORE
                                      : trace::Instruction::STORE;
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceWrite,
                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_UINT64,
                       inst, IARG_END);
    } else if (INS_IsCacheLineFlush(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceFlush,
                       IARG_MEMORYOP_EA, 0, IARG_UINT64, GetFlushType(opcode),
                       IARG_END);
    } else if (IsFence(opcode)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceFence, IARG_END);
    }
}
// DETERMINISTIC COLLECT TRACE END

// DETERMINISTIC COLLECT BACKTRACES BEGIN
bool SetNextBugId() {
    string line;
    while (true) {
        if (getline(detected_bugs_in, line)) {
            istringstream iss(line);
            vector<string> trace_line{istream_iterator<string>{iss},
                                      istream_iterator<string>{}};
            trace::BugType type = static_cast<trace::BugType>(
                strtoul(trace_line[0].c_str(), NULL, 10));
            uint32_t id = strtoul(trace_line[1].c_str(), NULL, 10);
            if (id == next_bug.instruction_id) {
                if (ReportBug(logged_addresses, latest_trace_symbols,
                              trace::BugTypeToStr(type), addresses,
                              num_addresses, bugs_out))
                    actual_bugs++;
            } else {
                next_bug.type = type;
                next_bug.instruction_id = id;
                return true;
            }
        } else {
            return false;
        }
    }
}

void CollectBacktrace(const CONTEXT *ctxt) {
    if (current_instruction_id == next_bug.instruction_id) {
        PIN_LockClient();
        num_addresses =
            PIN_Backtrace(ctxt, addresses, BACKTRACE_ADDRESSES_LIMIT);
        PIN_UnlockClient();
        if (ReportBug(logged_addresses, latest_trace_symbols,
                      trace::BugTypeToStr(next_bug.type), addresses,
                      num_addresses, bugs_out))
            actual_bugs++;
        if (!SetNextBugId()) {
            PIN_ExitApplication(0);
        }
    }

    // Always increment the index of the instructions being instrumented
    current_instruction_id++;
}

void CollectWrite(const CONTEXT *ctxt, ADDRINT address, uint32_t size) {
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        CollectBacktrace(ctxt);
    }
}

void CollectFlush(const CONTEXT *ctxt, ADDRINT address) {
    if (IsPMAddress(address, 0)) {
        COUNT_FLUSH;
        CollectBacktrace(ctxt);
    }
}

void CollectFence(const CONTEXT *ctxt) {
    COUNT_FENCE;
    CollectBacktrace(ctxt);
}

void CollectRMW(const CONTEXT *ctxt) {
    COUNT_RMW;
    CollectBacktrace(ctxt);
}

VOID CollectBacktraces(INS ins, VOID *v) {
    // Look for memory operands of an instruction to detect which memory address
    // the write is working with
    int opcode = INS_Opcode(ins);

    if (INS_IsAtomicUpdate(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CollectRMW,
                       IARG_CONST_CONTEXT, IARG_END);
    } else if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CollectWrite,
                       IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE, IARG_END);
    } else if (INS_IsCacheLineFlush(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CollectFlush,
                       IARG_CONST_CONTEXT, IARG_MEMORYOP_EA, 0, IARG_END);
    } else if (IsFence(opcode)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CollectFence,
                       IARG_CONST_CONTEXT, IARG_END);
    }
}
// DETERMINISTIC COLLECT BACKTRACES END

// NON-DETERMINISTIC BEGIN
void InitializeMmapMemory(uint64_t size) {
    string addresses_file = out_path + "/backtrace-addresses.bin";
    fd = open(addresses_file.c_str(), O_CREAT | O_RDWR, 0666);
    OS_Ftruncate(fd, size);
    OS_MapFileToMemory(
        NATIVE_PID_CURRENT,
        OS_PAGE_PROTECTION_TYPE_READ | OS_PAGE_PROTECTION_TYPE_WRITE, size,
        OS_MEMORY_FLAGS_SHARED, fd, 0, (void **)&recorded_operations);
}

void IncreaseMmapMemory() {
    total_inserted += RECORD_OPERATIONS_LIMIT;
    uint64_t offset = total_inserted * sizeof(struct RecordOperation);
    uint64_t segment_size =
        RECORD_OPERATIONS_LIMIT * sizeof(struct RecordOperation);
    // Full allocated size until the moment
    uint64_t full_size = segment_size + offset;

    // Free memory
    OS_FreeMemory(NATIVE_PID_CURRENT, recorded_operations, offset);

    // Prepare mmap file with appropriate size
    OS_Ftruncate(fd, full_size);

    // Allocate mmap space
    OS_MapFileToMemory(
        NATIVE_PID_CURRENT,
        OS_PAGE_PROTECTION_TYPE_READ | OS_PAGE_PROTECTION_TYPE_WRITE, full_size,
        OS_MEMORY_FLAGS_SHARED, fd, 0, (void **)&recorded_operations);

    inserted_since_increase = 0;
}

void ReportAllBugs() {
    string mmap_size_file = out_path + "/mmap-size.txt";
    ifstream mmap_size_in(mmap_size_file.c_str());
    string line;
    uint64_t size = 0;
    while (getline(mmap_size_in, line)) {
        size = Uint64FromString(line);
    }

    InitializeMmapMemory(size);
    while (getline(detected_bugs_in, line)) {
        istringstream iss(line);
        vector<string> trace_line{istream_iterator<string>{iss},
                                  istream_iterator<string>{}};
        trace::BugType type = static_cast<trace::BugType>(
            strtoul(trace_line[0].c_str(), NULL, 10));
        uint32_t id = strtoul(trace_line[1].c_str(), NULL, 10);
        RecordOperation op = recorded_operations[id];
        if (ReportBug(logged_addresses, latest_trace_symbols,
                      trace::BugTypeToStr(type), op.addresses, op.num_addresses,
                      bugs_out))
            actual_bugs++;
    }
    PIN_ExitApplication(0);
}

void RecordTraceAddresses(const CONTEXT *ctxt) {
    // this needs to be here, otherwise the trace addresses do not match
    // this is the earliest we can obtain them
    if (collecting_backtraces) ReportAllBugs();
    if (total_inserted == 0) {
        InitializeMmapMemory(RECORD_OPERATIONS_LIMIT * sizeof(RecordOperation));
        total_inserted = RECORD_OPERATIONS_LIMIT;
    }
    if (inserted_since_increase == RECORD_OPERATIONS_LIMIT)
        IncreaseMmapMemory();

    recorded_operations[current_instruction_id] = RecordOperation(ctxt);
    inserted_since_increase++;
}

void TraceWriteWithContext(const CONTEXT *ctxt, ADDRINT address, uint32_t size,
                           trace::Instruction type) {
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        RecordTraceAddresses(ctxt);
        RegisterRecordOperation(type, address, size);
    }
}

void TraceFlushWithContext(const CONTEXT *ctxt, ADDRINT address,
                           trace::Instruction flush_type) {
    if (IsPMAddress(address, 0)) {  // only the start matters in this case
        COUNT_FLUSH;
        RecordTraceAddresses(ctxt);
        RegisterRecordOperation(flush_type, address);
    }
}

void TraceFenceWithContext(const CONTEXT *ctxt) {
    COUNT_FENCE;
    RecordTraceAddresses(ctxt);
    RegisterRecordOperation(trace::Instruction::FENCE);
}
void TraceRMWWithContext(const CONTEXT *ctxt, ADDRINT address, uint32_t size) {
    COUNT_RMW;
    RecordTraceAddresses(ctxt);
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        RegisterRecordOperation(trace::Instruction::RMW, address, size);
    } else {
        RegisterRecordOperation(trace::Instruction::RMW, 0, 0);
    }
}

VOID TraceInstructionsWithContext(INS ins, VOID *v) {
    int opcode = INS_Opcode(ins);

    if (INS_IsAtomicUpdate(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceRMWWithContext,
                       IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE, IARG_END);
    } else if (INS_IsMemoryWrite(ins)) {
        trace::Instruction inst = IsMovnt(opcode)
                                      ? trace::Instruction::NON_TEMPORAL_STORE
                                      : trace::Instruction::STORE;
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceWriteWithContext,
                       IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE, IARG_UINT64, inst, IARG_END);
    } else if (INS_IsCacheLineFlush(ins)) {
        // The flush instruction will contain the memory operand 0, as the
        // first operand will always be the flush address.
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceFlushWithContext,
                       IARG_CONST_CONTEXT, IARG_MEMORYOP_EA, 0, IARG_UINT64,
                       GetFlushType(opcode), IARG_END);
    } else if (IsFence(opcode)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceFenceWithContext,
                       IARG_CONST_CONTEXT, IARG_END);
    }
}
// NON-DETERMINISTIC END

VOID Fini(INT32 code, VOID *v) {
    if (!collecting_backtraces) {
        trace_out.flush();
        trace_out.close();

        // Output the number of logged instructions
        cerr << "Actual number of logged instructions: " << logged_instructions
             << endl;
        if (non_deterministic) {
            string mmap_size_file = out_path + "/mmap-size.txt";
            ofstream mmap_size_out(mmap_size_file.c_str());
            uint64_t mmap_size =
                total_inserted * sizeof(struct RecordOperation);
            mmap_size_out << mmap_size << endl;
            mmap_size_out.close();
        }
    } else {
        bugs_out.close();
        detected_bugs_in.close();

        // Output the number of obtained bugs and warnings
        cerr << "Actual number of obtained bugs: " << actual_bugs << endl;
        // cerr << "Actual number of obtained warnings: " <<
        // actualNumberOfWarnings
        //  << endl;
    }

    OUTPUT_INSTRUCTION_COUNT;
}

KNOB<std::string> KnobOutDir(KNOB_MODE_WRITEONCE, "pintool", "dir",
                             "/mnt/ramdisk/", "directory for tool i/o");

KNOB<bool> KnobCollectBacktraceMode(
    KNOB_MODE_WRITEONCE, "pintool", "backtrace", "false",
    "collect backtraces, otherwise trace relevant instructions");

KNOB<bool> KnobNonDeterministic(
    KNOB_MODE_WRITEONCE, "pintool", "non-deterministic", "false",
    "analysis does not assume determinism of target");

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");

INT32 Usage() {
    cerr << "This tool will trace all relevant instructions for offline "
            "analysis."
         << std::endl;
    cerr << "This tool assumes that the application is fully deterministic "
            "and "
            "therefore the order of the instructions is the same between "
            "executions."
         << std::endl;
    cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    out_path = KnobOutDir.Value();
    pm_mount = KnobPMMount.Value().c_str();
    collecting_backtraces = KnobCollectBacktraceMode.Value();
    non_deterministic = KnobNonDeterministic.Value();

    if (collecting_backtraces) {
        string bugs_out_file = out_path + "/bug-report.txt";
        string detected_bugs_file = out_path + "/bug-ids.txt";
        bugs_out.open(bugs_out_file.c_str(), ofstream::out | ofstream::trunc);
        detected_bugs_in.open(detected_bugs_file.c_str(), ifstream::in);
        if (non_deterministic) {
            INS_AddInstrumentFunction(TraceInstructionsWithContext, 0);
        } else {
            INS_AddInstrumentFunction(CollectBacktraces, 0);
            if (!SetNextBugId()) {
                Fini(0, 0);
                return 0;
            }
        }
    } else {
        string trace_out_file = out_path + "/pm-trace.txt";
        trace_out.open(trace_out_file.c_str(), ofstream::out | ofstream::trunc);
        if (non_deterministic) {
            INS_AddInstrumentFunction(TraceInstructionsWithContext, 0);
        } else {
            INS_AddInstrumentFunction(TraceInstructions, 0);
        }
    }

    IMG_AddInstrumentFunction(ReplaceNonDeterministicRoutines, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddFiniFunction(Fini, 0);
    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
