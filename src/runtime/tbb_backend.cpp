#include "./tbb_backend.hpp"

void TBBBackend::init() {
    pthread_rwlock_init(&pifrs_mutex, NULL);
}

size_t TBBBackend::fini() {
    size_t still_open = pifrs.size();
    pifrs.clear();
    return still_open;
}

PIFR* TBBBackend::build_pifr(intptr_t pifr_id, bool is_write, size_t size, pid_t tid, intptr_t start_iaddr, std::vector<uint64_t> backtrace) {
    return new PIFR{
        .is_write = is_write,
        .size = size,
        .tid = tid,
        .dirty = false,
        .start_iaddr = start_iaddr,
        .pifr_id = pifr_id,
        .backtrace = backtrace
    };
}

std::vector<PIFR> TBBBackend::find_overlaps(intptr_t mem_addr, PIFR *p) {
    std::vector<PIFR> overlaps;
    pthread_rwlock_rdlock(&pifrs_mutex);
    auto range = pifrs.equal_range(mem_addr);
    for (auto itr = range.first; itr != range.second; itr++) {
        PIFR other_pifr = itr->second;
        if (p->pifr_id == other_pifr.pifr_id) {
            pthread_rwlock_unlock(&pifrs_mutex);
            return {};
        }
        if (p->tid != other_pifr.tid &&
            p->is_write == !other_pifr.is_write) {  // R/W or W/R
            overlaps.push_back(other_pifr);
        }
    }
    pthread_rwlock_unlock(&pifrs_mutex);
    return overlaps;
}

bool TBBBackend::open_region(intptr_t mem_addr, PIFR p) {
    if (pifrs_to_close.find(p.tid) == pifrs_to_close.end()) {
        pifrs_to_close[p.tid] = tbb::concurrent_vector<PIFRitr>();
    }
    auto pair = std::make_pair(mem_addr, p);
    pthread_rwlock_rdlock(&pifrs_mutex);
    auto new_itr = pifrs.insert(pair);
    pthread_rwlock_unlock(&pifrs_mutex);
    if (!p.dirty && new_itr.second) {
        pifrs_to_close[p.tid].push_back(new_itr.first);
    }
    return new_itr.second;
}

void TBBBackend::flush(pid_t tid, intptr_t mem_addr, size_t size) {
    if (pifrs_to_close.find(tid) == pifrs_to_close.end()) {
        return;
    }
    pthread_rwlock_rdlock(&pifrs_mutex);
    for (size_t offset = 0; offset < 64; offset++) {
        auto range = pifrs.equal_range(mem_addr + offset);
        for (auto itr = range.first; itr != range.second; itr++) {
            PIFR* pifr = &itr->second;
            if (pifr->tid == tid && pifr->dirty) {
                pifr->dirty = false;
                pifrs_to_close[pifr->tid].push_back(itr);
            }
        }
    }
    pthread_rwlock_unlock(&pifrs_mutex);
}

void TBBBackend::end_regions(pid_t tid) {
    pthread_rwlock_wrlock(&pifrs_mutex);
    for (PIFRitr itr : pifrs_to_close[tid]) {
        // LOG_DEBUG("(%d) [%p] end", mutex_gettid(), itr->second->pifr_id);
        pifrs.unsafe_erase(itr);
    }
    pifrs_to_close[tid].clear();
    pthread_rwlock_unlock(&pifrs_mutex);
}
