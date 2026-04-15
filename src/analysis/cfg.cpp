// Copyright 2023 João Gonçalves

#include "./cfg.hpp"

#include <Zydis/Utils.h>

#include <deque>
#include <fstream>
#include <iomanip>

#include "../utils/tracing.hpp"

template <typename T>
std::string hex(T i) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex
           << i;
    return stream.str();
}

namespace palantir {
bool EdgeIsJump(EdgeType type) {
    return type == JUMP || type == CONDITIONAL_JUMP;
}

void BasicBlock::add_successor(BasicBlock* bb) {
    // for (BasicBlock* successor : successors) {
    //     if (successor->addr == bb->addr) return;
    // }
    successors.push_back(bb);
}

void BasicBlock::add_predecessor(BasicBlock* bb) {
    // for (BasicBlock* predecessor : predecessors) {
    //     if (predecessor->addr == bb->addr) return;
    // }
    predecessors.push_back(bb);
}

void BasicBlock::add_instruction(Instruction* i) {
    instructions.push_back(i);
    i->set_parent(this);
}

void Function::add_instruction(x86::Instruction* x86, IAddr addr) {
    Instruction* i = new Instruction(this, addr, x86);
    instructions.push_back(i);
    mapped_instructions[addr] = i;
}

bool Instruction::has_fallthrough() {
    if (!x86->has_fallthrough()) return false;
    // if it is not a call, and has a next inst, it has fallthrough
    if (!x86->is_call()) return true;
    // if it is a call, it can't be a known "does not return" call (e.g. exit)
    auto target = get_call_target();
    // if we don't know the target, we do not assume it does not return
    // it should only happen when `_start` calls the stub for `main`
    return !target || !target->does_not_return();
}

bool Instruction::flushes(bool consider_calls) {
    if (x86->is_flush()) {
        return true;
    }
    if (x86->is_call()) {
        Function* target = get_call_target();
        if (target != nullptr && target->get_cfg()->Flushes(consider_calls)) {
            return true;
        }
    }
    return false;
}

bool Instruction::releases(bool consider_calls) {
    if (type == tracing::pm::RELEASE) {
        return true;
    }
    if (x86->is_call()) {
        Function* target = get_call_target();
        if (target != nullptr && target->get_cfg()->Releases(consider_calls)) {
            return true;
        }
    }
    return false;
}

bool Instruction::dominates(Instruction* b) {
    return parent->dominates(b->get_parent());
}
bool Instruction::postdominates(Instruction* b) {
    return parent->postdominates(b->get_parent());
}

void Function::BuildCFG() {
    this->cfg = new CFG(this);
    // define edges
    for (auto it = instructions.begin(); it != instructions.end(); it++) {
        Instruction* ins = *it;
        if (ins->has_fallthrough() && it + 1 != instructions.end()) {
            cfg->add_edge(new Edge{ins->get_addr(), ins->x86->next_inst_addr(),
                                   EdgeType::FALLTHROUGH});
        }
        if (ins->x86->is_jump()) {
            if (ins->x86->is_direct_jump()) {
                cfg->add_edge(new Edge{ins->get_addr(), ins->x86->jump_target(),
                                       EdgeType::JUMP});
            } else {
                cfg->add_edge(new Edge{ins->get_addr(), ins->x86->jump_target(),
                                       EdgeType::CONDITIONAL_JUMP});
            }
        }
    }

    // build bb cfg
    BasicBlock* bb = nullptr;
    for (auto it = instructions.begin(); it != instructions.end(); it++) {
        Instruction* ins = *it;
        IAddr ip = ins->get_addr();

        if (bb == nullptr) bb = this->cfg->new_block(ip);
        bb->add_instruction(ins);

        // TODO(task3r): change jump edge type based on conditional
        // (jump if true?, fallthrough if false?)
        bool next_inst_is_start_bb = false;
        if ((it + 1) != instructions.end()) {
            Instruction* next_inst = *(it + 1);
            IAddr next_ip = next_inst->get_addr();
            std::vector<Edge*> back_edges =
                this->cfg->get_backward_edges(next_ip);
            for (Edge* edge : back_edges) {
                if (EdgeIsJump(edge->type)) {
                    next_inst_is_start_bb = true;
                    break;
                }
            }
        }
        bool this_inst_jumps = false;
        std::vector<Edge*> front_edges = this->cfg->get_forward_edges(ip);
        for (Edge* edge : front_edges) {
            if (EdgeIsJump(edge->type)) {
                this_inst_jumps = true;
                break;
            }
        }
        if (this_inst_jumps || next_inst_is_start_bb) {
            for (Edge* edge : front_edges) {
                BasicBlock* new_bb = this->cfg->new_block(edge->dst);
                bb->add_successor(new_bb);
                new_bb->add_predecessor(bb);
            }
            bb = nullptr;
        }
        if (!front_edges.size())  // does not have fallthrough
            bb = nullptr;
    }

#ifdef DEBUG
    std::cout << "📐 " << this->get_name() << std::endl;
    for (BasicBlock* bb : cfg->get_blocks()) {
        std::cout << "\t📦 " << std::hex << bb->get_addr() << std::endl;
        for (BasicBlock* p : bb->get_predecessors())
            std::cout << "\t\t⬅️ " << std::hex << p->get_addr() << std::endl;
        for (BasicBlock* s : bb->get_successors())
            std::cout << "\t\t➡️ " << std::hex << s->get_addr() << std::endl;
    }
#endif

    if (this->cfg->get_entrypoint() == nullptr) return;

    Instruction* entrypoint =
        this->cfg->get_entrypoint()->get_instructions()[0];
    if (entrypoint->get_type() == tracing::pm::Instruction::OTHER)
        entrypoint->set_type(tracing::pm::Instruction::BRANCH_TARGET);
}

void CFG::AddPersistDependencies(
    std::map<IAddr, std::set<IAddr>> persist_dependencies) {
    Function* f = this->get_function();
    for (auto dependency : persist_dependencies) {
        IAddr src = dependency.first;
        if (!f->has_instruction(src)) continue;
        for (auto target : dependency.second) {
            if (!f->has_instruction(target)) continue;
            this->add_edge(new Edge{src, target, EdgeType::PERSISTS});
        }
    }
}

void CFG::AddRuntimeBranchTargets(
    std::map<IAddr, std::set<IAddr>> runtime_branches) {
    Function* f = this->get_function();
    for (auto branch : runtime_branches) {
        IAddr src = branch.first;
        if (!f->has_instruction(src)) continue;
        for (auto target : branch.second) {
            if (!f->has_instruction(target)) continue;
            this->add_edge(new Edge{src, target, EdgeType::JUMP});
            Instruction* target_instruction = f->get_instruction(target);
            if (target_instruction->get_type() ==
                tracing::pm::Instruction::OTHER)
                target_instruction->set_type(
                    tracing::pm::Instruction::BRANCH_TARGET);
        }
    }
}

void CFG::ComputeDominators() {
    if (computed_dominators) return;
    for (size_t i = 0; i < blocks.size(); i++) {
        dominators.push_back(std::set<BasicBlock*>());
        std::copy(blocks.begin(), blocks.end(),
                  inserter(dominators[i], dominators[i].end()));
        // for (BasicBlock* b : blocks) dominators[i].insert(b);
    }
    std::deque<size_t> worklist;
    worklist.push_back(0);
    while (!worklist.empty()) {
        size_t y = worklist.front();
        worklist.pop_front();
        BasicBlock* bb_y = blocks[y];
        // std::cout << "Processing " << bb_y->get_idx() << std::endl;
        std::set<BasicBlock*> new_dom_y;
        bool first = true;
        for (auto bb_x : bb_y->get_predecessors()) {
            if (bb_x == bb_y) continue;
            auto x_dom = dominators[bb_x->get_idx()];
            if (first) {
                std::copy(x_dom.begin(), x_dom.end(),
                          inserter(new_dom_y, new_dom_y.end()));
                first = false;
            } else {
                std::vector<BasicBlock*> to_remove;
                for (BasicBlock* b : new_dom_y) {
                    if (x_dom.find(b) == x_dom.end()) to_remove.push_back(b);
                }
                for (BasicBlock* b : to_remove) new_dom_y.erase(b);
            }
        }
        new_dom_y.insert(bb_y);
        bool changed = false;
        if (dominators[y].size() != new_dom_y.size()) {
            changed = true;
        } else {
            for (auto bb : dominators[y]) {
                if (new_dom_y.find(bb) == new_dom_y.end()) {
                    changed = true;
                    break;
                }
            }
        }
        if (changed) {
            dominators[y] = new_dom_y;
            for (auto bb : bb_y->get_successors()) {
                // std::cout << "\tAdding " << bb->get_idx() << std::endl;
                worklist.push_back(bb->get_idx());
            }
        }
    }

    // postdominators
    // std::deque<size_t> worklist;
    worklist.clear();
    for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks[i]->get_successors().empty()) {
            worklist.push_back(i);
            // postdominators[i].insert(blocks[i]);
        }
        postdominators.push_back(std::set<BasicBlock*>());
        std::copy(blocks.begin(), blocks.end(),
                  inserter(postdominators[i], postdominators[i].end()));
    }

    while (!worklist.empty()) {
        size_t y = worklist.front();
        worklist.pop_front();
        BasicBlock* bb_y = blocks[y];
        // std::cout << "Processing " << bb_y->get_idx() << std::endl;
        std::set<BasicBlock*> new_pdom_y;
        bool first = true;
        for (auto bb_x : bb_y->get_successors()) {
            if (bb_x == bb_y) continue;
            auto x_pdom = postdominators[bb_x->get_idx()];
            if (first) {
                std::copy(x_pdom.begin(), x_pdom.end(),
                          inserter(new_pdom_y, new_pdom_y.end()));
                first = false;
            } else {
                std::vector<BasicBlock*> to_remove;
                for (BasicBlock* b : new_pdom_y) {
                    if (x_pdom.find(b) == x_pdom.end()) to_remove.push_back(b);
                }
                for (BasicBlock* b : to_remove) new_pdom_y.erase(b);
            }
        }
        new_pdom_y.insert(bb_y);
        bool changed = false;
        if (postdominators[y].size() != new_pdom_y.size()) {
            changed = true;
        } else {
            for (auto bb : postdominators[y]) {
                if (new_pdom_y.find(bb) == new_pdom_y.end()) {
                    changed = true;
                    break;
                }
            }
        }
        if (changed) {
            postdominators[y] = new_pdom_y;
            for (auto bb : bb_y->get_predecessors()) {
                // std::cout << "\tAdding " << bb->get_idx() << std::endl;
                worklist.push_back(bb->get_idx());
            }
        }
    }

    computed_dominators = true;
    DefineLoops();
}

