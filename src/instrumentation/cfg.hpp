#ifndef MUMAK_CFG
#define MUMAK_CFG

#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../utils/tracing.hpp"
#include "pin.H"
#include "pin_utils.hpp"

using tracing::pm::Bug;
using tracing::pm::BugType;
using tracing::pm::Instruction;
using tracing::pm::proc;

std::ofstream TraceFile;

std::vector<ADDRINT> img_addr;

// branches src -> [dest]
std::map<ADDRINT, std::set<ADDRINT>> branches;
std::set<ADDRINT> branch_destinations;

std::map<ADDRINT, std::set<ADDRINT>> pm_stores;
std::map<ADDRINT, std::set<ADDRINT>> pm_flushes;
std::map<ADDRINT, std::set<ADDRINT>> pm_loads;
std::map<ADDRINT, std::set<ADDRINT>> persist_deps;
std::map<ADDRINT, std::string> sync_calls;

namespace cfg {

enum EdgeType { FALLTHROUGH, CONDITIONAL_JUMP, JUMP, PERSISTS };

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

const char* color(Instruction t) {
    switch (t) {
        case Instruction::STORE:
        case Instruction::NON_TEMPORAL_STORE:
        case Instruction::RMW:
            return "red";
        case Instruction::LOAD:
            return "blue";
        case Instruction::SYNC:
            return "darkgreen";
        case Instruction::FENCE:
        case Instruction::CLFLUSH:
        case Instruction::CLWB:
        case Instruction::CLFLUSHOPT:
            return "orange";
        case Instruction::BRANCH:
            return "purple";
        case Instruction::BRANCH_TARGET:
        case Instruction::ERROR:
        case Instruction::OTHER:
        default:
            return "black";
    }
}

struct INSWrapper {
    INS instruction;
    Instruction type;
    ADDRINT addr;
    ADDRINT offset;
    std::set<ADDRINT> accesses;

    INSWrapper(INS instruction, Instruction type, ADDRINT addr, ADDRINT offset)
        : instruction(instruction), type(type), addr(addr), offset(offset) {}

    INSWrapper(INS instruction, Instruction type, ADDRINT addr, ADDRINT offset,
               std::set<ADDRINT> accesses)
        : instruction(instruction),
          type(type),
          addr(addr),
          offset(offset),
          accesses(accesses) {}

    void dot(std::ostream& os) {
        os << "<font color=\"" << color(type) << "\">";
        os << hexstr(addr) << ": " << INS_Disassemble(instruction);
        if (type == Instruction::SYNC)
            os << " ("
               << sync_calls
                      .find(INS_DirectControlFlowTargetAddress(instruction) -
                            offset)
                      ->second
               << ")";
        // else if (type == Instruction::STORE ||
        //          type == Instruction::LOAD) {
        //     os << "\n[";
        //     for (auto acc : accesses) os << acc << ",";
        //     os << "]";
        // }
        os << "</font>";
    }
    void set_accesses(std::set<ADDRINT> acc) { accesses = acc; }
};

struct Node {
    ADDRINT addr;
    INSWrapper* instruction;
    Node(ADDRINT addr, INSWrapper* instruction)
        : addr(addr), instruction(instruction) {}
    void dot(std::ostream& os) {
        os << addr << " [ label =< " << std::endl;
        instruction->dot(os);
        os << "> ]" << std::endl;
    }
};

struct Edge {
    ADDRINT src;
    ADDRINT target;
    EdgeType type;
    Edge(ADDRINT src, ADDRINT target, EdgeType type)
        : src(src), target(target), type(type) {}

