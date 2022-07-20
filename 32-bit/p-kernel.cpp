/*
 * Copyright (C) 2022 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/audio/speaker.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/kb/kb.h"
#include <stdlib.h>
#include <text.h>

extern "C" void _start() {
    Cursor::enableCursor(0, 10);
    print("Booting PumpkinOS (ver: 0)\n", COLOR_CYAN | COLOR_BLACK << 4);
    initializeMem();
    IDTInstall();
    ISRInstall();
    IRQInstall();
    printf("%s || %c || %d", "Hello, World!", 'a', 1);
    asm volatile ("sti");
    if (are_interrupts_enabled()) print("\n - Interupts Enabled!\n", COLOR_GREEN | COLOR_BLACK << 4);
    KeyboardInit();
    print(" - Keyboard Enabled! Type anything!\n", COLOR_GREEN | COLOR_BLACK << 4);
    // char* ptr = kmalloc(6 * sizeof(char));
    // ptr = "Hello!";
    // kfree(ptr);
    // print(ptr);
}
