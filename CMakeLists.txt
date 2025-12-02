cmake_minimum_required(VERSION 3.16)

# Prefer Clang if available and no compiler is explicitly set
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    find_program(CLANG_EXE clang)
    if(CLANG_EXE)
        set(CMAKE_C_COMPILER ${CLANG_EXE})
    endif()
endif()

project(ELF_from_zero C)

# Generate compile_commands.json for editor integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM Include Dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM Lib Dir: ${LLVM_LIBRARY_DIRS}")

# Find Python3 (for config generation script)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ------------------------------------------------------------------------------
# Code Generation (Architecture Config)
# ------------------------------------------------------------------------------

set(GEN_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
set(GEN_HEADER ${GEN_HEADER_DIR}/generated_arch_config.h)
set(ARCH_CATALOG ${CMAKE_CURRENT_SOURCE_DIR}/data/arch_catalog.json)
set(GEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_arch_config.py)

# Allow user to specify target architecture
set(TARGET_ARCH "" CACHE STRING "Target architecture to generate (empty for host default)")

# Ensure the generated header directory exists before compilation
file(MAKE_DIRECTORY ${GEN_HEADER_DIR})

set(GEN_ARGS --catalog ${ARCH_CATALOG} --output ${GEN_HEADER} --verbose)
if(TARGET_ARCH)
    list(APPEND GEN_ARGS --arch "${TARGET_ARCH}")
endif()

add_custom_command(
    OUTPUT ${GEN_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT} ${GEN_ARGS}
    DEPENDS ${GEN_SCRIPT} ${ARCH_CATALOG}
    COMMENT "Generating architecture configuration header..."
)

# ------------------------------------------------------------------------------
# Main Executable Target
# ------------------------------------------------------------------------------

set(SRC_FILES
    elf_creator.c
    src/arch_support.c
    src/elf_writer.c
    src/llvm_emit.c
    src/llvm_runtime.c
)

add_executable(elf_creator ${SRC_FILES} ${GEN_HEADER})

# ------------------------------------------------------------------------------
# Compiler Settings (Target-Based)
# ------------------------------------------------------------------------------

set_target_properties(elf_creator PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Option for paranoid hardening
option(ENABLE_PARANOID_HARDENING "Enable strict security flags and sanitizers" OFF)

if(ENABLE_PARANOID_HARDENING)
    # Paranoid Warning Flags
    target_compile_options(elf_creator PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
        -Wconversion
        -Wshadow
        -Wunused
        -Wformat=2
        -Wcast-align
        -Wmissing-prototypes
        -Wstrict-prototypes
        -Wold-style-definition
        -Wfloat-equal
        -Wundef
    )

    # Handle implicit-fallthrough differences
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(elf_creator PRIVATE -Wimplicit-fallthrough=5)
    else()
        target_compile_options(elf_creator PRIVATE -Wimplicit-fallthrough)
    endif()

    # Security Hardening Flags (Compile)
    target_compile_options(elf_creator PRIVATE
        -O2
        -fstack-protector-strong
        -fPIE
        -fno-common
        -fno-strict-aliasing
    )

    # Enable compile-time buffer checks
    target_compile_definitions(elf_creator PRIVATE _FORTIFY_SOURCE=2)

    # Linker Security Flags
    target_link_options(elf_creator PRIVATE
        -pie
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
    )

    # Sanitizers
    target_compile_options(elf_creator PRIVATE -fsanitize=address,undefined,leak -fno-omit-frame-pointer)
    target_link_options(elf_creator PRIVATE -fsanitize=address,undefined,leak)
else()
    # Basic warnings for educational build
    target_compile_options(elf_creator PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------------------------
# Include Directories & Definitions
# ------------------------------------------------------------------------------

target_include_directories(elf_creator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(elf_creator PRIVATE ${LLVM_DEFINITIONS})

# ------------------------------------------------------------------------------
# Linking LLVM
# ------------------------------------------------------------------------------

# Map LLVM components to libraries
find_program(LLVM_CONFIG_EXE NAMES llvm-config HINTS ${LLVM_TOOLS_BINARY_DIR})

# Link directories for LLVM
link_directories(${LLVM_LIBRARY_DIRS})

if(LLVM_CONFIG_EXE)
    message(STATUS "Using llvm-config: ${LLVM_CONFIG_EXE}")
    execute_process(
        COMMAND ${LLVM_CONFIG_EXE} --libs --system-libs core support target native object analysis
        OUTPUT_VARIABLE LLVM_LDFLAGS_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Clean up output
    string(REPLACE "\n" " " LLVM_LDFLAGS_LIBS ${LLVM_LDFLAGS_LIBS})
    string(REPLACE " " ";" LLVM_LIBS_LIST ${LLVM_LDFLAGS_LIBS})

    target_link_libraries(elf_creator PRIVATE ${LLVM_LIBS_LIST})
else()
    message(WARNING "llvm-config not found, falling back to llvm_map_components_to_libnames")
    llvm_map_components_to_libnames(llvm_libs core support target native object analysis)
    target_link_libraries(elf_creator PRIVATE ${llvm_libs})
endif()

# Ensure RPATH is set so the executable can find LLVM shared libs
set_target_properties(elf_creator PROPERTIES
    INSTALL_RPATH "${LLVM_LIBRARY_DIRS}"
    BUILD_WITH_INSTALL_RPATH TRUE
)



# ------------------------------------------------------------------------------
# Custom Targets
# ------------------------------------------------------------------------------

# Target to run the creator and then execute the resulting ELF binary
add_custom_target(run_demo
    COMMAND ./elf_creator
    COMMAND echo "----------------------------------------"
    COMMAND echo "Executing generated ELF binary:"
    COMMAND ./elf
    DEPENDS elf_creator
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running elf_creator and executing the generated binary"
)

# Target to check Python type safety
find_program(MYPY_EXECUTABLE mypy)
if(MYPY_EXECUTABLE)
    add_custom_target(check_types
        COMMAND ${MYPY_EXECUTABLE} ${GEN_SCRIPT}
        COMMENT "Checking Python type hints with mypy"
    )
endif()

# ------------------------------------------------------------------------------
# Documentation (Doxygen)
# ------------------------------------------------------------------------------
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")

    # Create a Doxyfile.in content
    set(DOXYFILE_CONTENT "
PROJECT_NAME           = \"ELF From Zero\"
PROJECT_BRIEF          = \"Educational ELF binary generator using LLVM\"
OUTPUT_DIRECTORY       = \"@DOXY_OUTPUT_DIR@\"
INPUT                  = \"@CMAKE_CURRENT_SOURCE_DIR@/src\" \"@CMAKE_CURRENT_SOURCE_DIR@/elf_creator.c\" \"@CMAKE_CURRENT_SOURCE_DIR@/include\"
RECURSIVE              = YES
OPTIMIZE_OUTPUT_FOR_C  = YES
GENERATE_LATEX         = YES
USE_PDFLATEX           = YES
HAVE_DOT               = NO
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = YES
EXTRACT_STATIC         = YES
WARN_IF_UNDOCUMENTED   = NO
")

    # Write the template file
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in "${DOXYFILE_CONTENT}")

    # Use configure_file to generate the actual Doxyfile
    configure_file(
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )

    # Target to generate HTML and LaTeX source
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating HTML and LaTeX documentation"
    )

    # Target to compile LaTeX to PDF (requires pdflatex and make)
    add_custom_target(docs_pdf
        COMMAND ${CMAKE_MAKE_PROGRAM}
        WORKING_DIRECTORY "${DOXY_OUTPUT_DIR}/latex"
        DEPENDS docs
        COMMENT "Building PDF documentation from LaTeX"
    )

    message(STATUS "Doxygen targets added: 'docs' (HTML/LaTeX) and 'docs_pdf' (PDF)")
else()
    message(WARNING "Doxygen not found. Documentation targets will not be available.")
endif()
