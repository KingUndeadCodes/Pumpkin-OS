#include "include/libboot.h"

void outb(uint16_t port, uint8_t data) {
	asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
	return;
};

uint8_t inb(uint16_t port) {
	uint8_t res;
	asm volatile("inb %1, %0" : "=a"(res) : "Nd"(port));
	return res;
};

const static size_t COLS = 80;
const static size_t ROWS = 25;

struct Char {
    uint8_t character, color;
};

static struct Char* buffer = (struct Char*) 0xb8000;
static size_t col = 0;
static size_t row = 0;

/*
namespace Cursor {
    void disbaleCursor() {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }
    void enableCursor(uint8_t cursor_start, uint8_t cursor_end) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
    }
    void moveCursor(int x, int y) {
        uint16_t pos = y * COLS + x + 1;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t) (pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    }
    uint16_t getCursor(void) {
        uint16_t pos = 0;
        outb(0x3D4, 0x0F);
        pos |= inb(0x3D5);
        outb(0x3D4, 0x0E);
        pos |= ((uint16_t)inb(0x3D5)) << 8;
        return pos;
    }
}
*/

size_t strlen(const char* str) {
	uint32_t len = 0;
	while (str[len] && str[len] != '\0') len++;
	return len;
}

void reverse(char s[]) {
    uint32_t length = strlen(s);
    uint32_t c, i, j;
    for (i = 0, j = length - 1; i < j; i++, j--){
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/*
void print(const char* string, uint8_t color = 15) {
    for (int i = 0; i < strlen(string); i++) {
        if (string[i] == '\n' || ((col + 1) >= COLS && string[i] != '\b')) {
            row++;
            col = 0;
        } else if (string[i] == '\b') {
	        col--;
            buffer[col + COLS * row] = (struct Char) {
                character: (uint8_t) ' ',
                color: color,
            };
        } else if (string[i] == '\t') {
            print("        ");
        } else if (string[i] == '\r') {
            for (col = 0; col < COLS; col++) {
                buffer[col + COLS * row] = (struct Char) {
                    character: (uint8_t) ' ',
                    color: (uint8_t) 15,
                };
            };
            col = 0;
        } else {
	        buffer[col + COLS * row] = (struct Char) {
                character: (uint8_t) string[i],
                color: color,
            };
            col++;
        }
        // Cursor::moveCursor(col - 1, row);
    }
}
*/

void print(const char* string, uint8_t color = 15) {
    for (int i = 0; i < strlen(string); i++) {
        if (string[i] == '\n' || ((col + 1) >= COLS)) {
            row++;
            col = 0;
        } else {
	        buffer[col + COLS * row] = (struct Char) {
                character: (uint8_t) string[i],
                color: color,
            };
            col++;
        }
        // Cursor::moveCursor(col - 1, row);
    }
}

const char* itoa_signed(int n, int base, char* dest) {
	char* buffer = dest;
	int m = n;
	int i = 0;
    if (n < 0) m = -n;
    while (m != 0) {
		buffer[i] = (char)((m % base)+ (m % base > 9 ? 55 : 48));
		m = m / base;
		i++;
	}
    if (n < 0) {
        buffer[i] = '-';
        i++;
    }
    if (n == 0){
        buffer[i] = '0';
        i++;
    }
	buffer[i] = '\0';
	reverse(buffer);
	return buffer;
}