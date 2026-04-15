#include "./runtime.hpp"

#include <pthread.h>

#include <atomic>
#include <utility>

#include "./pm_addr_checker.h"
#include "./tbb_backend.hpp"
#include "../patching/stdlib.c"
#include "./constants.h"
#include "./tracing.hpp"

#define LOG(...) debug_impl(__VA_ARGS__)
#ifdef DEBUG
#define LOG_DEBUG(...) debug_impl(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

thread_local clock_t prof_start;
#define TIME_FORMAT "%c %ld\n"
#define PROFILING_START \
    if (profiling) prof_start = clock()
#define PROFILING_END(name)                                          \
    {                                                                \
        if (profiling) {                                             \
            clock_t prof_end = clock();                              \
            long duration = prof_end - prof_start;                   \
            char buf[100];                                           \
            int n = snprintf(buf, 100, TIME_FORMAT, name, duration); \
            fwrite(buf, n, 1, times_f);                              \
        }                                                            \
    }

// Configs
static bool minimal_regions = false;
static bool sampling = false;
static int sampling_rate = 0;
static bool delay = false;
static int delay_micro = 0;
static int decay = 0;
static bool profiling = false;
static bool tracing = false;
static int report = 1;

static PIFRBackend* backend;

static uint64_t offset;
static pthread_mutex_t bt_mutex;
static FILE* bugs_f;
static FILE* bugs_bin;
static FILE* times_f;

static std::atomic_int op;
thread_local int sample = 0;
static std::atomic_int pifr_overlap_counter_rw;

EXPORT void pifrrt_set_offset(const void* addr, const void* base_addr) {
    offset = (uint64_t)addr - (uint64_t)base_addr;
    LOG("set offset: %p\n", offset);
}

std::vector<uint64_t> get_backtrace(uint64_t original_rbp,
                                    uint64_t original_rip) {
    std::vector<uint64_t> trace;
    uint64_t* rbp = (uint64_t*)original_rbp;
    trace.push_back(
        original_rip -
        offset);  // FIXME: when acc_iaddr goes away I need to sub offset
    for (size_t i = 0; i < 5; i++) {
        uint64_t* next_rbp = (uint64_t*)*(rbp);
        uint64_t rbp_i = (uint64_t)rbp;
        uint64_t next_rbp_i = (uint64_t)next_rbp;
        size_t diff =
            rbp_i > next_rbp_i ? rbp_i - next_rbp_i : next_rbp_i - rbp_i;
        if (diff > 8192) break;
        trace.push_back(*(rbp + 1) - offset - 5);
        rbp = next_rbp;
    }
    return trace;
}

void bug(intptr_t mem_addr, PIFR* pifr, PIFR* other_pifr) {
    if (report == 0) return;

    intptr_t id1, id2;
    if (pifr->pifr_id > other_pifr->pifr_id) {
        id1 = pifr->pifr_id;
        id2 = other_pifr->pifr_id;
    } else {
        id1 = other_pifr->pifr_id;
        id2 = pifr->pifr_id;
    }

    char buf[100];
    int n = snprintf(buf, 100, BUG_REPORT_FORMAT, mem_addr, id1, id2,
                     pifr->tid != other_pifr->tid);
    fwrite(buf, n, 1, bugs_f);

    if (report != 2) return;

    pthread_mutex_lock(&bt_mutex);
    fwrite(&mem_addr, sizeof(intptr_t), 1, bugs_bin);
    fwrite(&pifr->pifr_id, sizeof(intptr_t), 1, bugs_bin);
    fwrite(&other_pifr->pifr_id, sizeof(intptr_t), 1, bugs_bin);
    fwrite(&pifr->tid, sizeof(pid_t), 1, bugs_bin);
    fwrite(&other_pifr->tid, sizeof(pid_t), 1, bugs_bin);
    n = pifr->backtrace.size();
    fwrite(&n, sizeof(int), 1, bugs_bin);
    n = other_pifr->backtrace.size();
    fwrite(&n, sizeof(int), 1, bugs_bin);
    for (uint64_t addr : pifr->backtrace) {
        fwrite(&addr, sizeof(uint64_t), 1, bugs_bin);
    }
    for (uint64_t addr : other_pifr->backtrace) {
        fwrite(&addr, sizeof(uint64_t), 1, bugs_bin);
    }
    pthread_mutex_unlock(&bt_mutex);
}

// static std::unordered_map<intptr_t, PIFR*> pifrs;

static tbb::concurrent_unordered_map<pid_t, bool> threads;
static size_t sleeping;
std::atomic_int delay_counter;
static tbb::concurrent_unordered_map<intptr_t, short>
    schedule_locations;  // schedule_locations -> [ addr, prob ]
