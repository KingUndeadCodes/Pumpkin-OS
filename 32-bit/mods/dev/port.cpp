#ifndef _PORT_CPP
#define _PORT_CPP
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline bool are_interrupts_enabled() {
    unsigned long flags;
    asm volatile ("pushf\n\t" "pop %0" : "=g"(flags));
    return flags & (1 << 9);
}
#endif