// Copyright 2023 João Gonçalves

#include "./x86.hpp"

#include <Zydis/Register.h>

#include <iostream>
#include <sstream>

#include "./x86_register_state.h"

namespace palantir {
namespace x86 {

bool same_mem_access(MemAccess m1, MemAccess m2) {
    return m1.base == m2.base && m1.index == m2.index && m1.scale == m2.scale &&
           m1.displacement == m2.displacement;
}

Instruction* DisassembleInstruction(uint64_t runtime_addr, uint8_t* buffer,
                                    uint64_t length) {
    ZydisDisassembledInstruction* zdi = new ZydisDisassembledInstruction();
    if (ZYAN_SUCCESS(ZydisDisassembleIntel(
            /* machine_mode:    */ ZYDIS_MACHINE_MODE_LONG_64,
            /* runtime_address: */ runtime_addr,
            /* buffer:          */ buffer,
            /* length:          */ length,
            /* instruction:     */ zdi))) {
        return new Instruction(zdi);
    }
    return nullptr;
}

bool Instruction::has_fallthrough() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_RET:
        case ZYDIS_MNEMONIC_JMP:
        case ZYDIS_MNEMONIC_HLT:
            return false;
        default:
            return true;
    }
}

bool Instruction::is_call() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_CALL:
            return true;
        default:
            return false;
    }
}

// bool Instruction::is_dynamic_call() const {
//     if (!is_call()) return false;
// }

bool Instruction::is_jump() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_JCXZ:   // 0xE3
        case ZYDIS_MNEMONIC_JECXZ:  // 0xE3
        case ZYDIS_MNEMONIC_JRCXZ:  // 0xE3 (???)
        case ZYDIS_MNEMONIC_JNLE:   // 0x7F (JG)
        case ZYDIS_MNEMONIC_JLE:    // 0x7E (JNG)
        case ZYDIS_MNEMONIC_JNL:    // 0x7D (JGE)
        case ZYDIS_MNEMONIC_JL:     // 0x7C (JNGE)
        case ZYDIS_MNEMONIC_JNP:    // 0x7B (JPO)
        case ZYDIS_MNEMONIC_JP:     // 0x7A (JPE)
        case ZYDIS_MNEMONIC_JNS:    // 0x79
        case ZYDIS_MNEMONIC_JS:     // 0x78
        case ZYDIS_MNEMONIC_JNBE:   // 0x77 (JA)
        case ZYDIS_MNEMONIC_JBE:    // 0x76 (JNA)
        case ZYDIS_MNEMONIC_JNZ:    // 0x75 (JNE)
        case ZYDIS_MNEMONIC_JZ:     // 0x74 (JE)
        case ZYDIS_MNEMONIC_JNB:    // 0x73 (JNC, JAE)
        case ZYDIS_MNEMONIC_JB:     // 0x72 (JC, JNAE)
        case ZYDIS_MNEMONIC_JNO:    // 0x71
        case ZYDIS_MNEMONIC_JO:     // 0x70
        case ZYDIS_MNEMONIC_JMP:    // 0xEB, 0xE9, FF, EA, REX.W FF
            return true;
        default:
            return false;
    }
}

bool Instruction::is_flush() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_CLWB:
        case ZYDIS_MNEMONIC_CLFLUSH:
        case ZYDIS_MNEMONIC_CLFLUSHOPT:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_direct_jump() const {
    switch (zdi->info.mnemonic) {
        // FIXME
        case ZYDIS_MNEMONIC_JMP:  // 0xEB, 0xE9, FF, EA, REX.W FF
            return true;
        default:
            return false;
    }
}