    void dot(std::ostream& os) {
        os << src << " -> " << target << "[arrowsize=0.5 color=\""
           << color(type) << "\"]" << std::endl;
    }
};

struct CFG {
    RTN rtn;
    BBL* start;
    std::map<ADDRINT, Node*> nodes;
    std::map<ADDRINT, std::map<ADDRINT, Edge*>> forward_edges;
    std::map<ADDRINT, std::map<ADDRINT, Edge*>> backward_edges;
    CFG(RTN rtn) : rtn(rtn) {}
    void add_node(Node* node) { nodes[node->addr] = node; }
    void add_edge(Edge* edge) {
        forward_edges[edge->src][edge->target] = edge;
        backward_edges[edge->target][edge->src] = edge;
    }
    bool has_node(ADDRINT addr) { return nodes.find(addr) != nodes.end(); }
    Node* get_node(ADDRINT addr) { return nodes.find(addr)->second; }
    void dot(std::ostream& os) {
        os << "digraph D {" << std::endl;
        os << "label=\"" << RTN_Name(rtn) << "\"" << std::endl;
        os << "node [shape=rectangle fontname=\"consolas\" fontsize=\"8\" "
              "nodesep=0.1]"
           << std::endl;
        for (auto pair : nodes) pair.second->dot(os);
        for (auto src : forward_edges)
            for (auto edge : src.second) edge.second->dot(os);
        os << "}" << std::endl;
    }
    void reduce() {
        std::set<ADDRINT> to_reduce;
        for (auto node : nodes) {
            if (node.second->instruction->type == Instruction::OTHER)
                to_reduce.insert(node.first);
        }
        for (auto node : to_reduce) {
            nodes.erase(node);
            for (auto back : backward_edges[node]) {
                for (auto forward : forward_edges[node]) {
                    if (back.first != forward.first)
                        add_edge(new cfg::Edge(back.first, forward.first,
                                               back.second->type));
                    backward_edges[forward.first].erase(node);
                }
                forward_edges[back.first].erase(node);
            }
            forward_edges.erase(node);
        }
    }

    size_t edge_instructions(INSWrapper* src, INSWrapper*** dest,
                             std::map<ADDRINT, Edge*> edges) {
        *dest = (cfg::INSWrapper**)malloc(sizeof(void*) * edges.size());
        size_t i = 0;
        for (auto e : edges) {
            *dest[i++] = nodes[e.second->src]->instruction;
        }
        return i;
    }

    size_t prev_instructions(INSWrapper* src, INSWrapper*** dest) {
        return edge_instructions(src, dest, backward_edges[src->addr]);
    }

    size_t next_instructions(INSWrapper* src, INSWrapper*** dest) {
        return edge_instructions(src, dest, forward_edges[src->addr]);
    }
};
}  // namespace cfg

KNOB<std::string> KnobPMMount(KNOB_MODE_WRITEONCE, "pintool", "pm-mount",
                              "/mnt/pmem0/", "PM mount in filesystem");
KNOB<std::string> KnobTraceFile(KNOB_MODE_WRITEONCE, "pintool", "trace",
                                "/mnt/ramdisk/trace.txt", "trace file");

void IndirectBranchHandler(ADDRINT src, ADDRINT dst, INT32 taken) {
    PIN_LockClient();
    TraceFile << Instruction::BRANCH << " " << src << " " << dst << " " << taken
              << std::endl;
    PIN_UnlockClient();
}

void StoreHandler(ADDRINT address, uint32_t size, ADDRINT ip) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        // FIXME: differenciate store types?
        TraceFile << Instruction::STORE << " " << ip << " " << address << " "
                  << size << std::endl;
    }
    PIN_UnlockClient();
}

void LoadHandler(ADDRINT address, uint32_t size, ADDRINT ip) {
    PIN_LockClient();
    if (IsPMAddress(address, size)) {
        TraceFile << Instruction::LOAD << " " << ip << " " << address << " "
                  << size << std::endl;
    }
    PIN_UnlockClient();
}

void FlushHandler(ADDRINT address, ADDRINT ip, Instruction flush_type) {
    PIN_LockClient();
    if (IsPMAddress(address, 0)) {
        // FIXME: differenciate flush types?
        TraceFile << flush_type << " " << ip << " " << address << std::endl;
    }
    PIN_UnlockClient();
}

void FenceHandler(ADDRINT ip) {
    PIN_LockClient();
    TraceFile << Instruction::FENCE << " " << ip << std::endl;
    PIN_UnlockClient();
}

void InstrumentCollectBranches(IMG img, VOID* v) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins);
                 ins = INS_Next(ins)) {
                ADDRINT base_addr = INS_Address(ins) - IMG_LowAddress(img);
                if (INS_IsIndirectControlFlow(ins) && !INS_IsXbegin(ins) &&
                    !INS_IsXend(ins) && !INS_IsCall(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE,
                                   (AFUNPTR)IndirectBranchHandler,
                                   IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR,
                                   IARG_BRANCH_TAKEN, IARG_END);
                } else if (INS_IsCacheLineFlush(ins)) {
                    // flushes are PIN memory reads, need to come before in
                    // these conditions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FlushHandler,
                                   IARG_MEMORYOP_EA, 0, IARG_ADDRINT, base_addr,
                                   IARG_UINT64, GetFlushType(ins), IARG_END);
                } else if (INS_IsMemoryWrite(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StoreHandler,
                                   IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_END);
                } else if (INS_IsMemoryRead(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LoadHandler,
                                   IARG_MEMORYOP_EA, 0, IARG_MEMORYREAD_SIZE,
                                   IARG_ADDRINT, base_addr, IARG_END);
                } else if (IsFence(ins)) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FenceHandler,
                                   IARG_ADDRINT, base_addr, IARG_END);
                }
            }
            RTN_Close(rtn);
        }
    }
}

