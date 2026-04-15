// Copyright 2023 João Gonçalves

#ifndef MUMAK_TRACING
#define MUMAK_TRACING

#define BACKTRACE_ADDRESSES_LIMIT 50
#define CACHELINE_SIZE 64

#include <iostream>
#include <map>
#include <vector>

namespace tracing {
namespace pm {
// Enum to define the type of all instructions being instrumented
enum Instruction {
    STORE,
    NON_TEMPORAL_STORE,
    CLFLUSH,
    CLFLUSHOPT,
    CLWB,
    FENCE,
    RMW,
    SYNC,
    RELEASE,
    ACQUIRE,
    BRANCH,
    BRANCH_COND,
    BRANCH_TARGET,
    LOAD,
    ERROR,
    OTHER
};

enum BugType {
    REDUNDANT_FLUSH,
    REDUNDANT_FENCE,
    MISSING_FLUSH,
    MISSING_FENCE,
    OVERWRITTEN_UNFLUSHED,
    OVERWRITTEN_UNFENCED,
    IMPLICIT_FLUSH,
    UNORDERED_FLUSHES,
    NONE
};

const char* BugTypeToStr(BugType type);

struct Bug {
    BugType type;
    uint32_t instruction_id;

    Bug(BugType type, uint32_t instruction_id)
        : type(type), instruction_id(instruction_id) {}
};

// Structure that will hold the size and previous value for each Write Operation
enum StoreState { MODIFIED, PARTIALLY_FLUSHED, FLUSHED, FENCED };

struct Store {
    uint32_t size;
    uint64_t instruction_id = 0;
    uint64_t flushed_by = 0;
    StoreState state;

    Store(uint32_t size) : size(size), state(MODIFIED) {}

    Store(uint32_t size, uint32_t instruction_id)
        : size(size), instruction_id(instruction_id), state(MODIFIED) {}

    void change_state(StoreState new_state) { state = new_state; }
    void flush(uint64_t by, Instruction flush_type) {
        state = FLUSHED;

        // Otherwise this is not relevant, since CLFLUSH persists the store
        if (flush_type == Instruction::CLFLUSHOPT ||
            flush_type == Instruction::CLWB) {
            flushed_by = by;
        }
    }
};

class Processer {
 private:
    std::map<uint64_t, Store> flush_pending;
    std::map<uint64_t, Store> fence_pending;
    uint32_t flushes = 0, implicit_flushes = 0, unordered_flushes = 0;

    // If target write is contained inside the source write
    bool SourceStoreContainsTargetStore(uint64_t source_addr,
                                        uint32_t source_size,
                                        uint64_t target_addr,
                                        uint64_t target_size) {
        return ((target_addr >= source_addr) &&
                (target_size <= (source_addr + source_size)));
    }

    // If target write contains the source write
    bool TargetStoreContainsSourceStore(uint64_t source_addr,
                                        uint32_t source_size,
                                        uint64_t target_addr,
                                        uint64_t target_size) {
        return ((target_addr <= source_addr) &&
                (target_size >= (source_addr + source_size)));
    }

    // If write is partially inside cacheline in the left side
    bool StoreIsPartiallyInsideCachelineInLeft(uint64_t flush_addr,
                                               uint32_t flush_size,
                                               uint64_t store_addr,
                                               uint32_t store_size) {
        return (flush_addr > store_addr) &&
               ((store_addr + store_size) > flush_addr) &&
               ((flush_addr + flush_size) > (store_addr + store_size));
    }

    // If write is partially inside cacheline in the right side
    bool StoreIsPartiallyInsideCachelineInRight(uint64_t flush_addr,
                                                uint32_t flush_size,
                                                uint64_t store_addr,
                                                uint32_t store_size) {
        return (flush_addr < store_addr) &&
               ((flush_addr + flush_size) > store_addr) &&
               ((flush_addr + flush_size) < (store_addr + store_size));
    }

