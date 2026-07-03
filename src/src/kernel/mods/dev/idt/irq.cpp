#include "irq.h"
#include "../tasking/tasking.h"

extern "C" void IRQ0();
extern "C" void IRQ1();
extern "C" void IRQ2();
extern "C" void IRQ3();
extern "C" void IRQ4();
extern "C" void IRQ5();
extern "C" void IRQ6();
extern "C" void IRQ7();
extern "C" void IRQ8();
extern "C" void IRQ9();
extern "C" void IRQ10();
extern "C" void IRQ11();
extern "C" void IRQ12();
extern "C" void IRQ13();
extern "C" void IRQ14();
extern "C" void IRQ15();
extern "C" void syscall_handler();

void irq_remap(void) {
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0); // 0xFB
	outb(0xA1, 0x0); // 0xFF
}

void IRQInstall() {
    irq_remap();
    IDTSetGate(32, (unsigned)IRQ0, 0x08, 0x8E);
    IDTSetGate(33, (unsigned)IRQ1, 0x08, 0x8E);
    IDTSetGate(34, (unsigned)IRQ2, 0x08, 0x8E);
    IDTSetGate(35, (unsigned)IRQ3, 0x08, 0x8E);
    IDTSetGate(36, (unsigned)IRQ4, 0x08, 0x8E);
    IDTSetGate(37, (unsigned)IRQ5, 0x08, 0x8E);
    IDTSetGate(38, (unsigned)IRQ6, 0x08, 0x8E);
    IDTSetGate(39, (unsigned)IRQ7, 0x08, 0x8E);
    IDTSetGate(40, (unsigned)IRQ8, 0x08, 0x8E);
    IDTSetGate(41, (unsigned)IRQ9, 0x08, 0x8E);
    IDTSetGate(42, (unsigned)IRQ10, 0x08, 0x8E);
    IDTSetGate(43, (unsigned)IRQ11, 0x08, 0x8E);
    IDTSetGate(44, (unsigned)IRQ12, 0x08, 0x8E);
    IDTSetGate(45, (unsigned)IRQ13, 0x08, 0x8E);
    IDTSetGate(46, (unsigned)IRQ14, 0x08, 0x8E);
    IDTSetGate(47, (unsigned)IRQ15, 0x08, 0x8E);
    IDTSetGate(0x80, (unsigned)syscall_handler, 0x08, 0x8E);
}

static irq_handler_t irq_routines[IRQ_COUNT][IRQ_MAX_HANDLERS] = {};

int currentInterrupt = -1;

static bool irq_valid(int irq) {
    return irq >= 0 && irq < IRQ_COUNT;
}

void irq_install_handler(int irq, irq_handler_t handler) {
    if (!irq_valid(irq) || handler == NULL) return;

    // Avoid registering same handler twice.
    for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
        if (irq_routines[irq][i] == handler) {
            return;
        }
    }

    // Find empty slot.
    for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
        if (irq_routines[irq][i] == NULL) {
            irq_routines[irq][i] = handler;
            return;
        }
    }

    // Optional: log that IRQ handler table is full.
}

void irq_remove_handler(int irq, irq_handler_t handler) {
    if (!irq_valid(irq) || handler == NULL) return;

    for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
        if (irq_routines[irq][i] == handler) {
            irq_routines[irq][i] = NULL;
            return;
        }
    }
}

// Keep your old function as "remove all handlers on this IRQ".
void irq_uninstall_handler(int irq) {
    if (!irq_valid(irq)) return;

    for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
        irq_routines[irq][i] = NULL;
    }
}

/*
void irq_wait(int irq) {
    if (!irq_valid(irq)) return;

    uint32_t start_count = irq_counters[irq];

    while (irq_counters[irq] == start_count) {
        asm volatile("hlt");
    }
}
*/

extern "C" uint32_t* _irq_handler(struct regs *r) {
    int irq = (int)r->int_no - 32;

    // Send EOI before dispatching to registered handlers, not after: a
    // handler chain can end up doing unbounded work (e.g. a mouse click
    // launching a program that blocks on keyboard input), and delaying EOI
    // until it returns would hold this IRQ line "in service" at the PIC for
    // that entire duration, starving further interrupts on it.
    if (r->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);

    if (irq_valid(irq)) {
        currentInterrupt = irq;

        for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
            irq_handler_t handler = irq_routines[irq][i];

            if (handler != NULL) {
                handler(r);
            }
        }
    }

    // Only IRQ0 should trigger task scheduling.
    if (r->int_no == 32) {
        return scheduler_on_tick((uint32_t*)r);
    }

    return (uint32_t*)r;
}