#include "kb.h"
#include "../serial/serial.h"

#define outputMode false

bool shift_key = false;
bool meta_mode = false;

#define MAX_KEYS 256
#define MAX_CALLBACKS_PER_KEY 10

static const char keys[256] = {
	0x0, 0x0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x0, '\t', // 0x00-0x0F
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0x0, 'a', 's',   // 0x10-0x1F
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x0, '\\', 'z', 'x', 'c', 'v',  // 0x20-0x2F
	'b', 'n', 'm', ',', '.', '/', 
};

static const char shifted_keys[256] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, '\t', 		 // 0x00-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',   // 0x10-0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',    // 0x20-0x2F
    'B', 'N', 'M', '<', '>', '?',
};

typedef struct CallbackEntry {
    int id;
    void (*callback)(void);
};

static CallbackEntry callbacks[MAX_KEYS][MAX_CALLBACKS_PER_KEY] = {0};
static int nextId[MAX_KEYS] = {0};

int kb_add_event(char key, void (*callback)(void)) {
	int keyIndex = (int)key;
	if (key < 0) {
		keyIndex = 128 + (key * -1);
	} 
    for (int i = 0; i < MAX_CALLBACKS_PER_KEY; ++i) {
        if (callbacks[keyIndex][i].callback == nullptr) {
            callbacks[keyIndex][i].callback = callback;
            callbacks[keyIndex][i].id = nextId[keyIndex];
            return nextId[keyIndex]++;
        }
    }
    return -1;
}

void kb_remove_event(char key, int id) {
    int keyIndex = (int)key;
	if (key < 0) {
		keyIndex = 128 + (key * -1);
	} 
    for (int i = 0; i < MAX_CALLBACKS_PER_KEY; ++i) {
        if (callbacks[keyIndex][i].id == id) {
            callbacks[keyIndex][i].callback = nullptr;
            callbacks[keyIndex][i].id = 0;
            break;
        }
    }
}

void kb_remove_all_events(char key) {
    int keyIndex = (int)key;
	if (key < 0) {
		keyIndex = 128 + (key * -1);
	} 
    for (int i = 0; i < MAX_CALLBACKS_PER_KEY; ++i) {
        callbacks[keyIndex][i].callback = nullptr;
        callbacks[keyIndex][i].id = 0;
    }
}

// Runs all callbacks associated with a given key
void kb_run_events(char key) {
    int keyIndex = (int)key;
	if (key < 0) {
		keyIndex = 128 + (key * -1);
	} 
    for (int i = 0; i < MAX_CALLBACKS_PER_KEY; i++) {
        if (callbacks[keyIndex][i].callback != nullptr) {
            callbacks[keyIndex][i].callback(); // Execute the callback
        }
    }
}

void output_key(char key) {
	const char string[] = { key, '\0' };
	// serial_write_string(string, false, NONE);
	printf("%c", key);
}

void KeyboardHandler(struct regs *r) {
    unsigned char scancode = inb(0x60);
	const char* tab = "        ";
	if (scancode & 0x80 && scancode == 0xaa) shift_key = false;
	if (meta_mode) {
		switch (scancode) {
			case 0x48: kb_run_events(-10); break;
			case 0x4B: kb_run_events(-20); break;
			case 0x4D: kb_run_events(-30); break;
			case 0x50: kb_run_events(-40); break;
			default: break;
		}
		meta_mode = false;
	} else {
		if (scancode == 0xE0 /* || scancode == 0x36 */) {
			meta_mode = true;
			return;
		} else if (scancode == 0x2A) { 
			shift_key = true;
			return;
		} else if (scancode == 0x39) {
			kb_run_events(' ');
			#if outputMode == true
			output_key(' ');
			#endif
			return;
		} else if (scancode == 0x0E) {
			kb_run_events('\b');
			#if outputMode == true
		 	output_key('\b');
			#endif
			return;
		}
		if (scancode > 0x3A) return;
		if (shift_key == true) {
			kb_run_events(shifted_keys[scancode]);
			#if outputMode == true
			output_key(shifted_keys[scancode]);
			#endif
		} else {
			kb_run_events(keys[scancode]);
			#if outputMode == true
			output_key(keys[scancode]);
			#endif
		}
	}
}

void KeyboardInit() {
	irq_install_handler(1, KeyboardHandler);
}