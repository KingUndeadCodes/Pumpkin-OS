/*
 Credits:
    [0] https://github.com/mell-o-tron/MellOs/blob/main/Memory/mem.cpp
*/
#ifndef _STDLIB_H
#define _STDLIB_H 1
#define FREE_MEM 0x10000;
#include <string.h>

// Development
void initializeMem();

void* kmalloc(size_t size);
void kfree(void* mem);

#endif
