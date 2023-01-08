#ifndef PIT_H
#define PIT_H

#include "../idt/irq.h"
#include "../idt/idt.h"
#include <text.h>

void TimerInit();
void timer_handler(struct regs *r);
void timer_wait(int ticks);

#endif
