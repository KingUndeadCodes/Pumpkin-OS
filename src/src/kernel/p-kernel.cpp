/*
 * Copyright (C) 2025 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/
#include "mods/core/wingman/headers/wingman.h"
#include "mods/dev/pci/drivers/ac97.h"
#include "mods/dev/tasking/tasking.h"
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
#include "mods/dev/port.cpp"
#include "mods/dev/kb/kb.h"
#include <graphics.h>
#include <logging.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

// TODO:
//  - ISS Folder Support
//  - Multitasking
//  - Create support for Programs and ELF next.

typedef uint32_t (*read_file)(const char* filename, uint8_t* dest);

static void test_load_flp(read_file load_floppy) {
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

// Disabled (not called from kernel_main): kept as a ready-to-run function so
// re-enabling WAV playback testing is a one-line change instead of
// uncommenting a whole block.
static void waveTest(read_file load_floppy) {
    uint8_t* lowmem_floppy = (uint8_t*)0x100000;
    disablePaging();
    uint32_t lengthFloppyBuffer = load_floppy("TEST.WAV", lowmem_floppy);
    enablePaging();
    if (lengthFloppyBuffer == 0) {
        serial_write_string("Failed to load TEST.WAV\n", false, FAIL);
        return;
    }
    char* floppyBuffer = malloc(lengthFloppyBuffer);
    if (!floppyBuffer) {
        serial_write_string("Failed to allocate WAV buffer\n", false, FAIL);
        return;
    }
    memcpy(floppyBuffer, lowmem_floppy, lengthFloppyBuffer);
    initalize();
    struct wav_info_t* wav_info = read_wav_info((uint8_t*)floppyBuffer, lengthFloppyBuffer);
    if (!wav_info) {
        serial_write_string("Failed to parse WAV file\n", false, FAIL);
        free(floppyBuffer);
        return;
    } else {
        char* string_alloc = malloc(512);
        sprintf(
            string_alloc,
            "WAV_AudioFile {\n\t\"start_of_pcm_data\": %d,\n\t\"length_of_pcm_data\": %d,\n\t\"pcm_data_number_of_channels\": %d,\n\t\"pcm_data_sample_rate\": %d,\n\t\"pcm_data_bits_per_sample\": %d,\n\t\"length_of_output_pcm_data\": %d,\n\t\"output_pcm_data_sample_rate\": %d\n}\n",
            wav_info->start_of_pcm_data,
            wav_info->length_of_pcm_data,
            wav_info->pcm_data_number_of_channels,
            wav_info->pcm_data_sample_rate,
            wav_info->pcm_data_bits_per_sample,
            wav_info->length_of_output_pcm_data,
            wav_info->output_pcm_data_sample_rate
        );
        serial_write_string(string_alloc, false, NONE);
        free(string_alloc);
    }
    play_wav(wav_info, 0);
    /*
     * Blocking test version:
     * Wait until the AC97 driver stops the stream, then free the source buffer.
     * This is okay for testing, but a real desktop should poll this from the
     * main loop instead of blocking the whole kernel here.
     */
    while (AC97IsPlaying()) {
        asm volatile("hlt");
    }
    free(floppyBuffer);
    serial_write_string("AC97 Audio Codec test has ended. WAV buffer freed.\n", false, NONE);
}