bool CFG::ImmediatlyDominates(BasicBlock* m, BasicBlock* n) {
    /*
     * (M idom N) iff
     *      (M sdom N) and
     *      (P sdom N) => (P dom M)
     */
    auto doms_n = dominators[n->get_idx()];
    if (m->get_addr() == n->get_addr() || doms_n.find(m) == doms_n.end())
        return false;

    auto doms_m = dominators[m->get_idx()];
    for (BasicBlock* d : doms_n) {
        if (d->get_addr() != n->get_addr() && doms_m.find(d) == doms_m.end())
            return false;
    }

    return true;
}

bool CFG::Releases(bool consider_calls) {
    if (releases != NONE) return releases == DOES;
    for (BasicBlock* bb : blocks) {
        for (Instruction* i : bb->get_instructions()) {
            if (i->releases(consider_calls)) {
                releases = DOES;
                return true;
            }
        }
    }
    releases = DOESNT;
    return false;
}

bool CFG::Flushes(bool consider_calls) {
    if (flushes != NONE) return flushes == DOES;
    for (BasicBlock* bb : blocks) {
        for (Instruction* i : bb->get_instructions()) {
            if (i->flushes(consider_calls)) {
                flushes = DOES;
                return true;
            }
        }
    }
    flushes = DOESNT;
    return false;
}

