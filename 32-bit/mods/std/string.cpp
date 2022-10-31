#include "include/string.h"

char* itoa(int val, int base){
	if (val == 0 && base == 10) return "0";
	static char buf[32] = {0};
   	int i = 30;
   	for(; val && i ; --i, val /= base) buf[i] = "0123456789abcdef"[val % base];
   	return &buf[i+1];
}

void* memset(void* dest, uint8_t val, size_t count) {
	uint8_t* destC = (unsigned char*) dest;
	for (size_t i = 0; i < count; i++) destC[i] = val;
	return dest;
}

void* memchr(const void* str, int c, size_t n) {
	uint8_t *p = (uint8_t*) str;
	uint8_t *end = p + n;
	while (p != end) {
        if (*p == c) return p; ++p;
    }
    return 0;
}

void* memmove(void* dst, const void* src, size_t n) {
	uint8_t *p = (uint8_t*)src;
	uint8_t *q = (uint8_t*)dst;
	uint8_t *end = p + n;
	if (q > p && q < end) {
		p = end;
		q += n;
		while (p != src) {
			*--q = *--p;
		}
	} else {
		while (p != end) {
			*q++ = *p++;
		}
	}
	return dst;
}

size_t strlen(const char* str) {
	uint32_t len = 0;
	while (str[len] && str[len] != '\0') len++;
	return len;
}

size_t strspn(const char* str1, const char* str2) {
	const char* s = str1;
	const char* c;
	while (*str1) {
		for (c = str2; *c; c++)
	    	if (*str1 == *c) break;
		if (*c == '\0') break;
		str1++;
    }
    return str1 - s;
}

char* strcat(char* dest, const char* src) {
	char *rdest = dest;
	while (*dest) dest++;
	while (*dest++ = *src++);
	return rdest;
}

// Taken from: https://wiki.osdev.org/Meaty_Skeleton
int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i]) return -1;
		else if (b[i] < a[i]) return 1;
	}
	return 0;
}

// Taken from: https://forum.osdev.org/viewtopic.php?f=13&t=1637
char *strcpy(char *s1, const char *s2) {
    char *s1_p = s1;
    while (*s1++ = *s2++);
    return s1_p;
}

// https://github.com/ApplePy/osdev/blob/master/string.c
int strcmp(const char *s1, const char *s2) {
	if (strlen(s1) != strlen(s2)) return s2-s1;
	return strncmp(s1, s2, strlen(s1));
}

// https://github.com/ApplePy/osdev/blob/master/string.c
int strncmp(const char *s1, const char *s2, size_t n) {
	unsigned int count = 0;
	while (count < n) {
		if (s1[count] == s2[count])
		{
			if (s1[count] == '\0') return 0;
			else {count++;}
		}
		else {return s1[count] - s2[count];}
	}
	return 0;
}

// Taken from: https://github.com/ApplePy/osdev/blob/master/string.c
char *strncpy(char *s1, const char *s2, size_t n) {
	unsigned int extern_iter = 0;
	unsigned int iterator = 0;
	for (iterator = 0; iterator < n; iterator++) {
		if (s2[iterator] != '\0')
			s1[iterator] = s2[iterator];
		else {
			s1[iterator] = s2[iterator];
			extern_iter = iterator + 1;
			break;
		}
	}
	while (extern_iter < n) {
		s1[extern_iter] = '\0';
		extern_iter++;
	}
	return s1;
}

// Taken from: https://wiki.osdev.org/Meaty_Skeleton#libc.2Fstring.2Fmemcpy.c
// Note: Removed Restrict Keyword so C++ can be happy.
void* memcpy(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*)dstptr;
	const unsigned char* src = (const unsigned char*)srcptr;
	for (size_t i = 0; i < size; i++) dst[i] = src[i];
	return dstptr;
}
