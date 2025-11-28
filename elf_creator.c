#include <ctype.h>
#include <elf.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Object.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_OFFSET 0x78
#define HELLO_LEN 7

typedef struct
{
    const char *keyword;
    uint16_t e_machine;
    uint64_t base_vaddr;
    const char *write_asm;
    const char *write_constraints;
    const char *exit_asm;
    const char *exit_constraints;
} ArchConfig;

typedef struct
{
    uint8_t *bytes;
    size_t size;
} MachineCode;

static const ArchConfig ARCHES[] = {
    {
        .keyword = "x86_64",
        .e_machine = EM_X86_64,
        .base_vaddr = 0x400000,
        .write_asm =
            "mov $$1, %edi\n\t"
            "mov $$7, %edx\n\t"
            "mov $$1, %eax\n\t"
            "syscall\n\t",
        .write_constraints = "{rsi},~{rax},~{rdi},~{rdx},~{rcx},~{r11},~{memory}",
        .exit_asm =
            "xor %edi, %edi\n\t"
            "mov $$60, %eax\n\t"
            "syscall\n\t",
        .exit_constraints = "~{rax},~{rdi},~{rcx},~{r11},~{memory}",
    },
    {
        .keyword = "riscv64",
        .e_machine = EM_RISCV,
        .base_vaddr = 0x400000,
        .write_asm =
            "li a0, 1\n\t"
            "li a2, 7\n\t"
            "li a7, 64\n\t"
            "ecall\n\t",
        .write_constraints = "{a1},~{a0},~{a2},~{a7},~{memory}",
        .exit_asm =
            "mv a0, zero\n\t"
            "li a7, 93\n\t"
            "ecall\n\t",
        .exit_constraints = "~{a0},~{a7},~{memory}",
    },
};
static void initialize_llvm(void)
{
    static bool initialized = false;
    if (initialized)
    {
        return;
    }

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    initialized = true;
}

static bool contains_keyword(const char *triple, const char *keyword)
{
    size_t needle_len = strlen(keyword);
    for (const char *p = triple; *p; ++p)
    {
        size_t matched = 0;
        while (matched < needle_len && p[matched] && tolower((unsigned char)p[matched]) == tolower((unsigned char)keyword[matched]))
        {
            ++matched;
        }
        if (matched == needle_len)
        {
            return true;
        }
    }
    return false;
}

static const ArchConfig *select_arch_config(const char *triple)
{
    for (size_t i = 0; i < sizeof(ARCHES) / sizeof(ARCHES[0]); ++i)
    {
        if (contains_keyword(triple, ARCHES[i].keyword))
        {
            return &ARCHES[i];
        }
    }
    return NULL;
}

