#include "elf_creator.h"

#include <stdio.h>
#include <sys/stat.h>

int write_elf_file(const ArchConfig *config, const MachineCode *code)
{
    if (!config || !code || !code->bytes || code->size == 0)
    {
        return 1;
    }

    const uint64_t base = config->base_vaddr ? config->base_vaddr : DEFAULT_BASE_VADDR;
    const uint64_t entry = base + TEXT_OFFSET;

    Elf64_Ehdr elf_hdr = {
        .e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_LINUX, 0},
        .e_type = ET_EXEC,
        .e_machine = config->e_machine,
        .e_version = EV_CURRENT,
        .e_entry = entry,
        .e_phoff = sizeof(Elf64_Ehdr),
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(Elf64_Ehdr),
        .e_phentsize = sizeof(Elf64_Phdr),
        .e_phnum = 1,
        .e_shentsize = sizeof(Elf64_Shdr),
        .e_shnum = 0,
        .e_shstrndx = SHN_UNDEF,
    };

    Elf64_Phdr phdr = {
        .p_type = PT_LOAD,
        .p_offset = TEXT_OFFSET,
        .p_vaddr = entry,
        .p_paddr = entry,
        .p_filesz = code->size,
        .p_memsz = code->size,
        .p_flags = PF_X | PF_R,
        .p_align = 0x1000,
    };

    FILE *out = fopen("elf", "wb");
    if (!out)
    {
        perror("Failed to open output file");
        return 1;
    }

    if (fwrite(&elf_hdr, 1, sizeof(elf_hdr), out) != sizeof(elf_hdr))
    {
        perror("Failed to write ELF header");
        fclose(out);
        return 1;
    }

    if (fwrite(&phdr, 1, sizeof(phdr), out) != sizeof(phdr))
    {
        perror("Failed to write program header");
        fclose(out);
        return 1;
    }

    if (fwrite(code->bytes, 1, code->size, out) != code->size)
    {
        perror("Failed to write machine code");
        fclose(out);
        return 1;
    }

    fclose(out);

    if (chmod("elf", 0755) != 0)
    {
        perror("Failed to set executable permissions");
        return 1;
    }

    return 0;
}
