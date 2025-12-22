#include "memory.h"
#include "../serial/serial.h"

#define MMAP_LOCATION 0x5000
#define MMAP_ENTRY_COUNT_PTR ((volatile uint16_t*)MMAP_LOCATION)
#define INFO 0 // or whatever level you're using

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
    int baseCount = 0;

    for (int i = 0; i < entries; i++) {
        SMAP_entry_t* entry = &memory_map[i];

        uint64_t base = ((uint64_t)entry->BaseH << 32) | entry->BaseL;
        uint64_t length = ((uint64_t)entry->LengthH << 32) | entry->LengthL;

        if (entry->Type == 1) {
            // Usable memory
            serial_write_string("SMAP Entry: Base: ", INFO);
            if (baseCount++ == 1) {
                mem_region_t region;
                region.base = base;
                region.length = length;
                region.type = 1; // Usable RAM
                // Copy region to targetEntries making sure to reallocate by one
                init_phys_allocator(&region, 1);
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
}