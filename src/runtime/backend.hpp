// Copyright 2025 João Gonçalves
#ifndef PIFR_RUNTIME_BACKEND
#define PIFR_RUNTIME_BACKEND

#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

struct PIFR {
    bool is_write;
    size_t size;
    pid_t tid;
    bool dirty;  // false -> writeback pending
    intptr_t start_iaddr;
    intptr_t pifr_id;
    std::vector<uint64_t> backtrace;
};

class PIFRBackend {
public:
    virtual PIFR* build_pifr(intptr_t pifr_id, bool is_write, size_t size, pid_t tid, intptr_t start_iaddr,std::vector<uint64_t> backtrace) = 0;
    virtual void init() = 0;
    virtual std::vector<PIFR> find_overlaps(intptr_t mem_addr, PIFR *p) = 0;
    virtual bool open_region(intptr_t mem_addr, PIFR p) = 0;
    virtual void flush(pid_t tid, intptr_t mem_addr, size_t size) = 0;
    virtual void end_regions(pid_t tid) = 0;
    virtual size_t fini() = 0;
};

#endif
