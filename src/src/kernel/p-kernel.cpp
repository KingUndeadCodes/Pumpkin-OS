/*
 * Copyright (C) 2026 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/
#include "mods/core/wingman/headers/wingman.h"
#include "mods/dev/pci/drivers/ac97.h"
#include "mods/dev/tasking/tasking.h"
#include "mods/dev/console/console.h"
#include "mods/dev/logging/logging.h"
#include "mods/dev/chorus/chorus.h"
#include "mods/dev/memory/memory.h"
#include "mods/dev/serial/serial.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/ramfs/ramfs.h"
#include "mods/dev/mouse/mouse.h"
#include "mods/dev/chorus/wav.h"
#include "mods/dev/chorus/mp3.h"
#include "mods/dev/cmos/cmos.h"
#include "mods/dev/elf/elf.h"
#include "mods/dev/idt/isr.h"
#include "mods/dev/pit/pit.h"
#include "mods/dev/pci/pci.h"
#include "mods/dev/vbe/vbe.h"
#include "mods/dev/vfs/vfs.h"
#include "mods/dev/net/udp.h"
#include "mods/dev/port.cpp"
#include "mods/dev/kb/kb.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

// TODO:
//  - ISS Folder Support
//  - Multitasking
//  - Create support for Programs and ELF next.

typedef uint32_t (*read_file)(const char* filename, uint8_t* dest);

static void test_vfs_file_io(read_file load_floppy) {
    // int total = get_kmalloc_free_bytes();
    char* floppyBuffer = malloc(64);
    uint8_t* lowmem_floppy = (uint8_t*)0x90000;
    disablePaging();
    load_floppy("TEST.TXT", lowmem_floppy);
    enablePaging();
    memcpy(floppyBuffer, lowmem_floppy, 64);
    serial_write_string(floppyBuffer, false, NONE);
    serial_write_string("\n", false, NONE);
    // sprintf(buffer, "Free heap bytes: %d\n", total);
    // serial_write_string(buffer, false, NONE);
    // Create a boot directory and put test.txt in it.
    FILE* fp = fopen("/test.txt", "a");
    if (!fp) {
        serial_write_string("Failed to create file.\n", false, FAIL);
    } else {
        fwrite(floppyBuffer, 1, 64, fp);
        serial_write_string("File created successfully!\n", INFO);
    }
    fclose(fp);
    free(floppyBuffer);
    fp = fopen("/test.txt", "r");
    bool jumped = false;
    jumppoint:;
    if (!fp) {
        serial_write_string("Failed to open file.\n", false, FAIL);
    } else {
        char* newBuffer = (char*)malloc(64);
        fread(newBuffer, 1, 64, fp);
        serial_write_string("File contents: ", false, NONE);
        serial_write_string(newBuffer, false, NONE);
        serial_write_string("\n", false, NONE);
        free(newBuffer);
    }
    fclose(fp);
    if (!jumped) {
        jumped = true;
        fp = fopen("/kmsglog", "r");
        serial_write_string("Jumped to kmsglog\n");
        goto jumppoint;
    }
}

static void copyLua() {
    // File
    FILE* luaFile = fopen("/m.lua", "a");
    fclose(luaFile);
}

// Loads a file off the boot floppy and copies its bytes into RAMFS at
// /<ramfs_name>, so Explorer can find and open it like any other file.
// Independent of whichever playback/execution test (if any) also runs
// against the same floppy file -- this is just "make it show up in RAMFS".
static void copy_floppy_file_to_ramfs(read_file load_floppy, const char* floppy_name, const char* ramfs_name) {
    uint8_t* lowmem = (uint8_t*)0x100000;
    disablePaging();
    uint32_t length = load_floppy(floppy_name, lowmem);
    enablePaging();
    if (length == 0) {
        serial_write_string("Failed to load floppy file for RAMFS copy\n", false, FAIL);
        return;
    }
    char* buffer = (char*)malloc(length);
    if (!buffer) {
        serial_write_string("Failed to allocate RAMFS copy buffer\n", false, FAIL);
        return;
    }
    memcpy(buffer, lowmem, length);
    size_t nameLen = strlen(ramfs_name);
    char* path = (char*)malloc(nameLen + 2);
    if (!path) {
        serial_write_string("Failed to allocate path buffer\n", false, FAIL);
        free(buffer);
        return;
    }
    path[0] = '/';
    memcpy(path + 1, ramfs_name, nameLen + 1);
    FILE* file = fopen(path, "a");
    free(path);
    if (file) {
        fwrite(buffer, 1, length, file);
        fclose(file);
    } else {
        serial_write_string("Failed to copy file into RAMFS\n", false, FAIL);
    }
    free(buffer);
}

static void test_elf_execution(read_file load_floppy) {
    uint8_t* lowmem_elf = (uint8_t*)0x100000;
    disablePaging();
    uint32_t lengthElfBuffer = load_floppy("MAIN.ELF", lowmem_elf);
    enablePaging();
    if (lengthElfBuffer == 0) {
        serial_write_string("Failed to load MAIN.ELF\n", false, FAIL);
        return;
    }
    char* floppyBuffer = malloc(lengthElfBuffer);
    if (!floppyBuffer) {
        serial_write_string("Failed to allocate ELF buffer\n", false, FAIL);
        return;
    }
    memcpy(floppyBuffer, lowmem_elf, lengthElfBuffer);
    void* entry = elf_load_file(floppyBuffer, lengthElfBuffer);
    if (entry) {
        serial_write_string("Spawning MAIN.ELF...\n", false, NONE);
        // See docs/DOCS.md ("mods/dev/elf/elf.cpp — elf_spawn()") for why
        // floppyBuffer is intentionally NOT freed here on success.
        if (!elf_spawn(entry)) free(floppyBuffer);
    } else {
        serial_write_string("Failed to load MAIN.ELF (no entry point)\n", false, FAIL);
        free(floppyBuffer);
    }
}

static void procMan() {
    mkdir("/hello", 0777);
    mkdir("/hello/world", 0777);
    mkdir("/hello/world/test", 0777);
    FILE* file = fopen("/hello/test.net", "a");
    const char* contents = "Hello, World!\n";
    fwrite(contents, 1, strlen(contents), file);
    fclose(file);
    // ...
    char* buffer = malloc(15);
    file = fopen("/hello/test.net", "r");
    fread(buffer, 1, 15, file);
    fclose(file);
    serial_write_string(buffer);
    free(buffer);
    // ...
    /*
    terminal_delete();
    fileManager = new FileManager();
    kb_add_event(&fm_key);
    */
    terminal_delete();
    initalizeWindowSystem();
}

