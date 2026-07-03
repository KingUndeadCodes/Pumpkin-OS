#include "elf.h"
#include <stdint.h>
#include <stdarg.h>
#include "../context/setjmp.h"
#include "../serial/serial.h"

// This needs to be worked on.

static inline Elf32_Shdr *elf_sheader(Elf32_Ehdr *hdr) {
    return (Elf32_Shdr *)((uint8_t *)hdr + (uintptr_t)hdr->e_shoff);
}

static inline Elf32_Shdr *elf_section(Elf32_Ehdr *hdr, uint32_t idx) {
    return &elf_sheader(hdr)[idx];
}

static inline char *elf_str_table(Elf32_Ehdr *hdr) {
    if (hdr->e_shstrndx == SHN_UNDEF) return NULL;
    Elf32_Shdr *s = elf_section(hdr, hdr->e_shstrndx);
    /* If the section was moved to memory, prefer sh_addr; otherwise use file offset. */
    if (s->sh_offset != 0) return (char *)hdr + s->sh_offset;
    return (char *)(uintptr_t)s->sh_addr;
}

static inline char *elf_lookup_string(Elf32_Ehdr *hdr, int offset) {
    char *strtab = elf_str_table(hdr);
    if (strtab == NULL) return NULL;
    return strtab + offset;
}

/* helper: return pointer to section data in-memory (either inside file buffer or allocated sh_addr) */
static inline void *elf_section_data(Elf32_Ehdr *hdr, Elf32_Shdr *section) {
    if (section->sh_type == SHT_NOBITS) {
        /* NOBITS won't be in file: should be allocated and pointed by sh_addr */
        return (void *)(uintptr_t)section->sh_addr;
    }
    if (section->sh_offset != 0) {
        return (void *)((uint8_t *)hdr + section->sh_offset);
    }
    /* fallback to sh_addr if sh_offset is zero */
    return (void *)(uintptr_t)section->sh_addr;
}

void *elf_lookup_symbol(const char *name) {
    // TODO : Implement a symbol lookup table
    return NULL;
}

static int elf_get_symval(Elf32_Ehdr *hdr, int table, uint32_t idx) {
    if (table == SHN_UNDEF || idx == SHN_UNDEF) return 0;

    Elf32_Shdr *symtab = elf_section(hdr, table);
    if (!symtab) {
        printf_serial(false, FAIL, "ERROR: symtab section %d not found\n", table);
        return ELF_RELOC_ERR;
    }
    if (symtab->sh_entsize == 0) {
        printf_serial(false, FAIL, "ERROR: symtab section %d has sh_entsize == 0\n", table);
        return ELF_RELOC_ERR;
    }

    uint32_t symtab_entries = symtab->sh_size / symtab->sh_entsize;
    if (idx >= symtab_entries) {
        printf_serial(false, FAIL, "Symbol Index out of Range (%d:%u).\n", table, idx);
        return ELF_RELOC_ERR;
    }

    /* get pointer to symbol table data (file-backed or allocated) */
    Elf32_Sym *symbols = (Elf32_Sym *)elf_section_data(hdr, symtab);
    if (!symbols) {
        printf_serial(false, FAIL, "ERROR: symtab data for section %d is NULL\n", table);
        return ELF_RELOC_ERR;
    }
    Elf32_Sym *symbol = &symbols[idx];

    if (symbol->st_shndx == SHN_UNDEF) {
        /* External symbol: look up in linked string table and call elf_lookup_symbol */
        Elf32_Shdr *strtab = elf_section(hdr, symtab->sh_link);
        if (!strtab) {
            printf_serial(false, FAIL, "ERROR: symtab linked strtab %u missing\n", symtab->sh_link);
            return ELF_RELOC_ERR;
        }
        const char *strbase = (const char *)elf_section_data(hdr, strtab);
        if (!strbase) {
            printf_serial(false, FAIL, "ERROR: strtab data is NULL for section %u\n", symtab->sh_link);
            return ELF_RELOC_ERR;
        }
        const char *name = strbase + symbol->st_name;

        void *target = elf_lookup_symbol(name);
        if (target == NULL) {
            /* Not found */
            if ((ELF32_ST_BIND(symbol->st_info) & STB_WEAK)) {
                return 0; /* weak symbol resolves to 0 */
            } else {
                printf_serial(false, FAIL, "Undefined External Symbol : %s.\n", name);
                return ELF_RELOC_ERR;
            }
        } else {
            return (int)(intptr_t)target;
        }
    } else if (symbol->st_shndx == SHN_ABS) {
        /* absolute symbol: value is the absolute value */
        return (int)symbol->st_value;
    } else {
        /* internal symbol: locate referenced section and compute runtime address */
        Elf32_Shdr *target = elf_section(hdr, symbol->st_shndx);
        if (!target) {
            printf_serial(false, FAIL, "ERROR: symbol references invalid section %u\n", symbol->st_shndx);
            return ELF_RELOC_ERR;
        }
        void *target_base = elf_section_data(hdr, target);
        if (!target_base) {
            printf_serial(false, FAIL, "ERROR: target section %u has no data\n", symbol->st_shndx);
            return ELF_RELOC_ERR;
        }
        /* symbol->st_value is the offset inside the section */
        return (int)((uintptr_t)target_base + (uintptr_t)symbol->st_value);
    }
}

