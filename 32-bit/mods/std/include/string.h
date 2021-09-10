#ifndef _STRING_H
#define _STRING_H 1
#include <stdint.h>
#include <stddef.h>

size_t strlen(const char* str);
size_t strspn(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);
void* memchr(const void* str, int c, size_t n);
void* memset(void* dest, uint8_t val, size_t count);
void printf(const char* string, uint8_t color = 15);

#endif
