#define LIBDL

#include "stdlib.c"
#include "../analysis/x86_register_state.h"

#define DLSYM_PATH "/tmp/pifr_dlsyms"
#define ENVIRON_PATH "/tmp/environ"

static bool has_dlsym = false;
static void *handle = NULL;
static void *pifrrt_start_pifr = NULL;
static void *pifrrt_end_pifrs = NULL;
static void *pifrrt_flush = NULL;
static void *pifrrt_set_offset = NULL;

bool retrieve_dlsyms() {
    if (has_dlsym) return true;
    FILE *f = fopen(DLSYM_PATH, "r");
    fread(&pifrrt_start_pifr, sizeof(void *), 1, f);
    fread(&pifrrt_end_pifrs, sizeof(void *), 1, f);
    fread(&pifrrt_flush, sizeof(void *), 1, f);
    fread(&dlerrno_impl, sizeof(void *), 1, f);
    fread(&pifrrt_set_offset, sizeof(void *), 1, f);
    fclose(f);
    has_dlsym = pifrrt_start_pifr && pifrrt_end_pifrs && pifrrt_flush &&
                pifrrt_set_offset;
    return has_dlsym;
}

uint64_t apply_mask(uint64_t reg_mask, const struct STATE *state) {
    uint64_t value = 0;
    if MATCHES_R15_FAMILY (reg_mask) {
        if (reg_mask & R15)
            value += state->r15;
        else if (reg_mask & R15D)
            value += state->r15d;
        else if (reg_mask & R15W)
            value += state->r15w;
        else if (reg_mask & R15B)
            value += state->r15b;
    }
    if MATCHES_R14_FAMILY (reg_mask) {
        if (reg_mask & R14)
            value += state->r14;
        else if (reg_mask & R14D)
            value += state->r14d;
        else if (reg_mask & R14W)
            value += state->r14w;
        else if (reg_mask & R14B)
            value += state->r14b;
    }
    if MATCHES_R13_FAMILY (reg_mask) {
        if (reg_mask & R13)
            value += state->r13;
        else if (reg_mask & R13D)
            value += state->r13d;
        else if (reg_mask & R13W)
            value += state->r13w;
        else if (reg_mask & R13B)
            value += state->r13b;
    }
    if MATCHES_R12_FAMILY (reg_mask) {
        if (reg_mask & R12)
            value += state->r12;
        else if (reg_mask & R12D)
            value += state->r12d;
        else if (reg_mask & R12W)
            value += state->r12w;
        else if (reg_mask & R12B)
            value += state->r12b;
    }
    if MATCHES_R11_FAMILY (reg_mask) {
        if (reg_mask & R11)
            value += state->r11;
        else if (reg_mask & R11D)
            value += state->r11d;
        else if (reg_mask & R11W)
            value += state->r11w;
        else if (reg_mask & R11B)
            value += state->r11b;
    }
    if MATCHES_R10_FAMILY (reg_mask) {
        if (reg_mask & R10)
            value += state->r10;
        else if (reg_mask & R10D)
            value += state->r10d;
        else if (reg_mask & R10W)
            value += state->r10w;
        else if (reg_mask & R10B)
            value += state->r10b;
    }
    if MATCHES_R09_FAMILY (reg_mask) {
        if (reg_mask & R09)
            value += state->r9;
        else if (reg_mask & R09D)
            value += state->r9d;
        else if (reg_mask & R09W)
            value += state->r9w;
        else if (reg_mask & R09B)
            value += state->r9b;
    }
    if MATCHES_R08_FAMILY (reg_mask) {
        if (reg_mask & R08)
            value += state->r8;
        else if (reg_mask & R08D)
            value += state->r8d;
        else if (reg_mask & R08W)
            value += state->r8w;
        else if (reg_mask & R08B)
            value += state->r8b;
    }
    if MATCHES_RSI_FAMILY (reg_mask) {
        if (reg_mask & RSI)
            value += state->rsi;
        else if (reg_mask & ESI)
            value += state->esi;
        else if (reg_mask & SI)
            value += state->si;
        else if (reg_mask & SIL)
            value += state->sil;
    }
    if MATCHES_RDI_FAMILY (reg_mask) {
        if (reg_mask & RDI)
            value += state->rdi;
        else if (reg_mask & EDI)
            value += state->edi;
        else if (reg_mask & DI)
            value += state->di;
        else if (reg_mask & DIL)
            value += state->dil;
    }
    if MATCHES_RSP_FAMILY (reg_mask) {
        if (reg_mask & RSP)
            value += state->rsp;
        else if (reg_mask & ESP)
            value += state->esp;
        else if (reg_mask & SP)
            value += state->sp;
        else if (reg_mask & SPL)
            value += state->spl;
    }
    if MATCHES_RBP_FAMILY (reg_mask) {
        if (reg_mask & RBP)
            value += state->rbp;
        else if (reg_mask & EBP)
            value += state->ebp;
        else if (reg_mask & BP)
            value += state->bp;
        else if (reg_mask & BPL)
            value += state->bpl;
    }
    if MATCHES_RDX_FAMILY (reg_mask) {
        if (reg_mask & RDX)
            value += state->rdx;
        else if (reg_mask & EDX)
            value += state->edx;
        else if (reg_mask & DX)
            value += state->dx;
        else if (reg_mask & DL)
            value += state->dl;
    }
    if MATCHES_RCX_FAMILY (reg_mask) {
        if (reg_mask & RCX)
            value += state->rcx;
        else if (reg_mask & ECX)
            value += state->ecx;
        else if (reg_mask & CX)
            value += state->cx;
        else if (reg_mask & CL)
            value += state->cl;
    }
    if MATCHES_RBX_FAMILY (reg_mask) {
        if (reg_mask & RBX)
            value += state->rbx;
        else if (reg_mask & EBX)
            value += state->ebx;
        else if (reg_mask & BX)
            value += state->bx;
        else if (reg_mask & BL)
            value += state->bl;
    }
    if MATCHES_RAX_FAMILY (reg_mask) {
        if (reg_mask & RAX)
            value += state->rax;
        else if (reg_mask & EAX)
            value += state->eax;
        else if (reg_mask & AX)
            value += state->ax;
        else if (reg_mask & AX)
            value += state->al;
    }

    return value;
}

