#include "isr.h"
extern "C" void ISR0();
extern "C" void ISR1();
extern "C" void ISR2();
extern "C" void ISR3();
extern "C" void ISR4();
extern "C" void ISR5();
extern "C" void ISR6();
extern "C" void ISR7();
extern "C" void ISR8();
extern "C" void ISR9();
extern "C" void ISR10();
extern "C" void ISR11();
extern "C" void ISR12();
extern "C" void ISR13();
extern "C" void ISR14();
extern "C" void ISR15();
extern "C" void ISR16();
extern "C" void ISR17();
extern "C" void ISR18();
extern "C" void ISR19();
extern "C" void ISR20();
extern "C" void ISR21();
extern "C" void ISR22();
extern "C" void ISR23();
extern "C" void ISR24();
extern "C" void ISR25();
extern "C" void ISR26();
extern "C" void ISR27();
extern "C" void ISR28();
extern "C" void ISR29();
extern "C" void ISR30();
extern "C" void ISR31();

void ISRInstall()
{
    IDTSetGate(0, (unsigned)ISR0, 0x08, 0x8E);
    IDTSetGate(1, (unsigned)ISR1, 0x08, 0x8E);
    IDTSetGate(2, (unsigned)ISR2, 0x08, 0x8E);
    IDTSetGate(3, (unsigned)ISR3, 0x08, 0x8E);
    IDTSetGate(4, (unsigned)ISR4, 0x08, 0x8E);
    IDTSetGate(5, (unsigned)ISR5, 0x08, 0x8E);
    IDTSetGate(6, (unsigned)ISR6, 0x08, 0x8E);
    IDTSetGate(7, (unsigned)ISR7, 0x08, 0x8E);
    IDTSetGate(8, (unsigned)ISR8, 0x08, 0x8E);
    IDTSetGate(9, (unsigned)ISR9, 0x08, 0x8E);
    IDTSetGate(10, (unsigned)ISR10, 0x08, 0x8E);
    IDTSetGate(11, (unsigned)ISR11, 0x08, 0x8E);
    IDTSetGate(12, (unsigned)ISR12, 0x08, 0x8E);
    IDTSetGate(13, (unsigned)ISR13, 0x08, 0x8E);
    IDTSetGate(14, (unsigned)ISR14, 0x08, 0x8E);
    IDTSetGate(15, (unsigned)ISR15, 0x08, 0x8E);
    IDTSetGate(16, (unsigned)ISR16, 0x08, 0x8E);
    IDTSetGate(17, (unsigned)ISR17, 0x08, 0x8E);
    IDTSetGate(18, (unsigned)ISR18, 0x08, 0x8E);
    IDTSetGate(19, (unsigned)ISR19, 0x08, 0x8E);
    IDTSetGate(20, (unsigned)ISR20, 0x08, 0x8E);
    IDTSetGate(21, (unsigned)ISR21, 0x08, 0x8E);
    IDTSetGate(22, (unsigned)ISR22, 0x08, 0x8E);
    IDTSetGate(23, (unsigned)ISR23, 0x08, 0x8E);
    IDTSetGate(24, (unsigned)ISR24, 0x08, 0x8E);
    IDTSetGate(25, (unsigned)ISR25, 0x08, 0x8E);
    IDTSetGate(26, (unsigned)ISR26, 0x08, 0x8E);
    IDTSetGate(27, (unsigned)ISR27, 0x08, 0x8E);
    IDTSetGate(28, (unsigned)ISR28, 0x08, 0x8E);
    IDTSetGate(29, (unsigned)ISR29, 0x08, 0x8E);
    IDTSetGate(30, (unsigned)ISR30, 0x08, 0x8E);
    IDTSetGate(31, (unsigned)ISR31, 0x08, 0x8E);
}


