/**
 * @file arch_support.c
 * @brief Implementation of architecture selection logic.
 */

#include "elf_creator.h"

#include <stddef.h>
#include <string.h>

#include "generated_arch_config.h"

#ifndef ELF_ARCH_CONFIG_DEFINED
#error "generated_arch_config.h is missing; generate it before building."
#endif


/**
 * @brief Select the architecture configuration based on the target name.
 *
 * Iterates through the generated architecture catalog and returns the first
 * configuration whose keyword matches the provided target name.
 *
 * @param target_name The canonical LLVM target name (e.g., "x86-64").
 * @return Pointer to the matching ArchConfig, or NULL if not found.
 */
const ArchConfig* select_arch_config(const char* target_name) {
    if (!target_name) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ARCHES) / sizeof(ARCHES[0]); ++i) {
        if (strcmp(target_name, ARCHES[i].keyword) == 0) {
            return &ARCHES[i];
        }
    }
    return NULL;
}