bool Instruction::is_mov() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_MOV:
        case ZYDIS_MNEMONIC_MOVAPD:
        case ZYDIS_MNEMONIC_MOVAPS:
        case ZYDIS_MNEMONIC_MOVBE:
        case ZYDIS_MNEMONIC_MOVD:
        case ZYDIS_MNEMONIC_MOVDDUP:
        case ZYDIS_MNEMONIC_MOVDIR64B:
        case ZYDIS_MNEMONIC_MOVDIRI:
        case ZYDIS_MNEMONIC_MOVDQ2Q:
        case ZYDIS_MNEMONIC_MOVDQA:
        case ZYDIS_MNEMONIC_MOVDQU:
        case ZYDIS_MNEMONIC_MOVHLPS:
        case ZYDIS_MNEMONIC_MOVHPD:
        case ZYDIS_MNEMONIC_MOVHPS:
        case ZYDIS_MNEMONIC_MOVLHPS:
        case ZYDIS_MNEMONIC_MOVLPD:
        case ZYDIS_MNEMONIC_MOVLPS:
        case ZYDIS_MNEMONIC_MOVMSKPD:
        case ZYDIS_MNEMONIC_MOVMSKPS:
        case ZYDIS_MNEMONIC_MOVNTDQ:
        case ZYDIS_MNEMONIC_MOVNTDQA:
        case ZYDIS_MNEMONIC_MOVNTI:
        case ZYDIS_MNEMONIC_MOVNTPD:
        case ZYDIS_MNEMONIC_MOVNTPS:
        case ZYDIS_MNEMONIC_MOVNTQ:
        case ZYDIS_MNEMONIC_MOVNTSD:
        case ZYDIS_MNEMONIC_MOVNTSS:
        case ZYDIS_MNEMONIC_MOVQ:
        case ZYDIS_MNEMONIC_MOVQ2DQ:
        case ZYDIS_MNEMONIC_MOVSB:
        case ZYDIS_MNEMONIC_MOVSD:
        case ZYDIS_MNEMONIC_MOVSHDUP:
        case ZYDIS_MNEMONIC_MOVSLDUP:
        case ZYDIS_MNEMONIC_MOVSQ:
        case ZYDIS_MNEMONIC_MOVSS:
        case ZYDIS_MNEMONIC_MOVSW:
        case ZYDIS_MNEMONIC_MOVSX:
        case ZYDIS_MNEMONIC_MOVSXD:
        case ZYDIS_MNEMONIC_MOVUPD:
        case ZYDIS_MNEMONIC_MOVUPS:
        case ZYDIS_MNEMONIC_MOVZX:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_add() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_ADD:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_sub() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_SUB:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_ntstore() const {
    switch (zdi->info.mnemonic) {
        case ZYDIS_MNEMONIC_MOVNTDQ:
        case ZYDIS_MNEMONIC_MOVNTDQA:
        case ZYDIS_MNEMONIC_MOVNTI:
        case ZYDIS_MNEMONIC_MOVNTPD:
        case ZYDIS_MNEMONIC_MOVNTPS:
        case ZYDIS_MNEMONIC_MOVNTQ:
        case ZYDIS_MNEMONIC_MOVNTSD:
        case ZYDIS_MNEMONIC_MOVNTSS:
        case ZYDIS_MNEMONIC_VMOVNTDQ:
        case ZYDIS_MNEMONIC_VMOVNTDQA:
        case ZYDIS_MNEMONIC_VMOVNTPD:
        case ZYDIS_MNEMONIC_VMOVNTPS:
            return true;
        default:
            return false;
    }
}

bool Instruction::writes_operand(uint8_t idx) const {
    ZydisDecodedOperand zdo = zdi->operands[idx];
    return zdo.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;
}

bool Instruction::reads_operand(uint8_t idx) const {
    ZydisDecodedOperand zdo = zdi->operands[idx];
    return zdo.actions & ZYDIS_OPERAND_ACTION_MASK_READ;
}

ZydisRegister Instruction::register_written() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) {
            switch (zdo.type) {
                case ZYDIS_OPERAND_TYPE_REGISTER:
                    return zdo.reg.value;
                default:
                    break;
            }
        }
    }
    return ZYDIS_REGISTER_NONE;
}

bool Instruction::reads_memory() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.type == ZYDIS_OPERAND_TYPE_MEMORY && reads_operand(i))
            return true;
    }
    return false;
}

ZydisRegister Instruction::reads_from_register() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.type == ZYDIS_OPERAND_TYPE_REGISTER && reads_operand(i))
            return zdo.reg.value;
    }
    return ZYDIS_REGISTER_NONE;
}

