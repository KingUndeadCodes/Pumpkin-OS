/*
 * Copyright (C) 2022 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/ports/sha256/sha256.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/audio/speaker.h"
#include "mods/dev/cmos/cmos.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/kb/kb.h"
#include <tasking.h>
#include <stdlib.h>
#include <text.h>
#include <time.h>

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
    if (are_interrupts_enabled()) print(" - Interupts Enabled!\n", GREEN);
    KeyboardInit();
    print(" - Keyboard Enabled!\n", GREEN);
    TimerInit();
    print(" - PIT Enabled!\n", GREEN);
    PagingInstall();
    print(" - Paging Enabled!\n", GREEN);
    initializeMem();
    initTasking();
    print("\n - Checking for PCI devices...\n", PURPLE);
    fork(checkAllBuses);
    yield();
    // printf("c - %d", FetchCurrentCMOSTime().century);
    /// MALLOC_TEST();
    /// SHA256_TEST();
    /// never_gonna();
}