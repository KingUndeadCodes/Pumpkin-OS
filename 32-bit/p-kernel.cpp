/*
 * Copyright (C) 2022 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/serial/serial.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/cmos/cmos.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/kb/kb.h"
#include <graphics.h>
#include <tasking.h>
#include <text.h>

extern "C" void _start() {
    #define BLUE (uint8_t)COLOR_CYAN | COLOR_BLACK << 4
    #define GREEN (uint8_t)COLOR_GREEN | COLOR_BLACK << 4
    #define PURPLE (uint8_t)COLOR_LIGHT_PURPLE | COLOR_BLACK << 4
    Cursor::enableCursor(0, 10);
    print("Booting PumpkinOS (ver: 0)\n\n", BLUE);
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
    initTasking();
    print(" - Checking for PCI devices...\n", PURPLE);
    fork(checkAllBuses);
    yield();
    // Screen::Fill();
    // Screen::DrawIcon(0, 145, 70);
    // Screen::DrawChar('.', 145, 100);
    // Screen::DrawChar('.', 157, 100);
    // Screen::DrawChar('.', 169, 100);
    /// MALLOC_TEST();
}