bool Instruction::writes_memory() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.type == ZYDIS_OPERAND_TYPE_MEMORY && writes_operand(i))
            return true;
    }
    return false;
}

MemAccess* Instruction::reads_from_memory() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.type == ZYDIS_OPERAND_TYPE_MEMORY && reads_operand(i)) {
            MemAccess* ma = new MemAccess();
            ma->base = zdo.mem.base;
            ma->index = zdo.mem.index;
            ma->scale = zdo.mem.scale;
            ma->displacement = zdo.mem.disp.value;
            return ma;
        }
    }
    return nullptr;
}

MemAccess* Instruction::writes_to_memory() const {
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        if (zdo.type == ZYDIS_OPERAND_TYPE_MEMORY && writes_operand(i)) {
            MemAccess* ma = new MemAccess();
            ma->base = zdo.mem.base;
            ma->index = zdo.mem.index;
            ma->scale = zdo.mem.scale;
            ma->displacement = zdo.mem.disp.value;
            return ma;
        }
    }
    return nullptr;
}

std::vector<ZydisRegister> Instruction::get_relevant_operands() const {
    // std::cout << "operands: " << +zdi->info.operand_count << std::endl;
    // std::cout << "visible: " << +zdi->info.operand_count_visible <<
    // std::endl;
    std::vector<ZydisRegister> registers;
    for (size_t i = 0; i < zdi->info.operand_count_visible; i++) {
        ZydisDecodedOperand zdo = zdi->operands[i];
        // std::cout << "operand " << +i << std::endl;
        // std::cout << "\tvisible: " << zdo.visibility << std::endl;
        // std::cout << "\tactions: " << std::hex << +zdo.actions <<
        // std::endl; std::cout << "\ttype: " << zdo.type << std::endl;
        switch (zdo.type) {
            case ZYDIS_OPERAND_TYPE_REGISTER:
                // std::cout << "\treg: " << zdo.reg.value << std::endl;
                registers.push_back(zdo.reg.value);
                break;
            case ZYDIS_OPERAND_TYPE_MEMORY:
                // std::cout << "\tmem: base (" << zdo.mem.base << ") index
                // ("
                //           << zdo.mem.index << ")" << std::endl;
                if (zdo.mem.base != ZYDIS_REGISTER_NONE)
                    registers.push_back(zdo.mem.base);
                if (zdo.mem.index != ZYDIS_REGISTER_NONE)
                    registers.push_back(zdo.mem.index);
                break;
            case ZYDIS_OPERAND_TYPE_POINTER:
                // std::cout << "\tptr: " << zdo.ptr.segment << "+"
                //           << zdo.ptr.offset << std::endl;
                break;
            case ZYDIS_OPERAND_TYPE_IMMEDIATE:
                // if (zdo.imm.is_signed)
                //     std::cout << "\timm: (signed) " << std::hex
                //               << zdo.imm.value.s << std::endl;
                // else
                //     std::cout << "\timm: (unsigned) " << std::hex
                //               << zdo.imm.value.u << std::endl;
                break;
            case ZYDIS_OPERAND_TYPE_UNUSED:
            default:
                // std::cout << "\tunused " << std::endl;
                break;
        }
    }
    // std::cout << "Relevant registers: ";
    // for (ZydisRegister r : regs)
    //     std::cout << ZydisRegisterGetString(r) << " (" <<
    //     +ZydisRegisterGetId(r)
    //               << "), ";
    // std::cout << std::endl;
    return registers;
}

std::string MemAccessToStr(MemAccess* ma) {
    std::stringstream ss;
    ss << "[" << ZydisRegisterGetString(ma->base);
    if (ma->index)
        ss << "+" << std::hex << +ma->scale << "*"
           << ZydisRegisterGetString(ma->index);
    if (ma->displacement) ss << "+" << std::hex << ma->displacement;
    ss << "]";
    if (ma->post_displacement) ss << " + " << std::hex << ma->post_displacement;
    return ss.str();
}

