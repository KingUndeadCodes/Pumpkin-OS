#ifndef PIT_H
#define PIT_H

#include "../tasking/tasking.h"
#include "../idt/irq.h"
#include "../idt/idt.h"
#include <graphics.h>
#include <text.h>

// void pit_init(uint32_t hz);

void TimerInit();
void timer_handler(struct regs *r);
void timer_wait(int ticks);

#endif
