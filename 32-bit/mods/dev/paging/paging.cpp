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

/*
void pfree(void *mem) {
	if((int)mem < pheap_begin || (int)mem > pheap_end) return;
	uint32_t ad = (uint32_t)mem;
	ad -= pheap_begin;
	ad /= 4096;
	pheap_desc[ad] = 0;
	return;
}

char* pmalloc(size_t size) {
	for(int i = 0; i < MAX_PAGE_ALIGNED_ALLOCS; i++) {
		if(pheap_desc[i]) continue;
		pheap_desc[i] = 1;
		return (char *)(pheap_begin + i*4096);
	}
	return 0;
}
*/