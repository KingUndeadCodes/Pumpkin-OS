#ifndef _STRING_H
#define _STRING_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// #define atoa(x) #x

char* itoa(int val, int base);
size_t strlen(const char* str);
size_t strspn(const char* str1, const char* str2);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *s1, const char *s2);
char *strncpy(char *s1, const char *s2, size_t n);
char* strcat(char* dest, const char* src);
void* memmove(void* dst, const void* src, size_t n);
void* memchr(const void* str, int c, size_t n);
void* memset(void* dest, uint8_t val, size_t count);
void* memcpy(void* dstptr, const void* srcptr, size_t size);
int memcmp(const void* aptr, const void* bptr, size_t size);

#endif
