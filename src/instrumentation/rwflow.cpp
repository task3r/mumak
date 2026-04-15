/*
 *  Mumak RW Flow Recorder
 *
 */

#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

#include "../utils/tracing.hpp"
#include "pin.H"

using tracing::rwflow::READ;
using tracing::rwflow::Trace;
using tracing::rwflow::WRITE;

std::ofstream trace_out;
PIN_MUTEX mtx;

void TraceWrite(ADDRINT address, UINT64 ip, UINT32 img_id) {
    PIN_MutexLock(&mtx);
    Trace(trace_out, WRITE, img_id, ip, address);
    PIN_MutexUnlock(&mtx);
}

void TraceRead(ADDRINT address, UINT64 ip, UINT32 img_id) {
    PIN_MutexLock(&mtx);
    Trace(trace_out, READ, img_id, ip, address);
    PIN_MutexUnlock(&mtx);
}

VOID TraceInstructions(IMG img, VOID *v) {
    UINT32 img_id = IMG_Id(img);
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins);
                 ins = INS_Next(ins)) {
                ADDRINT base_addr = INS_Address(ins) - IMG_LowAddress(img);
                if (INS_IsMemoryWrite(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceWrite,
                                   IARG_MEMORYWRITE_EA, IARG_UINT64, base_addr,
                                   IARG_UINT32, img_id, IARG_END);
                } else if (INS_IsMemoryRead(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TraceRead,
                                   IARG_MEMORYREAD_EA, IARG_UINT64, base_addr,
                                   IARG_UINT32, img_id, IARG_END);
                }
            }
            RTN_Close(rtn);
        }
    }
}

VOID Fini(INT32 code, VOID *v) {
    trace_out.flush();
    trace_out.close();
    PIN_MutexFini(&mtx);
}

KNOB<std::string> KnobOutDir(KNOB_MODE_WRITEONCE, "pintool", "dir",
                             "/mnt/ramdisk/", "directory for tool i/o");

INT32 Usage() {
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    PIN_MutexInit(&mtx);

    std::string out_path = KnobOutDir.Value();
    std::string trace_out_file = out_path + "/rwflow-trace.txt";
    trace_out.open(trace_out_file.c_str(), std::ofstream::out |
                                               std::ofstream::trunc |
                                               std::ofstream::binary);

    IMG_AddInstrumentFunction(TraceInstructions, 0);

    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}
