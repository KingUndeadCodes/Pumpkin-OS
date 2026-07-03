#include "pit.h"

volatile uint64_t timer_ticks = 0;

/* Handles the timer. In this case, it's very simple: We
*  increment the 'timer_ticks' variable every time the
*  timer fires. Rate is whatever pit_init() last programmed
*  (1000Hz if called as recommended, so this is milliseconds
*  since boot rather than the PIT's uninitialized ~18.222Hz default). */
void timer_handler(struct regs *r)
{
    /* Increment our 'tick count' */
    timer_ticks++;
    // scheduler_tick(r);
}

/* Programs PIT channel 0 (ports 0x40/0x43) for the given interrupt rate.
 * Call before enabling interrupts so the very first IRQ0 already ticks at
 * the desired rate. */
void pit_init(uint32_t hz)
{
    uint32_t divisor = PIT_BASE_FREQUENCY / hz;
    outb(0x43, 0x36); // channel 0, lobyte/hibyte access, mode 3 (square wave), binary
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

/* Sets up the system clock by installing the timer handler
*  into IRQ0 */
void TimerInit()
{
    /* Installs 'timer_handler' to IRQ0 */
    irq_install_handler(0, timer_handler);
}

void timer_wait(int ticks)
{
    uint64_t eticks;
    eticks = timer_ticks + ticks;
    while(timer_ticks < eticks) asm("hlt");
}
