/**
 * @file llvm_emit.c
 * @brief Implementation of LLVM IR generation and machine code emission.
 */

#include "elf_creator.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Object.h>
#include <llvm-c/TargetMachine.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Context structure holding LLVM objects for code generation.
 */
typedef struct
{
    LLVMContextRef context;       /**< Global LLVM context. */
    LLVMModuleRef module;         /**< LLVM module being built. */
    LLVMBuilderRef builder;       /**< IR builder. */
    LLVMTargetMachineRef machine; /**< Target machine for code generation. */
} LLVMEmitContext;

/**
 * @brief Helper to build a store instruction for a single byte in an array.
 *
 * @param builder The LLVM builder.
 * @param alloca_inst The alloca instruction for the array.
 * @param array_type The type of the array.
 * @param byte_type The type of a byte (i8).
 * @param value The byte value to store.
 * @param index The index in the array.
 * @param context The LLVM context.
 * @return The store instruction value.
 */
static LLVMValueRef build_store(LLVMBuilderRef builder,
                                LLVMValueRef alloca_inst,
                                LLVMTypeRef array_type,
                                LLVMTypeRef byte_type,
                                uint8_t value,
                                uint32_t index,
                                LLVMContextRef context)
{
    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
    LLVMValueRef idx = LLVMConstInt(LLVMInt32TypeInContext(context), index, 0);
    LLVMValueRef indices[2] = {zero, idx};
    LLVMValueRef ptr = LLVMBuildInBoundsGEP2(builder, array_type, alloca_inst, indices, 2, "byte_ptr");
    return LLVMBuildStore(builder, LLVMConstInt(byte_type, value, 0), ptr);
}

/**
 * @brief Clean up LLVM resources in the emit context.
 *
 * @param ctx Pointer to the context to dispose.
 */
static void dispose_emit_context(LLVMEmitContext *ctx)
{
    if (!ctx)
    {
        return;
    }
    if (ctx->builder)
    {
        LLVMDisposeBuilder(ctx->builder);
    }
    if (ctx->module)
    {
        LLVMDisposeModule(ctx->module);
    }
    if (ctx->machine)
    {
        LLVMDisposeTargetMachine(ctx->machine);
    }
    if (ctx->context)
    {
        LLVMContextDispose(ctx->context);
    }
}

/**
 * @brief Generate machine code for the configured architecture.
 *
 * This function performs the following steps:
 * 1. Initializes LLVM context, module, and builder.
 * 2. Configures the target machine based on the triple.
 * 3. Generates LLVM IR for a function that writes "Hello!\\n" and exits.
 * 4. Compiles the IR to an object file in memory.
 * 5. Extracts the raw machine code from the .text section.
 *
 * @param config Architecture configuration.
 * @param target_triple LLVM target triple.
 * @param out Output structure for machine code.
 * @return 0 on success, 1 on failure.
 */
