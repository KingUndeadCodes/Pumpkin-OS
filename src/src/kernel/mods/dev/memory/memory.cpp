#include "memory.h"
#include "../serial/serial.h"

#define MMAP_LOCATION 0x5000
#define MMAP_ENTRY_COUNT_PTR ((volatile uint16_t*)MMAP_LOCATION)
#define INFO 0 // or whatever level you're using
#define MAX_USABLE_REGIONS 32

// Helper: convert uint64_t to hex string
char* utoa64_hex(uint64_t val, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[17 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    return buf;
}

extern uint32_t endkernel; // Defined in linker script, this is the end of the kernel binary in memory.

void queryMemoryMap(void) {
    
    // serial_write_string("End of kernel: ", INFO);
    // char end_buf[20];
    // serial_write_string(utoa64_hex((uint64_t)&endkernel, end_buf), false, NONE);
    // serial_write_string("\n", false, NONE);

    SMAP_entry_t* memory_map = (SMAP_entry_t*)(MMAP_LOCATION + 4);
    uint16_t entries = *MMAP_ENTRY_COUNT_PTR;

    // See docs/DOCS.md ("mods/dev/memory/memory.cpp" section) for why every
    // usable region is collected before calling init_phys_allocator() once,
    // instead of the old one-region-picked-by-counter approach.
    mem_region_t usable_regions[MAX_USABLE_REGIONS];
    int usable_count = 0;

    for (int i = 0; i < entries; i++) {
        SMAP_entry_t* entry = &memory_map[i];

        uint64_t base = ((uint64_t)entry->BaseH << 32) | entry->BaseL;
        uint64_t length = ((uint64_t)entry->LengthH << 32) | entry->LengthL;

        if (entry->Type == 1) {
            // Usable memory
            serial_write_string("SMAP Entry: Base: ", false, INFO);
            if (usable_count < MAX_USABLE_REGIONS) {
                usable_regions[usable_count].base = base;
                usable_regions[usable_count].length = length;
                usable_regions[usable_count].type = 1; // Usable RAM
                usable_count++;
            }
        } else if (entry->Type == 2) {
            // Reserved memory
            serial_write_string("SMAP Entry: Reserved Base: ", false, INFO);
        } else if (entry->Type == 3) {
            // ACPI reclaimable memory
            serial_write_string("SMAP Entry: ACPI Base: ", false, INFO);
        } else {
            // Unknown type
            serial_write_string("SMAP Entry: Unknown Type ", false, INFO);
        }
        char base_buf[20], length_buf[20];
        serial_write_string(utoa64_hex(base, base_buf), false, NONE);
        serial_write_string(", Length: ", false, NONE);
        serial_write_string(utoa64_hex(length, length_buf), false, NONE);
        serial_write_string("\n", false, NONE);
    }

    if (usable_count > 0) {
        uint64_t total_usable_bytes = 0;
        for (int i = 0; i < usable_count; i++) total_usable_bytes += usable_regions[i].length;
        printf_serial(false, INFO, "Physical allocator: %u usable region(s), %u KB total\n",
            (unsigned)usable_count, (unsigned)(total_usable_bytes / 1024));
        init_phys_allocator(usable_regions, usable_count);
    }
}