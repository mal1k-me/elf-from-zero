/**
 * @file elf_creator.h
 * @brief Core definitions and function prototypes for the ELF creation tool.
 *
 * This header defines the structures and functions used to generate
 * ELF executables containing machine code generated via LLVM.
 */

#ifndef ELF_CREATOR_H
#define ELF_CREATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Offset of the .text section in the ELF file. */
#define TEXT_OFFSET 0x78
/** @brief Default base virtual address if not specified by architecture config.
 */
#define DEFAULT_BASE_VADDR 0x400000
/** @brief Length of the "Hello!\\n" string. */
#define HELLO_LEN 7

/**
 * @brief Configuration for a specific target architecture.
 *
 * Holds the necessary parameters to generate valid ELF headers and
 * inline assembly for system calls for a given architecture.
 *
 * @note This structure is populated from `data/arch_catalog.json` via the
 * `tools/gen_arch_config.py` script. This approach acts as a "Micro-Libc",
 * providing the minimal OS-specific assembly required for a freestanding
 * binary without linking against a full C library. This allows for lightweight
 * cross-architecture support without external toolchains.
 */
typedef struct {
    const char* keyword;   /**< Substring to match in LLVM target triple (e.g.,
                              "x86_64"). */
    uint16_t e_machine;    /**< ELF machine architecture (e.g., EM_X86_64). */
    uint64_t base_vaddr;   /**< Base virtual address for the executable. */
    const char* write_asm; /**< Inline assembly for the write syscall. */
    const char*
        write_constraints; /**< LLVM constraints string for write syscall. */
    const char* exit_asm;  /**< Inline assembly for the exit syscall. */
    const char*
        exit_constraints; /**< LLVM constraints string for exit syscall. */
} ArchConfig;

/**
 * @brief Container for generated machine code.
 */
typedef struct {
    uint8_t* bytes; /**< Pointer to the buffer containing raw machine code. */
    size_t size;    /**< Size of the machine code buffer in bytes. */
} MachineCode;

/**
 * @brief Initialize LLVM target registry.
 *
 * Must be called before any other LLVM operations.
 */
void initialize_llvm_targets(void);

/**
 * @brief Select the architecture configuration based on the target triple.
 *
 * @param triple The LLVM target triple string.
 * @return Pointer to the matching ArchConfig, or NULL if not found.
 */
const ArchConfig* select_arch_config(const char* triple);

/**
 * @brief Generate machine code for the "Hello World" program.
 *
 * Uses LLVM to compile a function that writes "Hello!\\n" to stdout and exits.
 *
 * @param config The architecture configuration to use.
 * @param target_triple The full LLVM target triple string.
 * @param out Pointer to a MachineCode structure to populate.
 * @return 0 on success, non-zero on failure.
 */
int emit_machine_code(const ArchConfig* config,
                      const char* target_triple,
                      MachineCode* out);

/**
 * @brief Write the generated machine code to an ELF executable file.
 *
 * Creates a file named "elf" in the current directory.
 *
 * @param config The architecture configuration (used for ELF headers).
 * @param code The generated machine code to write into the .text section.
 * @return 0 on success, non-zero on failure.
 */
int write_elf_file(const ArchConfig* config, const MachineCode* code);

#endif /* ELF_CREATOR_H */