int emit_machine_code(const ArchConfig *config, const char *target_triple, MachineCode *out)
{
    if (!config || !target_triple || !out)
    {
        return 1;
    }

    printf("Generating code for target: %s\n", target_triple);

    LLVMEmitContext ctx = {0};
    ctx.context = LLVMContextCreate();
    if (!ctx.context)
    {
        return 1;
    }

    ctx.module = LLVMModuleCreateWithNameInContext("elf_from_zero", ctx.context);
    if (!ctx.module)
    {
        dispose_emit_context(&ctx);
        return 1;
    }

    ctx.builder = LLVMCreateBuilderInContext(ctx.context);
    if (!ctx.builder)
    {
        dispose_emit_context(&ctx);
        return 1;
    }

    LLVMTargetRef target = NULL;
    char *error = NULL;
    if (LLVMGetTargetFromTriple(target_triple, &target, &error))
    {
        LLVMDisposeMessage(error);
        dispose_emit_context(&ctx);
        return 1;
    }

    ctx.machine = LLVMCreateTargetMachine(target, target_triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelSmall);
    if (!ctx.machine)
    {
        dispose_emit_context(&ctx);
        return 1;
    }

    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(ctx.machine);
    char *layout_str = LLVMCopyStringRepOfTargetData(data_layout);
    LLVMSetDataLayout(ctx.module, layout_str);
    LLVMDisposeMessage(layout_str);
    LLVMDisposeTargetData(data_layout);
    LLVMSetTarget(ctx.module, target_triple);

    LLVMTypeRef void_type = LLVMVoidTypeInContext(ctx.context);
    LLVMValueRef func = LLVMAddFunction(ctx.module, "bootstrap", LLVMFunctionType(void_type, NULL, 0, 0));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx.context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx.builder, entry);

    LLVMTypeRef byte_type = LLVMInt8TypeInContext(ctx.context);
    LLVMTypeRef array_type = LLVMArrayType(byte_type, HELLO_LEN);
    LLVMValueRef hello_alloca = LLVMBuildAlloca(ctx.builder, array_type, "hello_buf");

    const uint8_t hello_bytes[HELLO_LEN] = {'H', 'e', 'l', 'l', 'o', '!', '\n'};
    for (uint32_t i = 0; i < HELLO_LEN; ++i)
    {
        (void)build_store(ctx.builder, hello_alloca, array_type, byte_type, hello_bytes[i], i, ctx.context);
    }

    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx.context), 0, 0);
    LLVMValueRef indices[2] = {zero, zero};
    LLVMValueRef hello_ptr = LLVMBuildInBoundsGEP2(ctx.builder, array_type, hello_alloca, indices, 2, "hello_ptr");

    LLVMTypeRef write_param_types[] = {LLVMPointerType(byte_type, 0)};
    LLVMTypeRef write_type = LLVMFunctionType(void_type, write_param_types, 1, 0);
    LLVMValueRef write_inline = LLVMConstInlineAsm(write_type, config->write_asm, config->write_constraints, true, false);
    LLVMBuildCall2(ctx.builder, write_type, write_inline, &hello_ptr, 1, "");

    LLVMTypeRef exit_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef exit_inline = LLVMConstInlineAsm(exit_type, config->exit_asm, config->exit_constraints, true, false);
    LLVMBuildCall2(ctx.builder, exit_type, exit_inline, NULL, 0, "");

    LLVMBuildRetVoid(ctx.builder);

    if (LLVMVerifyModule(ctx.module, LLVMAbortProcessAction, &error))
    {
        LLVMDisposeMessage(error);
        dispose_emit_context(&ctx);
        return 1;
    }

    printf("Generated LLVM IR:\n");
    char *ir_str = LLVMPrintModuleToString(ctx.module);
    if (ir_str)
    {
        printf("%s\n", ir_str);
        LLVMDisposeMessage(ir_str);
    }

    LLVMMemoryBufferRef object_buffer = NULL;
    if (LLVMTargetMachineEmitToMemoryBuffer(ctx.machine, ctx.module, LLVMObjectFile, &error, &object_buffer))
    {
        LLVMDisposeMessage(error);
        dispose_emit_context(&ctx);
        return 1;
    }

    LLVMObjectFileRef object_file = LLVMCreateObjectFile(object_buffer);
    if (!object_file)
    {
        LLVMDisposeMemoryBuffer(object_buffer);
        dispose_emit_context(&ctx);
        return 1;
    }

    LLVMSectionIteratorRef sections = LLVMGetSections(object_file);
    if (!sections)
    {
        LLVMDisposeObjectFile(object_file);
        LLVMDisposeMemoryBuffer(object_buffer);
        dispose_emit_context(&ctx);
        return 1;
    }

    bool found_text = false;
    for (; !LLVMIsSectionIteratorAtEnd(object_file, sections); LLVMMoveToNextSection(sections))
    {
        const char *section_name = LLVMGetSectionName(sections);
        if (section_name && strcmp(section_name, ".text") == 0)
        {
            size_t section_size = (size_t)LLVMGetSectionSize(sections);
            if (section_size == 0)
            {
                break;
            }
            const char *contents = LLVMGetSectionContents(sections);
            out->bytes = (uint8_t *)malloc(section_size);
            if (!out->bytes)
            {
                break;
            }
            memcpy(out->bytes, contents, section_size);
            out->size = section_size;
            found_text = true;

            printf("Extracted .text section (%zu bytes):\n", section_size);
            for (size_t i = 0; i < section_size; ++i)
            {
                printf("%02x ", out->bytes[i]);
                if ((i + 1) % 16 == 0)
                    printf("\n");
            }
            printf("\n");

            break;
        }
    }

    LLVMDisposeSectionIterator(sections);
    LLVMDisposeObjectFile(object_file);
    dispose_emit_context(&ctx);

    return found_text ? 0 : 1;
}