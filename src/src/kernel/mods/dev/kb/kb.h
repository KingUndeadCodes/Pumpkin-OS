#ifndef __KB_H
#define __KB_H

#include "../idt/irq.h"
#include "../idt/idt.h"

#define MAX_CALLBACKS 32

typedef void (*GlobalKbCallback)(char key, bool shift, bool meta, unsigned char scancode);

typedef struct GlobalCallbackEntry {
	int id;
	GlobalKbCallback callback;
};

static const char keys[256] = {
	0x0, 0x0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x0, '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0x0, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x0, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/',
};

static const char shifted_keys[256] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?',
};

int kb_add_event(GlobalKbCallback callback);
void kb_remove_event(int id);
void kb_run_events(char key, unsigned char scancode);

void KeyboardHandler(struct regs *r);
void KeyboardInit();

#endif