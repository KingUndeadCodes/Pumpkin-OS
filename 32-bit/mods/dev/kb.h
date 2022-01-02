#include "idt/irq.h"
#include "idt/idt.h"
#include <text.h>

void KeyboardHandler(struct regs *r);
void KeyboardInit();
