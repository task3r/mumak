/*
 *  Mumak Basic Fault Injection
 */

#include <execinfo.h>
#include <stdio.h>
#include <time.h>

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "fp_tree.h"
#include "pin.H"
#include "utils.hpp"

// Map used to track all visited basic blocks
static std::unordered_map<ADDRINT, bool> bbl_visited;
bool track_bb = false;

// Variable used to track whether a flush instruction was encountered in a basic
// block
bool target_encountered = false;

// Variable used to track whether the tool can start tracking the vulnerability
// points or injecting faults depending on the toolMode
bool fi_enabled = false;

// Target instruction opcode
xed_iclass_enum_t instruction_type;
bool detect_all_fps = false;
bool ignore_rmw = false;
bool performed_store_since_last_fp = false;

// If the target instruction is a non-temporal store
bool target_is_movnt = false;

// Variable to decide whether a flush was encountered in a basic block
bool found_target = false;

// Tool Arguments
const char *tree_out, *fp_count_out, *backtrace_out;
bool is_injecting;
IPOINT failure_location;

// Failure Point Tree root
fp_tree::FPTraceAddr *root = NULL;

uint64_t target_fp;
uint64_t current_fp = 0;
bool fail_only_at_target = false;

// Function to instrument failure points
// it either inserts a fault or adds an FP to the tree depending on the knob
VOID InstrumentFPs(const CONTEXT *ctxt) {
    if (fi_enabled) {
        found_target = true;
        void *addrs[100];
        int size;
        if (is_injecting) {
            PIN_LockClient();
            size = PIN_Backtrace(ctxt, addrs, 100);
            bool inject = fp_tree::InjectFP(&root, addrs, size);
            PIN_UnlockClient();
            if (inject) {
                if (fail_only_at_target && current_fp != target_fp) {
                    current_fp++;
                    return;
                }
                PIN_LockClient();
                fp_tree::PrintBacktraceSymbols(addrs, size, backtrace_out);
                PIN_UnlockClient();
                PIN_ExitApplication(1);
            }
        } else {
            PIN_LockClient();
            size = PIN_Backtrace(ctxt, addrs, 100);
            fp_tree::InsertFPTrace(&root, addrs, size);
            PIN_UnlockClient();
        }
    }
}

VOID InstrumentFPsWrapper(const CONTEXT *ctxt) {
    if (performed_store_since_last_fp) {
        InstrumentFPs(ctxt);
    }
    performed_store_since_last_fp = false;
}

void FPWriteHandler(ADDRINT address, uint32_t size, CONTEXT *ctxt) {
    if (IsPMAddress(address, size)) {
        InstrumentFPs(ctxt);
    }
}

void NonFPWriteHandler(ADDRINT address, uint32_t size) {
    if (IsPMAddress(address, size)) {
        performed_store_since_last_fp = true;
    }
}

void RMWFPHandler(const CONTEXT *ctxt, ADDRINT address, uint32_t size) {
    if (IsPMAddress(address, size)) {
        InstrumentFPs(ctxt);
    } else if (performed_store_since_last_fp) {  // not on PM, but acts as fence
        InstrumentFPs(ctxt);
    }
    performed_store_since_last_fp = false;
}

VOID InstrumentAll(INS ins, VOID *v) {
    int opcode = INS_Opcode(ins);
    switch (opcode) {
        case XED_ICLASS_CLWB:
        case XED_ICLASS_CLFLUSHOPT:
        case XED_ICLASS_CLFLUSH:
        case XED_ICLASS_SFENCE:
        case XED_ICLASS_MOVNTDQ:
        case XED_ICLASS_MOVNTDQA:
        case XED_ICLASS_MOVNTI:
        case XED_ICLASS_MOVNTPD:
        case XED_ICLASS_MOVNTPS:
        case XED_ICLASS_MOVNTQ:
        case XED_ICLASS_MOVNTSD:
        case XED_ICLASS_MOVNTSS:
        case XED_ICLASS_VMOVNTDQ:
        case XED_ICLASS_VMOVNTDQA:
        case XED_ICLASS_VMOVNTPD:
        case XED_ICLASS_VMOVNTPS:
            if (track_bb) {
                target_encountered = true;
            } else {
                fi_enabled = true;
            }
            INS_InsertCall(ins, failure_location, (AFUNPTR)InstrumentFPsWrapper,
                           IARG_CONST_CONTEXT, IARG_END);
            break;
        default:
            if (!ignore_rmw && INS_IsAtomicUpdate(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RMWFPHandler,
                               IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA,
                               IARG_MEMORYWRITE_SIZE, IARG_END);
            } else if (INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)NonFPWriteHandler,
                               IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                               IARG_END);
            }
    }
}

