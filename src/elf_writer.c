/**
 * @file elf_writer.c
 * @brief Implementation of ELF file generation.
 */

#include "elf_creator.h"

#include <elf.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#define PAGE_ALIGN 0x1000
#define EXEC_PERMS 0755

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HOST_IS_LITTLE_ENDIAN 1
#else
#define HOST_IS_LITTLE_ENDIAN 0
#endif

/**
 * @brief Swap byte order of ELF64 Executable Header fields.
 *
 * Converts fields between host and target endianness.
 *
 * @param hdr Pointer to the ELF64 header to swap.
 */
static void swap_elf64_ehdr(Elf64_Ehdr* hdr) {
    hdr->e_type = __builtin_bswap16(hdr->e_type);
    hdr->e_machine = __builtin_bswap16(hdr->e_machine);
    hdr->e_version = __builtin_bswap32(hdr->e_version);
    hdr->e_entry = __builtin_bswap64(hdr->e_entry);
    hdr->e_phoff = __builtin_bswap64(hdr->e_phoff);
    hdr->e_shoff = __builtin_bswap64(hdr->e_shoff);
    hdr->e_flags = __builtin_bswap32(hdr->e_flags);
    hdr->e_ehsize = __builtin_bswap16(hdr->e_ehsize);
    hdr->e_phentsize = __builtin_bswap16(hdr->e_phentsize);
    hdr->e_phnum = __builtin_bswap16(hdr->e_phnum);
    hdr->e_shentsize = __builtin_bswap16(hdr->e_shentsize);
    hdr->e_shnum = __builtin_bswap16(hdr->e_shnum);
    hdr->e_shstrndx = __builtin_bswap16(hdr->e_shstrndx);
}

/**
 * @brief Swap byte order of ELF64 Program Header fields.
 *
 * Converts fields between host and target endianness.
 *
 * @param phdr Pointer to the ELF64 program header to swap.
 */
static void swap_elf64_phdr(Elf64_Phdr* phdr) {
    phdr->p_type = __builtin_bswap32(phdr->p_type);
    phdr->p_flags = __builtin_bswap32(phdr->p_flags);
    phdr->p_offset = __builtin_bswap64(phdr->p_offset);
    phdr->p_vaddr = __builtin_bswap64(phdr->p_vaddr);
    phdr->p_paddr = __builtin_bswap64(phdr->p_paddr);
    phdr->p_filesz = __builtin_bswap64(phdr->p_filesz);
    phdr->p_memsz = __builtin_bswap64(phdr->p_memsz);
    phdr->p_align = __builtin_bswap64(phdr->p_align);
}

/**
 * @brief Swap byte order of ELF32 Executable Header fields.
 *
 * Converts fields between host and target endianness.
 *
 * @param hdr Pointer to the ELF32 header to swap.
 */
static void swap_elf32_ehdr(Elf32_Ehdr* hdr) {
    hdr->e_type = __builtin_bswap16(hdr->e_type);
    hdr->e_machine = __builtin_bswap16(hdr->e_machine);
    hdr->e_version = __builtin_bswap32(hdr->e_version);
    hdr->e_entry = __builtin_bswap32(hdr->e_entry);
    hdr->e_phoff = __builtin_bswap32(hdr->e_phoff);
    hdr->e_shoff = __builtin_bswap32(hdr->e_shoff);
    hdr->e_flags = __builtin_bswap32(hdr->e_flags);
    hdr->e_ehsize = __builtin_bswap16(hdr->e_ehsize);
    hdr->e_phentsize = __builtin_bswap16(hdr->e_phentsize);
    hdr->e_phnum = __builtin_bswap16(hdr->e_phnum);
    hdr->e_shentsize = __builtin_bswap16(hdr->e_shentsize);
    hdr->e_shnum = __builtin_bswap16(hdr->e_shnum);
    hdr->e_shstrndx = __builtin_bswap16(hdr->e_shstrndx);
}

