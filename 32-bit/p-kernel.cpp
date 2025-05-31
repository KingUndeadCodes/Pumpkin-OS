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
#include <logging.h>
#include <tasking.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <text.h>

extern "C" void _start() {
    Cursor::enableCursor(0, 10);
    IDTInstall();
    ISRInstall();
    IRQInstall();
    asm volatile ("sti");
    Logging::capture();
    if (!are_interrupts_enabled()) { serial_write_string("interupt setup failed. system halted!\n", FAIL); abort(); }
    Logging::log("Interupts Enabled!");
    KeyboardInit();
    Logging::log("Keyboard Enabled!");
    mouse_install();
    Logging::log("Mouse Enabled!");
    TimerInit();
    Logging::log("PIT Enabled!");
    PagingInstall();
    Logging::log("Paging Enabled!");
    initialize_memory_pool();
    Logging::log("Memory pool initialized!");
    initTasking();
    Logging::log("Tasking Enabled!");
    Logging::log("Checking for PCI devices...");
    checkAllBuses();
    graphics_initalize_stage1();
    initLogging:
        LogDevice printDevice = { .log = &print };
        LogDevice terminalDevice = { .log = &terminal_write };
        LogDevice serialDevice = { .log = &serial_write_string };
        Logging::addLogDevice(&printDevice);
        Logging::addLogDevice(&terminalDevice);
        Logging::addLogDevice(&serialDevice);
        Logging::flush();
    graphics_initalize_stage2();
    print("\n\n# Welcome to PumpkinOS!\n", 240);
    // syscall(0, "loq.txt", 3, 4, 5, 6);
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