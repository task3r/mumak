#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "../utils/cvector.h"
#include "pm_addr_checker.h"

static bool inited = false;
static const char* pmem_mount;

typedef struct {
    intptr_t start;
    size_t size;
} alloc;

static cvector_vector_type(alloc) allocs;

// Structure used when instrumenting mmap allocations
bool fd_points_to_pmem(int fd) {
    char fd_path[32];
    sprintf(fd_path, "/proc/self/fd/%d", fd);
    char file_path[100];
    int size = readlink(fd_path, file_path, 100);
    if (size != -1) {
        int i = 0;
        while (pmem_mount[i] != '\0' && i <= size) {
            if (pmem_mount[i] != file_path[i]) return false;
            i++;
        }
        return true;
    }

    return false;
}

void* (*libc_mmap)(void* start, size_t length, int prot, int flags, int fd,
                   off_t offset);

int (*libc_munmap)(void* addr, size_t length);

void* (*libc_memcpy)(void* dest, const void* src, size_t n);

void* (*libc_memset)(void* s, int c, size_t n);

void init() {
    if (inited) return;

    libc_mmap = (void* (*)(void*, size_t, int, int, int, off_t))dlsym(RTLD_NEXT,
                                                                      "mmap");
    if (libc_mmap == NULL) {
        fprintf(stderr, "dlsym(\"mmap\") failed\n");
    }

    libc_munmap = (int (*)(void*, size_t))dlsym(RTLD_NEXT, "munmap");
    if (libc_munmap == NULL) {
        fprintf(stderr, "dlsym(\"munmap\") failed\n");
    }
    libc_memcpy =
        (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
    if (libc_memcpy == NULL) {
        fprintf(stderr, "dlsym(\"memcpy\") failed\n");
    }
    libc_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
    if (libc_memset == NULL) {
        fprintf(stderr, "dlsym(\"memset\") failed\n");
    }

    pmem_mount = getenv("PMEM_MOUNT");
    if (pmem_mount == NULL) {
        fprintf(stderr, "PMEM_MOUNT not set\n");
        return;
    }
    inited = true;
    fprintf(stderr, "mmap_wrapper loaded.\n");
}

void* mmap(void* start, size_t length, int prot, int flags, int fd,
           off_t offset) {
    init();
    void* ptr = libc_mmap(start, length, prot, flags, fd, offset);

    // MAP_SHARED or MAP_SHARED_VALIDATE flags and fd poinst to PM
    if (ptr != (void*)-1 &&
        (((flags & 0x01) == 0x01) || ((flags & 0x03) == 0x03)) &&
        fd_points_to_pmem(fd)) {
        alloc allocation = {(intptr_t)ptr, length};
        cvector_push_back(allocs, allocation);
    }
    return ptr;
}

int munmap(void* addr, size_t length) {
    init();
    int ret = libc_munmap(addr, length);
    if (ret == 0) {
        for (size_t i = 0; i < cvector_size(allocs); ++i) {
            alloc allocation = allocs[i];
            if (((intptr_t)addr == allocation.start)) {
                cvector_erase(allocs, i);
                break;
            }
        }
    }
    return ret;
}

void (*pifrrt_start_pifr)(intptr_t pifr_id, bool is_write, intptr_t mem_addr,
                          size_t size, intptr_t rbp, intptr_t rip);
void call_libpifrrt(intptr_t pifr_id, bool is_write, intptr_t mem_addr,
                    size_t size, intptr_t rbp, intptr_t rip) {
    if (pifrrt_start_pifr == NULL) {
        char lib_path[100];
        sprintf(lib_path, "%s/libpifrrt.so", getenv("LIBPIFRRT_ROOT"));
        void* handle = dlopen(lib_path, RTLD_LAZY);
        if (handle == NULL) {
            fprintf(stderr, "dlopen(\"%s\") failed\n", lib_path);
            return;
        }
        pifrrt_start_pifr =
            (void (*)(intptr_t, bool, intptr_t, size_t, intptr_t,
                      intptr_t))dlsym(handle, "pifrrt_start_pifr");
    }
    if (pifrrt_start_pifr != NULL)
        pifrrt_start_pifr(pifr_id, is_write, mem_addr, size, rbp, rip);
    else
        fprintf(stderr, "dlsym(\"pifrrt_start_pifr\") failed\n");
}

#define GET_RET_ADDR \
    (intptr_t) __builtin_extract_return_addr(__builtin_return_address(0)) - 5
#define GET_FRAME_PTR (intptr_t) __builtin_frame_address(0)

void* memcpy(void* dest, const void* src, size_t n) {
    init();
    if (is_pmem_addr((intptr_t)dest, n)) {
        // for (int i = 0; i < n; i = i + 8) {
        call_libpifrrt(GET_RET_ADDR, true, (intptr_t)dest, n, GET_FRAME_PTR,
                       GET_RET_ADDR);
        // }
    }
    if (is_pmem_addr((intptr_t)src, n)) {
        // for (int i = 0; i < n; i = i + 8) {
        call_libpifrrt(GET_RET_ADDR, false, (intptr_t)src, n, GET_FRAME_PTR,
                       GET_RET_ADDR);
        // }
    }
    return libc_memcpy(dest, src, n);
}

void* memset(void* s, int c, size_t n) {
    init();
    if (is_pmem_addr((intptr_t)s, n)) {
        // for (int i = 0; i < n; i = i + 8) {
        call_libpifrrt(GET_RET_ADDR, true, (intptr_t)s, n, GET_FRAME_PTR,
                       GET_RET_ADDR);
        // }
    }
    return libc_memset(s, c, n);
}

static long total = 0;
static long pmem = 0;
EXPORT int is_pmem_addr(intptr_t addr, size_t size) {
    __atomic_fetch_add(&total, 1, __ATOMIC_SEQ_CST);
    intptr_t range_end = addr + size;

    for (size_t i = 0; i < cvector_size(allocs); ++i) {
        alloc allocation = allocs[i];
        if ((addr >= allocation.start) &&
            ((range_end) <= (allocation.start + allocation.size))) {
            __atomic_fetch_add(&pmem, 1, __ATOMIC_SEQ_CST);
            return true;
        }
    }

    return false;
}

EXPORT void pmem_stats() {
    fprintf(stderr, "pmem_stats: %ld/%ld (%f%%)\n", pmem, total,
            (float)pmem / total * 100);
}

void _init(void) { init(); }
