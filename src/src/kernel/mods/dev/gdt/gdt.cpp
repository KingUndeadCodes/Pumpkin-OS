#include "gdt.h"
#include <string.h>

// A second, C++-built GDT superseding the bootloader's -- the bootloader's can't host a
// TSS descriptor, since a TSS needs &g_tss, a C++ address that doesn't exist at boot time.
struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct TSS {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

struct GDTEntry GDT[8];
struct GDTPointer _GDTPointer;
static struct TSS g_tss;

extern "C" void GDTLoad();

static void GDTSetEntry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    GDT[idx].base_low    = base & 0xFFFF;
    GDT[idx].base_mid    = (base >> 16) & 0xFF;
    GDT[idx].base_high   = (base >> 24) & 0xFF;
    GDT[idx].limit_low   = limit & 0xFFFF;
    GDT[idx].granularity = (granularity & 0xF0) | ((limit >> 16) & 0x0F);
    GDT[idx].access      = access;
}

void GDTInstall(void) {
    GDTSetEntry(0, 0, 0, 0x00, 0x00);                    // null
    GDTSetEntry(1, 0, 0xFFFFF, 0x9A, 0xC0);               // 0x08 ring0 code
    GDTSetEntry(2, 0, 0xFFFFF, 0x92, 0xC0);               // 0x10 ring0 data
    GDTSetEntry(3, 0, 0xFFFFF, 0xBA, 0xC0);               // 0x18 ring1 code
    GDTSetEntry(4, 0, 0xFFFFF, 0xB2, 0xC0);               // 0x20 ring1 data
    GDTSetEntry(5, 0, 0xFFFFF, 0xFA, 0xC0);               // 0x28 ring3 code
    GDTSetEntry(6, 0, 0xFFFFF, 0xF2, 0xC0);               // 0x30 ring3 data
    GDTSetEntry(7, 0, 0, 0x89, 0x00);                     // 0x38 TSS -- base/limit patched by TSSInstall()

    _GDTPointer.limit = sizeof(GDT) - 1;
    _GDTPointer.base = (uint32_t)&GDT;
    GDTLoad();
}

void TSSInstall(void) {
    memset(&g_tss, 0, sizeof(g_tss));
    g_tss.ss0 = 0x10;
    g_tss.esp0 = 0; // patched by tasking.cpp on every context switch once tasks exist
    g_tss.iomap_base = sizeof(struct TSS);

    GDTSetEntry(7, (uint32_t)&g_tss, sizeof(struct TSS) - 1, 0x89, 0x00);

    asm volatile ("ltr %%ax" :: "a"((uint16_t)0x38));
}

// Called every context switch so esp0 is valid before any interrupt can catch a ring>0 task running.
void GDTSetKernelStack(uint32_t esp0) {
    g_tss.esp0 = esp0;
}