static LLVMValueRef build_store(LLVMBuilderRef builder, LLVMValueRef alloca_inst, LLVMTypeRef array_type, LLVMTypeRef byte_type, uint8_t value, uint32_t index, LLVMContextRef context)
{
    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
    LLVMValueRef idx = LLVMConstInt(LLVMInt32TypeInContext(context), index, 0);
    LLVMValueRef indices[2] = {zero, idx};
    LLVMValueRef ptr = LLVMBuildInBoundsGEP2(builder, array_type, alloca_inst, indices, 2, "byte_ptr");
    return LLVMBuildStore(builder, LLVMConstInt(byte_type, value, 0), ptr);
}
static int emit_machine_code(const ArchConfig *config, const char *target_triple, MachineCode *out)
{
    LLVMContextRef context = LLVMContextCreate();
    if (!context)
    {
        fprintf(stderr, "Failed to create LLVM context\n");
        return 1;
    }

    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("hello_module", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMTargetMachineRef target_machine = NULL;
    LLVMMemoryBufferRef object_buffer = NULL;
    LLVMObjectFileRef object_file = NULL;
    LLVMSectionIteratorRef sections = NULL;
    char *error = NULL;
    int status = 0;

    LLVMSetTarget(module, target_triple);

    LLVMTypeRef void_type = LLVMVoidTypeInContext(context);
    LLVMValueRef func = LLVMAddFunction(module, "bootstrap", LLVMFunctionType(void_type, NULL, 0, 0));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, func, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMTypeRef byte_type = LLVMInt8TypeInContext(context);
    LLVMTypeRef array_type = LLVMArrayType(byte_type, HELLO_LEN);
    LLVMValueRef hello_alloca = LLVMBuildAlloca(builder, array_type, "hello_buf");

    const uint8_t hello_bytes[HELLO_LEN] = {'H', 'e', 'l', 'l', 'o', '!', '\n'};
    for (uint32_t i = 0; i < HELLO_LEN; ++i)
    {
        (void)build_store(builder, hello_alloca, array_type, byte_type, hello_bytes[i], i, context);
    }

    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
    LLVMValueRef indices[2] = {zero, zero};
    LLVMValueRef hello_ptr = LLVMBuildInBoundsGEP2(builder, array_type, hello_alloca, indices, 2, "hello_ptr");
    LLVMTypeRef write_param_types[] = {LLVMPointerType(byte_type, 0)};
    LLVMTypeRef write_type = LLVMFunctionType(void_type, write_param_types, 1, 0);
    LLVMValueRef write_inline = LLVMConstInlineAsm(write_type, config->write_asm, config->write_constraints, true, false);
    LLVMBuildCall2(builder, write_type, write_inline, &hello_ptr, 1, "");

    LLVMTypeRef exit_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef exit_inline = LLVMConstInlineAsm(exit_type, config->exit_asm, config->exit_constraints, true, false);
    LLVMBuildCall2(builder, exit_type, exit_inline, NULL, 0, "");

    LLVMBuildRetVoid(builder);

    if (LLVMVerifyModule(module, LLVMAbortProcessAction, &error))
    {
        LLVMDisposeMessage(error);
        status = 1;
        goto cleanup;
    }

    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(target_triple, &target, &error))
    {
        fprintf(stderr, "%s\n", error);
        LLVMDisposeMessage(error);
        status = 1;
        goto cleanup;
    }

    target_machine = LLVMCreateTargetMachine(target, target_triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelSmall);
    if (!target_machine)
    {
        fprintf(stderr, "Failed to create target machine\n");
        status = 1;
        goto cleanup;
    }

    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    char *layout_str = LLVMCopyStringRepOfTargetData(data_layout);
    LLVMSetDataLayout(module, layout_str);
    LLVMDisposeMessage(layout_str);
    LLVMDisposeTargetData(data_layout);
    if (LLVMTargetMachineEmitToMemoryBuffer(target_machine, module, LLVMObjectFile, &error, &object_buffer))
    {
        fprintf(stderr, "%s\n", error);
        LLVMDisposeMessage(error);
        status = 1;
        goto cleanup;
    }

    object_file = LLVMCreateObjectFile(object_buffer);
    if (!object_file)
    {
        fprintf(stderr, "Failed to parse generated object file\n");
        status = 1;
        goto cleanup;
    }
    object_buffer = NULL;

    sections = LLVMGetSections(object_file);
    if (!sections)
    {
        fprintf(stderr, "Failed to iterate object file sections\n");
        status = 1;
        goto cleanup;
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
                fprintf(stderr, ".text section is empty\n");
                status = 1;
                goto cleanup;
            }
            const char *contents = LLVMGetSectionContents(sections);
            out->bytes = (uint8_t *)malloc(section_size);
            if (!out->bytes)
            {
                fprintf(stderr, "Failed to allocate memory for machine code\n");
                status = 1;
                goto cleanup;
            }
            memcpy(out->bytes, contents, section_size);
            out->size = section_size;
            found_text = true;
            break;
        }
    }

    if (!found_text)
    {
        fprintf(stderr, "Failed to locate .text section in generated object\n");
        status = 1;
    }

cleanup:
    if (sections)
    {
        LLVMDisposeSectionIterator(sections);
    }
    if (object_file)
    {
        LLVMDisposeObjectFile(object_file);
    }
    if (object_buffer)
    {
        LLVMDisposeMemoryBuffer(object_buffer);
    }
    if (target_machine)
    {
        LLVMDisposeTargetMachine(target_machine);
    }
    if (builder)
    {
        LLVMDisposeBuilder(builder);
    }
    if (module)
    {
        LLVMDisposeModule(module);
    }
    if (context)
    {
        LLVMContextDispose(context);
    }

    return status;
}

