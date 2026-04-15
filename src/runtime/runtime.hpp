// Copyright 2025 João Gonçalves
#ifndef LIBPIFRRT
#define LIBPIFRRT

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

#include <signal.h>

typedef long intptr_t;

EXPORT void pifrrt_start_pifr(intptr_t pifr_id, bool is_write,
                              intptr_t mem_addr, size_t size, intptr_t rbp,
                              intptr_t rip);
EXPORT void pifrrt_end_pifrs(intptr_t current_iaddr);
EXPORT void pifrrt_flush(intptr_t current_iaddr, intptr_t mem_addr,
                         size_t size);
EXPORT void pifrrt_set_offset(const void* addr, const void* base_addr);

#endif
