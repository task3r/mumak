#include "../fp_tree.h"

int main(void) {
    fp_tree::FPTraceAddr* root;
    fp_tree::DeSerialize(&root, "out.bin");
    fp_tree::Print(root);
    fp_tree::Clear();
}
