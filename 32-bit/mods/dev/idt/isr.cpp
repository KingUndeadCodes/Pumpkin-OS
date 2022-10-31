#include "isr.h"
extern "C" void ISR0();
extern "C" void ISR1();
extern "C" void ISR2();
extern "C" void ISR3();
extern "C" void ISR4();
extern "C" void ISR5();
extern "C" void ISR6();
extern "C" void ISR7();
extern "C" void ISR8();
extern "C" void ISR9();
extern "C" void ISR10();
extern "C" void ISR11();
extern "C" void ISR12();
extern "C" void ISR13();
extern "C" void ISR14();
extern "C" void ISR15();
extern "C" void ISR16();
extern "C" void ISR17();
extern "C" void ISR18();
extern "C" void ISR19();
extern "C" void ISR20();
extern "C" void ISR21();
extern "C" void ISR22();
extern "C" void ISR23();
extern "C" void ISR24();
extern "C" void ISR25();
extern "C" void ISR26();
extern "C" void ISR27();
extern "C" void ISR28();
extern "C" void ISR29();
extern "C" void ISR30();
extern "C" void ISR31();

void ISRInstall()
{
    IDTSetGate(0, (unsigned)ISR0, 0x08, 0x8E);
    IDTSetGate(1, (unsigned)ISR1, 0x08, 0x8E);
    IDTSetGate(2, (unsigned)ISR2, 0x08, 0x8E);
    IDTSetGate(3, (unsigned)ISR3, 0x08, 0x8E);
    IDTSetGate(4, (unsigned)ISR4, 0x08, 0x8E);
    IDTSetGate(5, (unsigned)ISR5, 0x08, 0x8E);
    IDTSetGate(6, (unsigned)ISR6, 0x08, 0x8E);
    IDTSetGate(7, (unsigned)ISR7, 0x08, 0x8E);
    IDTSetGate(8, (unsigned)ISR8, 0x08, 0x8E);
    IDTSetGate(9, (unsigned)ISR9, 0x08, 0x8E);
    IDTSetGate(10, (unsigned)ISR10, 0x08, 0x8E);
    IDTSetGate(11, (unsigned)ISR11, 0x08, 0x8E);
    IDTSetGate(12, (unsigned)ISR12, 0x08, 0x8E);
    IDTSetGate(13, (unsigned)ISR13, 0x08, 0x8E);
    IDTSetGate(14, (unsigned)ISR14, 0x08, 0x8E);
    IDTSetGate(15, (unsigned)ISR15, 0x08, 0x8E);
    IDTSetGate(16, (unsigned)ISR16, 0x08, 0x8E);
    IDTSetGate(17, (unsigned)ISR17, 0x08, 0x8E);
    IDTSetGate(18, (unsigned)ISR18, 0x08, 0x8E);
    IDTSetGate(19, (unsigned)ISR19, 0x08, 0x8E);
    IDTSetGate(20, (unsigned)ISR20, 0x08, 0x8E);
    IDTSetGate(21, (unsigned)ISR21, 0x08, 0x8E);
    IDTSetGate(22, (unsigned)ISR22, 0x08, 0x8E);
    IDTSetGate(23, (unsigned)ISR23, 0x08, 0x8E);
    IDTSetGate(24, (unsigned)ISR24, 0x08, 0x8E);
    IDTSetGate(25, (unsigned)ISR25, 0x08, 0x8E);
    IDTSetGate(26, (unsigned)ISR26, 0x08, 0x8E);
    IDTSetGate(27, (unsigned)ISR27, 0x08, 0x8E);
    IDTSetGate(28, (unsigned)ISR28, 0x08, 0x8E);
    IDTSetGate(29, (unsigned)ISR29, 0x08, 0x8E);
    IDTSetGate(30, (unsigned)ISR30, 0x08, 0x8E);
    IDTSetGate(31, (unsigned)ISR31, 0x08, 0x8E);
}


const char* exception_messages[] = 
{
	"Division By Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Into Detected Overflow",
	"Out of Bounds",
	"Invalid Opcode",
	"No Coprocessor",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt",
	"Coprocessor Fault",
	"Alignment Check",
	"Machine Check",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

extern "C" void _fault_handler(struct regs *r)
{
    if (r->int_no < 32) {
        printf(exception_messages[r->int_no]);
        printf(" Exception. System Halted!\n");
        for (;;);
    }
}
