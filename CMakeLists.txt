cmake_minimum_required(VERSION 3.16)
project(ELF_from_zero C)

# ------------------------------------------------------------------------------
# Compiler Settings (C99 Standard)
# ------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM Include Dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM Lib Dir: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Find Python3 (for config generation script)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ------------------------------------------------------------------------------
# Project Configuration
# ------------------------------------------------------------------------------

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRC_FILES
    elf_creator.c
    src/arch_support.c
    src/elf_writer.c
    src/llvm_emit.c
    src/llvm_runtime.c
)

# ------------------------------------------------------------------------------
# Code Generation (Architecture Config)
# ------------------------------------------------------------------------------

set(GEN_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(GEN_HEADER ${GEN_HEADER_DIR}/generated_arch_config.h)
set(ARCH_CATALOG ${CMAKE_CURRENT_SOURCE_DIR}/data/arch_catalog.json)
set(GEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_arch_config.py)

add_custom_command(
    OUTPUT ${GEN_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT} --catalog ${ARCH_CATALOG} --output ${GEN_HEADER} --verbose
    DEPENDS ${GEN_SCRIPT} ${ARCH_CATALOG}
    COMMENT "Generating architecture configuration header..."
)

# ------------------------------------------------------------------------------
# Main Executable
# ------------------------------------------------------------------------------

add_executable(elf_creator ${SRC_FILES} ${GEN_HEADER})

# Map LLVM components to libraries
# 'native' ensures we link the backend for the host architecture
# We use llvm-config explicitly to handle monolithic vs split libraries correctly
find_program(LLVM_CONFIG_EXE NAMES llvm-config HINTS ${LLVM_TOOLS_BINARY_DIR})

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
set_target_properties(elf_creator PROPERTIES INSTALL_RPATH "${LLVM_LIBRARY_DIRS}")
set_target_properties(elf_creator PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)

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
# Note: The output directory 'docs' should be added to .gitignore
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    
    # Generate a Doxyfile programmatically to avoid needing a separate file in the repo
    set(DOXYFILE_CONTENT "
        PROJECT_NAME           = \"ELF From Zero\"
        PROJECT_BRIEF          = \"Educational ELF binary generator using LLVM\"
        OUTPUT_DIRECTORY       = \"${DOXY_OUTPUT_DIR}\"
        INPUT                  = \"${CMAKE_CURRENT_SOURCE_DIR}/src\" \"${CMAKE_CURRENT_SOURCE_DIR}/elf_creator.c\" \"${CMAKE_CURRENT_SOURCE_DIR}/include\"
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
    
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile "${DOXYFILE_CONTENT}")

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