const char* exception_messages[] =
{
	"Division By Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Into Detected Overflow",
	"Out of Bounds",
	"Invalid Opcode",
	"No Coprocessor",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt",
	"Coprocessor Fault",
	"Alignment Check",
	"Machine Check",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

#include "../serial/serial.h"
#include "../vbe/vbe.h"
#include <string.h>

char* _utoa64_hex(uint64_t val, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[17 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    return buf;
}

namespace FaultHandler {
    // See docs/DOCS.md ("mods/dev/idt/isr.cpp" section) for why only #DB/#BP recover.
    static bool is_recoverable(unsigned int int_no) {
        return int_no == 1 || int_no == 3; // Debug, Breakpoint
    }

    static char* utoa32_hex(uint32_t val, char* buf) {
        const char* hex = "0123456789ABCDEF";
        buf[0] = '0'; buf[1] = 'x';
        for (int i = 0; i < 8; i++) {
            buf[9 - i] = hex[(val >> (i * 4)) & 0xF];
        }
        buf[10] = '\0';
        return buf;
    }

    // See docs/DOCS.md ("mods/dev/idt/isr.cpp" section) for the bounded-walk rationale.
    static void print_stack_trace(unsigned int ebp) {
        char buf[11];
        serial_write_string("Stack trace (return addresses):\n", false, NONE);
        for (int frame = 0; frame < 8; frame++) {
            if (ebp < 0x1000 || ebp > 0x08000000) break; // implausible frame pointer
            unsigned int* frame_ptr = (unsigned int*)ebp;
            unsigned int return_addr = frame_ptr[1];
            unsigned int next_ebp = frame_ptr[0];
            serial_write_string("  #", false, NONE);
            serial_write_string(itoa(frame, 10), false, NONE);
            serial_write_string(" ", false, NONE);
            serial_write_string(utoa32_hex(return_addr, buf), false, NONE);
            serial_write_string("\n", false, NONE);
            if (next_ebp <= ebp) break; // not a valid ascending chain (corrupt or cyclic)
            ebp = next_ebp;
        }
    }

    static void panic_draw_string(unsigned x, unsigned y, const char* str, unsigned scale) {
        unsigned pos = x;
        for (int i = 0; str[i] != '\0'; i++) {
            draw_char(pos, y, str[i], 0xFFFFFFFF, scale);
            pos += 8 * scale;
        }
    }

    // See docs/DOCS.md ("mods/dev/idt/isr.cpp" section) for why this can't use malloc/Surface/etc.
    static void draw_panic_screen(unsigned int int_no, unsigned int eip, unsigned int cr2, bool has_cr2) {
        // A panic can fire while region 1 is displayed; force region 0 back
        // on screen, permanently, since the system halts right after. See
        // docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- hardware double buffering").
        BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0);
        fill(0xFF5C1010);
        char buf[11];
        panic_draw_string(40, 40, "KERNEL PANIC", 4);
        panic_draw_string(40, 100, exception_messages[int_no], 3);
        panic_draw_string(40, 150, "EIP:", 2);
        panic_draw_string(40 + 5 * 16, 150, utoa32_hex(eip, buf), 2);
        if (has_cr2) {
            panic_draw_string(40, 175, "Fault address (CR2):", 2);
            panic_draw_string(40 + 21 * 16, 175, utoa32_hex(cr2, buf), 2);
        }
        panic_draw_string(40, 220, "See serial log for full diagnostics.", 2);
    }

} // namespace FaultHandler

extern "C" void _fault_handler(struct regs *r)
{
    if (r->int_no >= 32) return; // IRQs route through _irq_handler, not here; defensive only.

    if (FaultHandler::is_recoverable(r->int_no)) {
        serial_write_string("\n", false, NONE);
        serial_write_string(exception_messages[r->int_no], true, INFO);
        serial_write_string(" (continuing)\n", false, NONE);
        return;
    }

    unsigned int cr2 = 0;
    bool has_cr2 = (r->int_no == 14);
    if (has_cr2) {
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
    }

    serial_write_string("\n", false, NONE);
    serial_write_string(exception_messages[r->int_no], true, FAIL);
    serial_write_string(" Exception. System Halted!\n", false, NONE);
    // eip points at the faulting instruction itself for #UD, so this is
    // enough to locate it with `objdump -d kernel.bin` / the .map file.
    printf_serial(false, FAIL, "eip=%u esp=%u int_no=%u err_code=%u eax=%u ebx=%u ecx=%u edx=%u\n",
        r->eip, r->esp, r->int_no, r->err_code, r->eax, r->ebx, r->ecx, r->edx);

    if (has_cr2) {
        char buf[11];
        serial_write_string("Fault address (CR2): ", false, NONE);
        serial_write_string(FaultHandler::utoa32_hex(cr2, buf), false, NONE);
        serial_write_string("  [", false, NONE);
        serial_write_string((r->err_code & 1) ? "protection violation" : "not present", false, NONE);
        serial_write_string(", ", false, NONE);
        serial_write_string((r->err_code & 2) ? "write" : "read", false, NONE);
        serial_write_string(", ", false, NONE);
        serial_write_string((r->err_code & 4) ? "user" : "supervisor", false, NONE);
        serial_write_string("]\n", false, NONE);
    }

    FaultHandler::print_stack_trace(r->ebp);
    FaultHandler::draw_panic_screen(r->int_no, r->eip, cr2, has_cr2);

    // Halt the system
    for (;;);
}