uint64_t RegToMask(ZydisRegister reg) {
    switch (reg) {
        case ZYDIS_REGISTER_AL:
            return AL;
        case ZYDIS_REGISTER_CL:
            return CL;
        case ZYDIS_REGISTER_DL:
            return DL;
        case ZYDIS_REGISTER_BL:
            return BL;
        case ZYDIS_REGISTER_SPL:
            return SPL;
        case ZYDIS_REGISTER_BPL:
            return BPL;
        case ZYDIS_REGISTER_SIL:
            return SIL;
        case ZYDIS_REGISTER_DIL:
            return DIL;
        case ZYDIS_REGISTER_R8B:
            return R08B;
        case ZYDIS_REGISTER_R9B:
            return R09B;
        case ZYDIS_REGISTER_R10B:
            return R10B;
        case ZYDIS_REGISTER_R11B:
            return R11B;
        case ZYDIS_REGISTER_R12B:
            return R12B;
        case ZYDIS_REGISTER_R13B:
            return R13B;
        case ZYDIS_REGISTER_R14B:
            return R14B;
        case ZYDIS_REGISTER_R15B:
            return R15B;
        case ZYDIS_REGISTER_AX:
            return AX;
        case ZYDIS_REGISTER_CX:
            return CX;
        case ZYDIS_REGISTER_DX:
            return DX;
        case ZYDIS_REGISTER_BX:
            return BX;
        case ZYDIS_REGISTER_SP:
            return SP;
        case ZYDIS_REGISTER_BP:
            return BP;
        case ZYDIS_REGISTER_SI:
            return SI;
        case ZYDIS_REGISTER_DI:
            return DI;
        case ZYDIS_REGISTER_R8W:
            return R08W;
        case ZYDIS_REGISTER_R9W:
            return R09W;
        case ZYDIS_REGISTER_R10W:
            return R10W;
        case ZYDIS_REGISTER_R11W:
            return R11W;
        case ZYDIS_REGISTER_R12W:
            return R12W;
        case ZYDIS_REGISTER_R13W:
            return R13W;
        case ZYDIS_REGISTER_R14W:
            return R14W;
        case ZYDIS_REGISTER_R15W:
            return R15W;
        case ZYDIS_REGISTER_EAX:
            return EAX;
        case ZYDIS_REGISTER_ECX:
            return ECX;
        case ZYDIS_REGISTER_EDX:
            return EDX;
        case ZYDIS_REGISTER_EBX:
            return EBX;
        case ZYDIS_REGISTER_ESP:
            return ESP;
        case ZYDIS_REGISTER_EBP:
            return EBP;
        case ZYDIS_REGISTER_ESI:
            return ESI;
        case ZYDIS_REGISTER_EDI:
            return EDI;
        case ZYDIS_REGISTER_R8D:
            return R08D;
        case ZYDIS_REGISTER_R9D:
            return R09D;
        case ZYDIS_REGISTER_R10D:
            return R10D;
        case ZYDIS_REGISTER_R11D:
            return R11D;
        case ZYDIS_REGISTER_R12D:
            return R12D;
        case ZYDIS_REGISTER_R13D:
            return R13D;
        case ZYDIS_REGISTER_R14D:
            return R14D;
        case ZYDIS_REGISTER_R15D:
            return R15D;
        case ZYDIS_REGISTER_RAX:
            return RAX;
        case ZYDIS_REGISTER_RCX:
            return RCX;
        case ZYDIS_REGISTER_RDX:
            return RDX;
        case ZYDIS_REGISTER_RBX:
            return RBX;
        case ZYDIS_REGISTER_RSP:
            return RSP;
        case ZYDIS_REGISTER_RBP:
            return RBP;
        case ZYDIS_REGISTER_RSI:
            return RSI;
        case ZYDIS_REGISTER_RDI:
            return RDI;
        case ZYDIS_REGISTER_R8:
            return R08;
        case ZYDIS_REGISTER_R9:
            return R09;
        case ZYDIS_REGISTER_R10:
            return R10;
        case ZYDIS_REGISTER_R11:
            return R11;
        case ZYDIS_REGISTER_R12:
            return R12;
        case ZYDIS_REGISTER_R13:
            return R13;
        case ZYDIS_REGISTER_R14:
            return R14;
        case ZYDIS_REGISTER_R15:
            return R15;
        default:
            return 0;
    }
}

