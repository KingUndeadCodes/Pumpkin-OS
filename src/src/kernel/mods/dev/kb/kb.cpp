#include "kb.h"
#include "../serial/serial.h"

bool shift_key = false;
bool meta_mode = false;

static GlobalCallbackEntry global_callbacks[MAX_CALLBACKS] = {0};
static int next_global_id = 1;

int kb_add_event(GlobalKbCallback callback) {
	for (int i = 0; i < MAX_CALLBACKS; i++) {
		if (global_callbacks[i].callback == NULL) {
			global_callbacks[i].callback = callback;
			global_callbacks[i].id = next_global_id++;
			return global_callbacks[i].id;
		}
	}
	return -1;
}

void kb_remove_event(int id) {
	for (int i = 0; i < MAX_CALLBACKS; i++) {
		if (global_callbacks[i].id == id) {
			global_callbacks[i].callback = NULL;
			global_callbacks[i].id = 0;
			break;
		}
	}
}

void kb_run_events(char key, unsigned char scancode) {
	for (int i = 0; i < MAX_CALLBACKS; i++) {
		if (global_callbacks[i].callback != NULL) {
			global_callbacks[i].callback(key, shift_key, meta_mode, scancode);
		}
	}
}

void KeyboardHandler(struct regs *r) {
	unsigned char scancode = inb(0x60);

	if (scancode & 0x80) {
		if (scancode == 0xaa) shift_key = false;
		return;  // ignore key releases
	}

	if (scancode == 0x2A) { // Shift down
		shift_key = true;
		return;
	}

	if (scancode == 0xE0) {
		meta_mode = true;
		return;
	}

	char key = 0;

	if (meta_mode) {
		switch (scancode) {
			case 0x48: key = -10; break; // Up
			case 0x4B: key = -20; break; // Left
			case 0x4D: key = -30; break; // Right
			case 0x50: key = -40; break; // Down
		}
		meta_mode = false;
	} else if (scancode == 0x39) {
		key = ' ';
	} else if (scancode == 0x0E) {
		key = '\b';
	} else if (scancode <= 0x3A) {
		key = shift_key ? shifted_keys[scancode] : keys[scancode];
	}

	if (key != 0) {
		kb_run_events(key, scancode);
	}
}

void resetKeyboard() {
	// You might need to flush the keyboard output buffer, especially if the bootloader uses the keyboard for input.
	// See Step 4 on the OSDev wiki for the PS/2 controller initialization:
	// https://wiki.osdev.org/I8042_PS/2_Controller
	inb(0x60); // Clear the keyboard buffer
	return;
}

void KeyboardInit() {
	resetKeyboard();
	irq_install_handler(1, KeyboardHandler);
}