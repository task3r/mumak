#include <stdint.h>

struct STATE {
    union {
        uint16_t rflags;
        uint64_t __padding;
    };
    union {
        int64_t r15;
        int32_t r15d;
        int16_t r15w;
        int8_t r15b;
    };
    union {
        int64_t r14;
        int32_t r14d;
        int16_t r14w;
        int8_t r14b;
    };
    union {
        int64_t r13;
        int32_t r13d;
        int16_t r13w;
        int8_t r13b;
    };
    union {
        int64_t r12;
        int32_t r12d;
        int16_t r12w;
        int8_t r12b;
    };
    union {
        int64_t r11;
        int32_t r11d;
        int16_t r11w;
        int8_t r11b;
    };
    union {
        int64_t r10;
        int32_t r10d;
        int16_t r10w;
        int8_t r10b;
    };
    union {
        int64_t r9;
        int32_t r9d;
        int16_t r9w;
        int8_t r9b;
    };
    union {
        int64_t r8;
        int32_t r8d;
        int16_t r8w;
        int8_t r8b;
    };
    union {
        int64_t rdi;
        int32_t edi;
        int16_t di;
        int8_t dil;
    };
    union {
        int64_t rsi;
        int32_t esi;
        int16_t si;
        int8_t sil;
    };
    union {
        int64_t rbp;
        int32_t ebp;
        int16_t bp;
        int8_t bpl;
    };
    union {
        int64_t rbx;
        int32_t ebx;
        int16_t bx;
        struct {
            int8_t bl;
            int8_t bh;
        };
    };
    union {
        int64_t rdx;
        int32_t edx;
        int16_t dx;
        struct {
            int8_t dl;
            int8_t dh;
        };
    };
    union {
        int64_t rcx;
        int32_t ecx;
        int16_t cx;
        struct {
            int8_t cl;
            int8_t ch;
        };
    };
    union {
        int64_t rax;
        int32_t eax;
        int16_t ax;
        struct {
            int8_t al;
            int8_t ah;
        };
    };
    union {
        int64_t rsp;
        int32_t esp;
        int16_t sp;
        int16_t spl;
    };
    const union {
        int64_t rip;
        int32_t eip;
        int16_t ip;
    };
};

/*
 * Flags.
 */
#define OF 0x0001
#define CF 0x0100
#define PF 0x0400
#define AF 0x1000
#define ZF 0x4000
#define SF 0x8000

#define R15_FAMILY 0x000000000000000f
#define R14_FAMILY 0x00000000000000f0
#define R13_FAMILY 0x0000000000000f00
#define R12_FAMILY 0x000000000000f000
#define R11_FAMILY 0x00000000000f0000
#define R10_FAMILY 0x0000000000f00000
#define R09_FAMILY 0x000000000f000000
#define R08_FAMILY 0x00000000f0000000
#define RSI_FAMILY 0x0000000f00000000
#define RDI_FAMILY 0x000000f000000000
#define RSP_FAMILY 0x00000f0000000000
#define RBP_FAMILY 0x0000f00000000000
#define RDX_FAMILY 0x000f000000000000
#define RCX_FAMILY 0x00f0000000000000
#define RBX_FAMILY 0x0f00000000000000
#define RAX_FAMILY 0xf000000000000000
#define R15 0x0000000000000001
#define R15D 0x0000000000000002
#define R15W 0x0000000000000004
#define R15B 0x0000000000000008
#define R14 0x0000000000000010
#define R14D 0x0000000000000020
#define R14W 0x0000000000000040
#define R14B 0x0000000000000080
#define R13 0x0000000000000100
#define R13D 0x0000000000000200
#define R13W 0x0000000000000400
#define R13B 0x0000000000000800
#define R12 0x0000000000001000
#define R12D 0x0000000000002000
#define R12W 0x0000000000004000
#define R12B 0x0000000000008000
#define R11 0x0000000000010000
#define R11D 0x0000000000020000
#define R11W 0x0000000000040000
#define R11B 0x0000000000080000
#define R10 0x0000000000100000
#define R10D 0x0000000000200000
#define R10W 0x0000000000400000
#define R10B 0x0000000000800000
#define R09 0x0000000001000000
#define R09D 0x0000000002000000
#define R09W 0x0000000004000000
#define R09B 0x0000000008000000
#define R08 0x0000000010000000
#define R08D 0x0000000020000000
#define R08W 0x0000000040000000
#define R08B 0x0000000080000000
#define RSI 0x0000000100000000
#define ESI 0x0000000200000000
#define SI 0x0000000400000000
#define SIL 0x0000000800000000
#define RDI 0x0000001000000000
#define EDI 0x0000002000000000
#define DI 0x0000004000000000
#define DIL 0x0000008000000000
#define RSP 0x0000010000000000
#define ESP 0x0000020000000000
#define SP 0x0000040000000000
#define SPL 0x0000080000000000
#define RBP 0x0000100000000000
#define EBP 0x0000200000000000
#define BP 0x0000400000000000
#define BPL 0x0000800000000000
#define RDX 0x0001000000000000
#define EDX 0x0002000000000000
#define DX 0x0004000000000000
#define DL 0x0008000000000000
#define RCX 0x0010000000000000
#define ECX 0x0020000000000000
#define CX 0x0040000000000000
#define CL 0x0080000000000000
#define RBX 0x0100000000000000
#define EBX 0x0200000000000000
#define BX 0x0400000000000000
#define BL 0x0800000000000000
#define RAX 0x1000000000000000
#define EAX 0x2000000000000000
#define AX 0x4000000000000000
#define AL 0x8000000000000000
#define MATCHES_R15_FAMILY(mask) (mask & R15_FAMILY)
#define MATCHES_R14_FAMILY(mask) (mask & R14_FAMILY)
#define MATCHES_R13_FAMILY(mask) (mask & R13_FAMILY)
#define MATCHES_R12_FAMILY(mask) (mask & R12_FAMILY)
#define MATCHES_R11_FAMILY(mask) (mask & R11_FAMILY)
#define MATCHES_R10_FAMILY(mask) (mask & R10_FAMILY)
#define MATCHES_R09_FAMILY(mask) (mask & R09_FAMILY)
#define MATCHES_R08_FAMILY(mask) (mask & R08_FAMILY)
#define MATCHES_RSI_FAMILY(mask) (mask & RSI_FAMILY)
#define MATCHES_RDI_FAMILY(mask) (mask & RDI_FAMILY)
#define MATCHES_RSP_FAMILY(mask) (mask & RSP_FAMILY)
#define MATCHES_RBP_FAMILY(mask) (mask & RBP_FAMILY)
#define MATCHES_RDX_FAMILY(mask) (mask & RDX_FAMILY)
#define MATCHES_RCX_FAMILY(mask) (mask & RCX_FAMILY)
#define MATCHES_RBX_FAMILY(mask) (mask & RBX_FAMILY)
#define MATCHES_RAX_FAMILY(mask) (mask & RAX_FAMILY)
