#include "kb.h"

bool shift_key = false;

void KeyboardHandler(struct regs *r) {
    unsigned char scancode = inb(0x60);
	const char* tab = "        ";
	switch (scancode) {
		// Too lazy to swap them.
		case 0x02: printf(shift_key ? "!" : "1"); break;
		case 0x03: printf(shift_key ? "@" : "2"); break;
		case 0x04: printf(shift_key ? "#" : "3"); break;
		case 0x05: printf(shift_key ? "$" : "4"); break;
		case 0x06: printf(shift_key ? "%" : "5"); break;
		case 0x07: printf(shift_key ? "^" : "6"); break;
		case 0x08: printf(shift_key ? "&" : "7"); break;
		case 0x09: printf(shift_key ? "*" : "8"); break;
		case 0x0A: printf(shift_key ? "(" : "9"); break;
		case 0x0B: printf(shift_key ? ")" : "0"); break;
		case 0x0C: printf(shift_key ? "_" : "-"); break;
		case 0x0D: printf(shift_key ? "+" : "="); break;
		case 0x10: printf(shift_key ? "Q" : "q"); break;
		case 0x11: printf(shift_key ? "W" : "w"); break;
		case 0x12: printf(shift_key ? "E" : "e"); break;
		case 0x13: printf(shift_key ? "R" : "r"); break;
		case 0x14: printf(shift_key ? "T" : "t"); break;
		case 0x15: printf(shift_key ? "Y" : "y"); break;
		case 0x16: printf(shift_key ? "U" : "u"); break;
		case 0x17: printf(shift_key ? "I" : "i"); break;
		case 0x18: printf(shift_key ? "O" : "o"); break;
		case 0x19: printf(shift_key ? "P" : "p"); break;
		case 0x1A: printf(shift_key ? "{" : "["); break;
		case 0x1B: printf(shift_key ? "}" : "]"); break;
		case 0x1D: printf(shift_key ? "|" : "\\"); break;
		case 0x1E: printf(shift_key ? "A" : "a"); break;
		case 0x1F: printf(shift_key ? "S" : "s"); break;
		case 0x20: printf(shift_key ? "D" : "d"); break;
		case 0x21: printf(shift_key ? "F" : "f"); break;
		case 0x22: printf(shift_key ? "G" : "g"); break;
		case 0x23: printf(shift_key ? "H" : "h"); break;
		case 0x24: printf(shift_key ? "J" : "j"); break;
		case 0x25: printf(shift_key ? "K" : "k"); break;
		case 0x26: printf(shift_key ? "L" : "l"); break;
		case 0x27: printf(shift_key ? ":" : ";"); break;
		case 0x28: printf(shift_key ? "\"" : "'"); break;
		case 0x2c: printf(shift_key ? "Z" : "z"); break;
		case 0x2D: printf(shift_key ? "X" : "x"); break;
		case 0x2E: printf(shift_key ? "C" : "c"); break;
		case 0x2F: printf(shift_key ? "V" : "v"); break;
		case 0x30: printf(shift_key ? "B" : "b"); break;
		case 0x31: printf(shift_key ? "N" : "n"); break;
		case 0x32: printf(shift_key ? "M" : "m"); break;
		case 0x33: printf(shift_key ? "<" : ","); break;
		case 0x34: printf(shift_key ? ">" : "."); break;
		case 0x35: printf(shift_key ? "?" : "/"); break;
        case 0x39: printf(" "); break;
		case 0x0F: print("\t"); break;
		case 0x0E: print("\b"); break;
		case 0x1C: print("\n"); break;
		case 0x2a: shift_key = true; break;
		// default: printf("{{{ %d }}}", scancode);
		// case 0x36: shift_key = true; break;
	}
	if (scancode & 0x80 && scancode == 0xaa) shift_key = false;
}

void KeyboardInit() {
	irq_install_handler(1, KeyboardHandler);
}