/*
 * Copyright (C) 2024 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include "mods/dev/audio/speaker.h"
#include "mods/dev/serial/serial.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/mouse/mouse.h"
#include "mods/dev/cmos/cmos.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/vbe/vbe.h"
#include "mods/dev/port.cpp"
#include "mods/dev/kb/kb.h"
#include <graphics.h>
#include <tasking.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <text.h>

extern "C" void _start() {
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
    mouse_install();
    print(" - Mouse Enabled!\n", GREEN);
    TimerInit();
    print(" - PIT Enabled!\n", GREEN);
    PagingInstall();
    print(" - Paging Enabled!\n", GREEN);
    initialize_memory_pool();
    print(" - Tasking Enabled!\n", GREEN);
    initTasking();
    print(" - Checking for PCI devices...\n", PURPLE);
    checkAllBuses();
    graphics_initalize();
    syscall(0, "loq.txt", 3, 4, 5, 6);
    /*
    file_test:
        FILE *f = fopen("test.txt", "w");
        if (!f) {
            serial_write_string("Failed to open file\n");
            return;
        } else {
            const char *msg = "Hello, RAM FS!";
            char* buffer = (char*)malloc(50);
            fwrite(msg, 1, strlen((const char*)msg), f);
            fseek(f, 0, SEEK_SET);
            memset(buffer, 0, sizeof(buffer));
            fread(buffer, 1, strlen(msg), f);
            serial_write_string(buffer);
            fclose(f);
            free(buffer);
        }
    */
}