#ifndef MUMAK_FP_TREE
#define MUMAK_FP_TREE

#define INIT_CHILDREN 1
#define REALLOC_RATIO 2
#define MAX_TRACE_LENGTH 100
#define DEFAULT_ALLOC_SIZE 32768

namespace fp_tree {
struct FPTraceAddr {
    void* addr;
    int children_count;
    int max_children;
    bool visited;
    FPTraceAddr** children;
    FPTraceAddr* parent;
};

void Init(unsigned long size = DEFAULT_ALLOC_SIZE);
void Clear();
void PrintBacktraceSymbols(void** addrs, int size);
void PrintBacktraceSymbols(void** addrs, int size, const char* filename);
void InsertFPTrace(FPTraceAddr** root, void** addrs, int count);
bool InjectFP(FPTraceAddr** root, void** addrs, int count);
void Print(FPTraceAddr* root);
void PrintTraces(FPTraceAddr* root);
void Serialize(FPTraceAddr* root, const char* filename);
void DeSerialize(FPTraceAddr** root, const char* filename);
int GetUniqFPs();
}  // namespace fp_tree

#endif
