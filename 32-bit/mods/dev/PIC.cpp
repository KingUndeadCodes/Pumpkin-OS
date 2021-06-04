#include "port.cpp"

#define PIC1 0x20
#define PIC2 0xA0
#define ICW1 0x11
#define ICW4 0x01
#define PIT_CTRL 0x43
#define PIT_CH_0 0x40

inline void init_pics(int pic1, int pic2) {
   outb(PIC1, ICW1);
   outb(PIC2, ICW1);
   outb(PIC1 + 1, pic1);
   outb(PIC2 + 1, pic2);
   outb(PIC1 + 1, 4);
   outb(PIC2 + 1, 2);
   outb(PIC1 + 1, ICW4);
   outb(PIC2 + 1, ICW4);
   outb(PIC1 + 1, 0xFF);
}

// https://forum.osdev.org/viewtopic.php?f=1&t=8330
inline void init_timer(unsigned char FREQ) {
   unsigned int counter = 1193181 / FREQ;
   outb(PIT_CTRL, 0x34);
   outb(PIT_CH_0, counter % 256);
   outb(PIT_CH_0, counter / 256);
}
