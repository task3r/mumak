/*
 *  Mumak Online Trace Analysis
 */

#include <execinfo.h>
#include <stdio.h>
#include <sys/mman.h>

#include <fstream>
#include <iostream>
#include <string>

#include "logger.hpp"
#include "pin.H"
#include "trace.hpp"
#include "utils.hpp"

using std::endl;
using std::hash;
using std::map;
using std::ofstream;
using std::ostringstream;
using std::pair;
using std::set;
using std::string;
using std::vector;

map<uint32_t, bool> logged_addresses;
string trace_container;
uint32_t total_bugs = 0, actual_bugs = 0, current_instruction_id = 0;
ofstream bugs_out;
std::vector<trace::Bug> bugs;

map<uint32_t, RecordOperation> recorded_ops;

void RecordPossibleBugs(const CONTEXT *ctxt) {
    if (bugs.size() > 0) {
        PIN_LockClient();
        void *backtrace_addresses[BACKTRACE_ADDRESSES_LIMIT];
        int backtrace_size =
            PIN_Backtrace(ctxt, backtrace_addresses, BACKTRACE_ADDRESSES_LIMIT);
        PIN_UnlockClient();
        for (auto bug : bugs) {
            total_bugs++;
            if (ReportBug(logged_addresses, trace_container,
                          trace::BugTypeToStr(bug.type), backtrace_addresses,
                          backtrace_size, bugs_out))
                actual_bugs++;
        }
        bugs.clear();
    }
}

void RecordPossibleBugs() {
    for (auto bug : bugs) {
        total_bugs++;
        RecordOperation op = recorded_ops[bug.instruction_id];
        if (ReportBug(logged_addresses, trace_container,
                      trace::BugTypeToStr(bug.type), op.addresses,
                      op.num_addresses, bugs_out))
            actual_bugs++;
    }
    bugs.clear();
}

void WriteHandler(const CONTEXT *ctxt, ADDRINT address, uint32_t size,
                  bool is_movnt) {
    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        trace::ta.ProcessStore(address, size, is_movnt, current_instruction_id,
                               bugs);
        if (recorded_ops.find(current_instruction_id) == recorded_ops.end()) {
            recorded_ops.insert(pair<uint32_t, RecordOperation>(
                current_instruction_id, RecordOperation(ctxt)));
        }
        RecordPossibleBugs();
        current_instruction_id++;
    }
}

void FlushHandler(const CONTEXT *ctxt, ADDRINT address,
                  trace::Instruction flush_type) {
    if (IsPMAddress(address, 0)) {  // only the start matters in this case
        COUNT_FLUSH;
        trace::ta.ProcessFlush(flush_type, address, current_instruction_id,
                               bugs);
        RecordPossibleBugs(ctxt);
        current_instruction_id++;
    }
}

void FenceHandler(const CONTEXT *ctxt) {
    COUNT_FENCE;
    std::vector<uint32_t> fully_persisted;
    trace::ta.ProcessFence(current_instruction_id, bugs, fully_persisted,
                           false);
    for (auto store_id : fully_persisted) recorded_ops.erase(store_id);
    RecordPossibleBugs(ctxt);
    current_instruction_id++;
}

void RMWHandler(const CONTEXT *ctxt, ADDRINT address, uint32_t size) {
    // RMW act as a fence, followed by a store, followed by a fence
    // but I think the second fence does not matter here since it still
    // needs to be flushed
    COUNT_RMW;
    std::vector<uint32_t> fully_persisted;
    trace::ta.ProcessFence(current_instruction_id, bugs, fully_persisted, true);
    for (auto store_id : fully_persisted) recorded_ops.erase(store_id);

    COUNT_STORE;
    if (IsPMAddress(address, size)) {
        COUNT_PM_STORE;
        trace::ta.ProcessStore(address, size, false, current_instruction_id,
                               bugs);
        if (recorded_ops.find(current_instruction_id) == recorded_ops.end()) {
            recorded_ops.insert(pair<uint32_t, RecordOperation>(
                current_instruction_id, RecordOperation(ctxt)));
        }
    }
    RecordPossibleBugs();
    current_instruction_id++;
}

// Instrumentation routine to set the analysis calls for the flush and store
// instructions
VOID Instruction(INS ins, VOID *v) {
    // Look for memory operands of an instruction to detect which memory
    // address the write is working with
    int opcode = INS_Opcode(ins);
    if (INS_IsAtomicUpdate(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RMWHandler,
                       IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE, IARG_END);
    } else if (INS_IsMemoryWrite(ins)) {
        bool is_movnt = IsMovnt(opcode);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteHandler,
                       IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE, IARG_BOOL, is_movnt, IARG_END);
    } else if (INS_IsCacheLineFlush(ins)) {
        // The flush instruction will contain the memory operand 0, as the
        // first operand will always be the flush address.
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FlushHandler,
                       IARG_CONST_CONTEXT, IARG_MEMORYOP_EA, 0, IARG_UINT64,
                       GetFlushType(opcode), IARG_END);
    } else if (IsFence(opcode)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FenceHandler,
                       IARG_CONST_CONTEXT, IARG_END);
    }
}

VOID BeforeFini(VOID *v) {
    // Check remaining write operations
    trace::ta.CheckRemainder(bugs);
    RecordPossibleBugs();

    // Output the number of logged instructions
    std::cerr << "Actual number of logged instructions: "
              << current_instruction_id << endl;

    // Output the number of detected bugs and warnings prior to removing the
    // duplicated ones
    std::cerr << "Total amount of bugs reported: " << total_bugs << endl;
    // std::cerr << "Total amount of warnings reported: " <<
    // totalNumberOfWarnings
    //   << endl;

    // Output the number of obtained bugs and warnings
    std::cerr << "Actual number of obtained bugs: " << actual_bugs << endl;
    // std::cerr << "Actual number of obtained warnings: "
    //   << actualNumberOfWarnings << endl;

    OUTPUT_INSTRUCTION_COUNT;
    bugs_out.close();
}

KNOB<std::string> KnobOutDir(KNOB_MODE_WRITEONCE, "pintool", "dir",
                             "/mnt/ramdisk/", "directory for tool i/o");

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");
INT32 Usage() {
    std::cerr << "This tool will trace all relevant instructions, generate "
                 "a state "
                 "for the stores modifying PM and detect possible bugs."
              << endl;
    std::cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    string out_path = KnobOutDir.Value();
    pm_mount = KnobPMMount.Value().c_str();

    string bugs_out_file = out_path + "/bug-report.txt";

    bugs_out.open(bugs_out_file.c_str(), ofstream::out | ofstream::trunc);

    INS_AddInstrumentFunction(Instruction, 0);
    IMG_AddInstrumentFunction(ReplaceNonDeterministicRoutines, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    // PIN_AddFiniFunction is unable to translate the logged symbols
    PIN_AddPrepareForFiniFunction(BeforeFini, 0);
    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
