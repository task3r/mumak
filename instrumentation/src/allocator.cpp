#include "allocator.h"

#include "fp_tree.h"

using byte = unsigned char;

#define CUSTOM_ALLOCATOR

#ifdef CUSTOM_ALLOCATOR

#include <stdio.h>

#include <iostream>

byte* _root = NULL;
int fd;
unsigned long idx = 1;  // just so that root is not zero (to check if it exists)
unsigned long current_size = 0;

void allocator::init(unsigned long size) {
    if (!_root) {
        _root = (byte*)malloc(sizeof(byte) * size);
        if (_root == NULL) {
            std::cerr << "Failed to allocate " << size << " bytes. Exiting..."
                      << std::endl;
            exit(1);
        }
        current_size = size;
    }
}
void check_if_should_truncate(unsigned long size) {
    if (idx + size >= current_size) {
        std::cerr << "Initial allocation of " << current_size
                  << " bytes was not enough to analyze "
                     "application. Please increase it using the -alloc flag. "
                     "Exiting..."
                  << std::endl;
        exit(1);
    }
}

void* allocator::allocate(unsigned long size) {
    check_if_should_truncate(size);
    unsigned long offset = idx;
    idx += size;
    return (void*)offset;
}

void* allocator::reallocate(void* ptr, unsigned long size) {
    check_if_should_truncate(size);
    unsigned long offset = (unsigned long)ptr;
    byte* casted_ptr = (byte*)(_root + offset);
    unsigned long new_offset = idx;
    byte* new_ptr = _root + idx;

    for (unsigned long i = 0; i < size / REALLOC_RATIO; i++) {
        new_ptr[i] = casted_ptr[i];
    }

    idx += size;
    return (void*)new_offset;
}

void* allocator::get(void* ptr) {
    unsigned long offset = (unsigned long)ptr;
    return (void*)(_root + offset);
}

void allocator::clear() { free(_root); }

#else
#include <cstdlib>
#include <set>

#define INIT_NUM_ALLOCS 100

static std::set<void*> allocs;

void allocator::init(unsigned long size) {}

void* allocator::allocate(unsigned long size) {
    void* ptr = malloc(size);
    allocs.insert(ptr);
    return ptr;
}

void* allocator::reallocate(void* ptr, unsigned long size) {
    allocs.erase(ptr);
    void* new_ptr = realloc(ptr, size);
    allocs.insert(new_ptr);
    return new_ptr;
}

void* allocator::get(void* ptr) { return ptr; }

void allocator::clear() {
    for (auto it = allocs.begin(); it != allocs.end(); it++) free(*it);
}

#endif
