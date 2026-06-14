#ifndef __MOUSE_H
#define __MOUSE_H

#include "../idt/irq.h"
#include "../idt/idt.h"
#include "../port.cpp"

#define MAX_MOUSE_CALLBACKS 32

#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

typedef void (*GlobalMouseCallback)(
    int x,
    int y,
    int dx,
    int dy,
    unsigned char buttons
);

typedef struct GlobalMouseCallbackEntry {
    int id;
    GlobalMouseCallback callback;
} GlobalMouseCallbackEntry;

int mouse_add_event(GlobalMouseCallback callback);
void mouse_remove_event(int id);
void mouse_run_events(int x, int y, int dx, int dy, unsigned char buttons);
void mouse_handler(struct regs *regs);
void mouse_wait(unsigned char type);
void mouse_write(unsigned char value);
unsigned char mouse_read(void);
void mouse_install(void);
int mouse_get_x(void);
int mouse_get_y(void);
unsigned char mouse_get_buttons(void);

#endif