    bool StoreIsPartiallyInsideCacheline(uint64_t flush_addr,
                                         uint32_t flush_size,
                                         uint64_t store_addr,
                                         uint32_t store_size) {
        return StoreIsPartiallyInsideCachelineInRight(flush_addr, flush_size,
                                                      store_addr, store_size) ||
               StoreIsPartiallyInsideCachelineInLeft(flush_addr, flush_size,
                                                     store_addr, store_size);
    }

    // If write is completely inside cacheline
    bool StoreIsInsideCacheline(uint64_t flush_addr, uint32_t flush_size,
                                uint64_t store_addr, uint32_t store_size) {
        return (store_addr >= flush_addr) &&
               (store_addr + store_size <= flush_addr + flush_size);
    }

    void CheckStore(uint64_t address, uint32_t size,
                    std::map<uint64_t, Store>& group, std::vector<Bug>& bugs,
                    BugType type) {
        std::map<uint64_t, Store>::iterator begin, end;
        uint64_t write_end = address + size;
        begin = group.lower_bound(address);
        end = group.upper_bound(write_end);
        // If it did find a first element contained in that range
        if (begin->first < write_end) {
            for (auto itr = begin, next = itr; itr != end; itr = next) {
                ++next;
                if (TargetStoreContainsSourceStore(itr->first, itr->second.size,
                                                   address, write_end) ||
                    SourceStoreContainsTargetStore(itr->first, itr->second.size,
                                                   address, write_end)) {
                    bugs.push_back(Bug(type, itr->second.instruction_id));

                    group.erase(itr);
                }
            }
        }
    }

    void CheckStore(uint64_t address, uint32_t size,
                    std::map<uint64_t, Store>& group,
                    std::vector<uint64_t>& stores_overwritten) {
        std::map<uint64_t, Store>::iterator begin, end;
        uint64_t write_end = address + size;
        begin = group.lower_bound(address);
        end = group.upper_bound(write_end);
        // If it did find a first element contained in that range
        if (begin->first < write_end) {
            for (auto itr = begin, next = itr; itr != end; itr = next) {
                ++next;
                if (TargetStoreContainsSourceStore(itr->first, itr->second.size,
                                                   address, write_end) ||
                    SourceStoreContainsTargetStore(itr->first, itr->second.size,
                                                   address, write_end)) {
                    stores_overwritten.push_back(itr->second.instruction_id);
                    group.erase(itr);
                }
            }
        }
    }

 public:
    void ProcessStore(uint64_t address, uint32_t size, bool is_movnt,
                      uint32_t id, std::vector<Bug>& bugs) {
        Store write = Store(size, id);
        // Check whether write address range is new
        // Report bug if previous write address range was not fully persisted
        CheckStore(address, size, flush_pending, bugs,
                   BugType::OVERWRITTEN_UNFLUSHED);
        CheckStore(address, size, fence_pending, bugs,
                   BugType::OVERWRITTEN_UNFENCED);
        // The write will be in the flush or fence pending map depending on the
        // type of the write operation
        if (is_movnt) {
            write.change_state(StoreState::FLUSHED);
            fence_pending.insert(std::pair<uint64_t, Store>(address, write));
        } else {
            flush_pending.insert(std::pair<uint64_t, Store>(address, write));
        }
    }

    void ProcessStore(uint64_t address, uint32_t size, bool is_movnt,
                      uint64_t id, std::vector<uint64_t>& stores_overwritten) {
        Store write = Store(size, id);
        // Check whether write address range is new
        CheckStore(address, size, flush_pending, stores_overwritten);
        CheckStore(address, size, fence_pending, stores_overwritten);
        // The write will be in the flush or fence pending map depending on the
        // type of the write operation
        if (is_movnt) {
            write.change_state(StoreState::FLUSHED);
            fence_pending.insert(std::pair<uint64_t, Store>(address, write));
        } else {
            flush_pending.insert(std::pair<uint64_t, Store>(address, write));
        }
    }

