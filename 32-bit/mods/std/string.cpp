#include "include/string.h"

void swap(char *a, char *b)                                                                                                                                                                       
  {
       if(!a || !b)
           return;

       char temp = *(a);
       *(a) = *(b);
       *(b) = temp;
   }

void reverse(char *str, int length) 
{ 
	int start = 0; 
	int end = length -1; 
	while (start < end) 
	{ 
		swap((str+start), (str+end)); 
		start++; 
		end--; 
	} 
} 

char* itoa(int num, char* str, int base) 
{ 
	int i = 0; 
	bool isNegative = false; 

	if (num == 0) 
	{ 
		str[i++] = '0'; 
		str[i] = '\0'; 
		return str; 
	}

	if (num < 0 && base == 10) 
	{ 
		isNegative = true;
		num = -num; 
	} 

	while (num != 0) 
	{ 
		int rem = num % base; 
		str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0'; 
		num = num/base; 
	}

	if (isNegative == true) 
		str[i++] = '-'; 

	str[i] = '\0';
	reverse(str, i); 
	return str; 
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
