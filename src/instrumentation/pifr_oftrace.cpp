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
using tracing::rwflow::ACQUIRE;
using tracing::rwflow::RELEASE;
using tracing::rwflow::ThreadedTrace;

std::ofstream TraceFile;
std::ofstream ImgFile;

std::set<std::string> Acquires;
std::set<std::string> Releases;

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");
KNOB<std::string> KnobTraceFile(KNOB_MODE_WRITEONCE, "pintool", "trace",
                                "/mnt/ramdisk/trace.txt", "trace file");
KNOB<std::string> KnobImgFile(KNOB_MODE_WRITEONCE, "pintool", "img",
                              "/mnt/ramdisk/img.txt", "trace file");
KNOB<std::string> KnobAcqFile(KNOB_MODE_WRITEONCE, "pintool", "acq",
                              "/mnt/ramdisk/acq.txt", "acquire calls");
KNOB<std::string> KnobRelFile(KNOB_MODE_WRITEONCE, "pintool", "rel",
                              "/mnt/ramdisk/rel.txt", "release calls");


void StoreHandler(THREADID threadid, ADDRINT address, uint32_t size, ADDRINT ip, uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        // FIXME: differenciate store types?
        ThreadedTrace(TraceFile, threadid, PM_WRITE, img_id, ip, address, size);
    }
    PIN_UnlockClient();
}

void LoadHandler(THREADID threadid, ADDRINT address, uint32_t size, ADDRINT ip, uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        ThreadedTrace(TraceFile, threadid, PM_READ, img_id, ip, address, size);
    }
    PIN_UnlockClient();
}

void FlushHandler(THREADID threadid, ADDRINT address, ADDRINT ip, Instruction flush_type,
                  uint32_t img_id) {
    PIN_LockClient();
    if (IsPMAddress(address, 0)) {
        ThreadedTrace(TraceFile, threadid, FLUSH, img_id, ip, address, 64);
    }
    PIN_UnlockClient();
}

void SyncHandler(THREADID threadid, ADDRINT ip, Instruction sync_type, uint32_t img_id) {
    PIN_LockClient();
    ThreadedTrace(TraceFile, threadid, sync_type, img_id, ip, 0, 0);
    PIN_UnlockClient();
}

void InstrumentCollectTrace(IMG img, VOID* v) {
    UINT32 img_id = IMG_Id(img);
    // ImgFile << img_id << std::endl;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            // rtn name in Acquires instrument
            std::string rtn_name = RTN_Name(rtn);
            if (Acquires.find(rtn_name) != Acquires.end()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)SyncHandler,
                    IARG_THREAD_ID, IARG_ADDRINT, RTN_Address(rtn),
                    IARG_UINT64, ACQUIRE, IARG_UINT32,
                    img_id, IARG_END);
                RTN_Close(rtn);
                std::cout << "Acquire instrumented " << rtn_name << std::endl;
                continue;
            }
            if (Releases.find(rtn_name) != Releases.end()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)SyncHandler,
                    IARG_THREAD_ID, IARG_ADDRINT, RTN_Address(rtn),
                    IARG_UINT64, RELEASE, IARG_UINT32,
                    img_id, IARG_END);
                RTN_Close(rtn);
                std::cout << "Release instrumented " << rtn_name << std::endl;
                continue;
            }

            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins);
                 ins = INS_Next(ins)) {
                ADDRINT base_addr = INS_Address(ins) - IMG_LowAddress(img);
                if (INS_IsCacheLineFlush(ins)) {
                    // flushes are PIN memory reads, need to come before in
                    // these conditions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FlushHandler,
                                   IARG_THREAD_ID, IARG_MEMORYOP_EA, 0, IARG_ADDRINT, base_addr,
                                   IARG_UINT64, GetFlushType(ins), IARG_UINT32,
                                   img_id, IARG_END);
                } else if (INS_IsMemoryWrite(ins) && !IsMovnt(ins)) {
                    if (img_id != 1) continue;
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StoreHandler,
                                   IARG_THREAD_ID, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_UINT32, img_id,
                                   IARG_END);
                } else if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
                    if (img_id != 1) continue;
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LoadHandler,
                                   IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_UINT32, img_id,
                                   IARG_END);
                }
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

    std::string line;
    std::ifstream acquire_file(KnobAcqFile.Value().c_str());
    std::ifstream release_file(KnobRelFile.Value().c_str());
    while (std::getline(acquire_file, line)) {
        Acquires.insert(line);
    }
    while (std::getline(release_file, line)) {
        Releases.insert(line);
    }

    CollectTrace();

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
