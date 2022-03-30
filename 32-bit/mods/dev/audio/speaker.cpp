#include "speaker.h"

static void PlaySound(uint32_t nFrequence) {
    uint32_t x;
    uint8_t y;
    x = 1193180 / nFrequence;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t)(x));
    outb(0x42, (uint8_t)(x >> 8));
    y = inb(0x61);
    if (y != (y | 3)) {
 	outb(0x61, y | 3);
    }
}

static void Quiet() {
       uint8_t x = inb(0x61) & 0xFC;
       outb(0x61, x);
}

void beep() {
     PlaySound(1000);
     // timer_wait(10);
     Quiet();
}