// UDP echo listener for netlab (experemental/netlab/net.py) to test against:
// send a UDP datagram to this OS's IP on port 7, get the same bytes back.
// Port 7 matches the classic "echo" service and is also net.py's default
// auto-echo port for its Simulated LAN hosts, so both sides already agree
// on the convention.
static void udpEchoHandler(uint8_t* src_ip, uint16_t src_port, void* data, uint16_t len) {
    char ip_str[20];
    InternetProtocol::convertString(ip_str, src_ip);
    serial_write_string("UDP echo: ", false, NONE);
    serial_write_string(ip_str, false, NONE);
    serial_write_string("\n", false, NONE);
    UserDatagramProtocol::sendPacket(src_ip, 7, src_port, data, len);
}

static void test_udp_echo() {
    UserDatagramProtocol::listen(7, udpEchoHandler);
}

// Phase 1 (docs/TODO.md's tasking proposal): prove the existing
// round-robin scheduler actually interleaves, via serial log.

long fibbanoci(long n) {
    if (n == 0) return 0;
    if (n == 1) return 1;
    return fibbanoci(n - 1) + fibbanoci(n - 2);
};

// idle task
void task0(void *arg) {
    for (;;) asm volatile("hlt");
};

void task1(void *arg) {
    (void)arg;
    char* string = (char*)malloc(128);
    // I_MAX cannot be more than 50
    for (int i = 0; i < 40; i++) {
        sprintf(string, "[1] fibbanoci(%d) = %d\n", i, fibbanoci(i));
        serial_write_string(string, false, NONE);
        memset(string, 0, 128);
        // for (volatile int k = 0; k < 0xFFFFFF; ++k);
    }
    free(string);
    return;
}

void task2(void *arg) {
    (void)arg;
    char* string = (char*)malloc(128);
    // I_MAX cannot be more than 50
    for (int i = 0; i < 40; i++) {
        sprintf(string, "[2] fibbanoci(%d) = %d\n", i, fibbanoci(i));
        serial_write_string(string, false, NONE);
        memset(string, 0, 128);
        // for (volatile int k = 0; k < 0xFFFFFF; ++k);
    }
    free(string);
    return;
}