short get_location_delay_prob(intptr_t location) {
    auto itr = schedule_locations.find(location);
    if (itr != schedule_locations.end()) return itr->second;
    schedule_locations[location] = 100;
    return 100;
}
void decay_location_delay_prob(intptr_t location) {
    schedule_locations[location] -= decay;
}

static short randoms[RANDOM_SIZE];
static short available_randoms = 0;
static short next_random = 0;
short get_next_random() {
    if (next_random != available_randoms) return randoms[next_random++];
    available_randoms = getrandom(randoms, sizeof(randoms), 0) / 2;
    next_random = 1;
    return randoms[0];
}

bool should_delay(short prob) {
    if (!delay) return false;
    if (sleeping * 2 >= threads.size()) return false;
    short r = get_next_random() % 100;
    return r <= prob;
}

bool is_new_thread(pid_t tid) {
    auto itr = threads.find(tid);
    return itr == threads.end();
}

static tbb::concurrent_unordered_set<uint64_t> rw_bug_hashes;
inline uint64_t hashit(const char* p, size_t s) {
    uint64_t result = 0;
    const uint64_t prime = 31;
    for (size_t i = 0; i < s; ++i) {
        result = p[i] + (result * prime);
    }
    return result;
}
bool report_bug(intptr_t mem_addr, PIFR* pifr, PIFR* other_pifr) {
#define BUG_LINE_SIZE (sizeof(intptr_t) * 2 + 1)
    char bug_line[BUG_LINE_SIZE];
    size_t base = 0;
    if (pifr->pifr_id > other_pifr->pifr_id) {
        char* char_iaddr = reinterpret_cast<char*>(&pifr->pifr_id);
        for (size_t idx = 0; idx < sizeof(intptr_t); idx++, base++)
            bug_line[base] = char_iaddr[idx];
        char_iaddr = reinterpret_cast<char*>(&other_pifr->pifr_id);
        for (size_t idx = 0; idx < sizeof(intptr_t); idx++, base++)
            bug_line[base] = char_iaddr[idx];
        bug_line[base] = pifr->tid != other_pifr->tid ? '1' : '0';
    } else {
        char* char_iaddr = reinterpret_cast<char*>(&other_pifr->pifr_id);
        for (size_t idx = 0; idx < sizeof(intptr_t); idx++, base++)
            bug_line[base] = char_iaddr[idx];
        char_iaddr = reinterpret_cast<char*>(&pifr->pifr_id);
        for (size_t idx = 0; idx < sizeof(intptr_t); idx++, base++)
            bug_line[base] = char_iaddr[idx];
        bug_line[base] = pifr->tid != other_pifr->tid ? '1' : '0';
    }

    uint64_t hash = hashit(bug_line, BUG_LINE_SIZE);
    auto pair = rw_bug_hashes.insert(hash);
    if (pair.second)  // if the hash is unique
        bug(mem_addr, pifr, other_pifr);
    return pair.second;
}

EXPORT void pifrrt_start_pifr(intptr_t pifr_id, bool is_write,
                              intptr_t mem_addr, size_t size, intptr_t rbp,
                              intptr_t rip) {
    if (!is_pmem_addr(mem_addr, size)) return;

    pid_t tid = mutex_gettid();
    LOG_DEBUG("(%d) [%d] (%x) %s %p %d\n", tid, pifr_id, rip,
              is_write ? "w" : "r", mem_addr, size);

    if (tracing) {
        trace_start(tid, pifr_id, is_write, mem_addr, size);
        return;
    }

    if (is_new_thread(tid)) threads[tid] = false;
    if (threads.size() == 1) return;

    PROFILING_START;

    bool is_dirty = minimal_regions ? false : is_write;
    std::vector<uint64_t> backtrace;
    if (report == 2) backtrace = get_backtrace(rbp, rip);
    PIFR* pifr =
        backend->build_pifr(pifr_id, is_dirty, size, tid, rip, backtrace);

    // PROFILING_END('1');PROFILING_START;
    auto overlaps = backend->find_overlaps(mem_addr, pifr);
    // PROFILING_END('2');PROFILING_START;
    pifr_overlap_counter_rw.fetch_add(overlaps.size());
    for (PIFR other_pifr : overlaps)
        if (report_bug(mem_addr, pifr, &other_pifr))
            threads[other_pifr.tid] = true;
    // PROFILING_END('3');PROFILING_START;

    if ((!minimal_regions || is_write) &&
        (!sampling || ++sample % sampling_rate == 0)) {
        sample = 0;
        if (backend->open_region(mem_addr, *pifr)) op++;
    }

    PROFILING_END('s');
}

