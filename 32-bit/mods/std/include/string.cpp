#include "string.h"

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
