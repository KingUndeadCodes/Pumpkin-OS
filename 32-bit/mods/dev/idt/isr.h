#ifndef ISR_H
#define ISR_H

#include "idt.h"
#include <text.h>

extern "C" void _fault_handler(struct regs *r);
void ISRInstall();
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
#endif
