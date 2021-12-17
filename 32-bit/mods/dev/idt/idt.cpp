#include <string.h>

struct IDTEntry {
	unsigned short base_lo;
	unsigned short sel;
	unsigned char always0;
	unsigned char flags;
	unsigned short base_hi;
} __attribute__((packed));

struct IDTPointer {
	unsigned short limit;
	unsigned int base;
} __attribute__((packed));


struct IDTEntry IDT[256];
struct IDTPointer _IDTPointer;

extern "C" void IDTLoad();

void IDTSetGate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
     IDT[num].base_lo = (base & 0xFFFF);
     IDT[num].base_hi = (base >> 16) & 0xFFFF;
     IDT[num].sel = sel;
     IDT[num].always0 = 0;
     IDT[num].flags = flags;
}

void IDTInstall() {
	_IDTPointer.limit = (sizeof (struct IDTEntry) * 256) - 1;
	_IDTPointer.base = (unsigned int)&IDT;
	memset(&IDT, 0, sizeof(struct IDTEntry) * 256);
	/* Add any new ISRs to the IDT here using idt_set_gate */
	/* Points the processor's internal register to the new IDT */
	IDTLoad();
}
