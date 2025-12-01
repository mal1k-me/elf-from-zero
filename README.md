# ELF_from_zero

A minimal ELF binary generator using LLVM, built from scratch for educational purposes.

## Features

- **Multi-Architecture Support**: Generates code for x86, x86_64, RISC-V 64, ARM, AArch64, and MIPS.
- **LLVM Backend**: Uses LLVM for portable machine code generation.
- **Educational Codebase**:
  - C99 standard compliance.
  - Extensive Doxygen-style comments.
  - Strict type checking in Python tooling.
- **Automated Build**: CMake-based build system.

## Prerequisites

- **LLVM**: Development libraries (libllvm).
- **Clang**: C compiler.
- **Python 3**: For configuration generation.
- **CMake**: Version 3.16 or higher.
- **Mypy**: (Optional) For Python type checking.
- **Doxygen**: (Optional) For documentation generation.

## Building

1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake:
   ```bash
   cmake ..
   ```

3. Build the project:
   ```bash
   make
   ```

## Running

To run the creator and execute the generated ELF binary in one go:

```bash
make run_demo
```

Or manually:

```bash
./elf_creator
./elf
```

## Documentation

To generate HTML and LaTeX documentation:

```bash
make docs
```

To generate a PDF (requires LaTeX):

```bash
make docs_pdf
```

## Development

- **Type Checking**: Run `make check_types` to verify Python scripts with `mypy`.
- **Architecture Config**: Edit `data/arch_catalog.json` to add or modify architectures. The `tools/gen_arch_config.py` script generates the C header.
Create a minimal ELF executable from scratch while synthesizing machine code on the fly with LLVM so it stays lean across architectures.

## Current Status

The repository intentionally ships **no architecture-specific code**.  
`elf_creator.c` expects an `include/generated_arch_config.h`, but the stock generator is a placeholder that emits an empty definition. The final ELF will therefore lack working syscall thunks until you plug in your own arch-aware generator.

## Prerequisites
- LLVM development headers and libraries (`llvm-config` must be in `PATH`)
- A recent GCC/Clang toolchain and GNU binutils

## Source Layout

```
.
├── data/arch_catalog.json     # Placeholder map of architectures (fill this in)
├── elf_creator.c              # Thin CLI front-end
├── src/
│   ├── arch_support.c         # Resolves ArchConfig entries
│   ├── elf_creator.h          # Shared types/constants
│   ├── elf_writer.c           # Emits the ELF headers + payload
│   ├── llvm_emit.c            # Builds IR and extracts .text bytes
│   └── llvm_runtime.c         # Initializes LLVM targets
└── tools/gen_arch_config.py   # Reads data/arch_catalog.json → include/generated_arch_config.h
```

## Workflow Overview

1. **Generate the arch config header (catalog-driven placeholder)**
   ```
   python3 tools/gen_arch_config.py --catalog data/arch_catalog.json
   ```
   The default catalog lists every ELF target recognized by the linker layer but leaves the syscall fields empty. Populate those `asm` / `constraints` strings to make the build meaningful, then re-run the generator to emit fresh entries into `include/generated_arch_config.h`.

2. **Compile `elf_creator`**
   ```
   clang $(llvm-config --cflags) -Iinclude elf_creator.c src/*.c -o elf_creator \
     $(llvm-config --ldflags --libs core target native object --system-libs) \
     -Wl,-rpath,$(llvm-config --libdir)
   ```
   *(You can swap `clang` for `gcc`; just keep the `llvm-config` fragments and include every file under `src/`.)*

3. **Emit the ELF payload**
   ```
   ./elf_creator [--target=<llvm-triple>]
   ```
   - By default the host LLVM triple is used (e.g., `x86_64-pc-linux-gnu`).
   - `--target=<triple>` lets you experiment with other backends, but without real syscall thunks the output remains incomplete.

4. **(Future) Run the produced binary**
   ```
   ./elf
   ```
   Once you supply actual syscall machine code, the binary should print `hello!`.

## Next Steps

- Replace `tools/gen_arch_config.py` with a generator that scrapes architectural data (from LLVM, kernel headers, etc.) and emits real inline assembly.
- Re-run the steps above; the rest of the pipeline already builds the ELF skeleton.

Until that logic exists, `elf_creator` serves as a scaffold for your experiments.