/* Find a defined (non-external) symbol by name and return its resolved
 * runtime address. Used to locate "_start" since e_entry is 0/unreliable
 * for ET_REL objects. */
static void *elf_find_symbol_addr(Elf32_Ehdr *hdr, const char *name) {
    Elf32_Shdr *shdr = elf_sheader(hdr);

    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        Elf32_Shdr *section = &shdr[i];
        if (section->sh_type != SHT_SYMTAB) continue;
        if (section->sh_entsize == 0) continue;

        Elf32_Shdr *strtab = elf_section(hdr, section->sh_link);
        const char *strbase = strtab ? (const char *)elf_section_data(hdr, strtab) : NULL;
        if (!strbase) continue;

        Elf32_Sym *symbols = (Elf32_Sym *)elf_section_data(hdr, section);
        if (!symbols) continue;

        uint32_t count = section->sh_size / section->sh_entsize;
        for (uint32_t s = 0; s < count; s++) {
            Elf32_Sym *symbol = &symbols[s];
            if (symbol->st_shndx == SHN_UNDEF) continue; // not defined in this file

            const char *symname = strbase + symbol->st_name;
            if (strcmp(symname, name) != 0) continue;

            if (symbol->st_shndx == SHN_ABS) {
                return (void *)(uintptr_t)symbol->st_value;
            }

            Elf32_Shdr *target = elf_section(hdr, symbol->st_shndx);
            void *target_base = target ? elf_section_data(hdr, target) : NULL;
            if (!target_base) continue;
            return (void *)((uintptr_t)target_base + (uintptr_t)symbol->st_value);
        }
    }

    return NULL;
}

// ...existing code...
static int elf_load_stage1(Elf32_Ehdr *hdr) {
    Elf32_Shdr *shdr = elf_sheader(hdr);
    unsigned int i;

    for (i = 0; i < hdr->e_shnum; i++) {
        Elf32_Shdr *section = &shdr[i];

        if (section->sh_type == SHT_NOBITS) {
            if (!section->sh_size) continue;
            if (section->sh_flags & SHF_ALLOC) {
                void *mem = malloc(section->sh_size);
                if (!mem) {
                    printf_serial(false, FAIL, "ERROR: malloc failed allocating %u bytes for section %u\n",
                           (unsigned)section->sh_size, i);
                    return ELF_RELOC_ERR;
                }
                memset(mem, 0, section->sh_size);

                /* store runtime pointer in sh_addr and clear sh_offset to indicate no file data */
                section->sh_addr = (Elf32_Addr)(uintptr_t)mem;
                section->sh_offset = 0;
                printf_serial(false, INFO, "DEBUG: Allocated %u bytes for section %u at %p.\n",
                       (unsigned)section->sh_size, i, mem);
            }
        }
    }
    return 0;
}
// ...existing code...

static int elf_do_reloc(Elf32_Ehdr *hdr, Elf32_Rel *rel, Elf32_Shdr *reltab_section) {
    if (!reltab_section) {
        printf_serial(false, FAIL, "ERROR: reltab_section is NULL\n");
        return ELF_RELOC_ERR;
    }
    Elf32_Shdr *target_section = elf_section(hdr, reltab_section->sh_info);
    if (!target_section) {
        printf_serial(false, FAIL, "ERROR: target section (sh_info=%u) missing\n", (unsigned)reltab_section->sh_info);
        return ELF_RELOC_ERR;
    }

    uint8_t *base = (uint8_t *)elf_section_data(hdr, target_section);
    if (!base) {
        printf_serial(false, FAIL, "ERROR: target section %u has no base pointer\n", (unsigned)reltab_section->sh_info);
        return ELF_RELOC_ERR;
    }

    /* sanity check offset */
    if ((uintptr_t)rel->r_offset + sizeof(int) > (uintptr_t)target_section->sh_size) {
        printf_serial(false, FAIL, "ERROR: relocation offset out of bounds (off=%u secsize=%u)\n",
               (unsigned)rel->r_offset, (unsigned)target_section->sh_size);
        return ELF_RELOC_ERR;
    }

    int *ref = (int *)(base + rel->r_offset);

    int symval = 0;
    if (ELF32_R_SYM(rel->r_info) != 0) {
        symval = elf_get_symval(hdr, reltab_section->sh_link, ELF32_R_SYM(rel->r_info));
        if (symval == ELF_RELOC_ERR) return ELF_RELOC_ERR;
    }

    switch (ELF32_R_TYPE(rel->r_info)) {
        case R_386_NONE:
            break;
        case R_386_32:
            *ref = DO_386_32(symval, *ref);
            break;
        case R_386_PC32: {
            /* compute the address of the relocated word as an integer */
            int ref_addr = (int)(intptr_t)ref;
            *ref = DO_386_PC32(symval, *ref, ref_addr);
            break;
        }
        default:
            printf_serial(false, FAIL, "Unsupported Relocation Type (%u).\n", (unsigned)ELF32_R_TYPE(rel->r_info));
            return ELF_RELOC_ERR;
    }
    return symval;
}


