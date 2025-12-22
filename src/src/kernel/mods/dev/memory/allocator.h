#ifndef PHYS_ALLOC_H
#define PHYS_ALLOC_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define MAX_PHYS_MEM (1024 * 1024 * 1024) // 1 GiB for example
#define MAX_PAGES (MAX_PHYS_MEM / PAGE_SIZE)

#define BITMAP_SIZE (MAX_PAGES / 8)

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type; // 1 = usable, other = reserved
} __attribute__((packed)) mem_region_t;

typedef struct {
    uintptr_t phys;  // physical address
    void* virt;      // virtual address
    size_t size;     // size of the buffer in bytes
} dma_buffer_t;

dma_buffer_t dma_alloc(size_t size, size_t alignment);

void init_phys_allocator(mem_region_t* regions, size_t region_count);

void* alloc_phys_page();           // returns physical address
void free_phys_page(void* addr);   // takes physical address

#endif
