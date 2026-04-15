// Copyright 2023 João Gonçalves

#ifndef PM_ADDR_CHECKER
#define PM_ADDR_CHECKER

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

#include <sys/types.h>
typedef long intptr_t;

EXPORT int is_pmem_addr(intptr_t addr, size_t size);
EXPORT void pmem_stats();

#endif