static int elf_load_stage2(Elf32_Ehdr *hdr) {
    Elf32_Shdr *shdr = elf_sheader(hdr);

    unsigned int i, idx;
    for (i = 0; i < hdr->e_shnum; i++) {
        /*
        char* string = (char*)malloc(50);
        sprintf_serial(false, FAIL, string, "DEBUG: Processing section %u/%u\n", i + 1, hdr->e_shnum);
        serial_write_string(string);
        free(string);
        */
		// printf_serial(false, FAIL, "DEBUG: Processing section %u/%u\n", i + 1, hdr->e_shnum);
        Elf32_Shdr *section = &shdr[i];

        if (section->sh_type == SHT_REL) {
            if (section->sh_entsize == 0) {
                printf_serial(false, FAIL, "ERROR: relocation section %u has sh_entsize == 0\n", i);
                return ELF_RELOC_ERR;
            }
            uint32_t entries = section->sh_size / section->sh_entsize;

            /* get pointer to the relocation table (file-backed or in-memory) */
            Elf32_Rel *rel_table = (Elf32_Rel *)elf_section_data(hdr, section);
            if (!rel_table) {
                printf_serial(false, FAIL, "ERROR: could not get relocation table pointer for section %u\n", i);
                return ELF_RELOC_ERR;
            }

            for (idx = 0; idx < entries; idx++) {
                Elf32_Rel *rel = &rel_table[idx];
                int result = elf_do_reloc(hdr, rel, section);
                if (result == ELF_RELOC_ERR) {
                    printf_serial(false, FAIL, "Failed to relocate symbol (rel idx %u in section %u).\n", idx, i);
                    return ELF_RELOC_ERR;
                }
            }
        }
    }
    return 0;
}

/*
void *load_segment_to_memory(void *mem, Elf64_Phdr *phdr, int elf_fd) {
    size_t mem_size = phdr->p_memsz;
    off_t mem_offset = phdr->p_offset;
    size_t file_size = phdr->p_filesz;
    void *vaddr = (void *)(phdr->p_vaddr);
    // mmap the memory region with the correct protections
    int prot = 0;
    if (phdr->p_flags & PF_R) prot |= PROT_READ;
    if (phdr->p_flags & PF_W) prot |= PROT_WRITE;
    if (phdr->p_flags & PF_X) prot |= PROT_EXEC;

    off_t page_offset = (uint64_t)vaddr % PAGE_SIZE;
    void *aligned_vaddr = (void *)((uint64_t)vaddr - page_offset);
    size_t aligned_size = mem_size + page_offset;

    mmap(aligned_vaddr, file_size + page_offset, prot, MAP_PRIVATE | MAP_ANONYMOUS, fd, mem_offset - page_offset);
    // technically we can just have vaddr as the first argument as mmap will 
    // automatically truncate to the start of the page
    if (mem_size > file_size) {
        void *page_break = ((uint64_t)vaddr + mem_offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        memset((uint64_t)vaddr + file_size, 0, (uint64_t)page_break - (uint64_t)vaddr - file_size);
        if (mem_size > page_break - (uint64_t)vaddr) {
            mmap(page_break, mem_size - (page_break - (uint64_t)vaddr), flags, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            memset(page_break, 0, mem_size - (page_break - (uint64_t)vaddr));
        }
    }
}
*/

void assert(bool condition, const char* message, bool* error) {
    if (!condition) {
        printf_serial(false, FAIL, "Assertion failed: %s\n", message);
        *error = true;
        return;
    }
}

bool elf_check_file(const Elf32_Ehdr* ehdr) {
    bool error = false;
    assert((ehdr->e_ident[0] == 0x7F), "Not a valid ELF file (invalid magic number)", &error);
    assert((ehdr->e_ident[1] == 'E'), "Not a valid ELF file (expected character 'E')", &error);
    assert((ehdr->e_ident[2] == 'L'), "Not a valid ELF file (expected character 'L')", &error);
    assert((ehdr->e_ident[3] == 'F'), "Not a valid ELF file (expected character 'F')", &error);
    return !error;
};

