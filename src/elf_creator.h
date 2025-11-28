#ifndef ELF_CREATOR_H
#define ELF_CREATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <elf.h>

#define TEXT_OFFSET 0x78
#define DEFAULT_BASE_VADDR 0x400000
#define HELLO_LEN 7

typedef struct
{
    const char *keyword;
    uint16_t e_machine;
    uint64_t base_vaddr;
    const char *write_asm;
    const char *write_constraints;
    const char *exit_asm;
    const char *exit_constraints;
} ArchConfig;

typedef struct
{
    uint8_t *bytes;
    size_t size;
} MachineCode;

void initialize_llvm_targets(void);
const ArchConfig *select_arch_config(const char *triple);
int emit_machine_code(const ArchConfig *config, const char *target_triple, MachineCode *out);
int write_elf_file(const ArchConfig *config, const MachineCode *code);

#endif /* ELF_CREATOR_H */