static int write_elf_file(const ArchConfig *config, const MachineCode *code)
{
    Elf64_Ehdr elf_hdr = {
        .e_ident = {
            ELFMAG0,
            ELFMAG1,
            ELFMAG2,
            ELFMAG3,
            ELFCLASS64,
            ELFDATA2LSB,
            EV_CURRENT,
            ELFOSABI_LINUX,
            0, 0, 0, 0, 0, 0, 0, 0},
        .e_type = ET_EXEC,
        .e_machine = config->e_machine,
        .e_version = EV_CURRENT,
        .e_entry = config->base_vaddr + TEXT_OFFSET,
        .e_phoff = sizeof(Elf64_Ehdr),
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(Elf64_Ehdr),
        .e_phentsize = sizeof(Elf64_Phdr),
        .e_phnum = 1,
        .e_shentsize = sizeof(Elf64_Shdr),
        .e_shnum = 0,
        .e_shstrndx = SHN_UNDEF,
    };

    Elf64_Phdr prgm_hdr = {
        .p_type = PT_LOAD,
        .p_offset = TEXT_OFFSET,
        .p_vaddr = config->base_vaddr + TEXT_OFFSET,
        .p_paddr = config->base_vaddr + TEXT_OFFSET,
        .p_filesz = code->size,
        .p_memsz = code->size,
        .p_flags = PF_X | PF_R,
        .p_align = 0x1000,
    };

    FILE *fptr = fopen("elf", "wb");
    if (!fptr)
    {
        perror("Failed to create/open the output elf file");
        return 1;
    }

    size_t written = fwrite(&elf_hdr, 1, sizeof(elf_hdr), fptr);
    if (written != sizeof(elf_hdr))
    {
        perror("Failed to write elf header into the output elf file");
        fclose(fptr);
        return 1;
    }

    written = fwrite(&prgm_hdr, 1, sizeof(prgm_hdr), fptr);
    if (written != sizeof(prgm_hdr))
    {
        perror("Failed to write program header into the output elf file");
        fclose(fptr);
        return 1;
    }

    written = fwrite(code->bytes, 1, code->size, fptr);
    if (written != code->size)
    {
        perror("Failed to write elf object code into the output elf file");
        fclose(fptr);
        return 1;
    }

    fclose(fptr);
    return 0;
}

int main(int argc, char **argv)
{
    const char *requested_triple = NULL;
    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--target=", 9) == 0)
        {
            requested_triple = argv[i] + 9;
        }
        else
        {
            fprintf(stderr, "Usage: %s [--target=<llvm-triple>]\n", argv[0]);
            return 1;
        }
    }

    initialize_llvm();

    char *default_triple = NULL;
    if (!requested_triple)
    {
        default_triple = LLVMGetDefaultTargetTriple();
        if (!default_triple)
        {
            fprintf(stderr, "Failed to determine host target triple\n");
            return 1;
        }
        requested_triple = default_triple;
    }

    char *triple_copy = strdup(requested_triple);
    if (!triple_copy)
    {
        fprintf(stderr, "Failed to allocate memory for target triple\n");
        if (default_triple)
        {
            LLVMDisposeMessage(default_triple);
        }
        return 1;
    }

    const ArchConfig *config = select_arch_config(triple_copy);
    if (default_triple)
    {
        LLVMDisposeMessage(default_triple);
    }

    if (!config)
    {
        fprintf(stderr, "Unsupported target triple: %s\n", triple_copy);
        free(triple_copy);
        return 1;
    }

    MachineCode code = {0};
    if (emit_machine_code(config, triple_copy, &code) != 0)
    {
        free(triple_copy);
        free(code.bytes);
        return 1;
    }

    int rc = write_elf_file(config, &code);
    free(code.bytes);
    free(triple_copy);

    return rc;
}
