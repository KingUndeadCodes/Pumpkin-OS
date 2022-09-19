/*
 * Copyright (C) 2022 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/audio/speaker.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/kb/kb.h"
#include "mods/dev/fs/fs.h"
#include <stdlib.h>
#include <text.h>

extern "C" void _start() {
    #define BLUE (uint8_t)COLOR_CYAN | COLOR_BLACK << 4
    #define GREEN (uint8_t)COLOR_GREEN | COLOR_BLACK << 4
    #define PURPLE (uint8_t)COLOR_LIGHT_PURPLE | COLOR_BLACK << 4
    Cursor::enableCursor(0, 10);
    print("Booting PumpkinOS (ver: 0)\n\n", BLUE);
    initializeMem();
    IDTInstall();
    ISRInstall();
    IRQInstall();
    asm volatile ("sti");
    if (are_interrupts_enabled()) print(" - Interupts Enabled!\n", GREEN);
    KeyboardInit();
    print(" - Keyboard Enabled!\n", GREEN);
    TimerInit();
    print(" - PIT Enabled!\n", GREEN);
    print(" - Checking for PCI devices...\n", PURPLE);
    checkAllBuses();
    // never_gonna();
    FS_TEST();
    // for (int i = 1; i < 1000; i += 100) beep(i, 1);
}