void CollectCFGFini(INT32 code, VOID* v) { TraceFile.close(); }

std::string REG_StringShortWrapper(REG reg) {
    if (reg == 0) return "0";
    return REG_StringShort(reg);
}

void HandleInst(INS ins) {
    for (size_t idx = 0; idx < INS_OperandCount(ins); idx++) {
        // size_t idx = INS_MemoryOperandIndexToOperandIndex(ins, midx);
        std::cout << " [" << idx << "]";
        if (INS_OperandReadOnly(ins, idx)) {
            std::cout << " R";
        } else if (INS_OperandWrittenOnly(ins, idx)) {
            std::cout << " W";
        } else if (INS_OperandReadAndWritten(ins, idx)) {
            std::cout << " RW";
        } else {
            std::cout << "BUG";
        }
        if (INS_OperandIsReg(ins, idx)) {
            REG reg = INS_OperandReg(ins, idx);
            std::cout << " - " << REG_StringShortWrapper(reg) << std::endl;
        } else if (INS_OperandIsImmediate(ins, idx)) {
            UINT64 value = INS_OperandImmediate(ins, idx);
            std::cout << " - " << hexstr(value) << std::endl;
        } else if (INS_OperandIsMemory(ins, idx)) {
            std::cout << " - "
                      << "memory" << std::endl;
            REG seg = INS_OperandMemorySegmentReg(ins, idx);
            REG base = INS_OperandMemoryBaseReg(ins, idx);
            ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, idx);
            REG index = INS_OperandMemoryIndexReg(ins, idx);
            INT32 scale = INS_OperandMemoryScale(ins, idx);
            std::cout << "\t";
            if (seg != 0) std::cout << REG_StringShort(seg) << ": ";
            std::cout << hexstr(displacement) << " (" << displacement << ")"
                      << " + " << REG_StringShortWrapper(base) << " + "
                      << REG_StringShortWrapper(index) << " * " << scale
                      << std::endl;
        } else if (INS_OperandIsBranchDisplacement(ins, idx)) {
            std::cout << " - "
                      << "branch displacement" << std::endl;
        } else if (INS_OperandIsImplicit(ins, idx)) {
            REG reg = INS_OperandReg(ins, idx);
            std::cout << " - " << reg << " " << REG_StringShortWrapper(reg)
                      << " (implicit)" << std::endl;
        } else if (INS_OperandIsFixedMemop(ins, idx)) {
            std::cout << " - "
                      << "fixed mem op (?!?!?)" << std::endl;
        }
        // never happen
        if (INS_OperandIsSegmentReg(ins, idx)) {
            std::cout << " - "
                      << "segment reg" << std::endl;
        }
        if (INS_OperandIsAddressGenerator(ins, idx)) {
            std::cout << " - "
                      << "address gen" << std::endl;
        }
    }
}

void HandleFunc(IMG img, RTN rtn) {
    RTN_Open(rtn);
    std::cout << "[F] " << hexstr(RTN_Address(rtn) - IMG_LowAddress(img)) << " "
              << RTN_Name(rtn) << std::endl;
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        ADDRINT base_addr = INS_Address(ins) - IMG_LowAddress(img);
        if (INS_IsMemoryWrite(ins)) {
            std::cout << "[W] " << hexstr(base_addr) << " "
                      << INS_Disassemble(ins) << std::endl;
            HandleInst(ins);

        } else if (INS_IsMemoryRead(ins) && !INS_IsRet(ins)) {
            std::cout << "[R] " << hexstr(base_addr) << " "
                      << INS_Disassemble(ins) << std::endl;
            HandleInst(ins);
        }
    }
    std::cout << std::endl;
    RTN_Close(rtn);
}

void GetOpAndArgs(IMG img, VOID* v) {
    if (IMG_Id(img) != 1) return;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            HandleFunc(img, rtn);
        }
    }
}

void CollectCFG() {
    // IMG_AddInstrumentFunction(GetOpAndArgs, 0);
    pm_mount = KnobPMMount.Value().c_str();
    TraceFile.open(KnobTraceFile.Value().c_str());
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    IMG_AddInstrumentFunction(InstrumentCollectBranches, 0);
    PIN_AddFiniFunction(CollectCFGFini, 0);
}

