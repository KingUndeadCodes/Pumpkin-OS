/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/text/text.cpp"
#include "mods/text/colors.h"
#include "mods/dev/PIC.cpp"

extern "C" void _start() {
    init_pics(0x20, 0x28);
    printf("Success", COLOR_LIGHT_GREEN);
}
