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

static inline uint32_t syscall(uint32_t number, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    uint32_t ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
        : "memory"
    );
    return ret;
}

inline void cpu_get_MSR(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

inline void cpu_set_MSR(uint32_t msr, uint32_t lo, uint32_t hi) {
    asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

inline uint16_t flip_short(uint16_t short_int) {
    uint32_t first_byte = *((uint8_t*)(&short_int));
    uint32_t second_byte = *((uint8_t*)(&short_int) + 1);
    return (first_byte << 8) | (second_byte);
}

inline uint8_t flip_byte(uint8_t byte, int num_bits) {
    uint8_t t = byte << (8 - num_bits);
    return t | (byte >> num_bits);
}

inline uint16_t htons(uint16_t hostshort) {
    uint32_t first_byte = *((uint8_t*)(&hostshort));
    uint32_t second_byte = *((uint8_t*)(&hostshort) + 1);
    return (first_byte << 8) | (second_byte);
}

inline uint16_t ntohs(uint16_t netshort) {
    uint32_t first_byte = *((uint8_t*)(&netshort));
    uint32_t second_byte = *((uint8_t*)(&netshort) + 1);
    return (first_byte << 8) | (second_byte);
}

inline uint8_t ntohb(uint8_t byte, int num_bits) {
    return flip_byte(byte, 8 - num_bits);
}

inline uint8_t htonb(uint8_t byte, int num_bits) {
    return flip_byte(byte, num_bits);
}

#endif