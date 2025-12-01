/**
 * @file arch_support.c
 * @brief Implementation of architecture selection logic.
 */

#include "elf_creator.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "generated_arch_config.h"

#ifndef ELF_ARCH_CONFIG_DEFINED
#error "generated_arch_config.h is missing; generate it before building."
#endif

/**
 * @brief Check if a target triple contains a specific keyword (case-insensitive).
 *
 * @param triple The LLVM target triple string.
 * @param keyword The keyword to search for (e.g., "x86_64").
 * @return true if the keyword is found, false otherwise.
 */
static bool contains_keyword(const char *triple, const char *keyword)
{
    if (!triple || !keyword)
    {
        return false;
    }

    size_t keyword_len = strlen(keyword);
    for (const char *cursor = triple; *cursor; ++cursor)
    {
        size_t matched = 0;
        while (matched < keyword_len &&
               cursor[matched] &&
               tolower((unsigned char)cursor[matched]) == tolower((unsigned char)keyword[matched]))
        {
            ++matched;
        }
        if (matched == keyword_len)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Select the architecture configuration based on the target triple.
 *
 * Iterates through the generated architecture catalog and returns the first
 * configuration whose keyword matches the provided target triple.
 *
 * @param triple The LLVM target triple string.
 * @return Pointer to the matching ArchConfig, or NULL if not found.
 */
const ArchConfig *select_arch_config(const char *triple)
{
    if (!triple)
    {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ARCHES) / sizeof(ARCHES[0]); ++i)
    {
        if (contains_keyword(triple, ARCHES[i].keyword))
        {
            return &ARCHES[i];
        }
    }
    return NULL;
}