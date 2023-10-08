#ifndef _STDLIB_H
#define _STDLIB_H 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <text.h>

typedef struct {
    int quot;
    int rem;
} div_t;

div_t div(int numerator, int denominator);

/*
    =================================
    OpenAI Generated The Malloc Code
    =================================

    ? > Where are you sourcing this code from?
    -   The code for the memory functions malloc, free, calloc, and realloc -
        that I provided earlier is a simple implementation based on the book "The C -
        Programming Language" by Brian W. Kernighan and Dennis M. Ritchie, and on my own -
        knowledge of memory management in C.
*/
void initialize_memory_pool();
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

__attribute__((__noreturn__)) void abort(void);

#endif
