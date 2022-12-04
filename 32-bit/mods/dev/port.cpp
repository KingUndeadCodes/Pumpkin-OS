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

static inline uint32_t inl(uint16_t port) {
    uint32_t res;
    asm volatile ("inl %%dx, %%eax" : "=a" (res) : "dN" (port));
    return res;
}

static inline void outl(uint16_t port, uint32_t value) {
    asm volatile ("out %%eax, %%dx\n" :: "d" (port), "a" (value));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a" (data) : "dN" (port));
    return data;
}

static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (data));
}

static inline bool are_interrupts_enabled() {
    unsigned long flags;
    asm volatile ("pushf\n\t" "pop %0" : "=g"(flags));
    return flags & (1 << 9);
}

static inline void io_wait(void) {
    outb(0x80, 0);
}
#endif