static void mp3Test(read_file load_floppy) {
    uint8_t* lowmem_floppy = (uint8_t*)0x100000;
    disablePaging();
    uint32_t lengthFloppyBuffer = load_floppy("TEST.MP3", lowmem_floppy);
    enablePaging();
    if (lengthFloppyBuffer == 0) {
        serial_write_string("Failed to load TEST.MP3\n", false, FAIL);
        return;
    }
    char* floppyBuffer = malloc(lengthFloppyBuffer);
    if (!floppyBuffer) {
        serial_write_string("Failed to allocate MP3 buffer\n", false, FAIL);
        return;
    }
    memcpy(floppyBuffer, lowmem_floppy, lengthFloppyBuffer);
    initalize();
    struct mp3_info_t* mp3_info = read_mp3_info((uint8_t*)floppyBuffer, lengthFloppyBuffer);
    if (!mp3_info) {
        serial_write_string("Failed to parse MP3 file\n", false, FAIL);
        free(floppyBuffer);
        return;
    } else {
        char* string_alloc = malloc(512);
        sprintf(
            string_alloc,
            "MP3_AudioFile {\n\t\"length_of_mp3_data\": %d,\n\t\"pcm_data_number_of_channels\": %d,\n\t\"pcm_data_sample_rate\": %d,\n\t\"length_of_output_pcm_data\": %d,\n\t\"output_pcm_data_sample_rate\": %d\n}\n",
            mp3_info->length_of_mp3_data,
            mp3_info->pcm_data_number_of_channels,
            mp3_info->pcm_data_sample_rate,
            mp3_info->length_of_output_pcm_data,
            mp3_info->output_pcm_data_sample_rate
        );
        serial_write_string(string_alloc, false, NONE);
        free(string_alloc);
    }
    play_mp3(mp3_info, 0);
    while (AC97IsPlaying()) {
        asm volatile("hlt");
    }
    free(floppyBuffer);
    serial_write_string("AC97 Audio Codec test has ended. MP3 buffer freed.\n", false, NONE);
}

static void procTestOne(read_file load_floppy) {
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

    // File
    FILE* file = fopen("/main.elf", "a");
    fwrite(floppyBuffer, 1, lengthElfBuffer, file);
    fclose(file);

    void* entry = elf_load_file(floppyBuffer);
    if (entry) {
        serial_write_string("Running MAIN.ELF...\n", false, NONE);
        int rc = elf_run(entry);
        char* rc_buf = (char*)malloc(64);
        sprintf(rc_buf, "MAIN.ELF exited with code %d\n", rc);
        serial_write_string(rc_buf, false, NONE);
        free(rc_buf);
    } else {
        serial_write_string("Failed to load MAIN.ELF (no entry point)\n", false, FAIL);
    }
    free(floppyBuffer);
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

static void tasks() {
    /*
    Theory:
        Logging::log does not work because of terminal being deleted.
        We need to add code that would allow for the deletion of a device.
    */
    /*
    tasking_init();
    Logging::log("Tasking Enabled!"); // This doesn't work for some reason, it causes a page fault.
    static uint8_t stack0[KSTACK_SIZE] __attribute__((aligned(16)));
    static uint8_t stack1[KSTACK_SIZE] __attribute__((aligned(16)));
    static uint8_t stack2[KSTACK_SIZE] __attribute__((aligned(16)));
    task_t *t0 = task_create(task0, NULL, stack0);
    task_t *t1 = task_create(task1, NULL, stack1);
    task_t *t2 = task_create(task2, NULL, stack2);
    serial_write_string("Tasks Reached\n");
    timer_wait(18);
    serial_write_string("hello\n", false, NONE);
    */
    // syscall(0, "loq.txt", 3, 4, 5, 6);
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
    initialize_memory_pool();
    vfs_init();
    vfs_node_t* ramfs_root = ramfs_init();
    vfs_mount("/", ramfs_root);
    Logging::capture();
    Logging::log("Interupts Enabled!"); // Interupts should be enabled before any Logging takes place, that's why this is here.
    Logging::log("Paging Enabled!"); // Paging should be enabled before any Logging takes place, that's why this is here.
    Logging::log("Memory pool initialized!"); // Memory pool is actually initalized before this point, because it's required for the VFS and Logging, but paging is initalized first.
    KeyboardInit();
    Logging::log("Keyboard Enabled!"); // The keyboard does not seem to be working.
    mouse_install();
    Logging::log("Mouse Enabled!");
    TimerInit();
    Logging::log("PIT Enabled!");
    Logging::log("Checking for PCI devices...");
    checkAllBuses();
    graphics_initalize_stage1();
    LogDevice terminalDevice = { .log = &terminal_write };
    LogDevice serialDevice = { .log = &serial_write_string };
    Logging::addLogDevice(&terminalDevice);
    Logging::addLogDevice(&serialDevice);
    Logging::flush();
    write_serial('\n');
    graphics_initalize_stage2();
    test_load_flp(load_floppy);
    serial_write_string("Starting test of AC97 Audio Codec...\n");
    copyLua();
    // waveTest(load_floppy); // disabled; see the function definition above
    mp3Test(load_floppy);
    procTestOne(load_floppy);
    procMan();
    // tasks();
}

/*
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
*/
