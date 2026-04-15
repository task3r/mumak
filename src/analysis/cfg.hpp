// Copyright 2023 João Gonçalves

#ifndef PALANTIR_CFG
#define PALANTIR_CFG

#include <Zydis/Zydis.h>

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../utils/tracing.hpp"
#include "./x86.hpp"

namespace palantir {

typedef uint64_t IAddr;

class Module;
class Function;
class CFG;
class BasicBlock;
class Instruction;

struct Loop {
    BasicBlock* header;
    BasicBlock* end;
    std::vector<BasicBlock*> body;
};

class CallGraph {
    Module* parent;
    std::map<IAddr, std::set<IAddr>> call_edges;
    std::map<IAddr, std::set<IAddr>> callee_edges;

 public:
    CallGraph(Module* parent) : parent(parent) {}
    Module* get_parent() { return parent; }
    void add_edge(IAddr caller, IAddr callee) {
        auto caller_itr = call_edges.find(caller);
        if (caller_itr == call_edges.end()) {
            call_edges.insert({caller, {callee}});
        } else {
            caller_itr->second.insert(callee);
        }

        auto callee_itr = call_edges.find(callee);
        if (callee_itr == call_edges.end()) {
            call_edges.insert({callee, {callee}});
        } else {
            callee_itr->second.insert(callee);
        }
    }
    void dot(std::ostream& os);
};

class Module {
    std::map<std::string, Function*> functions;
    std::map<IAddr, Function*> functions_by_addr;
    CallGraph call_graph;
    bool built_cg = false;
    std::string name;

 public:
    explicit Module(const char* name)
        : call_graph(CallGraph(this)), name(name) {}
    const std::map<std::string, Function*>& get_functions() {
        return functions;
    }
    void add_function(Function* function, const char* name, IAddr addr);
    Function* get_function(std::string name) {
        auto itr = functions.find(name);
        if (itr != functions.end()) return itr->second;
        return nullptr;
    }
    Function* get_function(IAddr addr) {
        auto itr = functions_by_addr.find(addr);
        if (itr != functions_by_addr.end()) return itr->second;
        return nullptr;
    }
    std::string get_name() { return name; }
    CallGraph* get_call_graph() { return &call_graph; }
    void dot(bool single_file = false);
    void dot(std::ostream& os);
    void dot_dominators();
    void BuildCallGraph();
};

enum EdgeType { FALLTHROUGH, CONDITIONAL_JUMP, JUMP, PERSISTS };

bool EdgeIsJump(EdgeType type);

struct Edge {
    IAddr src, dst;
    EdgeType type;
};

class BasicBlock {
    IAddr addr;
    CFG* parent;
    size_t idx;
    std::vector<BasicBlock*> predecessors;
    std::vector<BasicBlock*> successors;
    std::vector<Instruction*> instructions;

 public:
    BasicBlock(CFG* cfg, IAddr addr, size_t idx)
        : addr(addr), parent(cfg), idx(idx) {}
    CFG* get_parent() { return parent; }
    IAddr get_addr() { return addr; }
    size_t get_idx() { return idx; }
    const std::vector<Instruction*>& get_instructions() { return instructions; }
    Instruction* get_last_instruction() {
        size_t n = instructions.size();
        return n ? instructions[n - 1] : nullptr;
    }
    Instruction* get_first_instruction() {
        return instructions.size() ? instructions[0] : nullptr;
    }
    void add_instruction(Instruction* i);
    void insert_at(std::vector<Instruction*>::const_iterator itr,
                   Instruction* i) {
        instructions.insert(itr, i);
    }
    void add_successor(BasicBlock* bb);
    void add_predecessor(BasicBlock* bb);
    std::set<BasicBlock*> get_dominators();
    std::set<BasicBlock*> get_postdominators();
    bool dominates(BasicBlock* b);
    bool postdominates(BasicBlock* b);
    bool ImmediatlyDominates(BasicBlock* n);
    bool is_cfg_terminator() { return successors.empty(); }
    const std::vector<BasicBlock*>& get_successors() { return successors; }
    const std::vector<BasicBlock*>& get_predecessors() { return predecessors; }
    void dot(std::ostream& os);
};

class CFG {
    std::vector<BasicBlock*> blocks;
    // TODO(task3r): vec -> set?
    std::map<IAddr, std::vector<Edge*>> forward_edges;
    std::map<IAddr, std::vector<Edge*>> backward_edges;
    Function* function;
    std::vector<std::set<BasicBlock*>> dominators;
    std::vector<std::set<BasicBlock*>> postdominators;
    bool computed_dominators = false;
    bool defined_loops = false;
    std::vector<Loop> loops;
    enum Helper { NONE, DOES, DOESNT };
    Helper flushes = NONE;
    Helper releases = NONE;

