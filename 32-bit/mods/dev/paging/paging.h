#ifndef _PAGING_H
#define _PAGING_H
#include <stdlib.h>
#include <stdint.h>
#include <test.h>

#define MAX_PAGE_ALIGNED_ALLOCS 32

extern "C" void loadPageDirectory(unsigned int*);
extern "C" void enablePaging();
void PagingInstall();

// char* pmalloc(size_t size);
// void pfree(void *mem);

#endif