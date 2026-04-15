// Copyright 2025 João Gonçalves
#ifndef PIFR_RUNTIME_BACKEND_TBB
#define PIFR_RUNTIME_BACKEND_TBB

#include "./backend.hpp"

#if __has_include(<oneapi/tbb.h>)
    #include <oneapi/tbb.h>
    #include <oneapi/tbb/concurrent_hash_map.h>
    #include <oneapi/tbb/concurrent_unordered_map.h>
    #include <oneapi/tbb/concurrent_unordered_set.h>
#elif __has_include(<tbb/tbb.h>)
    #include <tbb/tbb.h>
    #include <tbb/concurrent_hash_map.h>
    #include <tbb/concurrent_unordered_map.h>
    #include <tbb/concurrent_unordered_set.h>
#else
    #error "TBB header not found. Install TBB or adjust include paths."
#endif

typedef tbb::concurrent_unordered_multimap<intptr_t, PIFR>::iterator PIFRitr;

class TBBBackend : public PIFRBackend {
    tbb::concurrent_unordered_multimap<intptr_t, PIFR> pifrs;
    tbb::concurrent_unordered_map<pid_t, tbb::concurrent_vector<PIFRitr>>
        pifrs_to_close;
    pthread_rwlock_t pifrs_mutex;
public:
    void init() override;
    size_t fini() override;
    PIFR* build_pifr(intptr_t pifr_id, bool is_write, size_t size, pid_t tid, intptr_t start_iaddr, std::vector<uint64_t> backtrace) override;
    std::vector<PIFR> find_overlaps(intptr_t mem_addr, PIFR *p) override;
    bool open_region(intptr_t mem_addr, PIFR p) override;
    void flush(pid_t tid, intptr_t mem_addr, size_t size) override;
    void end_regions(pid_t tid) override;
};


#endif