bool BasicBlock::ImmediatlyDominates(BasicBlock* n) {
    return parent->ImmediatlyDominates(this, n);
}

bool BasicBlock::dominates(BasicBlock* b) {
    for (BasicBlock* d : b->get_dominators())
        if (d == this) return true;
    return false;
}

bool BasicBlock::postdominates(BasicBlock* b) {
    for (BasicBlock* p : b->get_postdominators())
        if (p == this) return true;
    return false;
}

std::set<BasicBlock*> BasicBlock::get_dominators() {
    return parent->get_dominators(idx);
}

std::set<BasicBlock*> BasicBlock::get_postdominators() {
    return parent->get_postdominators(idx);
}

void CFG::DefineLoops() {
    if (defined_loops) return;

    // for each backedge, define loop body
    // body = {H} push N onto an empty stack;
    // while (stack != empty) {
    //     pop D from the stack;
    //     if (D not in body) {
    //         body = { D } union body;
    //         push each predecessor of D onto the stack.
    //     }
    // }
    for (BasicBlock* n : blocks) {
        for (BasicBlock* h : n->get_successors()) {
            if (h->dominates(n)) {  // n->h is a backedge iff h dom n
                Loop l;
                l.header = h;
                l.end = n;
                l.body.push_back(h);
                std::deque<BasicBlock*> worklist;
                worklist.push_back(n);
                while (!worklist.empty()) {
                    BasicBlock* d = worklist.front();
                    worklist.pop_front();
                    bool d_in_body = false;
                    for (auto b : l.body) {
                        if (b->get_addr() == d->get_addr()) {
                            d_in_body = true;
                            break;
                        }
                    }
                    if (!d_in_body) {
                        l.body.push_back(d);
                        for (BasicBlock* b : d->get_predecessors())
                            worklist.push_back(b);
                    }
                }
                loops.push_back(l);
            }
        }
    }

    defined_loops = true;
}

