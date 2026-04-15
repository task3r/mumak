// Copyright 2023 João Gonçalves
#ifndef PALANTIR_STATICA
#define PALANTIR_STATICA

#include <fstream>

#include "./cfg.hpp"
#include "./x86.hpp"

namespace palantir {

void IdentifyFlowControlRW(Function* f);
void IdentifyFlowControlRW(Module* m);

class PIFR {
    typedef void* Value;
    enum Type { MAY_WRITE, MUST_WRITE };
    Type type;
    std::set<Instruction*> instructions;
    Value value;
};

typedef std::vector<PIFR> PIFRs;

struct PIFRContext {
    std::map<IAddr, PIFRs> bb_pifrs;
    std::map<Instruction*, PIFRs> call_pifrs;
    std::map<Instruction*, PIFRs> release_pifrs;
};

struct Context {
    std::map<IAddr, std::pair<tracing::pm::Instruction, uint32_t>>
        relevant_insts;
    std::set<std::string> acquire_calls;
    std::set<std::string> release_calls;
    std::ofstream out;
};

Context CreateContext(uint8_t img_id, const char* trace_path);
Context CreateContext(uint8_t img_id, const char* trace_path,
                      const char* acquire_calls_path,
                      const char* release_calls_path);

void FindPIFRs(Module* module, Context& ctx, bool persist_only,
               bool define_ends = false);
void FindMinimalWindows(Module* module, Context& ctx);
void FindRetBBs(Module* module);
void FindCalls(Module* module);

}  // namespace palantir
#endif
