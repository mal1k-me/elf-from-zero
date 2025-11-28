#!/usr/bin/env python3
"""
Placeholder generator for include/generated_arch_config.h.

This intentionally contains no architecture-specific knowledge. Run it to
materialize an empty header that keeps elf_creator.c happy while you work on
a smarter generator that can synthesize the missing bits dynamically.
"""

from __future__ import annotations

from pathlib import Path
from textwrap import dedent


def main() -> int:
    header = dedent(
        """\
        // Auto-generated placeholder.
        #ifndef ELF_ARCH_CONFIG_DEFINED
        #define ELF_ARCH_CONFIG_DEFINED 1

        static const ArchConfig ARCHES[] = {};

        #endif // ELF_ARCH_CONFIG_DEFINED
        """
    )

    Path("include").mkdir(exist_ok=True)
    Path("include/generated_arch_config.h").write_text(header)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())