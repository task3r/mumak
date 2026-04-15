// Copyright 2023 João Gonçalves
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include <set>
#include <map>

#include "../runtime/backend.hpp"
#include "../runtime/tbb_backend.hpp"
#include "tracing.hpp"

using tracing::pm::Instruction;
using tracing::rwflow::BRANCH;
using tracing::rwflow::FLUSH;
using tracing::rwflow::PM_READ;
using tracing::rwflow::PM_WRITE;
using tracing::rwflow::ACQUIRE;
using tracing::rwflow::RELEASE;
using tracing::rwflow::ThreadedTraceLine;

struct TLTWrapper {
    ThreadedTraceLine l;
    std::vector<std::pair<uint64_t,PIFR*>> pifrs;
};

static PIFRBackend* backend;
static FILE* bugs_f;

std::vector<TLTWrapper> mapping(char* in_file) {
    std::vector<TLTWrapper> res;
    std::map<uint64_t, size_t> most_recent_acquires;
    std::ifstream in(in_file, std::ifstream::binary);
    while (true) {
        ThreadedTraceLine l = {0, 0, 0, 0, 0, 0};
        in.read(reinterpret_cast<char*>(&l),
                sizeof(ThreadedTraceLine));
        if (in.eof()) break;
        switch (l.type) {
            case ACQUIRE:
                res.push_back({l, {}});
                most_recent_acquires[l.thread] = res.size() - 1;
                break;
            case PM_WRITE:
            case PM_READ:
                if (most_recent_acquires.find(l.thread) == most_recent_acquires.end()) {
                    res.push_back({l, {}});
                    (&res.back())->l.type = ACQUIRE;
                    most_recent_acquires[l.thread] = res.size() - 1;
                }
                // for (auto& pifr : res[most_recent_acquires[l.thread]].pifrs) {
                //     if (pifr->pifr_id == l.target) continue;
                // }
                res[most_recent_acquires[l.thread]].pifrs.push_back({l.target, backend->build_pifr(l.ip, l.type == PM_WRITE, l.size, l.thread, 0, {})});
                break;
            case FLUSH:
            case RELEASE:
                res.push_back({l, {}});
                break;
            default:
                std::cerr << "Unrecognized trace line " << (int) l.type << std::endl;
                continue;
        }
    }
    in.close();
    return res;
}

std::set<uint64_t> rw_bug_hashes;
inline uint64_t hashit(const char* p, size_t s) {
    uint64_t result = 0;
    const uint64_t prime = 31;
    for (size_t i = 0; i < s; ++i) {
        result = p[i] + (result * prime);
    }
    return result;
}

void bug(intptr_t mem_addr, PIFR* pifr, PIFR* other_pifr) {
    intptr_t id1, id2;
    if (pifr->is_write) {
        id1 = pifr->pifr_id;
        id2 = other_pifr->pifr_id;
    } else {
        id1 = other_pifr->pifr_id;
        id2 = pifr->pifr_id;
    }

    char buf[100];
    int n = snprintf(buf, 100, "0x%lx 0x%lx\n", id1, id2);
    fwrite(buf, n, 1, bugs_f);
}

bool report_bug(intptr_t mem_addr, PIFR* pifr, PIFR* other_pifr) {
    #define BUG_LINE_SIZE (sizeof(intptr_t) * 2 + 1)
    char bug_line[BUG_LINE_SIZE];
    size_t base = 0;
    if (pifr->is_write) {
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


void consume(std::vector<TLTWrapper>& trace) {
    std::cerr << "Consuming trace... " << trace.size() << std::endl;
    size_t slice = trace.size() / 10;
    for (size_t i = 0; i < trace.size(); i++) {
        auto w = trace[i];
        auto l = w.l;
        PIFR* pifr;
        uint64_t addr;
        switch (l.type) {
            case ACQUIRE:
                for (auto pair : w.pifrs) {
                    addr = pair.first;
                    pifr = pair.second;
                    for (auto overlap : backend->find_overlaps(addr, pifr)) {
                        report_bug(addr, pifr, &overlap);
                    }
                    backend->open_region(addr, *pifr);
                }
                break;
            case RELEASE:
                backend->end_regions(l.thread);
                break;
            case FLUSH:
                backend->flush(l.thread, l.target, l.size);
                break;
            default:
                //std::cerr << "Unrecognized trace line" << std::endl;
                continue;
        }
        if (i % slice == 0)
            std::cerr << "Processed " << (float)i / trace.size() * 100 << "%" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    bugs_f = fopen("bugs.txt", "w");
    backend = new TBBBackend();
    backend->init();
    auto trace = mapping(argv[1]);
    consume(trace);
    fclose(bugs_f);
    return 0;
}