/**
 * @brief Swap byte order of ELF32 Program Header fields.
 *
 * Converts fields between host and target endianness.
 *
 * @param phdr Pointer to the ELF32 program header to swap.
 */
static void swap_elf32_phdr(Elf32_Phdr* phdr) {
    phdr->p_type = __builtin_bswap32(phdr->p_type);
    phdr->p_offset = __builtin_bswap32(phdr->p_offset);
    phdr->p_vaddr = __builtin_bswap32(phdr->p_vaddr);
    phdr->p_paddr = __builtin_bswap32(phdr->p_paddr);
    phdr->p_filesz = __builtin_bswap32(phdr->p_filesz);
    phdr->p_memsz = __builtin_bswap32(phdr->p_memsz);
    phdr->p_flags = __builtin_bswap32(phdr->p_flags);
    phdr->p_align = __builtin_bswap32(phdr->p_align);
}

/**
 * @brief Write ELF64 headers to the output file.
 *
 * Constructs and writes the ELF64 Executable Header and Program Header.
 * Handles endianness swapping if necessary.
 *
 * @param out Output file stream.
 * @param config Architecture configuration.
 * @param code Machine code information.
 * @param entry Entry point virtual address.
 * @param text_offset File offset of the .text section.
 * @param need_swap True if byte swapping is required.
 * @return 0 on success, 1 on failure.
 */
static int write_elf64_headers(FILE* out,
                               const ArchConfig* config,
                               const MachineCode* code,
                               uint64_t entry,
                               size_t text_offset,
                               bool need_swap) {
    Elf64_Ehdr elf_hdr = {
        .e_ident = {ELFMAG0,
                    ELFMAG1,
                    ELFMAG2,
                    ELFMAG3,
                    ELFCLASS64,
                    config->endianness,
                    EV_CURRENT,
                    ELFOSABI_LINUX,
                    0},
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
        .p_offset = text_offset,
        .p_vaddr = entry,
        .p_paddr = entry,
        .p_filesz = code->size,
        .p_memsz = code->size,
        .p_flags = PF_X | PF_R,
        .p_align = PAGE_ALIGN,
    };

    if (need_swap) {
        swap_elf64_ehdr(&elf_hdr);
        swap_elf64_phdr(&phdr);
    }

    if (fwrite(&elf_hdr, 1, sizeof(elf_hdr), out) != sizeof(elf_hdr)) {
        perror("Failed to write ELF header");
        return 1;
    }

    if (fwrite(&phdr, 1, sizeof(phdr), out) != sizeof(phdr)) {
        perror("Failed to write program header");
        return 1;
    }

    return 0;
}

/**
 * @brief Write ELF32 headers to the output file.
 *
 * Constructs and writes the ELF32 Executable Header and Program Header.
 * Handles endianness swapping if necessary.
 *
 * @param out Output file stream.
 * @param config Architecture configuration.
 * @param code Machine code information.
 * @param entry Entry point virtual address.
 * @param text_offset File offset of the .text section.
 * @param need_swap True if byte swapping is required.
 * @return 0 on success, 1 on failure.
 */