void info_registers(const struct STATE *state) {
    fprintf(stderr, "=====================\n");
    fprintf(stderr, "rax\t\t%p\n", state->rax);
    fprintf(stderr, "rbx\t\t%p\n", state->rbx);
    fprintf(stderr, "rcx\t\t%p\n", state->rcx);
    fprintf(stderr, "rdx\t\t%p\n", state->rdx);
    fprintf(stderr, "rsi\t\t%p\n", state->rsi);
    fprintf(stderr, "rdi\t\t%p\n", state->rdi);
    fprintf(stderr, "rbp\t\t%p\n", state->rbp);
    fprintf(stderr, "rsp\t\t%p\n", state->rsp);
    fprintf(stderr, "r8 \t\t%p\n", state->r8);
    fprintf(stderr, "r9 \t\t%p\n", state->r9);
    fprintf(stderr, "r10\t\t%p\n", state->r10);
    fprintf(stderr, "r11\t\t%p\n", state->r11);
    fprintf(stderr, "r12\t\t%p\n", state->r12);
    fprintf(stderr, "r13\t\t%p\n", state->r13);
    fprintf(stderr, "r14\t\t%p\n", state->r14);
    fprintf(stderr, "r15\t\t%p\n", state->r15);
    fprintf(stderr, "rip\t\t%p\n", state->rip);
    fprintf(stderr, "=====================\n");
}

uintptr_t compute_mem_addr(uint64_t base_reg_mask, uint64_t index_reg_mask,
                           uint64_t scale, uint64_t displacement,
                           const struct STATE *state) {
    uintptr_t ret = apply_mask(base_reg_mask, state) +
                    apply_mask(index_reg_mask, state) * scale + displacement;

    return ret;
}

void patch_start_pifr_write(int64_t pifr_id, int64_t base_reg_mask,
                            int64_t scale, int64_t index_reg_mask,
                            int64_t displacement, size_t size,
                            const struct STATE *state) {
    if (!base_reg_mask) return;
    // fprintf(stderr, "before pifrrt_start(w)1\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_start(w), failed to retrieve dlsyms\n");
        return;
    }
    // fprintf(stderr, "before pifrrt_start(w)1\n");
    uintptr_t mem_addr = compute_mem_addr(base_reg_mask, index_reg_mask, scale,
                                          displacement, state);
    // fprintf(stderr, "before pifrrt_start(w)1\n");
    dlcall(pifrrt_start_pifr, pifr_id, true, mem_addr, size, state->rbp,
           state->rip);
    // fprintf(stderr, "after pifrrt_start(w)1\n");
}

void patch_start_pifr_read(int64_t pifr_id, int64_t base_reg_mask,
                           int64_t scale, int64_t index_reg_mask,
                           int64_t displacement, size_t size,
                           const struct STATE *state) {
    if (!base_reg_mask) return;
    // fprintf(stderr, "before pifrrt_start(r)\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_start(r), failed to retrieve dlsyms\n");
        return;
    }
    uintptr_t mem_addr = compute_mem_addr(base_reg_mask, index_reg_mask, scale,
                                          displacement, state);
    dlcall(pifrrt_start_pifr, pifr_id, false, mem_addr, size, state->rbp,
           state->rip);
}

void patch_start_pifr_indirect_write(int64_t pifr_id, int64_t base_reg_mask,
                                     int64_t scale, int64_t index_reg_mask,
                                     int64_t displacement, size_t size,
                                     int64_t post_displacement,
                                     const struct STATE *state) {
    if (!base_reg_mask) return;
    // fprintf(stderr, "before pifrrt_start(w)2\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_start(w), failed to retrieve dlsyms\n");
        return;
    }
    // fprintf(stderr, "before pifrrt_start(w)2 %d\n", pifr_id);
    // info_registers(state);
    uintptr_t base_addr = compute_mem_addr(base_reg_mask, index_reg_mask, scale,
                                           displacement, state);
    uintptr_t mem_addr = *(intptr_t *)base_addr + post_displacement;
    // fprintf(stderr, "before pifrrt_start(w)2\n");
    dlcall(pifrrt_start_pifr, pifr_id, true, mem_addr, size, state->rbp,
           state->rip);
    // fprintf(stderr, "after pifrrt_start(w)2\n");
}

