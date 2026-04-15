// Copyright 2025 João Gonçalves
#ifndef LIBPIFRRT_TRACING
#define LIBPIFRRT_TRACING

#include <pthread.h>
#include <sys/types.h>

static FILE* trace_f;
static pthread_mutex_t trace_mutex;

enum TraceType { START, END, FLUSH };

void trace_start(pid_t tid, intptr_t pifr_id, bool is_write, intptr_t mem_addr, size_t size) {
    TraceType type = START;
    pthread_mutex_lock(&trace_mutex);
    fwrite(&tid, sizeof(pid_t), 1, trace_f);
    fwrite(&type, sizeof(TraceType), 1, trace_f);
    fwrite(&pifr_id, sizeof(intptr_t), 1, trace_f);
    fwrite(&is_write, sizeof(bool), 1, trace_f);
    fwrite(&mem_addr, sizeof(intptr_t), 1, trace_f);
    fwrite(&size, sizeof(size_t), 1, trace_f);
    pthread_mutex_unlock(&trace_mutex);
}

void trace_end(pid_t tid) {
    TraceType type = END;
    pthread_mutex_lock(&trace_mutex);
    fwrite(&tid, sizeof(pid_t), 1, trace_f);
    fwrite(&type, sizeof(TraceType), 1, trace_f);
    pthread_mutex_unlock(&trace_mutex);
}

void trace_flush(pid_t tid, intptr_t mem_addr, size_t size) {
    TraceType type = FLUSH;
    pthread_mutex_lock(&trace_mutex);
    fwrite(&tid, sizeof(pid_t), 1, trace_f);
    fwrite(&type, sizeof(TraceType), 1, trace_f);
    fwrite(&mem_addr, sizeof(intptr_t), 1, trace_f);
    fwrite(&size, sizeof(size_t), 1, trace_f);
    pthread_mutex_unlock(&trace_mutex);
}

#endif
