/*
#include "paging.h"

unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));
uint32_t pheap_begin = 0;
uint32_t pheap_end = 0;
uint8_t *pheap_desc = 0;

void PagingInstall(void) {
    for (int i = 0; i < 1024; i++) page_directory[i] = 0x00000002;
    for (uint32_t i = 0; i < 1024; i++) { first_page_table[i] = (i * 0x1000) | 3; }
    page_directory[0] = ((unsigned int)first_page_table) | 3;
    loadPageDirectory(page_directory);
    enablePaging();
    // Other Page Alloc Stuff
    pheap_end = 0x400000;
	pheap_begin = pheap_end - (MAX_PAGE_ALIGNED_ALLOCS * 4096);
    pheap_desc = (uint8_t*)malloc(MAX_PAGE_ALIGNED_ALLOCS);
};
*/
// #include "paging_utils.h"
#include "../memory/allocator.h"
#include "paging.h"
#include "pat/pat.h"

void initialize_page_directory (unsigned int* page_directory){
    int i;
    for(i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000000 | (PT_READWRITE);    // Last 3 bits are 010:
                                                            // - U/S = 0 : only the supervisor can access the page
                                                            // - R/W = 1 : the page is read/write
                                                            // - P   = 0 : the page is not present
    }   
}


void add_page (unsigned int * page_directory, unsigned int * page_table, int index, int offset, PT_FLAGS ptf, PD_FLAGS pdf){
    for(unsigned int i = 0; i < 1024; i++) {
        page_table[i] = (offset + (i * 0x1000)) | ptf; // U/S, R/W same. P = 1 present.
    }
    page_directory[index] = ((unsigned int)page_table) | pdf;
}

void init_paging(unsigned int * page_directory, unsigned int * first_page_table, unsigned int * second_page_table) {
    initialize_page_directory(page_directory);
    PD_FLAGS page_directory_flags = PD_PRESENT | PD_READWRITE;
    PT_FLAGS first_page_table_flags = PT_PRESENT | PT_READWRITE;
    add_page(page_directory, first_page_table,  0, 0, first_page_table_flags, page_directory_flags);
    // For now, since the kernel is mapped at 0x400000, we identity-map from 0x0 to 0x800000. Eventually, it would be nice to have a proper "kernel on the high memory"
    add_page(page_directory, second_page_table,  1, 0x400000, first_page_table_flags, page_directory_flags);
    loadPageDirectory(page_directory);
    enablePaging();
}

void stop_paging(){
    disablePaging();
}

void switch_page_directory(unsigned int * page_directory){
    loadPageDirectory(page_directory);
    enablePaging();
}

// Setup
#define NUM_MANY_PAGES (uint32_t)512

uint32_t page_directory[1024]       __attribute__((aligned(4096)));
uint32_t first_page_table[1024]     __attribute__((aligned(4096)));
uint32_t second_page_table[1024]    __attribute__((aligned(4096)));
uint32_t lots_of_pages[NUM_MANY_PAGES][1024]  __attribute__((aligned(4096)));

PD_FLAGS page_directory_flags   = PD_PRESENT | PD_READWRITE;
PT_FLAGS first_page_table_flags = PT_PRESENT | PT_READWRITE;

void PagingInstall(void) {
    init_paging(page_directory, first_page_table, second_page_table); // PagingInstall();
    setup_pat();
    for(uint32_t i = 0; i < NUM_MANY_PAGES; i++){
        add_page(page_directory, lots_of_pages[i], i + 2, 0x400000 * (i + 2), first_page_table_flags, page_directory_flags);
    }
}
// End Setup

// =====

uint32_t* get_or_create_page_table(size_t pd_index, uint32_t flags) {
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        uintptr_t new_table_phys = (uintptr_t)alloc_phys_page();
        
        // Optional: identity-map the new page table temporarily so you can write to it
        map_physical_memory(new_table_phys, new_table_phys, PAGE_SIZE, PAGE_PRESENT | PAGE_WRITABLE);

        // Clear the table
        uint32_t* new_table_virt = (uint32_t*)new_table_phys;
        for (int i = 0; i < 1024; i++) new_table_virt[i] = 0;

        page_directory[pd_index] = (new_table_phys & ~0xFFF) | flags | PAGE_PRESENT;
    }

    // Get the virtual address of the page table
    // return (uint32_t*)((page_directory[pd_index] & ~0xFFF));
    uintptr_t phys = page_directory[pd_index] & ~0xFFF;
    return (uint32_t*)phys;
}

void map_physical_memory(uintptr_t phys_addr, uintptr_t virt_addr, size_t size, uint32_t flags) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++) {
        uintptr_t va = virt_addr + i * PAGE_SIZE;
        uintptr_t pa = phys_addr + i * PAGE_SIZE;

        size_t pd_index = (va >> 22) & 0x3FF;
        size_t pt_index = (va >> 12) & 0x3FF;

        uint32_t* page_table = get_or_create_page_table(pd_index, flags);
        page_table[pt_index] = (pa & ~0xFFF) | flags | PAGE_PRESENT;
    }

    // Optional: flush TLB
    asm volatile (
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        :::"eax"
    );
}

// =====
