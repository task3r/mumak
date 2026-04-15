// Copyright 2023 João Gonçalves
#ifndef PALANTIR_X86
#define PALANTIR_X86

#include <Zydis/Zydis.h>

#include <cstdint>
#include <sstream>
#include <vector>

namespace palantir {
namespace x86 {

struct MemAccess {
    bool indirect;
    ZydisRegister base;
    ZydisRegister index;
    uint8_t scale;
    int64_t displacement;
    int64_t post_displacement;
};

bool same_mem_access(MemAccess m1, MemAccess m2);

std::string MemAccessToStr(MemAccess* ma);
uint64_t RegToMask(ZydisRegister reg);
uint64_t RegToFamily(ZydisRegister reg);

class Instruction {
    ZydisDisassembledInstruction* zdi;

 public:
    explicit Instruction(ZydisDisassembledInstruction* zdi) : zdi(zdi) {}
    ZydisDisassembledInstruction* get_zdi() { return zdi; }
    uint64_t get_length() const { return zdi->info.length; }
    uint64_t next_inst_addr() const {
        return zdi->runtime_address + get_length();
    }
    uint64_t jump_target() const {
        // assert is jump
        return next_inst_addr() + zdi->operands[0].imm.value.s;
    }
    const char* disassembled() const { return zdi->text; }
    bool has_fallthrough() const;
    bool is_call() const;
    // bool is_dynamic_call() const;
    bool is_jump() const;
    bool is_flush() const;
    bool is_direct_jump() const;
    bool is_mov() const;
    bool is_add() const;
    bool is_sub() const;
    bool is_ntstore() const;
    // FIXME(task3r): bad name vvvvvvvvvvvvvvvvvvvvv
    std::vector<ZydisRegister> get_relevant_operands() const;
    bool writes_operand(uint8_t idx) const;
    bool reads_operand(uint8_t idx) const;
    bool reads_memory() const;
    bool writes_memory() const;
    ZydisRegister reads_from_register() const;
    ZydisRegister register_written() const;
    MemAccess* reads_from_memory() const;
    MemAccess* writes_to_memory() const;
};

Instruction* DisassembleInstruction(uint64_t runtime_addr, uint8_t* buffer,
                                    uint64_t length);

}  // namespace x86
}  // namespace palantir
#endif
