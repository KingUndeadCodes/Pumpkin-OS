/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/text/text.cpp"
#include "mods/text/colors.h"
#include "mods/dev/PIC.cpp"

extern "C" void _start() {
    printf("Success", COLOR_LIGHT_GREEN);
    char c = 0;
    init_pics(0x20, 0x28);
    do {
        if(inb(0x60) != c) {
            c = inb(0x60);
            if(c > 0) {
                printf(c, COLOR_LIGHT_GREEN);
            }
        }
    }
    while(c != 1);
}