extern "C" void kernel_main(read_file load_floppy) {
    queryMemoryMap();
    PagingInstall();
    IDTInstall();
    ISRInstall();
    IRQInstall();
    pit_init(1000); // 1000Hz: timer_ticks becomes milliseconds since boot
    asm volatile ("sti");
    if (!are_interrupts_enabled()) { serial_write_string("interupt setup failed. system halted!\n", FAIL); abort(); }
    // See docs/DOCS.md ("p-kernel.cpp — tasking_init() placement") for why
    // calling this early, before any tasks exist, is safe.
    tasking_init();
    initialize_memory_pool();
    vfs_init();
    vfs_node_t* ramfs_root = ramfs_init();
    vfs_mount("/", ramfs_root);
    Logging::capture();
    Logging::log("Interupts Enabled!\n"); // Interupts should be enabled before any Logging takes place, that's why this is here.
    Logging::log("Paging Enabled!\n"); // Paging should be enabled before any Logging takes place, that's why this is here.
    Logging::log("Memory pool initialized!\n"); // Memory pool is actually initalized before this point, because it's required for the VFS and Logging, but paging is initalized first.
    KeyboardInit();
    Logging::log("Keyboard Enabled!\n"); // The keyboard does not seem to be working.
    mouse_install();
    Logging::log("Mouse Enabled!\n");
    TimerInit();
    Logging::log("PIT Enabled!\n");
    Logging::log("Checking for PCI devices...\n");
    checkAllBuses();
    graphics_initalize_stage1();
    LogDevice terminalDevice = { .log = &terminal_write };
    LogDevice serialDevice = { .log = &serial_write_string };
    Logging::addLogDevice(&terminalDevice);
    Logging::addLogDevice(&serialDevice);
    Logging::flush();
    write_serial('\n');
    graphics_initalize_stage2();
    // test_fault_handler();
    test_vfs_file_io(load_floppy);
    // serial_write_string("Initializing AC97 Audio Codec...\n");
    // initalize();
    copyLua();
    copy_floppy_file_to_ramfs(load_floppy, "TEST.WAV", "test.wav");
    copy_floppy_file_to_ramfs(load_floppy, "TEST.MP3", "test.mp3");
    copy_floppy_file_to_ramfs(load_floppy, "MAIN.ELF", "main.elf");
    // WAV/MP3 playback smoke tests moved to mods/dev/chorus/wav.cpp and
    // mods/dev/chorus/mp3.cpp as reference comments.
    // See docs/DOCS.md ("p-kernel.cpp — kernel_main() task-creation race")
    // for why every task_create() call in this function (direct, or
    // indirect via elf_spawn()) has to happen inside one sched_lock()/
    // sched_unlock() span: a real timer tick landing after the *first*
    // task exists but before the *last* one is created hijacks execution
    // into whatever's in the runqueue so far and permanently abandons the
    // rest of this function -- sched_lock() makes scheduler_on_tick() a
    // complete no-op until every startup task actually exists. Everything
    // else in this span (test_udp_echo/procMan) doesn't itself need the
    // lock, but sits between the first and last task_create() call in the
    // existing boot order, so it's inside the span too rather than
    // reordering unrelated boot steps just to shrink it.
    sched_lock();
    // test_elf_execution(load_floppy);
    test_udp_echo();
    procMan();
    // See docs/DOCS.md ("p-kernel.cpp — tasking_init() placement") for why
    // this is safe to spawn now, at the very end of boot.
    static uint8_t stack0[KSTACK_SIZE] __attribute__((aligned(16)));
    task_create(task0, NULL, stack0);
    // Temporarily disabled while debugging the ELF-stdin character-doubling
    // bug, to rule out interference from these two CPU-heavy tasks.
    // static uint8_t stack1[KSTACK_SIZE] __attribute__((aligned(16)));
    // static uint8_t stack2[KSTACK_SIZE] __attribute__((aligned(16)));
    // task_create(task1, NULL, stack1);
    // task_create(task2, NULL, stack2);
    sched_unlock();
    serial_write_string("Tasking Enabled! Tasks spawned.\n");
}