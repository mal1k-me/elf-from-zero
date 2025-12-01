/**
 * @file elf_creator.c
 * @brief Main entry point for the ELF creation tool.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Support.h>
#include <llvm-c/TargetMachine.h>

#include "src/elf_creator.h"

/**
 * @brief Print usage information to stderr.
 *
 * @param prog_name The name of the program (argv[0]).
 */
static void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [--target=<llvm-triple>]\n", prog_name ? prog_name : "elf_creator");
}

/**
 * @brief Duplicate a string.
 *
 * @param input The string to duplicate.
 * @return A pointer to the new string, or NULL on failure.
 */
static char *dup_string(const char *input)
{
    if (!input)
    {
        return NULL;
    }

    size_t len = strlen(input);
    char *copy = (char *)malloc(len + 1);
    if (!copy)
    {
        return NULL;
    }

    memcpy(copy, input, len + 1);
    return copy;
}

/**
 * @brief Main function.
 *
 * Parses arguments, initializes LLVM, selects architecture configuration,
 * generates machine code, and writes the ELF file.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on failure.
 */
int main(int argc, char **argv)
{
    const char *requested_triple = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--target=", 9) == 0)
        {
            requested_triple = argv[i] + 9;
        }
        else
        {
            print_usage(argv[0]);
            return 1;
        }
    }

    initialize_llvm_targets();

    char *default_triple = NULL;
    if (!requested_triple)
    {
        default_triple = LLVMGetDefaultTargetTriple();
        if (!default_triple)
        {
            fprintf(stderr, "Failed to determine host target triple\n");
            return 1;
        }
        requested_triple = default_triple;
    }

    char *triple_copy = dup_string(requested_triple);
    if (!triple_copy)
    {
        fprintf(stderr, "Failed to copy target triple string\n");
        if (default_triple)
        {
            LLVMDisposeMessage(default_triple);
        }
        return 1;
    }

    const ArchConfig *config = select_arch_config(triple_copy);
    if (default_triple)
    {
        LLVMDisposeMessage(default_triple);
    }

    if (!config)
    {
        fprintf(stderr,
                "No architecture config matched triple '%s'.\n"
                "Ensure include/generated_arch_config.h contains entries for this target.\n",
                triple_copy);
        free(triple_copy);
        return 1;
    }

    printf("Detected Architecture: %s\n", config->keyword);
    printf("Target Triple:         %s\n", triple_copy);

    MachineCode machine_code = {0};
    if (emit_machine_code(config, triple_copy, &machine_code) != 0)
    {
        free(triple_copy);
        free(machine_code.bytes);
        return 1;
    }

    if (write_elf_file(config, &machine_code) != 0)
    {
        free(triple_copy);
        free(machine_code.bytes);
        return 1;
    }

    free(triple_copy);
    free(machine_code.bytes);

    return 0;
}