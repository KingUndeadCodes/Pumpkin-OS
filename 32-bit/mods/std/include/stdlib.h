/*
 Credits:
    [0] https://github.com/mell-o-tron/MellOs/blob/main/Memory/mem.cpp
*/
#ifndef _STDLIB_H
#define _STDLIB_H 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <text.h>
#include <test.h>

#define FREE_MEM 0x10000

// void* calloc(size_t size);
// void* realloc(size_t size);

test_t MALLOC_TEST(void);

// Remove Later
void print_memory(void);

__attribute__((__noreturn__)) void abort(void);
void initializeMem();
void free(void* mem);
void* malloc(size_t size);

#endif
