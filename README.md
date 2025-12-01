# ELF From Zero

A minimal, educational ELF binary generator built from scratch using C11 and LLVM.

![Standard](https://img.shields.io/badge/C-11-green.svg)
![Build](https://img.shields.io/badge/build-CMake-orange.svg)

**ELF From Zero** demonstrates how to construct an Executable and Linkable Format (ELF) file manually while leveraging the LLVM backend to synthesize machine code dynamically. It serves as a bridge between high-level compiler concepts and low-level systems programming, making it an ideal study resource for understanding binary formats, cross-compilation, and the LLVM C API.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Features & Security](#features--security)
3. [Supported Architectures](#supported-architectures)
4. [Prerequisites](#prerequisites)
5. [Building the Project](#building-the-project)
6. [Usage](#usage)
7. [Design Decisions](#design-decisions)
8. [Project Structure](#project-structure)
9. [Documentation](#documentation)

---

## Project Overview

The primary goal of this project is to demystify the creation of executable binaries. Instead of relying on a standard linker (`ld`), this tool:

1.  **Generates Machine Code**: Uses LLVM to compile a simple "Hello World" function into raw machine code for a specific target architecture.
2.  **Constructs ELF Headers**: Manually populates `Elf64_Ehdr` and `Elf64_Phdr` structs to create a valid executable container.
3.  **Injects Code**: Places the generated machine code into the `.text` segment of the ELF file.
4.  **Handles Syscalls**: Uses inline assembly (defined in a JSON catalog) to perform system calls (write, exit) across different architectures.

## Features & Security

This project adheres to **paranoid** coding standards to serve as a reference for secure C development:

*   **Strict C11 Compliance**: No non-standard extensions.
*   **Compiler Agnostic**: Supports both **Clang** (preferred) and **GCC**.
*   **Hardened Build**:
    *   Full Relocation Read-Only (`RELRO`) and Immediate Binding (`BIND_NOW`).
    *   Non-executable stack (`NX`).
    *   Position Independent Executable (`PIE`).
    *   Stack Canaries (`-fstack-protector-strong`).
    *   Compile-time buffer checks (`_FORTIFY_SOURCE=2`).
*   **Runtime Sanitizers**: Built with AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and LeakSanitizer (LSan) enabled by default.
*   **Static Analysis**: Verified with `clang-tidy` using strict configuration (zero warnings).

## Supported Architectures

The project currently supports code generation for the following architectures:

*   **x86_64** (AMD64)
*   **x86** (i386)
*   **RISC-V 64**
*   **AArch64** (ARM64)
*   **ARM** (32-bit)
*   **MIPS**

## Prerequisites

To build and run this project, you need the following tools installed on your system:

*   **C Compiler**: Clang (recommended) or GCC, supporting C11.
*   **CMake**: Version 3.16 or higher.
*   **LLVM Development Libraries**: `libllvm` (headers and libraries).
*   **Python 3**: For the configuration generator script.
*   **Make** or **Ninja**: Build system generator.

**Optional:**
*   **Doxygen**: For generating API documentation.
*   **LaTeX/pdfTeX**: For generating PDF documentation.
*   **Mypy**: For static type checking of Python scripts.

## Building the Project

We use CMake to manage the build configuration. This project employs "Modern CMake" practices, ensuring out-of-source builds and preventing source tree pollution.

1.  **Create a build directory:**
    ```bash
    mkdir build && cd build
    ```

2.  **Configure the project:**
    ```bash
    cmake ..
    ```
    *Note: CMake will prefer Clang if available. To force GCC, use `cmake -DCMAKE_C_COMPILER=gcc ..`*

3.  **Compile:**
    ```bash
    make
    ```

## Usage

### Running the Demo
To build the tool, generate an ELF binary for your host architecture, and execute it immediately:

```bash
make run_demo
```

### Manual Execution
You can also run the steps manually:

1.  **Run the creator:**
    ```bash
    ./elf_creator
    ```
    *Output:* A file named `elf` in the current directory.
    *Verbose output will show the detected architecture, generated IR, and machine code hex dump.*

2.  **Execute the generated binary:**
    ```bash
    ./elf
    ```
    *Output:* `Hello!`

### Cross-Compilation (Experimental)
You can attempt to generate code for a different target by passing the LLVM triple:
```bash
./elf_creator --target=riscv64-unknown-linux-gnu
```
*Note: You will need a compatible emulator (like QEMU) to run the resulting binary if it doesn't match your host architecture.*

## Design Decisions

### Why a JSON Catalog for Syscalls?

You might wonder why we manually define assembly strings in `data/arch_catalog.json` instead of using `libc` or asking LLVM to generate them.

1.  **Freestanding Environment**: We are building a binary "from zero," meaning no `libc` is linked. We must provide the raw system call instructions ourselves.
2.  **Cross-Architecture Support**: A local `libc` only supports the host architecture. To support RISC-V, ARM, and MIPS simultaneously without installing massive cross-compilation toolchains, we define the minimal required assembly (the "Micro-Libc") in a lightweight JSON format.
3.  **LLVM Limitations**: LLVM is a compiler backend, not an OS interface. It knows how to generate machine code, but it does not inherently know that Linux `write` is syscall `1` on x86_64 or `64` on RISC-V. This OS-specific knowledge must be supplied externally.

## Project Structure

The project follows a clean separation of concerns. Generated files are kept strictly within the build directory.

```text
ELF_from_zero/
├── CMakeLists.txt          # Main build configuration
├── README.md               # Project documentation
├── data/
│   └── arch_catalog.json   # JSON database of architecture syscalls
├── src/
│   ├── arch_support.c      # Architecture selection logic
│   ├── elf_creator.h       # Core definitions and API
│   ├── elf_writer.c        # ELF binary file construction
│   ├── llvm_emit.c         # LLVM IR generation and compilation
│   └── llvm_runtime.c      # LLVM initialization
├── elf_creator.c           # Main entry point
└── tools/
    └── gen_arch_config.py  # Python script to generate C config headers
```

## Documentation

The source code is extensively documented using Doxygen-style comments.

To generate the documentation locally:

*   **HTML**:
    ```bash
    make docs
    ```
    Open `build/docs/html/index.html` in your browser.

*   **PDF**:
    ```bash
    make docs_pdf
    ```
    The PDF will be available at `build/docs/latex/refman.pdf`.

### Development Tools

*   **Type Checking**: Run `make check_types` to verify Python scripts with `mypy`.
*   **Editor Integration**: Link `build/compile_commands.json` to your project root for LSP support.
