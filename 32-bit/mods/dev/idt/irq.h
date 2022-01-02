#include "idt.h"
#include "../port.cpp"
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
void irq_install_handler(int irq, void (*handler)(struct regs *r));
void irq_uninstall_handler(int irq);
void irq_remap(void);
void IRQInstall();
extern "C" void _irq_handler(struct regs *r);
void irq_wait(int n);
