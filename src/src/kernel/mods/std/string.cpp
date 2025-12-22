#include "include/string.h"

int sprintf(char* buffer, const char* format, ...) {
    va_list list;
    va_start(list, format);
    char* buf_ptr = buffer;

    for (int i = 0; format[i]; ++i) {
        if (format[i] == '%') {
            switch ((char)format[++i]) {
                case 'c': {
                    *buf_ptr++ = (char)va_arg(list, int);
                    break;
                }
                case 's': {
                    const char* str = va_arg(list, char*);
                    while (*str) {
                        *buf_ptr++ = *str++;
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    char* temp = itoa(va_arg(list, int), 10);
                    for (char* t = temp; *t; ++t) {
                        *buf_ptr++ = *t;
                    }
                    break;
                }
                case 'u':
                case 'o':
                case 'x':
                case 'X': {
                    char* temp = itoa(va_arg(list, unsigned int), 10);
                    for (char* t = temp; *t; ++t) {
                        *buf_ptr++ = *t;
                    }
                    break;
                }
                default: {
                    *buf_ptr++ = format[i];
                    break;
                }
            }
        } else {
            *buf_ptr++ = format[i];
        }
    }

    *buf_ptr = '\0';
    va_end(list);
    return buf_ptr - buffer;
}

char* strchr(const char *str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return (char *)str;
        }
        ++str;
    }

    return NULL;
}

char* strtok(char *str, const char *delim) {
    static char *lastToken = NULL;  // Keeps track of the last token

    if (str != NULL) {
        lastToken = str;
    } else if (lastToken == NULL) {
        return NULL;  // No more tokens to be found
    }

    // Skip leading delimiters
    char *startToken = lastToken;
    while (*startToken != '\0' && strchr(delim, *startToken) != NULL) {
        ++startToken;
    }

    if (*startToken == '\0') {
        return NULL;  // Reached the end of the input string
    }

    // Find the end of the token
    char *endToken = startToken + 1;
    while (*endToken != '\0' && strchr(delim, *endToken) == NULL) {
        ++endToken;
    }

    // Update the last token pointer for the next call
    lastToken = (*endToken != '\0') ? endToken + 1 : NULL;

    // Null-terminate the token
    *endToken = '\0';

    return startToken;
}

char* itoa(int val, int base){
	if (val == 0 && base == 10) return "0";
	static char buf[32] = {0};
   	int i = 30;
   	for(; val && i ; --i, val /= base) buf[i] = "0123456789abcdef"[val % base];
   	return &buf[i+1];
}

int atoi(char *s) {
    int acum = 0;
    int factor = 1;
    
    if (*s == '-') {
        factor = -1;
        s++;
    }
    
    while ((*s >= '0') && (*s <= '9')) {
        acum = acum * 10;
        acum = acum + (*s - 48);
        s++;
    }

    return (factor * acum);
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

char* strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str);
    char *new_str = malloc(len + 1);
    if (new_str == NULL) {
        return NULL;
    }
    strcpy(new_str, str);
    return new_str;
}

char* strtok_r(char *str, const char *delim, char **saveptr) {
    char *start;
    if (str) {
        start = str;
    } else if (*saveptr) {
        start = *saveptr;
    } else {
        return NULL;
    }

    // Skip leading delimiters
    start += strspn(start, delim);
    if (*start == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    // Find end of token
    char *end = start + strcspn(start, delim);
    if (*end != '\0') {
        *end = '\0';      // Null-terminate token
        *saveptr = end + 1; // Save position for next call
    } else {
        *saveptr = NULL;  // No more tokens
    }

    return start;
}

size_t strcspn(const char *s, const char *reject) {
    const char *p, *r;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p) {
        for (r = reject; *r != '\0'; ++r) {
            if (*p == *r) {
                return count;
            }
        }
        ++count;
    }

    return count;
}

// Write a strrchr function
char* strrchr(const char *str, int c) {
    const char *last = NULL;
    while (*str) {
        if (*str == c) {
            last = str;
        }
        str++;
    }
    return (char *)last; // Return the last occurrence or NULL if not found
}