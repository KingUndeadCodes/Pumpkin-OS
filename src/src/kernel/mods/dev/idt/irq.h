#ifndef __IRQ_H
#define __IRQ_H

#include "idt.h"
#include "isr.h"

#ifndef _PORT_CPP
    #include "../port.cpp"
#endif

#define IRQ_COUNT 16
#define IRQ_MAX_HANDLERS 8

typedef void (*irq_handler_t)(struct regs *r);

void irq_install_handler(int irq, irq_handler_t handler);
void irq_remove_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

void irq_remap(void);
void IRQInstall();

extern "C" uint32_t* _irq_handler(struct regs *r);

// void irq_wait(int irq);

#endif