// Copyright 2023 João Gonçalves

#include "./statica.hpp"

#include <Zydis/Zydis.h>

#include <algorithm>
#include <csignal>
#include <deque>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>

#include "cfg.hpp"
#include "x86.hpp"

namespace palantir {

Context CreateContext(uint8_t img_id, const char* trace_path) {
    Context ctx;

    std::ifstream trace(trace_path, std::ifstream::binary);
    std::vector<char> buffer(1000 * TRACE_LINE_SIZE, 0);
    while (true) {
        tracing::rwflow::TraceLine l = {0, 0, 0, 0, 0};
        trace.read(reinterpret_cast<char*>(&l),
                   sizeof(tracing::rwflow::TraceLine));
        if (trace.eof()) break;
        if (l.img == img_id) {
            switch (l.type) {
                // case tracing::rwflow::WRITE:
                case tracing::rwflow::PM_WRITE:
                    ctx.relevant_insts[l.ip] = {tracing::pm::Instruction::STORE,
                                                l.size};
                    break;
                // case tracing::rwflow::READ:
                case tracing::rwflow::PM_READ:
                    ctx.relevant_insts[l.ip] = {tracing::pm::Instruction::LOAD,
                                                l.size};
                    break;
                case tracing::rwflow::FLUSH:
                    ctx.relevant_insts[l.ip] = {tracing::pm::Instruction::CLWB,
                                                256};
                    break;
                default:
                    break;
            }
        }
    }
    ctx.out.open("to_patch.csv");
    return ctx;
}

Context CreateContext(uint8_t img_id, const char* trace_path,
                      const char* acquire_calls_path,
                      const char* release_calls_path) {
    Context ctx = CreateContext(img_id, trace_path);

    std::string func;
    std::ifstream acquire(acquire_calls_path);
    while (getline(acquire, func)) {
        ctx.acquire_calls.insert(func);
    }

    std::ifstream release(release_calls_path);
    while (getline(release, func)) {
        ctx.release_calls.insert(func);
    }

    return ctx;
}

void IdentifyFlowControlRW(Function* f) {
    CFG* cfg = f->get_cfg();

    /*
     for block in cfg
        if edge is only fall:
            continue
        interesting regs =  get from inst (block[-2])
        for inst in remainder:
            if loads mem to inst reg
            if loads imm to inst reg
    */
    std::cout << f->get_name() << std::endl;
    for (BasicBlock* bb : cfg->get_blocks()) {
        std::vector<Instruction*> instructions = bb->get_instructions();
        if (!instructions.size()) continue;  // empty bb?

        const x86::Instruction* terminator =
            instructions[instructions.size() - 1]->x86;
        if (!terminator->is_jump() || terminator->is_direct_jump()) continue;

        if (instructions.size() < 2) {
            std::cerr << "🤨" << std::endl;
            continue;
        }

        Instruction* cond = instructions[instructions.size() - 2];
        cond->set_type(tracing::pm::Instruction::BRANCH_COND);
        const x86::Instruction* condx86 = cond->x86;

        std::cout << "🤷 " << condx86->disassembled() << std::endl;
        std::vector<ZydisRegister> relevant_registers =
            condx86->get_relevant_operands();

        BasicBlock* current = bb;
        while (relevant_registers.size()) {
            for (int i = instructions.size() - 1; i >= 0; i--) {
                const x86::Instruction* inst = instructions[i]->x86;
                ZydisRegister w = inst->register_written();
                if (w == ZYDIS_REGISTER_NONE) continue;
                auto itr = std::find(relevant_registers.begin(),
                                     relevant_registers.end(), w);
                if (itr == relevant_registers.end()) continue;
                relevant_registers.erase(itr);
                instructions[i]->set_type(tracing::pm::Instruction::SYNC);

                if (!relevant_registers.size()) goto block_end;
            }
            std::vector<BasicBlock*> pred = current->get_predecessors();
            if (pred.size() != 1) {
                std::cerr << "Troubles in paradise: " << pred.size()
                          << std::endl;
                goto block_end;
            }
            current = pred[0];
        }
    block_end:
        continue;
    }
}

void IdentifyFlowControlRW(Module* m) {
    std::cout << "identify flow" << std::endl;
    for (auto pair : m->get_functions()) {
        Function* f = pair.second;
        if (!f->is_internal()) continue;
        IdentifyFlowControlRW(f);
    }
}

void FindPIFRs(Context& ctx, Function* f);
void FindPIFRsWithEnds(Context& ctx, Function* f);
void FindFlushes(Context& ctx, Function* f);
void FindPIFRs(Module* m, Context& ctx, bool persist_only, bool define_ends) {
    for (auto fpair : m->get_functions()) {
        if (persist_only) {
            FindFlushes(ctx, fpair.second);
        }
        if (define_ends) {
            FindPIFRsWithEnds(ctx, fpair.second);
        } else {
            FindPIFRs(ctx, fpair.second);
        }
    }
    ctx.out.close();
}

void FindMinimalWindows(Context& ctx, Function* f);
void FindMinimalWindows(Module* m, Context& ctx) {
    for (auto fpair : m->get_functions()) {
        FindMinimalWindows(ctx, fpair.second);
    }
    ctx.out.close();
}

typedef std::vector<Instruction*>::const_iterator InstItr;
typedef std::vector<Instruction*>::const_reverse_iterator RevInstItr;
#define REV_ITR(itr) std::vector<Instruction*>::const_reverse_iterator(itr)
#define FW_ITR(itr) itr.base() - 1

struct PatchPoint {
    InstItr inst;
    x86::MemAccess target;
};

struct Step {
    BasicBlock* block;
    PatchPoint pp;
};

enum Result { OK, NOK_BLOCK, NOK_WALK };

struct ProcessResult {
    Result r;
    PatchPoint pp;
    std::vector<BasicBlock*> preds;
};

struct WalkBlockResult {
    bool possible;
    PatchPoint pp;
};

struct WalkCFGResult {
    bool possible;
    std::vector<BasicBlock*> preds;
};

typedef std::vector<Step> Path;

bool UpdateTarget(Instruction* inst, x86::MemAccess* target) {
    if (target->indirect && inst->x86->writes_memory()) {
        x86::MemAccess* new_ma = inst->x86->writes_to_memory();
        if (x86::same_mem_access(*target, *new_ma)) {
            if (!inst->x86->is_mov()) return false;
            ZydisRegister src = inst->x86->reads_from_register();
            if (src) {
                target->indirect = false;
                target->base = src;
                target->index = ZYDIS_REGISTER_NONE;
                target->displacement = target->post_displacement;
                target->post_displacement = 0;
                target->scale = 0;
                return true;
            }
            return false;
        }
        return true;
    } else {
        ZydisRegister reg = inst->x86->register_written();
        if (reg == ZYDIS_REGISTER_NONE) return true;  // nothing to do
        auto rf = x86::RegToFamily(reg);
        if (rf == x86::RegToFamily(target->base) ||
            rf == x86::RegToFamily(target->index)) {
            // std::cout << inst->disassembled() << std::endl;
            if (inst->x86->is_mov()) {
                // in threory, this could be an imm but pm does not work
                // that way. only a volatile global addr could be an imm
                ZydisRegister src = inst->x86->reads_from_register();
                if (src) {
                    if (reg == target->index) target->index = src;
                    if (reg == target->base) target->base = src;
                    return true;
                } else {
                    if (reg == target->base) {
                        if (!target->index) {
                            x86::MemAccess* new_ma =
                                inst->x86->reads_from_memory();
                            target->post_displacement = target->displacement;
                            target->displacement = new_ma->displacement;
                            target->base = new_ma->base;
                            target->index = new_ma->index;
                            target->scale = new_ma->scale;
                            target->indirect = true;
                            return true;
                        }
                        return false;
                    }
                }
            }

            return false;
        }
    }

    return true;
}

WalkBlockResult WalkBlock(Step s, BasicBlock* orig_bb) {
    PatchPoint pp = s.pp;
    x86::MemAccess target = pp.target;
    RevInstItr bb_start = s.block->get_instructions().rend();
    auto itr = REV_ITR(pp.inst);
    for (; itr != bb_start; itr++) {
        Instruction* inst = *itr;
        if (inst->get_type() == tracing::pm::ACQUIRE)
            return {false, {FW_ITR(itr), target}};
        if (inst->x86->is_call() && (target.base == ZYDIS_REGISTER_RAX ||
                                     target.index == ZYDIS_REGISTER_RAX))
            return {false, {FW_ITR(itr), target}};
        if (!UpdateTarget(inst, &target)) return {false, {FW_ITR(itr), target}};
        std::cout << inst->disassembled() << std::endl;
        std::cout << x86::MemAccessToStr(&target) << std::endl;
    }
    itr--;

    return {true, {FW_ITR(itr), target}};
}

WalkCFGResult WalkCFG(BasicBlock* current, BasicBlock* orig_bb,
                      bool changed_target) {
    std::vector<BasicBlock*> preds = current->get_predecessors();
    std::vector<BasicBlock*> rets;
    if (preds.size() == 0) return {false, {}};
    // for (BasicBlock* pred : preds) {
    for (BasicBlock* pred : preds) {
        if (pred->get_addr() == current->get_addr() && changed_target)
            return {false, {}};
        // if (pred->get_postdominators().find(orig_bb) ==
        //     pred->get_postdominators().end())
        if (!orig_bb->postdominates(pred)) return {false, {}};
        if (!current->dominates(pred)) rets.push_back(pred);
    }
    std::cout << "walked" << std::endl;
    return {true, rets};
}

ProcessResult ProcessBlock(Step s, BasicBlock* orig_bb) {
    ProcessResult pr;
    WalkBlockResult wbr = WalkBlock(s, orig_bb);
    if (!wbr.possible) return {NOK_BLOCK, wbr.pp, {}};
    auto wcr = WalkCFG(s.block, orig_bb,
                       x86::same_mem_access(s.pp.target, wbr.pp.target));
    return {wcr.possible ? OK : NOK_WALK, wbr.pp, wcr.preds};
}

bool same_patch_point(PatchPoint pp1, PatchPoint pp2) {
    auto i1 = *pp1.inst;
    auto i2 = *pp2.inst;
    return i1->get_addr() == i2->get_addr() &&
           x86::same_mem_access(pp1.target, pp2.target);
}

bool BlockInPath(BasicBlock* bb, Path p) {
    for (Step s : p) {
        if (bb->get_addr() == s.block->get_addr()) return true;
    }
    return false;
}

std::vector<PatchPoint> FindPossiblePIFRStart(
    BasicBlock* origin_bb, std::vector<Instruction*>::const_iterator inst_itr,
    x86::MemAccess target) {
    CFG* cfg = origin_bb->get_parent();
    BasicBlock* current_bb = origin_bb;
    std::vector<Path> paths;
    Path current_path;
    Step current_step = {current_bb, {inst_itr, target}};

    struct WorklistItem {
        Step s;
        Path p;
    };
    std::deque<WorklistItem> worklist;
    while (true) {
        bool should_progress = true;
        ProcessResult pr = ProcessBlock(current_step, origin_bb);
        current_path.push_back({current_step.block, pr.pp});
        if (pr.r == OK) {
            std::vector<Loop> loops = cfg->LoopsWith(current_step.block);
            for (Loop l : loops) {
                bool loop_has_orig = false;
                for (BasicBlock* b : l.body) {
                    if (b->get_addr() == origin_bb->get_addr())
                        loop_has_orig = true;
                }

                // if current_step in loop that orig is not and changed target
                // go back to previous step outside loop

                if (!loop_has_orig &&
                    !x86::same_mem_access(current_step.pp.target,
                                          pr.pp.target)) {
                    should_progress = false;
                    Path new_path;
                    for (auto step : current_path) {
                        bool step_in_loop = false;
                        for (BasicBlock* b : l.body) {
                            if (b->get_addr() == step.block->get_addr())
                                step_in_loop = true;
                        }
                        if (step_in_loop) break;
                        new_path.push_back(step);
                    }
                    paths.push_back(new_path);
                }

                // if current step is the header of a loop that contains access
                // confirm if target is changed inside that loop and close
                // region if that is the case
                if (loop_has_orig && current_step.block == l.header) {
                    bool should_close = false;
                    for (BasicBlock* b : l.body) {
                        // if (BlockInPath(b, current_path)) continue;
                        for (Instruction* inst : b->get_instructions()) {
                            /*if (inst->get_type() == tracing::pm::ACQUIRE) {
                                should_close = true;
                                break;
                            } else*/
                            if (inst->x86->is_call() &&
                                (pr.pp.target.base == ZYDIS_REGISTER_RAX ||
                                 pr.pp.target.index == ZYDIS_REGISTER_RAX)) {
                                should_close = true;
                                break;
                            }

                            ZydisRegister reg = inst->x86->register_written();
                            if (reg == ZYDIS_REGISTER_NONE) continue;
                            if (reg == pr.pp.target.base ||
                                reg == pr.pp.target.index) {
                                should_close = true;
                                break;
                            }
                        }
                        if (should_close) break;
                    }
                    if (should_close) {
                        paths.push_back(current_path);

                        should_progress = false;
                    }
                }
            }
            if (should_progress) {
                for (BasicBlock* pred : pr.preds) {
                    if (BlockInPath(pred, current_path)) continue;
                    worklist.push_back(
                        {{pred, {pred->get_instructions().end(), pr.pp.target}},
                         current_path});
                }
            }
        } else {
            // if impossible and current_step block in a loop that orig_bb is
            // not go back to previous step outside that loop
            if (pr.r == NOK_BLOCK) {
                std::vector<Loop> loops = cfg->LoopsWith(current_step.block);

                for (Loop l : loops) {
                    bool loop_has_orig = false;
                    for (BasicBlock* b : l.body) {
                        if (b->get_addr() == origin_bb->get_addr())
                            loop_has_orig = true;
                    }

                    if (!loop_has_orig) {
                        Path new_path;
                        for (auto step : current_path) {
                            bool step_in_loop = false;
                            for (BasicBlock* b : l.body) {
                                if (b->get_addr() == step.block->get_addr())
                                    step_in_loop = true;
                            }
                            if (step_in_loop) break;
                            new_path.push_back(step);
                        }
                        paths.push_back(new_path);
                    }
                }
            }
            paths.push_back(current_path);
        }
        if (worklist.empty()) break;
        auto item = worklist.front();
        worklist.pop_front();
        current_step = item.s;
        current_path = item.p;
    }
restart:
    for (auto p1 = paths.begin(); p1 < paths.end(); p1++) {
        for (auto p2 = paths.begin(); p2 < paths.end(); p2++) {
            if (p1 == p2) continue;
            auto s1 = p1->back();
            auto s2 = p2->back();
            if (s1.block == s2.block) {
                if (!same_patch_point(s1.pp, s2.pp)) {
                    // incompatible paths, must discard last step of
                    // each
                    p1->pop_back();
                    p2->pop_back();
                } else {
                    // paths converge (aka duplicates), must discard one
                    paths.erase(p2);
                }
                goto restart;
            }
            if ((*s1.pp.inst)->get_type() == tracing::pm::ACQUIRE &&
                s2.block->dominates(s1.block)) {
                p2->pop_back();
                goto restart;
            }
            // else if ((*s2.pp.inst)->get_type() == tracing::pm::ACQUIRE &&
            //            s1.block->dominates(s2.block)) {
            //     p1->pop_back();
            //     goto restart;
            // }
            if (s1.block->postdominates(s2.block)) {
                p2->pop_back();
                goto restart;
            }
            // if (s2.block->postdominates(s1.block)) {
            //     p1->pop_back();
            //     goto restart;
            // }
        }
    }
    std::vector<PatchPoint> results;

    for (Path p : paths) {
        results.push_back(p[p.size() - 1].pp);
    }

    return results;
}

void csv_out_mem_access(Context& ctx, Instruction* inst, x86::MemAccess* ma) {
    ctx.out << "0x" << std::hex << inst->get_addr() << ",0x" << std::hex
            << x86::RegToMask(ma->base) << ",0x" << std::hex << +ma->scale
            << ",0x" << std::hex << x86::RegToMask(ma->index) << ",0x"
            << std::hex << ma->displacement << "," << std::dec
            << ctx.relevant_insts[inst->get_addr()].second;
    if (ma->indirect) ctx.out << ",0x" << std::hex << ma->post_displacement;
    ctx.out << std::endl;
}

typedef std::vector<Instruction*> DiscretePath;
typedef std::vector<BasicBlock*> CoarsePath;

bool BlockInPath(BasicBlock* b, CoarsePath p) {
    for (BasicBlock* bb : p) {
        if (bb->get_addr() == bb->get_addr()) return true;
    }
    return false;
}

std::vector<DiscretePath> AllPathsBetween(InstItr src, InstItr dest) {
    std::vector<CoarsePath> coarse_paths;
    std::deque<CoarsePath> worklist;
    BasicBlock* src_block = (*src)->get_parent();
    BasicBlock* dest_block = (*dest)->get_parent();
    worklist.push_back({src_block});

    while (!worklist.empty()) {
        CoarsePath current_path = worklist.front();
        BasicBlock* current_block = current_path.back();
        worklist.pop_front();
        for (BasicBlock* succ : current_block->get_successors()) {
            if (succ->get_addr() == dest_block->get_addr()) {
                CoarsePath new_path = current_path;
                new_path.push_back(succ);
                coarse_paths.push_back(new_path);
            } else if (!succ->is_cfg_terminator() &&
                       !BlockInPath(succ, current_path)) {
                CoarsePath new_path = current_path;
                new_path.push_back(succ);
                worklist.push_back(new_path);
            }
        }
    }

    std::vector<DiscretePath> discrete_paths;
    for (CoarsePath p : coarse_paths) {
        DiscretePath path;
        for (BasicBlock* bb : p) {
            for (auto itr = (bb->get_addr() == src_block->get_addr())
                                ? src
                                : bb->get_instructions().cbegin();
                 itr != bb->get_instructions().cend(); itr++) {
                if (bb->get_addr() == dest_block->get_addr() && itr == dest)
                    break;
                path.push_back(*itr);
            }
        }
        discrete_paths.push_back(path);
    }

    return discrete_paths;
}

std::vector<InstItr> GetCFGEnds(CFG* cfg) {
    std::vector<InstItr> ends;
    for (BasicBlock* b : cfg->get_blocks()) {
        std::cout << b->get_addr() << " " << b->is_cfg_terminator()
                  << std::endl;
        if (b->is_cfg_terminator())
            ends.push_back(b->get_instructions().cend() - 1);
    }
    std::cout << "[helper] CFG ends #" << ends.size() << std::endl;
    return ends;
}

std::vector<InstItr> FindPossiblePIFREnds(BasicBlock* origin_bb,
                                          InstItr access_itr, bool is_read) {
    std::vector<InstItr> releases;
    std::vector<InstItr> flushes;
    std::vector<InstItr> pifr_ends;
    CFG* cfg = origin_bb->get_parent();

    // short-circuit
    // if (is_read) {
    //     if (!cfg->Releases()) goto force_ends;
    // } else
    if (!cfg->Flushes() || !cfg->Releases()) goto force_ends;

    // get releases and flushes
    for (BasicBlock* bb : cfg->get_blocks()) {
        for (auto itr = bb->get_instructions().cbegin();
             itr != bb->get_instructions().cend(); itr++) {
            auto i = *itr;
            if (i->releases())
                releases.push_back(itr);
            else if (i->flushes())
                flushes.push_back(itr);
        }
    }

    /*  for all release inst i in g:
            if there is path p where a ->p i
                and there is an inst i' in p
                    such that i' is  a flush:*/
    for (auto release_itr : releases) {
        auto paths = AllPathsBetween(access_itr, release_itr);
        bool should_end = false;
        for (auto path : paths) {
            for (Instruction* i : path) {
                if (is_read || i->flushes()) {
                    should_end = true;
                    break;
                }
            }
            if (should_end) break;
        }
        if (should_end) pifr_ends.push_back(release_itr);
    }

force_ends:
    /*  for all cfg end inst i in g:
            if there is a path p such that a ->p i
                and there is not an inst i' in ends
                    such that i' dom i: */
    auto cfg_ends = GetCFGEnds(cfg);
    std::cout << "CFG ends #" << cfg_ends.size() << std::endl;
    for (auto end_itr : cfg_ends) {
        auto paths = AllPathsBetween(access_itr, end_itr);
        bool should_end = true;
        std::cout << "cfg_end: 0x" << std::hex << (*end_itr)->get_addr();
        for (auto path : paths) {
            bool covered_path = false;
            for (Instruction* i : path) {
                for (InstItr pifr_end_itr : pifr_ends) {
                    if ((*pifr_end_itr)->dominates(i)) {
                        covered_path = true;
                        break;
                    }
                }
                if (covered_path) break;
            }
            if (covered_path) {
                should_end = false;
                break;
            }
        }
        if (should_end) {
            pifr_ends.push_back(end_itr);
            std::cout << " yes" << std::endl;
        } else
            std::cout << " no" << std::endl;
    }

    return pifr_ends;
}

static int pifr_id = 0;
void DefinePIFR(Context& ctx, BasicBlock* origin_bb, InstItr pm_access_itr,
                bool define_ends = false) {
    Instruction* pm_access = *pm_access_itr;
    x86::MemAccess* ma;
    bool is_read = pm_access->get_type() == tracing::pm::Instruction::LOAD;

    if (is_read) {
        ma = pm_access->x86->reads_from_memory();
        std::cout << "READ PIFR" << std::endl;
    } else {
        ma = pm_access->x86->writes_to_memory();
        if (pm_access->x86->is_ntstore()) return;  // ignore nt stores
        std::cout << "WRITE PIFR" << std::endl;
    }

    if (ma == nullptr) {
        std::cerr << "BUG: (ma is null): " << pm_access->get_addr() << ": "
                  << pm_access->disassembled() << std::endl;
        return;
    }

    std::cout << "TARGET:" << std::endl;
    std::cout << std::hex << (*pm_access_itr)->get_addr() << ": "
              << (*pm_access_itr)->disassembled() << " "
              << x86::MemAccessToStr(ma) << std::endl;

    std::vector<PatchPoint> starts =
        FindPossiblePIFRStart(origin_bb, pm_access_itr, *ma);
    // PatchPoint pp;
    // pp.inst =
    // std::vector<Instruction*>::const_reverse_iterator(pm_access_itr);
    // pp.target = *ma;
    // starts.push_back(pp);

    for (PatchPoint pp : starts) {
        Instruction* inst = *pp.inst;
        x86::MemAccess target = pp.target;
        std::cout << "START PIFR AT" << std::endl;
        std::cout << std::hex << inst->get_addr() << ": "
                  << inst->disassembled() << " " << x86::MemAccessToStr(&target)
                  << std::endl;

        if (target.indirect) {
            ctx.out << (pm_access->get_type() == tracing::pm::Instruction::LOAD
                            ? "indirect_read"
                            : "indirect_write");
        } else {
            ctx.out << (pm_access->get_type() == tracing::pm::Instruction::LOAD
                            ? "read"
                            : "write");
        }
        // << "," << pifr_id
        ctx.out << ",0x" << std::hex << inst->get_addr() << ",";
        csv_out_mem_access(ctx, pm_access, &target);

        std::cout << std::endl;
    }

    if (define_ends) {
        std::cout << "END PIFR AT" << std::endl;
        for (auto end :
             FindPossiblePIFREnds(origin_bb, pm_access_itr, is_read)) {
            std::cout << "0x" << std::hex << (*end)->get_addr() << std::endl;
            ctx.out << "release,0x" << std::hex << (*end)->get_addr()
                    << std::endl;
            // << "," << init_pifr << "," << pifr_id - 1 << std::endl;
        }
        pifr_id++;
    }
}

bool annotate_relevant_insts(Function* f, Context& ctx) {
    auto instructions = f->get_instructions();
    bool ret = false;
    for (auto it = instructions.begin(); it != instructions.end(); it++) {
        Instruction* ins = *it;
        if (ins->x86->is_call()) {
            Function* callee =
                f->get_parent()->get_function(ins->x86->jump_target());
            if (callee != nullptr) {
                if (ctx.acquire_calls.find(callee->get_name()) !=
                    ctx.acquire_calls.end())
                    ins->set_type(tracing::pm::ACQUIRE);
                if (ctx.release_calls.find(callee->get_name()) !=
                    ctx.release_calls.end())
                    ins->set_type(tracing::pm::RELEASE);
            }
        } else {
            auto f = ctx.relevant_insts.find(ins->get_addr());
            if (f != ctx.relevant_insts.end()) {
                ret = true;
                ins->set_type(f->second.first);
            }
        }
    }
    return ret;
}

void FindFlushes(Context& ctx, Function* f) {
    if (!f->is_internal()) return;
    CFG* cfg = f->get_cfg();

    for (BasicBlock* current_bb : cfg->get_blocks()) {
        for (auto itr = current_bb->get_instructions().begin();
             itr != current_bb->get_instructions().end(); itr++) {
            Instruction* ins = *itr;
            if (ins->x86->is_flush()) {
                ctx.out << "flush,";
                csv_out_mem_access(ctx, ins, ins->x86->reads_from_memory());
            }
        }
    }
}

void FindPIFRs(Context& ctx, Function* f) {
    if (!f->is_internal()) return;

    // if (!annotate_relevant_insts(f, ctx)) return;
    annotate_relevant_insts(f, ctx);

    CFG* cfg = f->get_cfg();

    // auto init_pifr = pifr_id;

    for (BasicBlock* current_bb : cfg->get_blocks()) {
        for (auto itr = current_bb->get_instructions().begin();
             itr != current_bb->get_instructions().end(); itr++) {
            Instruction* ins = *itr;
            switch (ins->get_type()) {
                case tracing::pm::STORE:
                case tracing::pm::NON_TEMPORAL_STORE:
                case tracing::pm::LOAD:
                    cfg->ComputeDominators();
                    DefinePIFR(ctx, current_bb, itr);
                    break;
                case tracing::pm::RELEASE:
                    ctx.out << "release,0x" << std::hex << ins->get_addr()
                            << std::endl;
                    break;
                case tracing::pm::CLFLUSH:
                case tracing::pm::CLFLUSHOPT:
                case tracing::pm::CLWB:
                    // ctx.out << "flush,";
                    // csv_out_mem_access(ctx, ins,
                    // ins->x86->reads_from_memory()); break;
                case tracing::pm::FENCE:
                case tracing::pm::RMW:
                case tracing::pm::SYNC:
                case tracing::pm::ACQUIRE:
                case tracing::pm::BRANCH:
                case tracing::pm::BRANCH_COND:
                case tracing::pm::BRANCH_TARGET:
                case tracing::pm::ERROR:
                case tracing::pm::OTHER:
                default:
                    break;
            }

            if (ins->x86->is_flush()) {
                ctx.out << "flush,";
                csv_out_mem_access(ctx, ins, ins->x86->reads_from_memory());
            }
        }
    }
    // close all
    // if (init_pifr == pifr_id) return;
    // for (BasicBlock* current_bb : cfg->get_blocks()) {
    //     if (current_bb->is_cfg_terminator()) {
    //         ctx.out << "release,0x"
    //                 << current_bb->get_instructions().back()->get_addr()
    //                 << std::endl;
    //         // << "," << init_pifr << "," << pifr_id - 1 << std::endl;
    //     }
    // }
}

void FindPIFRsWithEnds(Context& ctx, Function* f) {
    if (!f->is_internal()) return;

    if (!annotate_relevant_insts(f, ctx)) return;

    CFG* cfg = f->get_cfg();

    for (BasicBlock* current_bb : cfg->get_blocks()) {
        for (auto itr = current_bb->get_instructions().begin();
             itr != current_bb->get_instructions().end(); itr++) {
            Instruction* ins = *itr;
            switch (ins->get_type()) {
                case tracing::pm::STORE:
                case tracing::pm::NON_TEMPORAL_STORE:
                case tracing::pm::LOAD:
                    cfg->ComputeDominators();
                    DefinePIFR(ctx, current_bb, itr, true);
                    break;
                case tracing::pm::CLFLUSH:
                case tracing::pm::CLFLUSHOPT:
                case tracing::pm::CLWB:
                case tracing::pm::RELEASE:
                case tracing::pm::FENCE:
                case tracing::pm::RMW:
                case tracing::pm::SYNC:
                case tracing::pm::ACQUIRE:
                case tracing::pm::BRANCH:
                case tracing::pm::BRANCH_COND:
                case tracing::pm::BRANCH_TARGET:
                case tracing::pm::ERROR:
                case tracing::pm::OTHER:
                default:
                    break;
            }
        }
    }
}

void FindMinimalWindows(Context& ctx, Function* f) {
    if (!f->is_internal()) return;

    if (!annotate_relevant_insts(f, ctx)) return;

    CFG* cfg = f->get_cfg();

    for (BasicBlock* current_bb : cfg->get_blocks()) {
        for (auto itr = current_bb->get_instructions().begin();
             itr != current_bb->get_instructions().end(); itr++) {
            Instruction* ins = *itr;
            x86::MemAccess* ma;
            switch (ins->get_type()) {
                case tracing::pm::STORE:
                case tracing::pm::NON_TEMPORAL_STORE:
                    ma = ins->x86->writes_to_memory();
                    if (ma) {
                        ctx.out << "write,";
                        ctx.out << "0x" << std::hex << ins->get_addr() << ",";
                        csv_out_mem_access(ctx, ins, ma);
                    }
                    break;
                case tracing::pm::LOAD:
                    ma = ins->x86->reads_from_memory();
                    if (ma) {
                        ctx.out << "read,";
                        ctx.out << "0x" << std::hex << ins->get_addr() << ",";
                        csv_out_mem_access(ctx, ins, ma);
                    }
                    break;
                case tracing::pm::CLFLUSH:
                case tracing::pm::CLFLUSHOPT:
                case tracing::pm::CLWB:
                    ma = ins->x86->reads_from_memory();
                    if (ma) {
                        ctx.out << "flush,";
                        csv_out_mem_access(ctx, ins,
                                           ins->x86->reads_from_memory());
                    }
                    break;
                case tracing::pm::RELEASE:
                case tracing::pm::FENCE:
                case tracing::pm::RMW:
                case tracing::pm::SYNC:
                case tracing::pm::ACQUIRE:
                case tracing::pm::BRANCH:
                case tracing::pm::BRANCH_COND:
                case tracing::pm::BRANCH_TARGET:
                case tracing::pm::ERROR:
                case tracing::pm::OTHER:
                default:
                    break;
            }
        }
    }
}

void FindDynamicCalls(Module* m) {
    for (auto pair : m->get_functions()) {
        Function* f = pair.second;
        if (!f->is_internal()) continue;
        for (BasicBlock* current_bb : f->get_cfg()->get_blocks()) {
            for (auto itr = current_bb->get_instructions().begin();
                 itr != current_bb->get_instructions().end(); itr++) {
                auto x86 = (*itr)->x86;
                if (!x86->is_call()) continue;
                // if
            }
        }
    }
}
}  // namespace palantir