cfg::INSWrapper* GetINSWrapper(INS ins, ADDRINT addr, ADDRINT offset) {
    if (INS_IsMemoryWrite(ins) && pm_stores.find(addr) != pm_stores.end()) {
        return new cfg::INSWrapper(ins, Instruction::STORE, addr, offset,
                                   pm_stores[addr]);
    } else if (INS_IsMemoryRead(ins) && pm_loads.find(addr) != pm_loads.end()) {
        return new cfg::INSWrapper(ins, Instruction::LOAD, addr, offset,
                                   pm_loads[addr]);
    } else if (INS_IsCacheLineFlush(ins) &&
               pm_flushes.find(addr) != pm_flushes.end()) {
        return new cfg::INSWrapper(ins, GetFlushType(ins), addr, offset,
                                   pm_flushes[addr]);
    } else if (IsFence(INS_Opcode(ins))) {
        return new cfg::INSWrapper(ins, Instruction::FENCE, addr, offset);
    } else if (INS_IsCall(ins) && INS_IsDirectControlFlow(ins)) {
        if (sync_calls.find(INS_DirectControlFlowTargetAddress(ins) - offset) !=
            sync_calls.end())
            return new cfg::INSWrapper(ins, Instruction::SYNC, addr, offset);
        else
            return new cfg::INSWrapper(ins, Instruction::OTHER, addr, offset);
    } else if (INS_IsControlFlow(ins)) {
        return new cfg::INSWrapper(ins, Instruction::BRANCH, addr, offset);
    } else {
        return new cfg::INSWrapper(ins, Instruction::OTHER, addr, offset);
    }
}

cfg::CFG* GetCFGforRTN(RTN rtn, IMG img) {
    RTN_Open(rtn);
    ADDRINT offset = IMG_LowAddress(img);
    cfg::CFG* cfg = new cfg::CFG(rtn);
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        ADDRINT addr = INS_Address(ins) - offset;
        cfg::Node* node = new cfg::Node(addr, GetINSWrapper(ins, addr, offset));
        if (node->instruction->type == tracing::pm::LOAD ||
            node->instruction->type == tracing::pm::STORE) {
            if (node->instruction->type == tracing::pm::LOAD)
                std::cout << "[R] ";
            else if (node->instruction->type == tracing::pm::STORE)
                std::cout << "[W] ";
            std::cout << hexstr(addr) << " " << INS_Disassemble(ins)
                      << std::endl;
            HandleInst(ins);
        }
        cfg->add_node(node);
        if (INS_Valid(INS_Next(ins)) &&
            (INS_HasFallThrough(ins) || INS_IsCall(ins))) {
            cfg->add_edge(
                new cfg::Edge(addr, INS_NextAddress(ins) - IMG_LowAddress(img),
                              cfg::EdgeType::FALLTHROUGH));
        }
        if (INS_IsDirectControlFlow(ins) && !INS_IsCall(ins)) {
            ADDRINT target =
                INS_DirectControlFlowTargetAddress(ins) - IMG_LowAddress(img);
            cfg->add_edge(new cfg::Edge(addr, target, cfg::EdgeType::JUMP));
        }
    }

    for (auto dep : persist_deps) {
        if (!cfg->has_node(dep.first)) continue;
        for (auto dep_target : dep.second) {
            if (!cfg->has_node(dep_target)) continue;
            cfg->add_edge(
                new cfg::Edge(dep.first, dep_target, cfg::EdgeType::PERSISTS));
        }
    }

    cfg->get_node(INS_Address(RTN_InsHead(rtn)) - offset)->instruction->type =
        Instruction::BRANCH_TARGET;

    for (auto branch : branches) {
        ADDRINT src = branch.first;
        if (!cfg->has_node(src)) continue;
        for (auto target : branch.second) {
            if (!cfg->has_node(target)) continue;
            cfg->add_edge(new cfg::Edge(src, target, cfg::EdgeType::JUMP));
            cfg::INSWrapper* target_ins = cfg->get_node(target)->instruction;
            if (target_ins->type == Instruction::OTHER)
                target_ins->type = Instruction::BRANCH_TARGET;
        }
    }
    RTN_Close(rtn);
    return cfg;
}

