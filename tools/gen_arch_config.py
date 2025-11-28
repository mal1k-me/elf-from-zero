#!/usr/bin/env python3
"""
Generate include/generated_arch_config.h from a JSON catalog.

The catalog is intentionally empty in this milestone; once you populate it with
real per-architecture syscall thunks the resulting header will immediately
reflect those entries.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from textwrap import dedent


DEFAULT_OUTPUT = "include/generated_arch_config.h"
DEFAULT_CATALOG = "data/arch_catalog.json"


def encode_c_string(value: str) -> str:
    """Return a best-effort C string literal fragment."""
    if value is None:
        return ""
    return value.replace("\\", "\\\\").replace('"', '\\"')


def load_catalog(path: Path) -> list[dict]:
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text())
    except json.JSONDecodeError:
        return []
    arches = data.get("arches")
    if not isinstance(arches, list):
        return []
    normalized = []
    for entry in arches:
        if not isinstance(entry, dict):
            continue
        normalized.append(entry)
    return normalized


def entry_has_real_asm(entry: dict) -> bool:
    write = entry.get("write") or {}
    exit_block = entry.get("exit") or {}
    return bool(write.get("asm")) and bool(exit_block.get("asm"))


def render_placeholder() -> str:
    return dedent(
        """\
        // Auto-generated placeholder. Populate data/arch_catalog.json to emit real entries.
        #ifndef ELF_ARCH_CONFIG_DEFINED
        #define ELF_ARCH_CONFIG_DEFINED 1

        static const ArchConfig ARCHES[] = {};

        #endif // ELF_ARCH_CONFIG_DEFINED
        """
    )


def render_entry(entry: dict) -> str:
    keyword = encode_c_string(entry.get("keyword", "unknown"))
    e_machine = entry.get("e_machine", "EM_NONE")
    base_vaddr = entry.get("base_vaddr", "0x400000")

    write = entry.get("write", {})
    exit_block = entry.get("exit", {})

    write_asm = encode_c_string(write.get("asm", ""))
    write_constraints = encode_c_string(write.get("constraints", ""))
    exit_asm = encode_c_string(exit_block.get("asm", ""))
    exit_constraints = encode_c_string(exit_block.get("constraints", ""))

    return dedent(
        f"""\
            {{
                .keyword = "{keyword}",
                .e_machine = {e_machine},
                .base_vaddr = {base_vaddr},
                .write_asm =
                    "{write_asm}",
                .write_constraints = "{write_constraints}",
                .exit_asm =
                    "{exit_asm}",
                .exit_constraints = "{exit_constraints}",
            }}"""
    )


def render_header(entries: list[dict], catalog_path: Path) -> str:
    if not entries:
        return render_placeholder()

    body = ",\n".join(render_entry(entry) for entry in entries)

    return dedent(
        f"""\
        // Auto-generated from {catalog_path}. Do not edit by hand.
        #ifndef ELF_ARCH_CONFIG_DEFINED
        #define ELF_ARCH_CONFIG_DEFINED 1

        static const ArchConfig ARCHES[] = {{
{body}
        }};

        #endif // ELF_ARCH_CONFIG_DEFINED
        """
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit include/generated_arch_config.h from a JSON catalog.")
    parser.add_argument("--catalog", default=DEFAULT_CATALOG, help="Path to the architecture catalog JSON file.")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Destination header path.")
    args = parser.parse_args()

    catalog_path = Path(args.catalog)
    entries = load_catalog(catalog_path)
    initial_count = len(entries)
    entries = [entry for entry in entries if entry_has_real_asm(entry)]
    print(f"[gen_arch_config] catalog entries={initial_count}, emitting={len(entries)}")

    header_text = render_header(entries, catalog_path)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())