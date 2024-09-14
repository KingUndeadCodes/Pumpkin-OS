#include "../idt/irq.h"
#include "../idt/idt.h"
#include <text.h>

int kb_add_event(char key, void (*callback)(void));
void kb_remove_event(char key, int id);
void kb_remove_all_events(char key);

void KeyboardHandler(struct regs *r);
void KeyboardInit();