VOID InstrumentStores(INS ins, VOID *v) {
    if (INS_IsMemoryWrite(ins)) {
        if (track_bb) {
            target_encountered = true;
        } else {
            fi_enabled = true;
        }
        INS_InsertCall(ins, failure_location, (AFUNPTR)FPWriteHandler,
                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                       IARG_CONST_CONTEXT, IARG_END);
    }
}

// Instrument the target instructions
VOID InstrumentOthers(INS ins, VOID *v) {
    int opcode = INS_Opcode(ins);
    if ((opcode == instruction_type) || (target_is_movnt && IsMovnt(opcode))) {
        if (track_bb) {
            target_encountered = true;
        } else {
            fi_enabled = true;
        }
        INS_InsertCall(ins, failure_location, (AFUNPTR)InstrumentFPs,
                       IARG_CONST_CONTEXT, IARG_END);
    }
}

// Trace Analysis Calls

// Function to track all visited basic blocks after at least one flush
// instruction was encountered
VOID BblTracker(ADDRINT address) {
    if (target_encountered) {
        if (!bbl_visited[address]) {
            fi_enabled = true;
            bbl_visited[address] = true;
        }
    }
}

// Function to track whenever we exit a basic block and encountered at least one
// flush instruction If this happens we disable the foundFlush and fiEnabled
// variables
VOID ExitBlock() {
    if (found_target) {
        fi_enabled = false;
        found_target = false;
    }
}

// Instrument the basic blocks
VOID Trace(TRACE trace, VOID *v) {
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        INS ins = BBL_InsHead(bbl);
        ADDRINT address = INS_Address(ins);
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)BblTracker, IARG_ADDRINT,
                       address, IARG_END);

        INS last_ins = BBL_InsTail(bbl);
        INS_InsertCall(last_ins, IPOINT_BEFORE, (AFUNPTR)ExitBlock, IARG_END);
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v) {
    if (!is_injecting) {
        std::ofstream OutFile;
        OutFile.open(fp_count_out);
        OutFile << fp_tree::GetUniqFPs() << std::endl;
        OutFile.close();
    }
    fp_tree::Serialize(root, tree_out);

    fp_tree::Clear();
}

VOID PrepareForFini(VOID *v) {
    PIN_LockClient();
    // PrintTraces(root);
    std::cout << "Unique FPs:" << fp_tree::GetUniqFPs() << std::endl;
    PIN_UnlockClient();
}

/* ===================================================================== */
/*  Knobs                                                                */
/* ===================================================================== */

KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "count-out",
                                 "/tmp/bfi-fp-count.out", "output file name");

KNOB<std::string> KnobBacktraceOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                                          "trace-out", "/tmp/fp-backtrace.out",
                                          "output file name");

KNOB<bool> KnobInject(KNOB_MODE_WRITEONCE, "pintool", "inject", "false",
                      "inject fault, otherwise count failure points");

KNOB<std::string> KnobFailAtTarget(KNOB_MODE_WRITEONCE, "pintool",
                                   "fail_target", "-1",
                                   "fail only at target failure point");

KNOB<bool> KnobInjectBefore(
    KNOB_MODE_WRITEONCE, "pintool", "inject-before", "true",
    "inject fault before fp, otherwise inject afterwards");

