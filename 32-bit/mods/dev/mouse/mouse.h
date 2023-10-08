#include "../idt/irq.h"
#include "../idt/idt.h"
#include "../port.cpp"
#include <text.h>

void mouse_handler(struct regs *regs);
inline void mouse_wait(unsigned char a_type);
inline void mouse_write(unsigned char a_write);
unsigned char mouse_read();
void mouse_install(void);