std::vector<Loop> CFG::LoopsWith(BasicBlock* b) {
    std::vector<Loop> rets;
    for (Loop l : loops) {
        for (BasicBlock* lb : l.body) {
            if (lb->get_addr() == b->get_addr()) {
                rets.push_back(l);
                break;
            }
        }
    }
    return rets;
}

void Module::add_function(Function* function, const char* name, IAddr addr) {
    functions[name] = function;
    functions_by_addr[addr] = function;
#ifdef DEBUG
    std::cout << "📐 " << hex(addr) << ": " << name << std::endl;
#endif
}

bool Function::does_not_return() {
    return name == "exit" || name == "cexit" || name == "c_exit" ||
           name == "abort" || name == "reboot" || name == "longjmp" ||
           name == "longjmp_chk" || name == "siglongjmp" || name == "panic" ||
           name == "stack_chk_fail" || name == "cxa_throw" ||
           name == "cxa_terminate" || name == "cxa_call_unexpected" ||
           name == "cxa_bad_cast" || name == "Unwind_Resume" ||
           name == "assert_fail" || name == "assert_rtn" ||
           name == "fortify_fail" || name == "ZSt9terminatev" ||
           name == "ZN10__cxxabiv111__terminateEPFvvE" ||
           name == "pthread_exit";
}

// DOT GRAPHVIZ
const char* color(EdgeType t) {
    switch (t) {
        case CONDITIONAL_JUMP:
        case JUMP:
            return "purple";
        case PERSISTS:
            return "green";
        case FALLTHROUGH:
        default:
            return "black";
    }
}

const char* color(tracing::pm::Instruction t) {
    switch (t) {
        case tracing::pm::Instruction::STORE:
        case tracing::pm::Instruction::NON_TEMPORAL_STORE:
        case tracing::pm::Instruction::RMW:
            return "red";
        case tracing::pm::Instruction::LOAD:
            return "blue";
        case tracing::pm::Instruction::SYNC:
        case tracing::pm::Instruction::RELEASE:
        case tracing::pm::Instruction::ACQUIRE:
            return "darkgreen";
        case tracing::pm::Instruction::FENCE:
        case tracing::pm::Instruction::CLFLUSH:
        case tracing::pm::Instruction::CLWB:
        case tracing::pm::Instruction::CLFLUSHOPT:
            return "orange";
        case tracing::pm::Instruction::BRANCH_COND:
        case tracing::pm::Instruction::BRANCH:
            return "purple";
        case tracing::pm::Instruction::BRANCH_TARGET:
        case tracing::pm::Instruction::ERROR:
        case tracing::pm::Instruction::OTHER:
        default:
            return "black";
    }
}

void CFG::dot_bb_only(std::ostream& os) {
    for (auto bb : blocks) bb->dot(os);
}

