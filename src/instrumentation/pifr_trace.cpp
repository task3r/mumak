#include <fstream>
#include <iostream>

#include "../utils/tracing.hpp"
#include "pin.H"
#include "pin_utils.hpp"
#include "types_vmapi.PH"

using tracing::pm::Instruction;
using tracing::rwflow::BRANCH;
using tracing::rwflow::FLUSH;
using tracing::rwflow::PM_READ;
using tracing::rwflow::PM_WRITE;
using tracing::rwflow::Trace;

std::ofstream TraceFile;
std::ofstream ImgFile;

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");
KNOB<std::string> KnobTraceFile(KNOB_MODE_WRITEONCE, "pintool", "trace",
                                "/mnt/ramdisk/trace.txt", "trace file");
KNOB<std::string> KnobImgFile(KNOB_MODE_WRITEONCE, "pintool", "img",
                              "/mnt/ramdisk/img.txt", "trace file");

void IndirectBranchHandler(ADDRINT src, ADDRINT dst, INT32 taken,
                           uint32_t img_id) {
    PIN_LockClient();
    Trace(TraceFile, BRANCH, img_id, src, dst, taken);
    PIN_UnlockClient();
}

void StoreHandler(ADDRINT address, uint32_t size, ADDRINT ip, uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        // FIXME: differenciate store types?
        Trace(TraceFile, PM_WRITE, img_id, ip, address, size);
    }
    PIN_UnlockClient();
}

void LoadHandler(ADDRINT address, uint32_t size, ADDRINT ip, uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        Trace(TraceFile, PM_READ, img_id, ip, address, size);
    }
    PIN_UnlockClient();
}

void FlushHandler(ADDRINT address, ADDRINT ip, Instruction flush_type,
                  uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, 0)) {
        // FIXME: differenciate flush types?
        Trace(TraceFile, FLUSH, img_id, ip, address, 64);
    }
    PIN_UnlockClient();
}

void InstrumentCollectTrace(IMG img, VOID* v) {
    UINT32 img_id = IMG_Id(img);
    ImgFile << img_id << " " << IMG_Name(img) << std::endl;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins);
                 ins = INS_Next(ins)) {
                ADDRINT base_addr = INS_Address(ins) - IMG_LowAddress(img);
                /*if (INS_IsIndirectControlFlow(ins) && !INS_IsXbegin(ins) &&
                    !INS_IsXend(ins) && !INS_IsCall(ins)) {
                    INS_InsertCall(
                        ins, IPOINT_BEFORE, (AFUNPTR)IndirectBranchHandler,
                        base_addr, IARG_BRANCH_TARGET_ADDR, IARG_ADDRINT,
                        IARG_BRANCH_TAKEN, IARG_UINT32, img_id, IARG_END);
                } else*/
                if (INS_IsCacheLineFlush(ins)) {
                    // flushes are PIN memory reads, need to come before in
                    // these conditions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FlushHandler,
                                   IARG_MEMORYOP_EA, 0, IARG_ADDRINT, base_addr,
                                   IARG_UINT64, GetFlushType(ins), IARG_UINT32,
                                   img_id, IARG_END);
                } else if (INS_IsMemoryWrite(ins) && !IsMovnt(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StoreHandler,
                                   IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_UINT32, img_id,
                                   IARG_END);
                } else if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LoadHandler,
                                   IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_UINT32, img_id,
                                   IARG_END);
                }
                // else if (IsFence(ins)) {
                //     INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FenceHandler,
                //                    IARG_ADDRINT, base_addr, IARG_END);
                // }
            }
            RTN_Close(rtn);
        }
    }
}

void Fini(INT32 code, VOID* v) { TraceFile.close(); }

void CollectTrace() {
    // IMG_AddInstrumentFunction(GetOpAndArgs, 0);
    pm_mount = KnobPMMount.Value().c_str();
    TraceFile.open(KnobTraceFile.Value().c_str(), std::ofstream::out |
                                                      std::ofstream::trunc |
                                                      std::ofstream::binary);
    ImgFile.open(KnobImgFile.Value().c_str(),
                 std::ofstream::out | std::ofstream::trunc);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    IMG_AddInstrumentFunction(InstrumentCollectTrace, 0);
    PIN_AddFiniFunction(Fini, 0);
}

INT32 Usage() {
    std::cerr << "This tool collects trace to construct PM CFGs." << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    // Initialize pin
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    CollectTrace();

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
