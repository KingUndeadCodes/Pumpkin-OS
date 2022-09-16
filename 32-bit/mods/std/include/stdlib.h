/*
 Credits:
    [0] https://github.com/mell-o-tron/MellOs/blob/main/Memory/mem.cpp
*/
#ifndef _STDLIB_H
#define _STDLIB_H 1
#define FREE_MEM 0x10000
#include <string.h>
#include <text.h>

int getFreeMem();
void initializeMem();
void free(void* mem);
void* malloc(size_t size);
// void* calloc(size_t size);
// void* realloc(size_t size);

#endif