    void ProcessFlush(Instruction flush_type, uint64_t address, uint32_t id,
                      std::vector<Bug>& bugs) {
        int flushed_stores = 0;
        uint64_t flush_end = address + CACHELINE_SIZE;
        std::map<uint64_t, Store>::iterator begin, end;
        begin = flush_pending.lower_bound(address);
        end = flush_pending.upper_bound(flush_end);
        // If it did find a first element contained in that range
        if (begin->first < flush_end) {
            // Iterate flush pending write operations
            for (auto itr = begin, next = itr; itr != end; itr = next) {
                ++next;
                // If write is located inside the cache line
                if (StoreIsInsideCacheline(address, CACHELINE_SIZE, itr->first,
                                           itr->second.size)) {
                    itr->second.flush(id, flush_type);
                    flushed_stores++;
                    fence_pending.insert(
                        std::pair<uint64_t, Store>(itr->first, itr->second));
                    flush_pending.erase(itr);
                } else if (StoreIsPartiallyInsideCacheline(
                               address, CACHELINE_SIZE, itr->first,
                               itr->second.size)) {
                    if (itr->second.state == StoreState::MODIFIED) {
                        itr->second.change_state(StoreState::PARTIALLY_FLUSHED);
                    } else if (itr->second.state ==
                               StoreState::PARTIALLY_FLUSHED) {
                        // FIXME: we cannot be certain of this!
                        itr->second.flush(id, flush_type);
                        fence_pending.insert(std::pair<uint64_t, Store>(
                            itr->first, itr->second));
                        flush_pending.erase(itr);
                    }
                    flushed_stores++;
                }
            }
        }
        // Report warning to ensure each flush operates on the right number of
        // write operations
        // if (flushed_stores > 1) {
        // sstm << "IMPLICITLY FLUSHES " << flushed_stores << " STORES";
        // recordPossibleBug(BugType::IMPLICIT_FLUSH, operationId);
        // }
        // Increment the total number of flushes and flushed stores
        flushes++;
        implicit_flushes += flushed_stores;
        if (flushed_stores > 0) {
            if (flush_type == Instruction::CLFLUSHOPT ||
                flush_type == Instruction::CLWB)
                unordered_flushes++;
        } else {
            bugs.push_back(Bug(BugType::REDUNDANT_FLUSH, id));
        }
    }

    void ProcessFlush(Instruction flush_type, uint64_t address, uint64_t id,
                      std::vector<uint64_t>& stores_flushed) {
        uint64_t flush_end = address + CACHELINE_SIZE;
        // std::cerr << "FLUSH@" << address << std::endl;
        std::map<uint64_t, Store>::iterator begin, end;
        begin = flush_pending.lower_bound(address);
        end = flush_pending.upper_bound(flush_end);
        // If it did find a first element contained in that range
        if (begin->first < flush_end) {
            // Iterate flush pending write operations
            for (auto itr = begin, next = itr; itr != end; itr = next) {
                ++next;
                // If write is located inside the cache line
                if (StoreIsInsideCacheline(address, CACHELINE_SIZE, itr->first,
                                           itr->second.size)) {
                    itr->second.flush(id, flush_type);
                    stores_flushed.push_back(itr->second.instruction_id);
                    // std::cerr << "\tflushed " << itr->first << std::endl;
                    fence_pending.insert(
                        std::pair<uint64_t, Store>(itr->first, itr->second));
                    flush_pending.erase(itr);
                } else if (StoreIsPartiallyInsideCacheline(
                               address, CACHELINE_SIZE, itr->first,
                               itr->second.size)) {
                    if (itr->second.state == StoreState::MODIFIED) {
                        itr->second.change_state(StoreState::PARTIALLY_FLUSHED);
                    } else if (itr->second.state ==
                               StoreState::PARTIALLY_FLUSHED) {
                        // FIXME: we cannot be certain of this!
                        itr->second.flush(id, flush_type);
                        stores_flushed.push_back(itr->second.instruction_id);
                        fence_pending.insert(std::pair<uint64_t, Store>(
                            itr->first, itr->second));
                        flush_pending.erase(itr);
                    }
                }
            }
        }
    }

