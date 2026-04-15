// Copyright 2023 João Gonçalves

#include "./tracing.hpp"

#include <cstdint>
#include <ostream>
#include <unordered_set>

static std::unordered_set<uint64_t> trace_hashes;

uint64_t hashit(const char *p, size_t s) {
    uint64_t result = 0;
    const uint64_t prime = 31;
    for (size_t i = 0; i < s; ++i) {
        result = p[i] + (result * prime);
    }
    return result;
}

namespace tracing {
namespace pm {

const char *BugTypeToStr(BugType type) {
    switch (type) {
        case REDUNDANT_FLUSH:
            return "[BUG] REDUNDANT FLUSH";
        case REDUNDANT_FENCE:
            return "[BUG] REDUNDANT FENCE";
        case MISSING_FLUSH:
            return "[BUG] STORE NOT FLUSHED";
        case MISSING_FENCE:
            return "[BUG] STORE NOT FENCED";
        case OVERWRITTEN_UNFLUSHED:
            return "[BUG] STORE OVERWRITTEN BEFORE FLUSH";
        case OVERWRITTEN_UNFENCED:
            return "[BUG] STORE OVERWRITTEN BEFORE FENCE";
        case UNORDERED_FLUSHES:
            return "[BUG] FENCE ACTS ON MULTIPLE UNORDERED FLUSHES";
        default:  // missing implicit flush
            return "[ERROR]";
    }
}

}  // namespace pm
namespace rwflow {

void Trace(std::ostream &out, char type, uint32_t img, uint64_t ip,
           uint64_t target, uint32_t size) {
    TraceLine l{type, img, ip, target, size};
    char trace_line[TRACE_LINE_SIZE];
    size_t base = 0;
    trace_line[base] = type;
    base += sizeof(char);
    char *char_img = reinterpret_cast<char *>(&img);
    for (size_t idx = 0; idx < sizeof(uint32_t); idx++, base++)
        trace_line[base] = char_img[idx];
    char *char_ip = reinterpret_cast<char *>(&ip);
    for (size_t idx = 0; idx < sizeof(uint64_t); idx++, base++)
        trace_line[base] = char_ip[idx];
    char *char_target = reinterpret_cast<char *>(&target);
    for (size_t idx = 0; idx < sizeof(uint64_t); idx++, base++)
        trace_line[base] = char_target[idx];
    char *char_size = reinterpret_cast<char *>(&size);
    for (size_t idx = 0; idx < sizeof(uint32_t); idx++, base++)
        trace_line[base] = char_size[idx];
    // tid
    uint64_t hash = hashit(trace_line, TRACE_LINE_SIZE);
    auto pair = trace_hashes.insert(hash);
    if (pair.second)  // if the hash is unique
        out.write(reinterpret_cast<char *>(&l), sizeof(TraceLine));
}
void ThreadedTrace(std::ostream &out, uint32_t tid, char type, uint32_t img, uint64_t ip,
           uint64_t target, uint32_t size) {
    ThreadedTraceLine l{type, tid, img, ip, target, size};
    // char trace_line[THREADED_TRACE_LINE_SIZE];
    // size_t base = 0;
    // trace_line[base] = type;
    // base += sizeof(char);
    // char *char_img = reinterpret_cast<char *>(&img);
    // for (size_t idx = 0; idx < sizeof(uint32_t); idx++, base++)
    //     trace_line[base] = char_img[idx];
    // char *char_tid = reinterpret_cast<char *>(&tid);
    // for (size_t idx = 0; idx < sizeof(uint32_t); idx++, base++)
    //     trace_line[base] = char_tid[idx];
    // char *char_ip = reinterpret_cast<char *>(&ip);
    // for (size_t idx = 0; idx < sizeof(uint64_t); idx++, base++)
    //     trace_line[base] = char_ip[idx];
    // char *char_target = reinterpret_cast<char *>(&target);
    // for (size_t idx = 0; idx < sizeof(uint64_t); idx++, base++)
    //     trace_line[base] = char_target[idx];
    // char *char_size = reinterpret_cast<char *>(&size);
    // for (size_t idx = 0; idx < sizeof(uint32_t); idx++, base++)
    //     trace_line[base] = char_size[idx];
    out.write(reinterpret_cast<char *>(&l), sizeof(ThreadedTraceLine));
}
}  // namespace rwflow
}  // namespace tracing
