#include "../dev/port.cpp"
#include "text.h"

struct Char* buffer = (struct Char*) 0xb8000;
size_t col = 0;
size_t row = 0;

void* memset(void* dest, uint8_t val, size_t count) {
    uint8_t* destC = (unsigned char*)dest;
    for (size_t i = 0; i < count; i++) destC[i] = val;
    return dest;
}

void* memchr(const void* str, int c, size_t n) {
    uint8_t *p = (uint8_t*)str;
    uint8_t *end = p + n;
    while (p != end) {
        if (*p == c) return p;
        ++p;
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

void d_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void e_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void m_cursor(int x, int y) {
    uint16_t pos = y * COLS + x + 1;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void printf(const char* string, uint8_t color = 15) {
    for (int i = 0; i < strlen(string); i++) {
        if (string[i] == '\n') {
	    row++;
	    col = 0;
        } else {
            buffer[col + COLS * row] = (struct Char) {
                character: (uint8_t) string[i],
                color: color,
            };
            col++;
       }
       m_cursor(col - 1, row);
    }
}