KNOB<std::string> KnobTreeOutputFile(KNOB_MODE_WRITEONCE, "pintool", "tree",
                                     "out.bin", "tree output file name");

KNOB<std::string> KnobAllocSize(KNOB_MODE_WRITEONCE, "pintool", "alloc",
                                "32768", "initial memory allocation");

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");

KNOB<std::string> KnobTargetInstruction(
    KNOB_MODE_WRITEONCE, "pintool", "target", "all",
    "target assembly instruction to count/inject failure points");

KNOB<bool> KnobTrackBasicBlocks(
    KNOB_MODE_WRITEONCE, "pintool", "bb", "false",
    "consider failure points only for unvisited basic blocks");

INT32 Usage() {
    std::cerr
        << "This tool detects vulnerability points reached following a unique "
           "paths and inserts a fault at specific vulnerability points."
        << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[]) {
    // Initialize pin
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // Decide on wether we are counting failure points or injecting faults
    is_injecting = KnobInject.Value();
    tree_out = KnobTreeOutputFile.Value().c_str();
    pm_mount = KnobPMMount.Value().c_str();
    fp_tree::Init(atoi(KnobAllocSize.Value().c_str()));
    if (is_injecting) {
        fp_tree::DeSerialize(&root, tree_out);
        int target_fp_aux = atoi(KnobFailAtTarget.Value().c_str());
        if (target_fp_aux >= 0) {
            fail_only_at_target = true;
            target_fp = target_fp_aux;
        }
    }

    // Decide on the application model to inject failures at
    failure_location = KnobInjectBefore.Value() ? IPOINT_BEFORE : IPOINT_AFTER;

    // Decide on the instruction to count/inject failures at
    bool target_stores = false;
    std::string target_instruction = KnobTargetInstruction.Value();
    if (target_instruction.compare("clwb") == 0) {
        instruction_type = XED_ICLASS_CLWB;
    } else if (target_instruction.compare("clflushopt") == 0) {
        instruction_type = XED_ICLASS_CLFLUSHOPT;
    } else if (target_instruction.compare("clflush") == 0) {
        instruction_type = XED_ICLASS_CLFLUSH;
    } else if (target_instruction.compare("sfence") == 0) {
        instruction_type = XED_ICLASS_SFENCE;
    } else if (target_instruction.compare("movnt") == 0) {
        target_is_movnt = true;
    } else if (target_instruction.compare("store") == 0) {
        target_stores = true;
    } else if (target_instruction.compare("all") == 0) {
        detect_all_fps = true;
    } else if (target_instruction.compare("allrmw") == 0) {
        detect_all_fps = true;
        ignore_rmw = true;
    } else {
        return -1;
    }

    // Open file to output final count of failure points
    fp_count_out = KnobOutputFile.Value().c_str();
    backtrace_out = KnobBacktraceOutputFile.Value().c_str();

    // Register Instruction to be called to instrument instructions
    if (detect_all_fps) {
        PIN_AddSyscallEntryFunction(SyscallEntry, 0);
        PIN_AddSyscallExitFunction(SyscallExit, 0);
        INS_AddInstrumentFunction(InstrumentAll, 0);
    } else if (target_stores) {
        PIN_AddSyscallEntryFunction(SyscallEntry, 0);
        PIN_AddSyscallExitFunction(SyscallExit, 0);
        INS_AddInstrumentFunction(InstrumentStores, 0);
    } else {
        INS_AddInstrumentFunction(InstrumentOthers, 0);
    }

    // Register ReplaceNonDeterministicRoutines to instrument any
    // non-deterministic method calls
    IMG_AddInstrumentFunction(ReplaceNonDeterministicRoutines, 0);

    // Register Trace to instrument the basic blocks
    track_bb = KnobTrackBasicBlocks.Value();
    if (track_bb) TRACE_AddInstrumentFunction(Trace, 0);

    // Register PrepareForFini to be called before application exits
    // (when it still has accces to application symbols, etc)
    PIN_AddPrepareForFiniFunction(PrepareForFini, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
