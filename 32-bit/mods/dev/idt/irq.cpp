#include "irq.h"
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

void *irq_routines[16] =
{
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};


void irq_install_handler(int irq, void (*handler)(struct regs *r))
{
	irq_routines[irq] = (void*)handler;
}


void irq_uninstall_handler(int irq)
{
	irq_routines[irq] = 0;
}


void irq_remap(void)
{
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);
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
}

int currentInterrupt = -1;

extern "C" void _irq_handler(struct regs *r)
{
	currentInterrupt = r -> int_no;
	void (*handler)(struct regs *r);
	handler = (void (*)(regs*))irq_routines[r->int_no - 32];
	if (handler) {
		handler(r);
	}
	if (r->int_no >= 40) {
	   	outb(0xA0, 0x20);
	}
	outb(0x20, 0x20);
}

void irq_wait(int n){
	while(currentInterrupt != n){};
	currentInterrupt = -1;
}
