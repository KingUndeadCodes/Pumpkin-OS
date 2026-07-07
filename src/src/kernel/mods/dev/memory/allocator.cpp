#include "../paging/paging.h"
#include "../serial/serial.h"
#include "allocator.h"
#include <string.h>

#define DMA_REGION_BASE 0xD0000000  // Pick an unused virtual range for DMA
static uintptr_t dma_next_virt = DMA_REGION_BASE;

static uint8_t bitmap[BITMAP_SIZE];
static uintptr_t phys_start = 0;  // lowest usable address
static uintptr_t phys_end = 0;    // highest usable address

// Macros for bit operations
#define BIT_SET(b, i)   (b[(i) / 8] |=  (1 << ((i) % 8)))
#define BIT_CLEAR(b, i) (b[(i) / 8] &= ~(1 << ((i) % 8)))
#define BIT_TEST(b, i)  (b[(i) / 8] &   (1 << ((i) % 8)))

void init_phys_allocator(mem_region_t* regions, size_t count) {
    memset(bitmap, 0xFF, BITMAP_SIZE); // mark all pages used
    
    for (size_t i = 0; i < count; i++) {
        if (regions[i].type != 1) continue; // not usable

        uintptr_t start = (uintptr_t)regions[i].base;
        uintptr_t end   = start + (uintptr_t)regions[i].length;

        // Ensure start and end are page-aligned

        if (phys_start == 0 || start < phys_start) phys_start = start;
        if (end > phys_end) phys_end = end;

        // Align to 4 KiB
        start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        end   = end & ~(PAGE_SIZE - 1);

        for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
            size_t page_index = addr / PAGE_SIZE;
            BIT_CLEAR(bitmap, page_index); // mark page as free
        }
    }

    // Optionally mark kernel pages as used here (identity mapping or kernel image)
    extern uint8_t endkernel;  // symbol defined in linker script
    uintptr_t kernel_end = ((uintptr_t)&endkernel + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uintptr_t addr = 0; addr < kernel_end; addr += PAGE_SIZE) {
        size_t index = addr / PAGE_SIZE;
        BIT_SET(bitmap, index);
    }
}

void* alloc_phys_page() {
    unsigned long flags = enter_critical();
    for (size_t i = 0; i < MAX_PAGES; i++) {
        if (!BIT_TEST(bitmap, i)) {
            BIT_SET(bitmap, i);
            exit_critical(flags);
            return (void*)(i * PAGE_SIZE);
        }
    }
    exit_critical(flags);
    return NULL; // out of memory
}

void free_phys_page(void* addr) {
    uintptr_t a = (uintptr_t)addr;
    if (a % PAGE_SIZE != 0) return; // must be aligned

    size_t index = a / PAGE_SIZE;
    if (index < MAX_PAGES) {
        unsigned long flags = enter_critical();
        BIT_CLEAR(bitmap, index);
        exit_critical(flags);
    }
}

// See docs/DOCS.md ("mods/dev/memory/allocator.cpp" section) for why this
// takes one critical section for the whole operation.
uintptr_t alloc_phys_pages(size_t count) {
    unsigned long flags = enter_critical();
    uintptr_t base = 0;

    for (size_t i = 0; i < count; i++) {
        uintptr_t addr = (uintptr_t)alloc_phys_page();
        if (!addr) {
            // Roll back previously allocated pages
            for (size_t j = 0; j < i; j++) {
                free_phys_page((void*)(base + j * PAGE_SIZE));
            }
            exit_critical(flags);
            return 0;
        }

        if (i == 0) {
            base = addr;
        } else if (addr != base + i * PAGE_SIZE) {
            // See docs/DOCS.md ("mods/dev/memory/allocator.cpp" section) for
            // the rollback-address fix here.
            free_phys_page((void*)addr);
            for (size_t j = 0; j < i; j++) {
                free_phys_page((void*)(base + j * PAGE_SIZE));
            }
            exit_critical(flags);
            return 0;
        }
    }

    exit_critical(flags);
    return base;
}

dma_buffer_t dma_alloc(size_t size, size_t alignment) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t total_size = pages * PAGE_SIZE;

    // Allocate contiguous physical pages
    uintptr_t phys = alloc_phys_pages(pages);
    if (!phys) return (dma_buffer_t){0}; // failed

    // Align virtual address
    if (dma_next_virt % alignment != 0)
        dma_next_virt = (dma_next_virt + alignment - 1) & ~(alignment - 1);

    uintptr_t virt = dma_next_virt;
    dma_next_virt += total_size;

    for (size_t i = 0; i < pages; i++) {
        map_physical_memory(phys + i * PAGE_SIZE, virt + i * PAGE_SIZE, PAGE_SIZE,
            PAGE_PRESENT | PAGE_WRITABLE);
    }

    return (dma_buffer_t){
        .phys = phys,
        .virt = (void*)virt,
        .size = total_size
    };
}
