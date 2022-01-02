#include <string.h>
#include "../dev/port.cpp"
#include "include/text.h"

struct Char* buffer = (struct Char*) 0xb8000;
size_t col = 0;
size_t row = 0;

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

void printf(const char* string, uint8_t color = 15) {
    for (int i = 0; i < strlen(string); i++) {
        if (string[i] == '\n' || (col + 1) >= COLS) {
	    row++;
	    col = 0;
        } else {
            buffer[col + COLS * row] = (struct Char) {
                character: (uint8_t) string[i],
                color: color,
            };
            col++;
       }
       Cursor::moveCursor(col - 1, row);
    }
}

void scanf(struct regs *r) {
    	const char kbdus[128] = {
        	0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        	'*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        	'-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    	};
	unsigned char scancode;
	scancode = inb(0x60);
	if (scancode & 0x80) {
		// pass
	} else {
		printf(kbdus[scancode]);
	}
}