 public:
    explicit CFG(Function* f) : function(f) {}
    BasicBlock* new_block(IAddr addr) {
        BasicBlock* bb = find_block(addr);
        if (bb == nullptr) {
            bb = new BasicBlock(this, addr, blocks.size());
            blocks.push_back(bb);
        }
        return bb;
    }
    BasicBlock* find_block(IAddr addr) {
        for (BasicBlock* bb : blocks) {
            if (bb->get_addr() == addr) return bb;
        }
        return nullptr;
    }
    const std::vector<BasicBlock*>& get_blocks() { return blocks; }
    const std::vector<Loop>& get_loops() { return loops; }
    Function* get_function() { return function; }
    BasicBlock* get_entrypoint() { return blocks.size() ? blocks[0] : nullptr; }
    void add_edge(Edge* edge) {
        forward_edges[edge->src].push_back(edge);
        backward_edges[edge->dst].push_back(edge);
    }
    void ComputeDominators();
    std::set<BasicBlock*> get_dominators(size_t idx) {
        if (!computed_dominators) ComputeDominators();
        return dominators[idx];
    }
    std::set<BasicBlock*> get_postdominators(size_t idx) {
        if (!computed_dominators) ComputeDominators();
        return postdominators[idx];
    }
    void DefineLoops();
    std::vector<Loop> LoopsWith(BasicBlock* b);
    bool ImmediatlyDominates(BasicBlock* m, BasicBlock* n);
    void AddPersistDependencies(
        std::map<IAddr, std::set<IAddr>> persist_dependencies);
    void AddRuntimeBranchTargets(
        std::map<IAddr, std::set<IAddr>> runtime_branches);
    void dot_ins(std::ostream& os);
    void dot_bb(std::ostream& os);
    void dot_bb_only(std::ostream& os);
    void dot_bb_edges(std::ostream& os);
    void dot_ins_only(std::ostream& os);
    void dot_ins_edges(std::ostream& os);
    const std::vector<Edge*>& get_forward_edges(IAddr ip) {
        return forward_edges[ip];
    }
    const std::vector<Edge*>& get_backward_edges(IAddr ip) {
        return backward_edges[ip];
    }
    bool Releases(bool consider_calls = true);
    bool Flushes(bool consider_calls = true);
};

class Function {
    Module* parent;
    IAddr base_addr;
    bool internal;
    std::string name;
    std::vector<Instruction*> instructions;
    std::map<IAddr, Instruction*> mapped_instructions;
    CFG* cfg = nullptr;

 public:
    Function(Module* parent, const char* name, IAddr addr, bool internal)
        : parent(parent), base_addr(addr), internal(internal), name(name) {}
    Module* get_parent() { return parent; }
    std::string get_name() { return name; }
    IAddr get_addr() { return base_addr; }
    bool is_internal() { return internal; }
    CFG* get_cfg() {
        if (cfg == nullptr) BuildCFG();
        return cfg;
    }
    bool has_instruction(IAddr addr) {
        return mapped_instructions.find(addr) != mapped_instructions.end();
    }
    Instruction* get_instruction(IAddr addr) {
        return has_instruction(addr) ? mapped_instructions[addr] : nullptr;
    }
    const std::vector<Instruction*>& get_instructions() { return instructions; }
    bool does_not_return();
    void add_instruction(x86::Instruction* x86, IAddr addr);
    void BuildCFG();
};

class PIFRDelimiter;
class Instruction {
    BasicBlock* parent;
    Function* function;
    tracing::pm::Instruction type = tracing::pm::Instruction::OTHER;
    IAddr addr;
    Instruction() {}
    friend PIFRDelimiter;

 public:
    const x86::Instruction* x86;

    Instruction(Function* function, IAddr addr, x86::Instruction* x86)
        : function(function), addr(addr), x86(x86) {}
    void set_parent(BasicBlock* b) { parent = b; }
    BasicBlock* get_parent() { return parent; }
    tracing::pm::Instruction get_type() { return type; }
    void set_type(tracing::pm::Instruction t) { type = t; }
    IAddr get_addr() { return addr; }
    bool has_fallthrough();
    void dot(std::ostream& os);
    const char* disassembled() { return x86->disassembled(); }
    Function* get_call_target() {
        if (!x86->is_call()) return nullptr;
        return function->get_parent()->get_function(x86->jump_target());
    }
    bool flushes(bool consider_calls = true);
    bool releases(bool consider_calls = true);
    bool dominates(Instruction* b);
    bool postdominates(Instruction* b);
};

class PIFRDelimiter : public Instruction {
    x86::MemAccess ma;
    bool is_start;

 public:
    PIFRDelimiter(x86::MemAccess ma, bool is_start)
        : ma(ma), is_start(is_start) {}
    const char* disassembled() {
        std::stringstream ss;
        if (is_start)
            ss << "START ";
        else
            ss << "END ";
        ss << x86::MemAccessToStr(&ma);
        std::string s = ss.str();
        return (new std::string(s))->c_str();  // canoja
    }
};

}  // namespace palantir
#endif  // PALANTIR
