/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/std/text.h"
#include "mods/dev/PIC.h"

extern "C" void _start() {
    Cursor::enableCursor(0, 10);
    printf("Booting PumpkinOS (ver: 0)\n", COLOR_CYAN | COLOR_BLACK << 4);
    // x86 Hardware Initialization
    init_pics(0x20, 0x28);
    printf("\nInitialized PIC!");
}