static int write_elf32_headers(FILE* out,
                               const ArchConfig* config,
                               const MachineCode* code,
                               uint64_t entry,
                               size_t text_offset,
                               bool need_swap) {
    Elf32_Ehdr elf_hdr = {
        .e_ident = {ELFMAG0,
                    ELFMAG1,
                    ELFMAG2,
                    ELFMAG3,
                    ELFCLASS32,
                    config->endianness,
                    EV_CURRENT,
                    ELFOSABI_LINUX,
                    0},
        .e_type = ET_EXEC,
        .e_machine = config->e_machine,
        .e_version = EV_CURRENT,
        .e_entry = (Elf32_Addr)entry,
        .e_phoff = sizeof(Elf32_Ehdr),
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(Elf32_Ehdr),
        .e_phentsize = sizeof(Elf32_Phdr),
        .e_phnum = 1,
        .e_shentsize = sizeof(Elf32_Shdr),
        .e_shnum = 0,
        .e_shstrndx = SHN_UNDEF,
    };

    Elf32_Phdr phdr = {
        .p_type = PT_LOAD,
        .p_offset = (Elf32_Off)text_offset,
        .p_vaddr = (Elf32_Addr)entry,
        .p_paddr = (Elf32_Addr)entry,
        .p_filesz = (Elf32_Word)code->size,
        .p_memsz = (Elf32_Word)code->size,
        .p_flags = PF_X | PF_R,
        .p_align = PAGE_ALIGN,
    };

    if (need_swap) {
        swap_elf32_ehdr(&elf_hdr);
        swap_elf32_phdr(&phdr);
    }

    if (fwrite(&elf_hdr, 1, sizeof(elf_hdr), out) != sizeof(elf_hdr)) {
        perror("Failed to write ELF header");
        return 1;
    }

    if (fwrite(&phdr, 1, sizeof(phdr), out) != sizeof(phdr)) {
        perror("Failed to write program header");
        return 1;
    }

    return 0;
}

/**
 * @brief Write the generated machine code to an ELF executable file.
 *
 * Constructs the ELF header and program header based on the architecture
 * configuration and writes them along with the machine code to a file named
 * "elf". Sets the output file permissions to 0755 (rwxr-xr-x).
 *
 * Supports both 32-bit and 64-bit ELF formats, as well as big and little
 * endian targets, performing necessary byte swapping if the host and target
 * endianness differ.
 *
 * @param config The architecture configuration.
 * @param code The machine code to write.
 * @return 0 on success, 1 on failure.
 */
int write_elf_file(const ArchConfig* config, const MachineCode* code) {
    if (!config || !code || !code->bytes || code->size == 0) {
        return 1;
    }

    (void)printf("Writing ELF file...\n");

    const uint64_t base =
        config->base_vaddr ? config->base_vaddr : DEFAULT_BASE_VADDR;

    // Calculate dynamic text offset
    size_t ehdr_size = (config->elf_class == ELFCLASS64) ? sizeof(Elf64_Ehdr)
                                                         : sizeof(Elf32_Ehdr);
    size_t phdr_size = (config->elf_class == ELFCLASS64) ? sizeof(Elf64_Phdr)
                                                         : sizeof(Elf32_Phdr);
    size_t text_offset = ehdr_size + phdr_size;

    const uint64_t entry = base + text_offset;

    (void)printf("  Base Address: 0x%" PRIx64 "\n", base);
    (void)printf("  Entry Point:  0x%" PRIx64 "\n", entry);
    (void)printf("  ELF Class:    %s\n",
                 (config->elf_class == ELFCLASS64) ? "64-bit" : "32-bit");
    (void)printf("  Endianness:   %s\n",
                 (config->endianness == ELFDATA2LSB) ? "Little Endian"
                                                     : "Big Endian");

    bool need_swap =
        (HOST_IS_LITTLE_ENDIAN && config->endianness == ELFDATA2MSB) ||
        (!HOST_IS_LITTLE_ENDIAN && config->endianness == ELFDATA2LSB);

    FILE* out = fopen("elf", "wb");
    if (!out) {
        perror("Failed to open output file");
        return 1;
    }

    int ret = 0;
    if (config->elf_class == ELFCLASS64) {
        ret = write_elf64_headers(out,
                                  config,
                                  code,
                                  entry,
                                  text_offset,
                                  need_swap);
    } else {
        ret = write_elf32_headers(out,
                                  config,
                                  code,
                                  entry,
                                  text_offset,
                                  need_swap);
    }

    if (ret != 0) {
        (void)fclose(out);
        return ret;
    }

    if (fwrite(code->bytes, 1, code->size, out) != code->size) {
        perror("Failed to write machine code");
        (void)fclose(out);
        return 1;
    }

    (void)fclose(out);

    if (chmod("elf", EXEC_PERMS) != 0) {
        perror("Failed to set executable permissions");
        return 1;
    }

    (void)printf("ELF file written successfully to 'elf'\n");

    return 0;
}