void CFG::dot_bb_edges(std::ostream& os) {
    for (auto src : blocks) {
        for (auto target : src->get_successors()) {
            if (!src->get_instructions().size() ||
                !target->get_instructions().size())
                continue;  // hammering
            os << src->get_addr() << " -> " << target->get_addr() << std::endl;
        }
    }
}

void CFG::dot_bb(std::ostream& os) {
    os << "digraph D {" << std::endl;
    os << "label=\"" << this->get_function()->get_name() << "\"" << std::endl;
    os << "node [shape=plaintext fontname=\"consolas\" fontsize=\"8\"]"
       << std::endl;
    dot_bb_only(os);
    dot_bb_edges(os);
    os << "}" << std::endl;
}
void CFG::dot_ins_only(std::ostream& os) {
    for (auto instruction : this->get_function()->get_instructions()) {
        os << instruction->get_addr() << " [ label =< " << std::endl;
        instruction->dot(os);
        os << "> ]" << std::endl;
    }
}

void CFG::dot_ins_edges(std::ostream& os) {
    for (auto src : forward_edges)
        for (auto edge : src.second)
            os << src.first << " -> " << edge->dst << "[arrowsize=0.5 color=\""
               << color(edge->type) << "\"]" << std::endl;
}

void CFG::dot_ins(std::ostream& os) {
    os << "digraph D {" << std::endl;
    os << "label=\"" << this->get_function()->get_name() << "\"" << std::endl;
    os << "node [shape=rectangle fontname=\"consolas\" fontsize=\"8\" "
          "nodesep=0.1]"
       << std::endl;
    dot_ins_only(os);
    dot_ins_edges(os);
    os << "}" << std::endl;
}

void BasicBlock::dot(std::ostream& os) {
    if (this->instructions.size() == 0) return;  // hammering
    os << this->get_addr() << " [ label =< " << std::endl
       << "<table border = \"1\" cellborder =\"0\" cellspacing =\"1\">"
       << std::endl;
    for (Instruction* instruction : this->get_instructions()) {
        os << "<tr><td align=\"left\">";
        instruction->dot(os);
        os << "</td></tr>" << std::endl;
    }
    os << "</table>> ]" << std::endl;
}

void Instruction::dot(std::ostream& os) {
    os << "<font color=\"" << color(type) << "\">";
    if (x86->is_call()) {
#ifdef DEBUG
        std::cout << this->disassembled() << std::endl;
        std::cout << std::hex << x86->jump_target() << std::endl;
#endif
        Function* f = get_call_target();
        if (f != nullptr) {
            os << hex(addr) << ": call " << f->get_name();
        } else {
            os << hex(addr) << ": " << this->disassembled();
        }
    } else {
        os << hex(addr) << ": " << this->disassembled();
    }
    // if (type == tracing::Instruction::SYNC)
    //     os << " ("
    //        << sync_calls
    //               .find(INS_DirectControlFlowTargetAddress(instruction) -
    //                     offset)
    //               ->second
    //        << ")";
    os << "</font>";
}

void Module::dot(bool single_file) {
    if (single_file) {
        std::ofstream out;
        std::stringstream ss;
        ss << this->name << ".dot";
        out.open(ss.str().c_str());
        this->dot(out);
        out.close();
    } else {
        for (auto pair : this->functions) {
            if (!pair.second->is_internal()) continue;
            Function* f = pair.second;
            std::ofstream os;
            std::stringstream ss;
            ss << f->get_name() << ".dot";
            os.open(ss.str().c_str());
            os << "digraph D {" << std::endl;
            os << "compound=true;" << std::endl;
            os << "label=\"" << f->get_name() << "\"" << std::endl;
            os << "node [shape=plaintext fontname=\"consolas\" fontsize=\"8\"]"
               << std::endl;
            f->get_cfg()->dot_bb_only(os);
            // os << "}" << std::endl;
            f->get_cfg()->dot_bb_edges(os);
            os << "}" << std::endl;
            os.close();
        }
    }
}

