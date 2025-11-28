# ELF_from_zero
Create a minimal ELF executable from scratch while synthesizing machine code on the fly with LLVM so it stays lean across architectures.

## Current Status

The repository intentionally ships **no architecture-specific code**.  
`elf_creator.c` expects an `include/generated_arch_config.h`, but the stock generator is a placeholder that emits an empty definition. The final ELF will therefore lack working syscall thunks until you plug in your own arch-aware generator.

## Prerequisites
- LLVM development headers and libraries (`llvm-config` must be in `PATH`)
- A recent GCC/Clang toolchain and GNU binutils

## Workflow Overview

1. **Generate the arch config header (placeholder)**
   ```
   python3 tools/gen_arch_config.py
   ```
   After this step `include/generated_arch_config.h` exists but contains no per-arch data. Replace this generator with your own logic when you’re ready to supply real write/exit thunks.

2. **Compile `elf_creator`**
   ```
   clang $(llvm-config --cflags) elf_creator.c -o elf_creator \
     $(llvm-config --ldflags --libs core target native object --system-libs) \
     -Wl,-rpath,$(llvm-config --libdir)
   ```
   *(You can swap `clang` for `gcc`; just keep the `llvm-config` fragments.)*

3. **Emit the ELF payload**
   ```
   ./elf_creator [--target=<llvm-triple>]
   ```
   - By default the host LLVM triple is used (e.g., `x86_64-pc-linux-gnu`).
   - `--target=<triple>` lets you experiment with other backends, but without real syscall thunks the output remains incomplete.

4. **(Future) Run the produced binary**
   ```
   chmod +x elf
   ./elf
   ```
   Once you supply actual syscall machine code, the binary should print `hello!`.

## Next Steps

- Replace `tools/gen_arch_config.py` with a generator that scrapes architectural data (from LLVM, kernel headers, etc.) and emits real inline assembly.
- Re-run the steps above; the rest of the pipeline already builds the ELF skeleton.

Until that logic exists, `elf_creator` serves as a scaffold for your experiments.