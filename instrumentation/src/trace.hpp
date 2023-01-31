#ifndef MUMAK_TRACE
#define MUMAK_TRACE

#define BACKTRACE_ADDRESSES_LIMIT 50
#define CACHELINE_SIZE 64

#include <map>
#include <vector>

namespace trace {
// Enum to define the type of all instructions being instrumented
enum Instruction {
    STORE,
    NON_TEMPORAL_STORE,
    CLFLUSH,
    CLFLUSHOPT,
    CLWB,
    FENCE,
    RMW,
    ERROR
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

struct Bug {
    BugType type;
    uint32_t instruction_id;

    Bug(BugType type, uint32_t instruction_id)
        : type(type), instruction_id(instruction_id) {}
};

const char* BugTypeToStr(BugType type) {
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

// Structure that will hold the size and previous value for each Write Operation
enum StoreState { MODIFIED, PARTIALLY_FLUSHED, FLUSHED, FENCED };

struct Store {
    uint32_t size;
    uint32_t instruction_id;
    StoreState state;

    Store(uint32_t size) : size(size), state(MODIFIED) {}

    Store(uint32_t size, uint32_t instruction_id)
        : size(size), instruction_id(instruction_id), state(MODIFIED) {}

    void change_state(StoreState new_state) { state = new_state; }
};

class TraceAnalyzer {
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
                    itr->second.change_state(StoreState::FLUSHED);
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
                        itr->second.change_state(StoreState::FLUSHED);
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

static TraceAnalyzer ta;

}  // namespace trace

#endif  // !MUMAK_TRACE