    void ProcessFence(uint32_t id, std::vector<Bug>& bugs, bool is_rmw) {
        if (fence_pending.size() == 0 && !is_rmw)
            bugs.push_back(Bug(BugType::REDUNDANT_FENCE, id));
        fence_pending.clear();
        if (unordered_flushes > 1)
            bugs.push_back(Bug(BugType::UNORDERED_FLUSHES, id));
        unordered_flushes = 0;
    }

    void ProcessFence(uint32_t id, std::vector<Bug>& bugs,
                      std::vector<uint32_t>& fully_persisted, bool is_rmw) {
        if (fence_pending.size() == 0 && !is_rmw)
            bugs.push_back(Bug(BugType::REDUNDANT_FENCE, id));
        for (auto store : fence_pending)
            fully_persisted.push_back(store.second.instruction_id);
        fence_pending.clear();
        if (unordered_flushes > 1)
            bugs.push_back(Bug(BugType::UNORDERED_FLUSHES, id));
        unordered_flushes = 0;
    }

    void ProcessFence(uint64_t id, bool is_rmw,
                      std::vector<uint64_t>& flushes_fenced) {
        for (auto store : fence_pending) {
            if (store.second.flushed_by) {
                flushes_fenced.push_back(store.second.flushed_by);
            }
        }
        fence_pending.clear();
    }

    void CheckRemainder(std::vector<Bug>& bugs) {
        for (auto store : flush_pending) {
            bugs.push_back(
                Bug(BugType::MISSING_FLUSH, store.second.instruction_id));
        }
        for (auto store : fence_pending) {
            bugs.push_back(
                Bug(BugType::MISSING_FENCE, store.second.instruction_id));
        }
        // Record total number of flushes and flushed stores as a warning
        // "[WARNING] TOTAL NUMBER OF FLUSHES: " << flushes
        //      << " AND IMPLICIT FLUSHED STORES: " << implicit_flushes;
    }
};

static Processer proc;
}  // namespace pm

namespace rwflow {

// type | img_id | ip | target | size
#define TRACE_LINE_SIZE \
    (sizeof(char) + sizeof(uint32_t) * 2 + sizeof(uint64_t) * 2)
#define THREADED_TRACE_LINE_SIZE \
    (sizeof(char) + sizeof(uint32_t) * 3 + sizeof(uint64_t) * 2)

#define GETUINT32(buffer, idx)                         \
    (uint32_t((unsigned char)(buffer[idx + 4]) << 24 | \
              (unsigned char)(buffer[idx + 3]) << 16 | \
              (unsigned char)(buffer[idx + 1]) << 8 |  \
              (unsigned char)(buffer[idx])))

#define GETUINT64(buffer, idx)                         \
    (uint64_t((unsigned char)(buffer[idx]) << 56 |     \
              (unsigned char)(buffer[idx + 1]) << 48 | \
              (unsigned char)(buffer[idx + 2]) << 40 | \
              (unsigned char)(buffer[idx + 3]) << 32 | \
              (unsigned char)(buffer[idx + 4]) << 24 | \
              (unsigned char)(buffer[idx + 5]) << 16 | \
              (unsigned char)(buffer[idx + 6]) << 8 |  \
              (unsigned char)(buffer[idx + 7])))

static const char WRITE = 0x0;
static const char READ = 0x1;
static const char PM_WRITE = 0x2;
static const char PM_READ = 0x3;
static const char FLUSH = 0x4;
static const char BRANCH = 0x5;
static const char ACQUIRE = 0x6;
static const char RELEASE = 0x7;

struct TraceLine {
    char type;
    uint32_t img;
    uint64_t ip;
    uint64_t target;
    uint32_t size;
};
struct ThreadedTraceLine {
    char type;
    uint32_t thread;
    uint32_t img;
    uint64_t ip;
    uint64_t target;
    uint32_t size;
};
void Trace(std::ostream& out, char type, uint32_t img, uint64_t ip,
           uint64_t target, uint32_t size);

void ThreadedTrace(std::ostream &out, uint32_t tid, char type, uint32_t img, uint64_t ip,
           uint64_t target, uint32_t size);

}  // namespace rwflow
}  // namespace tracing

#endif  // !MUMAK_TRACE
