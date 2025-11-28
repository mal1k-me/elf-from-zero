#include "elf_creator.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "generated_arch_config.h"

#ifndef ELF_ARCH_CONFIG_DEFINED
#error "generated_arch_config.h is missing; generate it before building."
#endif

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