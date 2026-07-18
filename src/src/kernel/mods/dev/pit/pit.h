#ifndef PIT_H
#define PIT_H

#include "../tasking/tasking.h"
#include "../idt/irq.h"
#include "../idt/idt.h"
#include "../console/console.h"
#include <stdint.h>

#define PIT_BASE_FREQUENCY 1193182u

// Ticks since boot. Once pit_init() has been called this many times per
// second, this is directly milliseconds since boot at 1000Hz.
extern volatile uint64_t timer_ticks;

void pit_init(uint32_t hz);
void TimerInit();
void timer_handler(struct regs *r);
void timer_wait(int ticks);

#endif
