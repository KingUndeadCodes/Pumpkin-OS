#include "kb.h"

void KeyboardHandler(struct regs *r) {
        unsigned char scancode = inb(0x60);
	switch (scancode) {
		case 0x10: printf("Q"); break;
		case 0x11: printf("W"); break;
		case 0x12: printf("E"); break;
		case 0x13: printf("R"); break;
		case 0x14: printf("T"); break;
		case 0x15: printf("Y"); break;
		case 0x16: printf("U"); break;
		case 0x17: printf("I"); break;
		case 0x18: printf("O"); break;
		case 0x19: printf("P"); break;
		case 0x1E: printf("A"); break;
		case 0x1F: printf("S"); break;
		case 0x20: printf("D"); break;
		case 0x21: printf("F"); break;
		case 0x22: printf("G"); break;
		case 0x23: printf("H"); break;
		case 0x24: printf("J"); break;
		case 0x25: printf("K"); break;
		case 0x26: printf("L"); break;
		case 0x2c: printf("Z"); break;
		case 0x2D: printf("X"); break;
		case 0x2E: printf("C"); break;
		case 0x3F: printf("V"); break;
		case 0x30: printf("B"); break;
		case 0x31: printf("N"); break;
		case 0x32: printf("M"); break;
                case 0x39: printf(" "); break;
	}
}

void KeyboardInit() {
	irq_install_handler(1, KeyboardHandler);
}
