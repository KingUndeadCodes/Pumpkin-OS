/*
 * Copyright (C) 2023 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/serial/serial.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/cmos/cmos.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/kb/kb.h"
#include <tasking.h>
#include <text.h>

extern "C" void kernel_start() {
    #define BLUE (uint8_t)COLOR_CYAN | COLOR_BLACK << 4
    #define GREEN (uint8_t)COLOR_GREEN | COLOR_BLACK << 4
    #define PURPLE (uint8_t)COLOR_LIGHT_PURPLE | COLOR_BLACK << 4
    Cursor::enableCursor(0, 10);
    print("Booting Pumpkin-OS (ver: 0)\n\n", BLUE);
    IDTInstall();
    ISRInstall();
    IRQInstall();
    asm volatile ("sti");
    if (!are_interrupts_enabled()) { serial_write_string("interupt setup failed. system halted!\n", FAIL); abort(); }
    print(" - Interupts Enabled!\n", GREEN);
    KeyboardInit();
    print(" - Keyboard Enabled!\n", GREEN);
    TimerInit();
    print(" - PIT Enabled!\n", GREEN);
    PagingInstall();
    print(" - Paging Enabled!\n", GREEN);
    initializeMem();
    print(" - Tasking Enabled!\n", GREEN);
    // initTasking();
    // print(" - Checking for PCI devices...\n", PURPLE);
    // fork(checkAllBuses);
    // checkAllBuses();
}