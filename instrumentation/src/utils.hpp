#ifndef MUMAK_UTILS
#define MUMAK_UTILS

#if defined(TARGET_MAC)
#include <sys/syscall.h>
#else
#include <syscall.h>
#endif

#include <execinfo.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "pin.H"
#include "trace.hpp"

static bool found_alloc = false;
static const char *pm_mount;

// Structure used when instrumenting mmap allocations
struct PMAllocation {
    PMAllocation(uint64_t size) : size(size), start(0) {}

    uint64_t get_size() { return size; }

    uint64_t get_start() { return start; }

    uint64_t get_end() { return start + size; }

    void set_start(uint64_t new_start) { start = new_start; }

    uint64_t size;
    uint64_t start;
};

static std::vector<PMAllocation> allocs;

bool FDPointsToPM(int fd) {
    char fd_path[32];
    sprintf(fd_path, "/proc/self/fd/%d", fd);
    char file_path[100];
    int size = readlink(fd_path, file_path, 100);
    if (size != -1) {
        int i = 0;
        while (pm_mount[i] != '\0' && i <= size) {
            if (pm_mount[i] != file_path[i]) return false;
            i++;
        }
        return true;
    }
    return false;
}

// Save the size for mmap allocation
VOID SysBefore(ADDRINT ip, ADDRINT num, ADDRINT size, ADDRINT flags,
               ADDRINT fd) {
    // If the mmap call is not creating a private mapping, that is, they use the
    // MAP_SHARED or MAP_SHARED_VALIDATE flags.
    if (num == SYS_mmap &&
        (((flags & 0x01) == 0x01) || ((flags & 0x03) == 0x03)) &&
        FDPointsToPM(fd)) {
        PMAllocation allocation = PMAllocation(size);
        allocs.push_back(allocation);
        found_alloc = true;
    }
}

// Save the returned memory address for each allocation
VOID SysAfter(ADDRINT return_value) {
    if (found_alloc == true) {
        allocs.back().set_start(return_value);
        found_alloc = false;
    }
}

VOID SyscallEntry(THREADID thread, CONTEXT *ctxt, SYSCALL_STANDARD std,
                  VOID *v) {
    SysBefore(PIN_GetContextReg(ctxt, REG_INST_PTR),
              PIN_GetSyscallNumber(ctxt, std),
              PIN_GetSyscallArgument(ctxt, std, 1),
              PIN_GetSyscallArgument(ctxt, std, 3),
              PIN_GetSyscallArgument(ctxt, std, 4));
}

VOID SyscallExit(THREADID thread, CONTEXT *ctxt, SYSCALL_STANDARD std,
                 VOID *v) {
    SysAfter(PIN_GetSyscallReturn(ctxt, std));
}

// Function will return true if the write is operating on Pmem
bool IsPMAddress(ADDRINT address, uint32_t size) {
    uint64_t range_end = address + size;

    for (PMAllocation allocation : allocs) {
        if ((address >= allocation.get_start()) &&
            ((range_end) <= (allocation.get_end()))) {
            return true;
        }
    }

    return false;
}

// Function to check if instruction is a non-temporal store
bool IsMovnt(int opcode) {
    bool inst_is_movnt = false;
    switch (opcode) {
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
            inst_is_movnt = true;
            break;
        default:
            inst_is_movnt = false;
    }
    return inst_is_movnt;
}

// Check if instruction is a fence
bool IsFence(int opcode) {
    bool inst_is_fence = false;
    switch (opcode) {
        case XED_ICLASS_SFENCE:
        case XED_ICLASS_MFENCE:
            inst_is_fence = true;
            break;
        default:
            inst_is_fence = false;
    }
    return inst_is_fence;
}

trace::Instruction GetFlushType(int opcode) {
    switch (opcode) {
        case XED_ICLASS_CLFLUSH:
            return trace::Instruction::CLFLUSH;
        case XED_ICLASS_CLFLUSHOPT:
            return trace::Instruction::CLFLUSHOPT;
        case XED_ICLASS_CLWB:
            return trace::Instruction::CLWB;
        default:
            return trace::Instruction::ERROR;
    }
}

static int rand_value = 0;
// Wrapper function for the rand() function to a set a constant value
int RandWrapper() { return ++rand_value; }

// Instrumentation of the image, consisting of the routines
VOID ReplaceNonDeterministicRoutines(IMG img, VOID *v) {
    RTN rand_rtn = RTN_FindByName(img, "rand");
    if (RTN_Valid(rand_rtn)) {
        RTN_ReplaceSignature(rand_rtn, AFUNPTR(RandWrapper), IARG_END);
    }
}

// Structure to hold the symbols addresses and symbols size
struct RecordOperation {
    void *addresses[BACKTRACE_ADDRESSES_LIMIT];
    int num_addresses;

    RecordOperation() {}

    RecordOperation(const CONTEXT *ctxt) {
        // Obtain the resulting addresses of PIN_Backtrace() in order to further
        // check if it is required to obtain the backtrace
        PIN_LockClient();
        num_addresses =
            PIN_Backtrace(ctxt, addresses, BACKTRACE_ADDRESSES_LIMIT);
        PIN_UnlockClient();
    }
};

// Function used to obtain a backtrace
bool ReportBug(std::map<uint32_t, bool> &logged_addresses,
               std::string &trace_container, const char *type,
               void **backtrace_addresses, int backtrace_size,
               std::ofstream &out_file) {
    // Confirm if backtrace actually has new addresses
    std::set<void *> unique_addresses;
    std::string concat_addresses;
    std::hash<std::string> hasher;
    for (int i = 0; i < backtrace_size; i++) {
        char *byte = (char *)&backtrace_addresses[i];
        auto insertionRet = unique_addresses.insert(backtrace_addresses[i]);
        if (insertionRet.second) {
            concat_addresses.append(byte);
        }
    }
    // FIXME: this should include bug type
    size_t val = hasher(concat_addresses);

    if (!logged_addresses[val]) {
        char **strings;
        PIN_LockClient();
        strings = backtrace_symbols(backtrace_addresses, backtrace_size);
        PIN_UnlockClient();
        trace_container.clear();
        for (int i = 0; i < backtrace_size; i++) {
            std::string str = strings[i];
            trace_container.append(str + "\n");
        }
        free(strings);

        logged_addresses[val] = true;
        out_file << type << "\n" << trace_container << "\n";
        return true;
    }
    return false;
}

#endif  // !MUMAK_UTILS
