// Copyright 2023 João Gonçalves

#include "./disassembler.hpp"

#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "./x86.hpp"

struct FunctionInfo {
    Elf64_Addr start;  // Starting address of the function
    Elf64_Addr size;   // Size of the function
    const char* name;
};

struct Section {
    Elf_Scn* scn;
    Elf64_Shdr* shdr;
    Elf_Data* data;
};

struct ElfSections {
    Section symtab;
    Section dynsym;
    Section text;
    Section relaplt;
    Section reladyn;
    Section pltsec;
    Section pltgot;
};

ElfSections* elf_getsections(Elf* elf) {
    ElfSections* sections = new ElfSections();
    Elf_Scn* scn = nullptr;
    Elf64_Ehdr* ep = elf64_getehdr(elf);
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        Elf64_Shdr* shdr = elf64_getshdr(scn);
        char* name = elf_strptr(elf, ep->e_shstrndx, shdr->sh_name);
        Elf_Data* data = elf_getdata(scn, 0);
        if (strcmp(name, ".symtab") == 0)
            sections->symtab = Section{scn, shdr, data};
        else if (strcmp(name, ".dynsym") == 0)
            sections->dynsym = Section{scn, shdr, data};
        else if (strcmp(name, ".text") == 0)
            sections->text = Section{scn, shdr, data};
        else if (strcmp(name, ".rela.plt") == 0)
            sections->relaplt = Section{scn, shdr, data};
        else if (strcmp(name, ".rela.dyn") == 0)
            sections->reladyn = Section{scn, shdr, data};
        else if (strcmp(name, ".plt.sec") == 0)
            sections->pltsec = Section{scn, shdr, data};
        else if (strcmp(name, ".plt.got") == 0)
            sections->pltgot = Section{scn, shdr, data};
        else {
            std::cout << "LOOOK" << name << std::endl;
        }
    }
    return sections;
}

int read_function_symbols(Elf* elf, Section* sym,
                          std::map<std::string, FunctionInfo*>* functions) {
    Elf64_Sym* symbols = reinterpret_cast<Elf64_Sym*>(sym->data->d_buf);
    int num_symbols = sym->shdr->sh_size / sym->shdr->sh_entsize;
    for (int k = 0; k < num_symbols; k++) {
        Elf64_Sym* symbol = &symbols[k];
        char* name = elf_strptr(elf, sym->shdr->sh_link, symbol->st_name);
        if (ELF64_ST_TYPE(symbol->st_info) == STT_FUNC &&
            symbol->st_size != 0) {
            FunctionInfo* info = new FunctionInfo();
            info->start = symbol->st_value;
            info->size = symbol->st_size;
            info->name = name;
            (*functions)[name] = info;
#ifdef DEBUG
            std::cout << "✔️ ";
        } else {
            std::cout << "  ";
        }
        std::cout << symbol->st_value << " -> " << symbol->st_size << "\t"
                  << name << std::endl;
#else
        }
#endif
    }
    return 0;
}

void extract_external_funcs(palantir::Module* module, Elf* elf,
                            ElfSections* s) {
#ifdef DEBUG
    std::cout << "RELA TABLE " << std::endl;
#endif
    Elf64_Rela* relas = reinterpret_cast<Elf64_Rela*>(s->relaplt.data->d_buf);
    int num_entries = s->relaplt.shdr->sh_size / s->relaplt.shdr->sh_entsize;
    Elf64_Shdr* plt_shdr = s->pltsec.shdr;
    if (s->pltsec.shdr) {
        plt_shdr = s->pltsec.shdr;
    } else {
        plt_shdr = s->pltgot.shdr;
    }
    Elf64_Addr plt_addr = plt_shdr->sh_addr;
    for (int k = 0; k < num_entries; k++) {
        Elf64_Rela* rela = &relas[k];
        GElf_Sym sym;
        gelf_getsym(s->dynsym.data, GELF_R_SYM(rela->r_info), &sym);
        char* name = elf_strptr(elf, s->dynsym.shdr->sh_link, sym.st_name);
#ifdef DEBUG
        std::cout << std::hex << plt_addr;
        std::cout << " " << name << std::endl;
#endif
        module->add_function(
            new palantir::Function(module, name, plt_addr, false), name,
            plt_addr);
        plt_addr += plt_shdr->sh_entsize;
    }
}

palantir::Function* disassemble_function(palantir::Module* module,
                                         const char* name, uint8_t* raw_bytes,
                                         Elf64_Addr start_addr, uint64_t size) {
    size_t offset = 0;
    uint64_t runtime_addr = start_addr + offset;
#ifdef DEBUG
    std::cerr << "⚙️ " << name << " " << start_addr << " " << size << std::endl;
#endif
    palantir::Function* function =
        new palantir::Function(module, name, runtime_addr, true);
    while (offset < size) {
        palantir::x86::Instruction* x86inst =
            palantir::x86::DisassembleInstruction(
                /* runtime_address: */ runtime_addr,
                /* buffer:          */ raw_bytes + offset,
                /* length:          */ size - offset);
        if (x86inst != nullptr) {
#ifdef DEBUG
            std::cout << std::hex << runtime_addr;
            std::cout << "\t" << x86inst->disassembled() << std::endl;
#endif
            function->add_instruction(x86inst, runtime_addr);
            offset += x86inst->get_length();
            runtime_addr += x86inst->get_length();
        } else {
#ifdef DEBUG
            std::cerr << "⚠️ " << name << " " << offset << " " << size
                      << std::endl;
#endif
            return nullptr;
        }
    }
    return function;
}

palantir::Module* disassemble_module(const char* elf_name) {
    // Open the ELF file
    int fd = open(elf_name, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open ELF file\n");
        return nullptr;
    }

    // Initialize the ELF library
    if (elf_version(EV_CURRENT) == EV_NONE) {
        printf("Failed to initialize ELF library\n");
        return nullptr;
    }

    // Open the ELF object
    Elf* elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) {
        printf("Failed to open ELF object\n");
        return nullptr;
    }

    palantir::Module* module = new palantir::Module(elf_name);
    std::map<std::string, FunctionInfo*> func_symbols;
    ElfSections* sections = elf_getsections(elf);
    extract_external_funcs(module, elf, sections);
    if (sections->symtab.data)
        read_function_symbols(elf, &sections->symtab, &func_symbols);
    else
        read_function_symbols(elf, &sections->dynsym, &func_symbols);
    for (auto func_info : func_symbols) {
        const char* name = func_info.first.c_str();
        palantir::Function* f = disassemble_function(
            module, name,
            reinterpret_cast<uint8_t*>(sections->text.data->d_buf) +
                func_info.second->start - sections->text.shdr->sh_addr,
            func_info.second->start, func_info.second->size);
        if (f != nullptr) module->add_function(f, name, f->get_addr());
    }

    // Clean up
    elf_end(elf);
    close(fd);

    return module;
}
