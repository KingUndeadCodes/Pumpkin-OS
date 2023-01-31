#include "../serial/serial.h"
#include "../idt/irq.h"
#include "../idt/idt.h"
#include "../port.cpp"
// #include <text.h>

unsigned char mouse_cycle = 0;
signed char mouse_byte[3];
signed char mouse_x = 0;
signed char mouse_y = 0;            

//Mouse functions
void mouse_handler(struct regs *regs) {
    switch(mouse_cycle) {
        case 0:
            mouse_byte[0] = inb(0x60);
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = inb(0x60);
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = inb(0x60);
            mouse_x = mouse_byte[1];
            mouse_y = mouse_byte[2];
            mouse_cycle = 0;
            break;
    }
    // serial_write_string(itoa((int)mouse_x, 10), false, NONE);
    // serial_write_string(" mouse!\n", false, NONE);
}

inline void mouse_wait(unsigned char a_type) {
    unsigned int _time_out = 100000; //unsigned int
    if(a_type==0) {
        while(_time_out--) {
            if((inb(0x64) & 1) == 1) return;
        }
        return;
    } else {
        while(_time_out--) if((inb(0x64) & 2) == 0) return;
        return;
    }
}

inline void mouse_write(unsigned char a_write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a_write);
}

unsigned char mouse_read() {
    mouse_wait(0); 
    return inb(0x60);
}

void mouse_install(void) {
    unsigned char _status;
    mouse_wait(1);
    outb(0x64, 0xA8);  
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    _status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, _status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
    irq_install_handler(12, mouse_handler);
}