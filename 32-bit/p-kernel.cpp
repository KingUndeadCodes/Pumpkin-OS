/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/PIC.h"
#include <text.h>

extern "C" void _start() {
    Cursor::enableCursor(0, 10);
    printf("Booting PumpkinOS (ver: 0)\n", COLOR_CYAN | COLOR_BLACK << 4);
    #if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
        init_pics(0x20, 0x28);
        printf("\nInitialized PIC!");
    #endif
}
