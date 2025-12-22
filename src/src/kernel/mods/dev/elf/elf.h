#ifndef ELF_H
#define ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define EI_NIDENT 16

// https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;

#define ELFARCH_UNDEFINED 0x00
#define ELFARCH_SPARC     0x02
#define ELFARCH_X86       0x03
#define ELFARCH_MIPS      0x08
#define ELFARCH_POWERPC   0x14
#define ELFARCH_ARM       0x28
#define ELFARCH_SUPERH    0x2A
#define ELFARCH_IA_64     0x32
#define ELFARCH_X86_64    0x3E
#define ELFARCH_AARCH64   0xB7
#define ELFARCH_RISCV     0xF3

#define ELFSEG_NULL       0x00 // ignore
#define ELFSEG_LOAD       0x01 
#define ELFSEG_DYNAMIC    0x02
#define ELFSEG_INTERP     0x03
#define ELFSEG_NOTE       0x04

#define ELFFLAG_EXEC      0x01
#define ELFFLAG_WRITE     0x02
#define ELFFLAG_READ      0x04

#define ELF_RELOC_ERR     -1

#define SHN_UNDEF   (0x00)  // Undefined/Not Present
#define SHN_ABS     0xFFF1  // Absolute symbol

#define ELF32_ST_BIND(INFO)	((INFO) >> 4)
#define ELF32_ST_TYPE(INFO)	((INFO) & 0x0F)

#define ELF32_R_SYM(INFO)	((INFO) >> 8)
#define ELF32_R_TYPE(INFO)	((uint8_t)(INFO))

#define DO_386_32(S, A)	((S) + (A))
#define DO_386_PC32(S, A, P) ((S) + (A) - (P))

typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf32_Word      p_type;
	Elf32_Off	    p_offset;
	Elf32_Addr	    p_vaddr;
	Elf32_Addr	    p_paddr;
	Elf32_Word	    p_filesz;
	Elf32_Word	    p_memsz;
	Elf32_Word	    p_flags;
	Elf32_Word	    p_align;
} Elf32_Phdr;

typedef struct {
	Elf32_Word		st_name;
	Elf32_Addr		st_value;
	Elf32_Word		st_size;
	uint8_t			st_info;
	uint8_t			st_other;
	Elf32_Half		st_shndx;
} Elf32_Sym;

typedef struct {
	Elf32_Addr		r_offset;
	Elf32_Word		r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr		r_offset;
	Elf32_Word		r_info;
	Elf32_Sword		r_addend;
} Elf32_Rela;

enum ShT_Types {
	SHT_NULL	    = 0,    // Null section
	SHT_PROGBITS    = 1,    // Program information
	SHT_SYMTAB	    = 2,    // Symbol table
	SHT_STRTAB	    = 3,    // String table
	SHT_RELA	    = 4,    // Relocation (w/ addend)
	SHT_NOBITS	    = 8,    // Not present in file
	SHT_REL		    = 9,    // Relocation (no addend)
};

enum ShT_Attributes {
	SHF_WRITE	    = 0x01, // Writable section
	SHF_ALLOC	    = 0x02  // Exists in memory
};

enum StT_Bindings {
	STB_LOCAL		= 0, // Local scope
	STB_GLOBAL		= 1, // Global scope
	STB_WEAK		= 2  // Weak, (ie. __attribute__((weak)))
};

enum StT_Types {
	STT_NOTYPE		= 0, // No type
	STT_OBJECT		= 1, // Variables, arrays, etc.
	STT_FUNC		= 2  // Methods or functions
};

enum Elf_Type {
	ET_NONE		= 0, // Unkown Type
	ET_REL		= 1, // Relocatable File
	ET_EXEC		= 2  // Executable File
};

enum RtT_Types {
	R_386_NONE		= 0, // No relocation
	R_386_32		= 1, // Symbol + Offset
	R_386_PC32		= 2  // Symbol + Offset - Section Offset
};

/*
static int elf_get_symval(Elf32_Ehdr *hdr, int table, uint32_t idx);
static int elf_load_stage1(Elf32_Ehdr *hdr);
static int elf_load_stage2(Elf32_Ehdr *hdr);
static int elf_do_reloc(Elf32_Ehdr *hdr, Elf32_Rel *rel, Elf32_Shdr *reltab);
*/

void* elf_load_file(void* b);

#endif