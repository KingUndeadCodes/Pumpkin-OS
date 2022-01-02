/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/kb.h"
#include "mods/dev/idt/isr.h"
#include <text.h>

extern "C" void _start() {
    Cursor::enableCursor(0, 10);
    printf("Booting PumpkinOS (ver: 0)\n", COLOR_CYAN | COLOR_BLACK << 4);
    IDTInstall();
    ISRInstall();
    IRQInstall();
    asm volatile ("sti");
    printf("\n - Interupts Enabled!\n", COLOR_GREEN | COLOR_BLACK << 4);
    KeyboardInit();
    printf(" - Keyboard Enabled! Type anything!\n", COLOR_GREEN | COLOR_BLACK << 4);
}
