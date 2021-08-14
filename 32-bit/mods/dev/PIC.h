#ifndef PIC_H
#define PIC_H
#include "port.cpp"

#define PIC1 0x20
#define PIC2 0xA0
#define ICW1 0x11
#define ICW4 0x01

void init_pics(int pic1, int pic2);

#endif