void close_regions(intptr_t current_iaddr, pid_t tid) {
    PROFILING_START;

    if (delay && should_delay(get_location_delay_prob(current_iaddr))) {
        ++sleeping;
        ++delay_counter;
        LOG_DEBUG("delay %d\n", tid);
        usleep(delay_micro);
        LOG_DEBUG("wake %d\n", tid);
        --sleeping;
        if (!threads[tid]) decay_location_delay_prob(current_iaddr);
    }
    threads[tid] = false;

    backend->end_regions(tid);

    PROFILING_END('e');
}

EXPORT void pifrrt_end_pifrs(intptr_t current_iaddr) {
    pid_t tid = mutex_gettid();
    if (is_new_thread(tid)) return;
    LOG_DEBUG("(%d) [%x] end\n", tid, current_iaddr);

    if (tracing) {
        trace_end(tid);
        return;
    }

    if (!minimal_regions) close_regions(current_iaddr, tid);
}

EXPORT void pifrrt_flush(intptr_t current_iaddr, intptr_t mem_addr,
                         size_t size) {
    pid_t tid = mutex_gettid();
    if (is_new_thread(tid)) return;
    LOG_DEBUG("(%d) [%x] flush %p %d\n", tid, current_iaddr, mem_addr, size);

    if (tracing) {
        trace_flush(tid, mem_addr, size);
        return;
    }

    if (minimal_regions) {
        close_regions(current_iaddr, tid);
        return;
    }

    PROFILING_START;

    backend->flush(tid, mem_addr, size);

    PROFILING_END('f');
}

__attribute__((constructor)) void init(void) {
    LOG("init libpiftrt\n");
    FILE* environ_f = fopen("/tmp/environ", "r");
    if (environ_f == NULL) {
        LOG("failed to open /tmp/environ\n");
        abort();
    }
    fread(&environ, 1, sizeof(environ), environ_f);
    fclose(environ_f);

    minimal_regions = GETENV_OR_DEFAULT(MINIMAL);
    sampling_rate = GETENV_OR_DEFAULT(SAMPLING);
    sampling = sampling_rate > 0;
    delay_micro = GETENV_OR_DEFAULT(DELAY) * 1000;
    delay = delay_micro > 0;
    decay = GETENV_OR_DEFAULT(DECAY);
    profiling = GETENV_OR_DEFAULT(PROFILING);
    tracing = GETENV_OR_DEFAULT(TRACING);
    report = GETENV_OR_DEFAULT(REPORT);

    LOG("regions: %s\n", minimal_regions ? "minimal" : "normal");
    LOG("sampling: %s\n", sampling ? "ON" : "OFF");
    LOG("report: %s\n", report ? "ON" : "OFF");
    LOG("backtraces: %s\n", report == 2 ? "ON" : "OFF");
    LOG("profiling: %s\n", profiling ? "ON" : "OFF");
    LOG("tracing: %s\n", tracing ? "ON" : "OFF");
    if (delay) {
        available_randoms = getrandom(randoms, sizeof(randoms), 0) / 2;
        LOG("delay: %d\n", delay_micro / 1000);
        LOG("decay: %d\n", decay);
    } else {
        LOG("delay: OFF\n");
    }

    backend = new TBBBackend();
    backend->init();

    if (report != 0) {
        bugs_f = fopen(BUGS_PATH, "w");
    }
    if (report == 2) {
        bugs_bin = fopen(BUGS_DATA_PATH, "w");
        pthread_mutex_init(&bt_mutex, NULL);
    }
    if (profiling) {
        times_f = fopen(TIMES_PATH, "w");
    }
    if (tracing) {
        trace_f = fopen(TRACE_PATH, "w");
        pthread_mutex_init(&trace_mutex, NULL);
    }
}

__attribute__((destructor)) void fini(void) {
    /*
    auto opens_f = fopen(OPENS_PATH, "w");
    for (auto pair : pifrs) {
        auto addr = pair.first;
        auto pifr = pair.second;
        char buf[100];
        int n = snprintf(buf, 100, "%d %x\n", pifr->pifr_id, addr);
        fwrite(buf, n, 1, opens_f);
    }
    fclose(opens_f);
    */

    if (report != 0) fclose(bugs_f);
    if (report == 2) fclose(bugs_bin);
    if (profiling) fclose(times_f);
    if (tracing) fclose(trace_f);

    pmem_stats();
    LOG("total  r/w pifr overlaps: %d\n", pifr_overlap_counter_rw.load());
    LOG("unique r/w pifr overlaps: %d\n", rw_bug_hashes.size());
    if (delay) LOG("delays: %d\n", delay_counter.load());
    LOG("opens: %d\n", op.load());
    size_t still_open = backend->fini();
    LOG("still open: %d (%d%%)\n", still_open, still_open * 100 / op.load());

    LOG_DEBUG("fini libpiftrt\n");
}
