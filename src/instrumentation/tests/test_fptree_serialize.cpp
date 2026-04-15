#include "../fp_tree.h"

int main(void) {
    fp_tree::FPTraceAddr* root;
    void* t1[] = {(void*)4, (void*)3, (void*)2, (void*)1, 0};
    void* t2[] = {(void*)7, (void*)6, (void*)5, (void*)1, 0};
    void* t3[] = {(void*)10, (void*)9, (void*)8, (void*)1, 0};
    void* t4[] = {(void*)7, (void*)6, (void*)5, (void*)1, 0};
    void* t5[] = {(void*)7, (void*)6, (void*)2, (void*)1, 0};
    void* t6[] = {(void*)7, (void*)6, (void*)3, (void*)1, 0};
    void* t7[] = {(void*)7, (void*)6, (void*)4, (void*)1, 0};
    fp_tree::InsertFPTrace(&root, t1, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t2, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t3, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t4, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t5, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t6, 5);
    fp_tree::Print(root);
    fp_tree::InsertFPTrace(&root, t7, 5);
    fp_tree::Print(root);
    fp_tree::Serialize(root, "out.bin");
    fp_tree::Clear();
}