void InstrumentConditions(cfg::CFG* cfg) {
    RTN_Open(cfg->rtn);
    std::cout << std::endl << "F: " << RTN_Name(cfg->rtn) << std::endl;
    for (auto node : cfg->nodes) {
        cfg::INSWrapper* wrapper = node.second->instruction;
        INS ins = wrapper->instruction;
        if (INS_IsControlFlow(ins) && !INS_IsCall(ins) && !INS_IsRet(ins) &&
            !IsUnconditionalJump(ins)) {
            std::cout << std::endl
                      << "CFG: " << INS_Disassemble(ins) << std::endl;
            cfg::INSWrapper** prevs;
            size_t n = cfg->prev_instructions(wrapper, &prevs);
            for (size_t i = 0; i < n; i++) {
                cfg::INSWrapper* prev_wrapper = prevs[i];
                INS prev_ins = prev_wrapper->instruction;
                std::cout << "COND: " << INS_Disassemble(prev_ins) << std::endl;
                for (size_t op_idx = 0; op_idx < INS_OperandCount(prev_ins);
                     op_idx++) {
                    if (INS_OperandIsReg(prev_ins, op_idx)) {
                        REG reg = INS_OperandReg(prev_ins, op_idx);
                        if (reg != REG_RFLAGS)
                            std::cout << REG_StringShort(reg) << std::endl;
                    }
                }
            }
            free(prevs);
        }
    }
    RTN_Close(cfg->rtn);
}

void InstrumentBuildCFG(IMG img, VOID* v) {
    if (IMG_Id(img) != 1) return;
    std::ofstream out;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::stringstream ss;
            ss << "/tmp/cfg/" << IMG_Id(img) << "-" << RTN_Name(rtn) << ".dot";
            out.open(ss.str().c_str());
            cfg::CFG* cfg = GetCFGforRTN(rtn, img);
            cfg->dot(out);
            out.close();

            // InstrumentConditions(cfg);

            ss << "reduced.dot";
            out.open(ss.str().c_str());
            cfg->reduce();
            cfg->dot(out);
            out.close();
        }
    }
}

void LoadTrace() {
    std::string str;
    std::ifstream in(KnobTraceFile.Value().c_str());
    std::vector<uint64_t> persists;
    uint64_t target_address = 0;
    uint32_t size = 0;
    ADDRINT instruction_address = 0;
    while (std::getline(in, str)) {
        if (str.size() > 0) {
            std::istringstream iss(str);
            std::vector<std::string> line{
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>{}};
            Instruction type =
                static_cast<Instruction>(strtoul(line[0].c_str(), NULL, 10));
            instruction_address = (ADDRINT)strtoul(line[1].c_str(), NULL, 10);
            switch (type) {
                case Instruction::RMW:
                    proc.ProcessFence(instruction_address, true, persists);
                case Instruction::STORE:
                case Instruction::NON_TEMPORAL_STORE:
                    target_address =
                        (ADDRINT)strtoul(line[2].c_str(), NULL, 10);
                    proc.ProcessStore(target_address, size,
                                      type == Instruction::NON_TEMPORAL_STORE,
                                      instruction_address, persists);
                    pm_stores[instruction_address].insert(target_address);
                    break;
                case Instruction::CLWB:
                case Instruction::CLFLUSH:
                case Instruction::CLFLUSHOPT:
                    target_address =
                        (ADDRINT)strtoul(line[2].c_str(), NULL, 10);
                    proc.ProcessFlush(type, target_address, instruction_address,
                                      persists);
                    pm_flushes[instruction_address].insert(target_address);
                    break;
                case Instruction::LOAD:
                    pm_loads[instruction_address].insert(
                        (ADDRINT)strtoul(line[2].c_str(), NULL, 10));
                    break;
                case Instruction::BRANCH:
                    target_address =
                        (ADDRINT)strtoul(line[2].c_str(), NULL, 10);
                    branches[instruction_address].insert(target_address);
                    branch_destinations.insert(target_address);
                    break;
                case Instruction::FENCE:
                    proc.ProcessFence(instruction_address, true, persists);
                    break;
                default:
                    break;
                    // std::cerr << "Found an unknown operation: " << type
                    // << endl;
            }
            for (auto addr : persists)
                persist_deps[instruction_address].insert(addr);
            persists.clear();
        }
    }
    in.close();

    std::ifstream plt_stubs_in("/tmp/stubs.txt");
    while (std::getline(plt_stubs_in, str)) {
        if (str.size() > 0) {
            std::istringstream iss(str);
            std::vector<std::string> line{
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>{}};
            sync_calls[(ADDRINT)strtoul(line[0].c_str(), NULL, 10)] = line[1];
        }
    }
    // for (auto pair : sync_calls) {
    //     std::cerr << hexstr(pair.first) << ": " << pair.second <<
    //     std::endl;
    // }

    plt_stubs_in.close();
}

void BuildCFG() {
    LoadTrace();
    IMG_AddInstrumentFunction(InstrumentBuildCFG, 0);
}

#endif
