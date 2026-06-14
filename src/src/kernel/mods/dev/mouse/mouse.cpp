#include "./mouse.h"
#include "../serial/serial.h"

static unsigned char mouse_cycle = 0;
static unsigned char mouse_packet[3];

static int mouse_x = 0;
static int mouse_y = 0;
static unsigned char mouse_buttons = 0;

static GlobalMouseCallbackEntry global_mouse_callbacks[MAX_MOUSE_CALLBACKS] = {0};
static int next_mouse_global_id = 1;

int mouse_add_event(GlobalMouseCallback callback) {
    for (int i = 0; i < MAX_MOUSE_CALLBACKS; i++) {
        if (global_mouse_callbacks[i].callback == NULL) {
            global_mouse_callbacks[i].callback = callback;
            global_mouse_callbacks[i].id = next_mouse_global_id++;
            return global_mouse_callbacks[i].id;
        }
    }

    return -1;
}

void mouse_remove_event(int id) {
    for (int i = 0; i < MAX_MOUSE_CALLBACKS; i++) {
        if (global_mouse_callbacks[i].id == id) {
            global_mouse_callbacks[i].callback = NULL;
            global_mouse_callbacks[i].id = 0;
            return;
        }
    }
}

void mouse_run_events(int x, int y, int dx, int dy, unsigned char buttons) {
    for (int i = 0; i < MAX_MOUSE_CALLBACKS; i++) {
        if (global_mouse_callbacks[i].callback != NULL) {
            global_mouse_callbacks[i].callback(x, y, dx, dy, buttons);
        }
    }
}

int mouse_get_x(void) {
    return mouse_x;
}

int mouse_get_y(void) {
    return mouse_y;
}

unsigned char mouse_get_buttons(void) {
    return mouse_buttons;
}

void mouse_handler(struct regs *regs) {
    unsigned char status = inb(0x64);

    /*
     * Bit 0 = output buffer full.
     * Bit 5 = data came from mouse/aux device.
     */
    if ((status & 0x01) == 0) {
        return;
    }

    if ((status & 0x20) == 0) {
        return;
    }

    unsigned char data = inb(0x60);

    if (mouse_cycle == 0) {
        /*
         * First mouse byte should always have bit 3 set.
         * This keeps packet alignment.
         */
        if ((data & 0x08) == 0) {
            return;
        }

        mouse_packet[0] = data;
        mouse_cycle = 1;
        return;
    }

    if (mouse_cycle == 1) {
        mouse_packet[1] = data;
        mouse_cycle = 2;
        return;
    }

    if (mouse_cycle == 2) {
        mouse_packet[2] = data;
        mouse_cycle = 0;

        /*
         * Drop packet if X/Y overflow happened.
         */
        if (mouse_packet[0] & 0xC0) {
            return;
        }

        int dx = (signed char)mouse_packet[1];
        int dy = (signed char)mouse_packet[2];

        mouse_buttons = mouse_packet[0] & 0x07;

        mouse_x += dx;
        mouse_y -= dy;

        /*
         * Optional simple clamping.
         * Change these to your actual screen dimensions.
         */
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;

        // Example for 1024x768:
        // if (mouse_x > 1023) mouse_x = 1023;
        // if (mouse_y > 767)  mouse_y = 767;

        mouse_run_events(mouse_x, mouse_y, dx, -dy, mouse_buttons);

        // Debug only. Disable later because serial inside IRQs can slow the mouse.
        // printf_serial(false, NONE, "Mouse: (%d, %d), delta=(%d, %d), buttons=%d\n",
        //     mouse_x, mouse_y, dx, -dy, mouse_buttons);
    }
}

void mouse_wait(unsigned char type) {
    unsigned int timeout = 100000;

    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 0x01) == 1) {
                return;
            }
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 0x02) == 0) {
                return;
            }
        }
    }
}

void mouse_write(unsigned char value) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, value);
}

unsigned char mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

static void mouse_flush(void) {
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
}

void mouse_install(void) {
    unsigned char status;
    mouse_cycle = 0;
    mouse_x = 0;
    mouse_y = 0;
    mouse_buttons = 0;
    mouse_flush();
    mouse_wait(1);
    outb(0x64, 0xA8);
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = inb(0x60);
    status |= 0x02;
    status &= ~0x20;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
    irq_install_handler(12, mouse_handler);
}