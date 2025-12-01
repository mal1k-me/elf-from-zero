#include "elf_creator.h"

#include <llvm-c/Target.h>

static bool g_initialized = false;

void initialize_llvm_targets(void) {
    if (g_initialized) {
        return;
    }

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    g_initialized = true;
}