bool elf_check_supported(const Elf32_Ehdr* ehdr) {
    bool error = false;
    if (elf_check_file(ehdr) == false) return false;
    assert((ehdr->e_ident[4] == 0x01), "Unsupported, Not a 32-bit ELF file", &error);
    assert((ehdr->e_ident[5] == 0x01), "Unsupported, Not a little-endian ELF file", &error);
    assert((ehdr->e_ident[6] == 0x01), "Unsupported, Foreign ELF version", &error);
    assert((ehdr->e_machine == ELFARCH_X86), "Unsupported, Not a valid architecture (non-x86 architecture detected)", &error);
	return !error;
};

static inline void *elf_load_rel(Elf32_Ehdr *hdr) {
	int result;
    result = elf_load_stage1(hdr);
    if(result == ELF_RELOC_ERR) {
		printf_serial(false, FAIL, "%s", "Unable to load ELF file.\n");
		return NULL;
	}
	result = elf_load_stage2(hdr);
	if(result == ELF_RELOC_ERR) {
		printf_serial(false, FAIL, "%s", "Unable to load ELF file.\n");
		return NULL;
	}
	// TODO : Parse the program header (if present)
	void *entry = elf_find_symbol_addr(hdr, "_start");
	if (!entry && hdr->e_entry != 0) {
		entry = (void*)hdr->e_entry;
	}
	if (!entry) {
		printf_serial(false, FAIL, "%s", "ELF: could not resolve _start entry point.\n");
	}
	return entry;
}

void* elf_load_file(void* b) {
	// Validate the ELF header
	// Elf32_Ehdr* ehdr = (Elf32_Ehdr*)b;
	Elf32_Ehdr* ehdr = (Elf32_Ehdr*)b;
    if (!elf_check_supported(ehdr)) {
        printf_serial(false, FAIL, "%s", "ELF cannot be loaded.\n");
        return NULL;
    }
    switch (ehdr->e_type) {
        case ET_EXEC: return NULL;
        case ET_REL: return elf_load_rel(ehdr);
    }
    // Load each program header
    return NULL;
};

static jmp_buf elf_exit_env;
static int elf_exit_code;
static bool elf_running = false;

/* Called by sys_exit to terminate the currently running loaded program.
 * There's no separate process context to unwind here (ring 0, shared
 * stack), so we longjmp back to the setjmp captured in elf_run(). This skips
 * syscall_handler's iretd, which is what would normally restore EFLAGS/IF
 * from the int 0x80 interrupt gate clearing it on entry — without an
 * explicit sti here, interrupts would stay disabled system-wide forever. */
void elf_exit(int code) {
    if (!elf_running) return;
    elf_exit_code = code;
    asm volatile("sti");
    longjmp(elf_exit_env, 1);
}

int elf_run(void *entry_point) {
    if (!entry_point) return -1;
    // elf_exit_env is a single shared buffer; a second, re-entrant elf_run()
    // call (e.g. clicking a .elf again while one is still blocked on stdin)
    // would clobber the first one's saved jump target and corrupt its
    // eventual sys_exit longjmp. Refuse to nest.
    if (elf_running) {
        printf_serial(false, FAIL, "%s", "ELF: a program is already running.\n");
        return -1;
    }

    elf_running = true;
    if (setjmp(elf_exit_env) == 0) {
        ((void (*)(void))entry_point)();
        elf_exit_code = 0; // fell off the end without an exit syscall
    }
    elf_running = false;

    return elf_exit_code;
}

/*
int main(int argc, char** argv) {
    if (argc < 2) {
        printf_serial(false, FAIL, "Usage: %s <elf_file>\n", argv[0]);
        return 1;
    }
    const char* elf_file = argv[1];
    FILE* fp = fopen(elf_file, "rb");
    if (!fp) {
        printf_serial(false, FAIL, "Failed to open file: %s\n", elf_file);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void* buffer = malloc(file_size);
    if (!buffer) {
        printf_serial(false, FAIL, "Failed to allocate memory for file\n");
        fclose(fp);
        return 1;
    }
    size_t read_size = fread(buffer, 1, file_size, fp);
    if (read_size != file_size) {
        printf_serial(false, FAIL, "Failed to read file\n");
        free(buffer);
        fclose(fp);
        return 1;
    }
    fclose(fp);
	void* entry = elf_load_file(buffer);
	// run the entry point
	// void (*entry_point)() = (void (*)())entry;
	// entry_point();
    free(buffer);
    return 0;
}
*/