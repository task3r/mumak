#ifndef MUMAK_ALLOCATOR
#define MUMAK_ALLOCATOR

namespace allocator {

void init(unsigned long size);
void* allocate(unsigned long size);
void* reallocate(void* ptr, unsigned long size);
void* get(void* ptr);
void clear();

}  // namespace allocator

#endif
