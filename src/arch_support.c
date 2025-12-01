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
 * @brief Check if a string starts with a given prefix (case-insensitive).
 *
 * @brief Structure to hold a keyword string and its length.
 */
typedef struct {
    const char* text; /**< The keyword text. */
    size_t len;       /**< The length of the keyword. */
} KeywordStr;

/**
 * @brief Wrapper for tolower to reduce cognitive complexity from macro
 * expansion.
 */
static int safe_tolower(int char_code) {
    return tolower(char_code);
}

/**
 * @brief Check if a string starts with a given prefix (case-insensitive).
 *
 * @param str The string to check.
 * @param prefix The prefix to look for.
 * @return true if str starts with prefix, false otherwise.
 */
static bool starts_with_ignore_case(const char* str, KeywordStr prefix) {
    for (size_t i = 0; i < prefix.len; ++i) {
        unsigned char input_char = (unsigned char)str[i];
        unsigned char prefix_char = (unsigned char)prefix.text[i];

        if (input_char == '\0') {
            return false;
        }

        if (safe_tolower(input_char) != safe_tolower(prefix_char)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if a target triple contains a specific keyword
 * (case-insensitive).
 *
 * @param triple The LLVM target triple string.
 * @param keyword The keyword to search for (e.g., "x86_64").
 * @return true if the keyword is found, false otherwise.
 */
static bool contains_keyword(const char* triple, const char* keyword) {
    if (!triple || !keyword) {
        return false;
    }

    size_t keyword_len = strlen(keyword);
    KeywordStr prefix = {keyword, keyword_len};
    for (const char* cursor = triple; *cursor; ++cursor) {
        if (starts_with_ignore_case(cursor, prefix)) {
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
const ArchConfig* select_arch_config(const char* triple) {
    if (!triple) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ARCHES) / sizeof(ARCHES[0]); ++i) {
        if (contains_keyword(triple, ARCHES[i].keyword)) {
            return &ARCHES[i];
        }
    }
    return NULL;
}
