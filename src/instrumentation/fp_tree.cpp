#include "fp_tree.h"

#include <execinfo.h>

#include <fstream>
#include <iostream>
#include <ostream>

#include "allocator.h"

#define GET(ptr) ((FPTraceAddr*)allocator::get(ptr))
#define GET_PTR(ptr) ((FPTraceAddr**)allocator::get(ptr))

using fp_tree::FPTraceAddr;

int uniq_fps = 0;

void fp_tree::Init(unsigned long size) { allocator::init(size); }

void fp_tree::Clear() { allocator::clear(); }

void PrintBacktraceSymbols(void** addrs, int size, std::ostream& out) {
    char** symbols;

    symbols = backtrace_symbols(addrs, size);

    for (int i = 0; i < size; i++) out << symbols[i] << std::endl;
    free(symbols);
}

void fp_tree::PrintBacktraceSymbols(void** addrs, int size) {
    ::PrintBacktraceSymbols(addrs, size, std::cout);
}

void fp_tree::PrintBacktraceSymbols(void** addrs, int size,
                                    const char* filename) {
    std::ofstream out;
    out.open(filename);
    ::PrintBacktraceSymbols(addrs, size, out);
    out.close();
}

void PrintTraceAdresses(std::ofstream& out, void** addrs, int size) {
    for (int i = 0; i < size; i++) {
        out << addrs[i] << std::endl;
    }
    out << std::endl;
}

FPTraceAddr* NewFPTraceAddr(void* addr, int current_child_count,
                            int max_child_count, bool visited,
                            FPTraceAddr* parent) {
    FPTraceAddr* ptr = (FPTraceAddr*)allocator::allocate(sizeof(FPTraceAddr));
    FPTraceAddr* node = GET(ptr);
    node->children_count = current_child_count;
    node->addr = addr;
    node->max_children = max_child_count;
    node->visited = visited;
    node->children =
        (FPTraceAddr**)allocator::allocate(sizeof(void*) * max_child_count);
    node->parent = parent;
    return ptr;
}

void fp_tree::InsertFPTrace(FPTraceAddr** root, void** addrs, int count) {
    if (!*root) {
        allocator::init(DEFAULT_ALLOC_SIZE);
        *root = NewFPTraceAddr(addrs[count - 1], 0, INIT_CHILDREN, false, NULL);
    }

    FPTraceAddr* current = GET(*root);
    bool uniq = false;
    // ignore last as it should be the root
    for (int i = count - 2; i >= 0; i--) {
        bool found = false;
        for (int j = 0; j < current->children_count; j++) {
            if (GET(GET_PTR(current->children)[j])->addr == addrs[i]) {
                current = GET(GET_PTR(current->children)[j]);
                found = true;
            }
        }
        if (!found) {
            uniq = true;
            if (current->children_count >= current->max_children) {
                current->max_children *= REALLOC_RATIO;
                current->children = (FPTraceAddr**)allocator::reallocate(
                    current->children, current->max_children * sizeof(void*));
            }
            FPTraceAddr* new_node =
                NewFPTraceAddr(addrs[i], 0, INIT_CHILDREN, false, current);
            GET_PTR(current->children)[current->children_count] = new_node;
            current->children_count++;
            current = GET(new_node);
        }
    }
    if (uniq) {
        uniq_fps++;
    }
}

bool fp_tree::InjectFP(FPTraceAddr** root, void** addrs, int count) {
    if (!*root) {
        return false;  // TODO: exit? there should be fps if we reach this point
    }
    FPTraceAddr* current = GET(*root);
    for (int i = count - 2; i >= 0; i--) {
        bool found = false;
        for (int j = 0; j < current->children_count; j++) {
            if (GET(GET_PTR(current->children)[j])->addr == addrs[i]) {
                current = GET(GET_PTR(current->children)[j]);
                if (current->visited) return false;
                found = true;
                break;
            }
        }
        if (!found) {
            // should not reach here, supposedly it should have found all
            // FPs in the first phase
            std::cout << "Unexpected new FP" << std::endl;
            fp_tree::PrintBacktraceSymbols(addrs, count);
            return false;
        }
    }
    // back propagate visited
    current->visited = true;
    while (current->parent) {
        current = GET(current->parent);
        for (int i = 0; i < current->children_count; i++) {
            if (!GET(GET_PTR(current->children)[i])->visited)
                return true;  // easy way of stopping both loops
        }
        current->visited = true;
    }
    return true;
}

void Print(FPTraceAddr* ptr, int level) {
    FPTraceAddr* current = GET(ptr);
    for (int i = 0; i < level; i++) {
        std::cout << "\t";
    }
    const char* check = current->visited ? "v" : "x";
    std::cout << current->addr << " " << check << std::endl;
    for (int j = 0; j < current->children_count; j++) {
        Print(GET_PTR(current->children)[j], level + 1);
    }
}

void fp_tree::Print(FPTraceAddr* current) {
    if (current) ::Print(current, 0);
}

int fp_tree::GetUniqFPs() { return uniq_fps; }

void PrintTraces(std::ofstream& out, FPTraceAddr* ptr, void** addrs,
                 int level) {
    FPTraceAddr* current = GET(ptr);
    addrs[level] = current->addr;
    level++;
    if (current->children_count) {
        for (int i = 0; i < current->children_count; i++) {
            PrintTraces(out, GET_PTR(current->children)[i], addrs, level);
        }
    } else {
        void** addrs_aux = (void**)malloc(sizeof(void*) * level);
        // reverse addrs so that it matches normal stack-like backtrace
        for (int i = 0, f = level - 1; i < level; i++, f--) {
            addrs_aux[f] = addrs[i];
        }
        PrintTraceAdresses(out, addrs_aux, level);
        // fp_tree::PrintBacktraceSymbols(addrs_aux, level);
        free(addrs_aux);
    }
}

void fp_tree::PrintTraces(FPTraceAddr* root) {
    if (!root) return;  // just in case it didn't find any fps

    std::ofstream out;
    out.open("addrs.txt");
    void** addrs = (void**)malloc(sizeof(void*) * MAX_TRACE_LENGTH);
    ::PrintTraces(out, root, addrs, 0);
    free(addrs);
    out.close();
}

void Serialize(FPTraceAddr* ptr, std::ofstream& out) {
    FPTraceAddr* current = GET(ptr);
    out << (long)current->addr << std::endl;
    out << current->children_count << std::endl;
    out << current->visited << std::endl;
    for (int i = 0; i < current->children_count; i++) {
        Serialize(GET_PTR(current->children)[i], out);
    }
}

void fp_tree::Serialize(FPTraceAddr* root, const char* filename) {
    if (!root) return;
    std::ofstream out;
    out.open(filename);
    ::Serialize(root, out);
    out.close();
}

void DeSerialize(FPTraceAddr* parent, FPTraceAddr** current,
                 std::ifstream& in) {
    long addr;
    int child_count;
    bool visited;
    in >> addr;
    in >> child_count;
    in >> visited;
    *current =
        NewFPTraceAddr((void*)addr, child_count, child_count, visited, parent);
    for (int i = 0; i < child_count; i++)
        DeSerialize(*current, &(GET_PTR(GET(*current)->children)[i]), in);
}

void fp_tree::DeSerialize(FPTraceAddr** root, const char* filename) {
    std::ifstream in;
    in.open(filename);
    allocator::init(DEFAULT_ALLOC_SIZE);
    ::DeSerialize(NULL, root, in);
    in.close();
}
