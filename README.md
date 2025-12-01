# ELF From Zero

A minimal, educational ELF binary generator built from scratch using C99 and LLVM.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Standard](https://img.shields.io/badge/C-99-green.svg)
![Build](https://img.shields.io/badge/build-CMake-orange.svg)

**ELF From Zero** demonstrates how to construct an Executable and Linkable Format (ELF) file manually while leveraging the LLVM backend to synthesize machine code dynamically. It serves as a bridge between high-level compiler concepts and low-level systems programming, making it an ideal study resource for understanding binary formats, cross-compilation, and the LLVM C API.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Supported Architectures](#supported-architectures)
3. [Technical Architecture](#technical-architecture)
4. [Prerequisites](#prerequisites)
5. [Building the Project](#building-the-project)
6. [Usage](#usage)
7. [Project Structure](#project-structure)
8. [Documentation](#documentation)
9. [Development](#development)

---

## Project Overview

The primary goal of this project is to demystify the creation of executable binaries. Instead of relying on a standard linker (`ld`), this tool:

1.  **Generates Machine Code**: Uses LLVM to compile a simple "Hello World" function into raw machine code for a specific target architecture.
2.  **Constructs ELF Headers**: Manually populates `Elf64_Ehdr` and `Elf64_Phdr` structs to create a valid executable container.
3.  **Injects Code**: Places the generated machine code into the `.text` segment of the ELF file.
4.  **Handles Syscalls**: Uses inline assembly (defined in a JSON catalog) to perform system calls (write, exit) across different architectures.

This approach highlights the essential components required to run code on a Linux system without the overhead of standard libraries (libc).

## Supported Architectures

The project currently supports code generation for the following architectures:

*   **x86_64** (AMD64)
*   **x86** (i386)
*   **RISC-V 64**
*   **AArch64** (ARM64)
*   **ARM** (32-bit)
*   **MIPS**

## Technical Architecture

The build process and runtime flow are designed to be modular and extensible.

### 1. Configuration Generation
*   **Source**: `data/arch_catalog.json` contains the architecture-specific inline assembly for system calls (e.g., `syscall` on x86_64, `ecall` on RISC-V).
*   **Tool**: `tools/gen_arch_config.py` reads this catalog. It is a strictly typed Python script that validates the data and generates a C header file.
*   **Output**: `include/generated_arch_config.h` provides a static lookup table used by the C runtime.

### 2. Machine Code Synthesis (LLVM)
*   The C program initializes the LLVM core and target subsystems.
*   It constructs a simple LLVM IR module containing a `bootstrap` function.
*   Architecture-specific assembly (from the generated header) is injected via LLVM's inline assembly support.
*   The module is compiled to an in-memory object file, and the `.text` section is extracted.

### 3. ELF Construction
*   The program calculates the necessary offsets and virtual addresses.
*   It writes the ELF Header, Program Header, and the extracted machine code to a binary file named `elf`.
*   Finally, it sets the executable permission bits (`0755`).

## Design Decisions

### Why a JSON Catalog for Syscalls?

You might wonder why we manually define assembly strings in `data/arch_catalog.json` instead of using `libc` or asking LLVM to generate them.

1.  **Freestanding Environment**: We are building a binary "from zero," meaning no `libc` is linked. We must provide the raw system call instructions ourselves.
2.  **Cross-Architecture Support**: A local `libc` only supports the host architecture. To support RISC-V, ARM, and MIPS simultaneously without installing massive cross-compilation toolchains, we define the minimal required assembly (the "Micro-Libc") in a lightweight JSON format.
3.  **LLVM Limitations**: LLVM is a compiler backend, not an OS interface. It knows how to generate machine code, but it does not inherently know that Linux `write` is syscall `1` on x86_64 or `64` on RISC-V. This OS-specific knowledge must be supplied externally.

## Prerequisites

To build and run this project, you need the following tools installed on your system:

*   **C Compiler**: GCC or Clang (supporting C99).
*   **CMake**: Version 3.16 or higher.
*   **LLVM Development Libraries**: `libllvm` (headers and libraries).
*   **Python 3**: For the configuration generator script.
*   **Make** or **Ninja**: Build system generator.

**Optional:**
*   **Doxygen**: For generating API documentation.
*   **LaTeX/pdfTeX**: For generating PDF documentation.
*   **Mypy**: For static type checking of Python scripts.

## Building the Project

We use CMake to manage the build configuration.

1.  **Create a build directory:**
    ```bash
    mkdir build && cd build
    ```

2.  **Configure the project:**
    ```bash
    cmake ..
    ```
    *Note: CMake will automatically detect your LLVM installation using `llvm-config`.*

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

1.  **Generate the ELF creator tool:**
    (Assuming you have run `make` inside `build/`)

2.  **Run the creator:**
    ```bash
    ./elf_creator
    ```
    *Output:* A file named `elf` in the current directory.
    *Verbose output will show the detected architecture, generated IR, and machine code hex dump.*

3.  **Execute the generated binary:**
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

## Project Structure

```text
ELF_from_zero/
├── CMakeLists.txt          # Main build configuration
├── README.md               # Project documentation
├── data/
│   └── arch_catalog.json   # JSON database of architecture syscalls
├── include/
│   └── (generated headers) # Output directory for gen_arch_config.py
├── src/
│   ├── arch_support.c      # Architecture selection logic
│   ├── elf_creator.h       # Core definitions and API
│   ├── elf_writer.c        # ELF binary file construction
│   ├── llvm_emit.c         # LLVM IR generation and compilation
│   └── llvm_runtime.c      # (Reserved for future runtime support)
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

## Development

### Python Type Checking
Ensure the tooling remains robust by running static type checks:
```bash
make check_types
```

### Editor Integration
The build system generates a `compile_commands.json` file in the build directory. You can link this to your project root to enable full IntelliSense/LSP support in editors like VS Code (with clangd) or CLion.

```bash
ln -s build/compile_commands.json .
```