uint64_t RegToFamily(ZydisRegister reg) {
    switch (reg) {
        case ZYDIS_REGISTER_AL:
        case ZYDIS_REGISTER_AH:
        case ZYDIS_REGISTER_AX:
        case ZYDIS_REGISTER_EAX:
        case ZYDIS_REGISTER_RAX:
            return RAX_FAMILY;
        case ZYDIS_REGISTER_BL:
        case ZYDIS_REGISTER_BH:
        case ZYDIS_REGISTER_BX:
        case ZYDIS_REGISTER_EBX:
        case ZYDIS_REGISTER_RBX:
            return RBX_FAMILY;
        case ZYDIS_REGISTER_CL:
        case ZYDIS_REGISTER_CH:
        case ZYDIS_REGISTER_CX:
        case ZYDIS_REGISTER_ECX:
        case ZYDIS_REGISTER_RCX:
            return RCX_FAMILY;
        case ZYDIS_REGISTER_DL:
        case ZYDIS_REGISTER_DH:
        case ZYDIS_REGISTER_DX:
        case ZYDIS_REGISTER_EDX:
        case ZYDIS_REGISTER_RDX:
            return RDX_FAMILY;
        case ZYDIS_REGISTER_SPL:
        case ZYDIS_REGISTER_SP:
        case ZYDIS_REGISTER_ESP:
        case ZYDIS_REGISTER_RSP:
            return RSP_FAMILY;
        case ZYDIS_REGISTER_BPL:
        case ZYDIS_REGISTER_BP:
        case ZYDIS_REGISTER_EBP:
        case ZYDIS_REGISTER_RBP:
            return RBP_FAMILY;
        case ZYDIS_REGISTER_SIL:
        case ZYDIS_REGISTER_SI:
        case ZYDIS_REGISTER_ESI:
        case ZYDIS_REGISTER_RSI:
            return RSI_FAMILY;
        case ZYDIS_REGISTER_DIL:
        case ZYDIS_REGISTER_DI:
        case ZYDIS_REGISTER_EDI:
        case ZYDIS_REGISTER_RDI:
            return RDI_FAMILY;
        case ZYDIS_REGISTER_R8B:
        case ZYDIS_REGISTER_R8W:
        case ZYDIS_REGISTER_R8D:
        case ZYDIS_REGISTER_R8:
            return R08_FAMILY;
        case ZYDIS_REGISTER_R9B:
        case ZYDIS_REGISTER_R9W:
        case ZYDIS_REGISTER_R9D:
        case ZYDIS_REGISTER_R9:
            return R09_FAMILY;
        case ZYDIS_REGISTER_R10B:
        case ZYDIS_REGISTER_R10W:
        case ZYDIS_REGISTER_R10D:
        case ZYDIS_REGISTER_R10:
            return R10_FAMILY;
        case ZYDIS_REGISTER_R11B:
        case ZYDIS_REGISTER_R11W:
        case ZYDIS_REGISTER_R11D:
        case ZYDIS_REGISTER_R11:
            return R11_FAMILY;
        case ZYDIS_REGISTER_R12B:
        case ZYDIS_REGISTER_R12W:
        case ZYDIS_REGISTER_R12D:
        case ZYDIS_REGISTER_R12:
            return R12_FAMILY;
        case ZYDIS_REGISTER_R13B:
        case ZYDIS_REGISTER_R13W:
        case ZYDIS_REGISTER_R13D:
        case ZYDIS_REGISTER_R13:
            return R13_FAMILY;
        case ZYDIS_REGISTER_R14B:
        case ZYDIS_REGISTER_R14W:
        case ZYDIS_REGISTER_R14D:
        case ZYDIS_REGISTER_R14:
            return R14_FAMILY;
        case ZYDIS_REGISTER_R15B:
        case ZYDIS_REGISTER_R15W:
        case ZYDIS_REGISTER_R15D:
        case ZYDIS_REGISTER_R15:
            return R15_FAMILY;
        default:
            return 0;
    }
}

}  // namespace x86
}  // namespace palantir