void patch_start_pifr_indirect_read(int64_t pifr_id, int64_t base_reg_mask,
                                    int64_t scale, int64_t index_reg_mask,
                                    int64_t displacement, size_t size,
                                    int64_t post_displacement,
                                    const struct STATE *state) {
    if (!base_reg_mask) return;
    // fprintf(stderr, "before pifrrt_start(r)\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_start(r), failed to retrieve dlsyms\n");
        return;
    }
    uintptr_t base_addr = compute_mem_addr(base_reg_mask, index_reg_mask, scale,
                                           displacement, state);
    uintptr_t mem_addr = *(intptr_t *)base_addr + post_displacement;
    dlcall(pifrrt_start_pifr, pifr_id, false, mem_addr, size, state->rbp,
           state->rip);
}

void patch_flush(intptr_t current_iaddr, uint64_t base_reg_mask, uint64_t scale,
                 uint64_t index_reg_mask, uint64_t displacement, size_t size,
                 const struct STATE *state) {
    if (!base_reg_mask) return;
    // fprintf(stderr, "before pifrrt_flush\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_flush, failed to retrieve dlsyms\n");
        return;
    }
    uintptr_t mem_addr = compute_mem_addr(base_reg_mask, index_reg_mask, scale,
                                          displacement, state);
    dlcall(pifrrt_flush, current_iaddr, mem_addr, size);
}

void patch_end_pifrs(int64_t current_iaddr) {
    // fprintf(stderr, "before pifrrt_end\n");
    if (!retrieve_dlsyms()) {
        fprintf(stderr, "before pifrrt_end, failed to retrieve dlsyms\n");
        return;
    }
    dlcall(pifrrt_end_pifrs, current_iaddr);
}

void patch_set_offset(const void *addr, const void *base_addr) {
    if (!retrieve_dlsyms()) {
        fprintf(stderr,
                "before pifrrt_set_offset, failed to retrieve dlsyms\n");
        return;
    }
    dlcall(pifrrt_set_offset, addr, base_addr);
}

void init(int argc, char **argv, char **envp, const void *dynamic) {
    if (dlinit(dynamic) != 0) {
        fprintf(stderr, "dlinit() failed: %s\n", strerror(errno));
        return;
    }
    fprintf(stderr, "dlinit() ok\n");

    environ = envp;
    FILE *f = fopen(ENVIRON_PATH, "w");
    if (!f) {
        fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
        abort();
    }
    fwrite(&environ, sizeof(environ), 1, f);
    fclose(f);

    char* libroot = getenv("LIBPIFRRT_ROOT");
    if (!libroot) {
        fprintf(stderr, "LIBPIFRRT_ROOT not set\n");
        abort();
    }
    char libpath[100];
    snprintf(libpath, 100, "%s/libpifrrt.so", libroot);
    handle = dlopen(libpath, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "dlopen(\"libpifrrt.so\") failed\n");
        abort();
    }

    pifrrt_start_pifr = dlsym(handle, "pifrrt_start_pifr");
    if (pifrrt_start_pifr == NULL) {
        fprintf(stderr, "dlsym(\"pifrrt_start_pifr\") failed\n");
        abort();
    }
    pifrrt_end_pifrs = dlsym(handle, "pifrrt_end_pifrs");
    if (pifrrt_end_pifrs == NULL) {
        fprintf(stderr, "dlsym(\"pifrrt_end_pifrs\") failed\n");
        abort();
    }
    pifrrt_flush = dlsym(handle, "pifrrt_flush");
    if (pifrrt_flush == NULL) {
        fprintf(stderr, "dlsym(\"pifrrt_flush\") failed\n");
        abort();
    }
    pifrrt_set_offset = dlsym(handle, "pifrrt_set_offset");
    if (pifrrt_set_offset == NULL) {
        fprintf(stderr, "dlsym(\"pifrrt_set_offset\") failed\n");
        abort();
    }
    has_dlsym = true;

    f = fopen(DLSYM_PATH, "w");
    if (!f) {
        fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
        abort();
    }
    fwrite(&pifrrt_start_pifr, sizeof(void *), 1, f);
    fwrite(&pifrrt_end_pifrs, sizeof(void *), 1, f);
    fwrite(&pifrrt_flush, sizeof(void *), 1, f);
    fwrite(&dlerrno_impl, sizeof(void *), 1, f);
    fwrite(&pifrrt_set_offset, sizeof(void *), 1, f);
    fclose(f);
    fprintf(stderr, "dlsyms persisted\n");
}
