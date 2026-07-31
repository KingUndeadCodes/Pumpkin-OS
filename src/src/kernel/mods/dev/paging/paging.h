#ifndef _PAGING_H
#define _PAGING_H
#include <stdlib.h>
#include <stdint.h>

#pragma once
/*
#define MAX_PAGE_ALIGNED_ALLOCS 32

extern "C" void loadPageDirectory(unsigned int*);
extern "C" void enablePaging();
void PagingInstall();
*/

extern "C" void loadPageDirectory(unsigned int*);
extern "C" void enablePaging();
extern "C" void disablePaging();

typedef enum {
    PD_PRESENT         = 0b1,
    PD_READWRITE       = 0b10,
    PD_USER            = 0b100,
    PD_WRITETHROUGH    = 0b1000,
    PD_CACHEDISABLE    = 0b10000,
    PD_ACCESSED        = 0b100000,
    PD_DIRTY           = 0b1000000, 
    PD_PAGESIZE        = 0b10000000,
    PD_GLOBAL          = 0b100000000,
} PD_FLAGS;

typedef enum {
    PT_WRITEBACK       = 0b0,
    PT_PRESENT         = 0b1,
    PT_READWRITE       = 0b10,
    PT_USER            = 0b100,
    PT_WRITETHROUGH    = 0b1000,
    PT_CACHEDISABLE    = 0b10000,
    PT_ACCESSED        = 0b100000,
    PT_DIRTY           = 0b1000000, 
    PT_PAT             = 0b10000000,
    PT_GLOBAL          = 0b100000000,
    PT_WRITEPROTECT    = 0b10000000,
    PT_WRITECOMBINING  = 0b10000000 | 0b1000,
} PT_FLAGS;

void init_paging(unsigned int * page_directory, unsigned int * first_page_table, unsigned int * second_page_table);
void stop_paging();
void add_page (unsigned int * page_directory, unsigned int * page_table, int index, int offset, PT_FLAGS ptf, PD_FLAGS pdf);

void initialize_page_directory (unsigned int* page_directory);
void switch_page_directory(unsigned int * page_directory);

void PagingInstall(void); // This is for the setup

#define PAGE_SIZE 4096
#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER 0x4

void map_physical_memory(uintptr_t phys_addr, uintptr_t virt_addr, size_t size, uint32_t flags);
// Marks an already-mapped region PAGE_USER at both PDE and PTE, needed for real ring-3 access.
void mark_region_user(uintptr_t addr, size_t size, bool writable);
// CPL0 code is exempt from the hardware U/S check, so a syscall validating a ring-3
// pointer has to replicate that check itself before trusting it.
bool is_user_accessible(uintptr_t addr, size_t size);

#endif