void Module::dot_dominators() {
    for (auto pair : this->functions) {
        if (!pair.second->is_internal()) continue;
        Function* f = pair.second;
        std::ofstream os;
        std::stringstream ss;
        ss << f->get_name() << ".dominators.txt";
        os.open(ss.str().c_str());
        CFG* cfg = f->get_cfg();
        std::vector<BasicBlock*> blocks = cfg->get_blocks();
        os << f->get_name() << std::endl << std::endl;

        os << "BLOCKS" << std::endl << std::endl;
        for (size_t i = 0; i < blocks.size(); i++) {
            os << std::dec << i + 1 << ": " << std::hex << blocks[i]->get_addr()
               << std::endl;
        }
        os << std::endl << "=====================" << std::endl << std::endl;
        os << "DOMINATORS" << std::endl << std::endl;
        for (size_t i = 0; i < blocks.size(); i++) {
            std::set<BasicBlock*> doms = cfg->get_dominators(i);
            os << std::dec << i + 1 << ": ";
            for (auto d : doms) {
                for (size_t i = 0; i < blocks.size(); i++) {
                    if (blocks[i] == d) {
                        os << std::dec << i + 1 << ", ";
                    }
                }
            }
            os << std::endl;
        }
        os << std::endl << "=====================" << std::endl << std::endl;
        os << "POSTDOMINATORS" << std::endl << std::endl;
        for (size_t i = 0; i < blocks.size(); i++) {
            std::set<BasicBlock*> pdoms = cfg->get_postdominators(i);
            os << std::dec << i + 1 << ": ";
            for (auto p : pdoms) {
                for (size_t i = 0; i < blocks.size(); i++) {
                    if (blocks[i] == p) {
                        os << std::dec << i + 1 << ", ";
                    }
                }
            }
            os << std::endl;
        }
        os << std::endl << "=====================" << std::endl << std::endl;
        os << "LOOPS" << std::endl << std::endl;
        for (auto loop : cfg->get_loops()) {
            for (auto b : loop.body) {
                os << std::hex << b->get_addr() << ", ";
            }
            os << std::endl;
        }
        os.close();
    }
}

void Module::dot(std::ostream& os) {
    os << "digraph D {" << std::endl;
    os << "compound=true;" << std::endl;
    os << "label=\"" << this->get_name() << "\"" << std::endl;
    for (auto pair : this->functions) {
        if (!pair.second->is_internal()) continue;
        os << "subgraph cluster" << pair.first << " {" << std::endl;
        os << "label=\"" << pair.first << "\"" << std::endl;
        os << "node [shape=plaintext fontname=\"consolas\" fontsize=\"8\"]"
           << std::endl;
        pair.second->get_cfg()->dot_bb_only(os);
        os << "}" << std::endl;
    }
    for (auto pair : this->functions) {
        if (!pair.second->is_internal()) continue;
        pair.second->get_cfg()->dot_bb_edges(os);
    }
    os << "}" << std::endl;
}

void Module::BuildCallGraph() {
    if (built_cg) return;

    for (auto pair : functions) {
        Function* f = pair.second;
        CFG* cfg = f->get_cfg();
        for (BasicBlock* b : cfg->get_blocks()) {
            for (Instruction* i : b->get_instructions()) {
                if (!i->x86->is_call()) continue;
                Function* target = i->get_call_target();
                if (target == nullptr) {
                    std::cerr << "Call target unresolved: 0x" << std::hex
                              << i->get_addr() << ":\t" << i->disassembled()
                              << std::endl;
                    continue;
                }
                call_graph.add_edge(f->get_addr(), target->get_addr());
            }
        }
    }

    built_cg = true;
}

void CallGraph::dot(std::ostream& os) {
    os << "digraph D {" << std::endl;
    os << "compound=true;" << std::endl;
    os << "label=\"" << this->parent->get_name() << "\"" << std::endl;
    for (auto pair : this->parent->get_functions()) {
        // if (!pair.second->is_internal()) continue;
        // os << "subgraph cluster" << pair.first << " {" << std::endl;

        os << pair.second->get_addr() << " [label=" << pair.first << "]"
           << std::endl;
        // pair.second->get_cfg()->dot_bb_only(os);
        // os << "}" << std::endl;
    }
    for (auto pair : call_edges) {
        IAddr caller = pair.first;
        for (IAddr callee : pair.second) {
            os << caller << " -> " << callee << std::endl;
        }
    }
    os << "}" << std::endl;
}
}  // namespace palantir
