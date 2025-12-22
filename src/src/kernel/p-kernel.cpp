/*
 * Copyright (C) 2025 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/
#include "mods/dev/tasking/tasking.h"
#include "mods/dev/chorus/chorus.h"
#include "mods/dev/memory/memory.h"
#include "mods/dev/serial/serial.h"
#include "mods/dev/paging/paging.h"
#include "mods/dev/ramfs/ramfs.h"
#include "mods/dev/mouse/mouse.h"
#include "mods/dev/chorus/wav.h"
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
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <text.h>

// TODO:
//  - ISS Folder Support
//  - Multitasking

typedef void (*read_file)(const char* filename, uint8_t* dest);

struct FileEntity {
    // struct FileEntity* children;
    // int childCount;
    char* filename; 
    int type;
};

// ISS - Integrated Software Suite
class FileManager {
    private:
        int EndsWith(const char *str, const char *suffix) {
            if (!str || !suffix) return 0;
            size_t lenstr = strlen(str);
            size_t lensuffix = strlen(suffix);
            if (lensuffix > lenstr) return 0;
            return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
        }
        FileEntity* readDirectory(const char* _path = "/") {
            vfs_node_t* dir = vfs_find(_path);
            if (!dir) {
                serial_write_string("Failed to find directory\n", FAIL);
                return nullptr;
            }
            if (!dir->readdir) {
                serial_write_string("Directory has no readdir\n", FAIL);
                return nullptr;
            }
            dirent entry;
            int realCount = 0;
            while (dir->readdir(dir, realCount, &entry) == 0) {
                realCount++;
            }
            bool isRoot = (strcmp(_path, "/") == 0);
            int extra = isRoot ? 0 : 1;
            int total = realCount + extra;
            FileEntity* list = (FileEntity*)malloc((total + 1) * sizeof(FileEntity)); // +1 sentinel
            if (!list) {
                serial_write_string("Failed to allocate file list\n", FAIL);
                return nullptr;
            }
            int outIndex = 0;
            if (!isRoot) {
                list[outIndex].filename = (char*)malloc(3);
                if (!list[outIndex].filename) { free(list); return nullptr; }
                strcpy(list[outIndex].filename, "..");
                list[outIndex].type = VFS_NODE_DIR;
                outIndex++;
            }
            for (int dirIndex = 0; dirIndex < realCount; dirIndex++) {
                if (dir->readdir(dir, dirIndex, &entry) != 0) break;
                size_t len = strlen(entry.d_name);
                char* nameCopy = (char*)malloc(len + 1);
                if (!nameCopy) break;
                memcpy(nameCopy, entry.d_name, len + 1);
                list[outIndex].filename = nameCopy;
                list[outIndex].type = entry.d_type;
                outIndex++;
            }
            list[outIndex].filename = nullptr;
            list[outIndex].type = 0;
            this->fileCount = outIndex; // number of entries actually written (excluding sentinel)
            return list;
        }
        char* parent_path(const char *path) {
            if (!path) return NULL;
            const char *last = strrchr(path, '/');
            // no slash -> copy whole string
            if (!last) {
                size_t n = strlen(path);
                char *out = malloc(n + 1);
                if (!out) return NULL;
                memcpy(out, path, n + 1);
                return out;
            }
            // "/" or "/something" -> return "/"
            if (last == path) {
                char *out = malloc(2);
                if (!out) return NULL;
                out[0] = '/';
                out[1] = '\0';
                return out;
            }
            // normal case
            size_t len = (size_t)(last - path);
            char *out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, path, len);
            out[len] = '\0';
            return out;
        }
    public:
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        int frames;
        int thickness;
        int currentSelection;
        int fileCount; // Initalized by readDirectory
        FileEntity* files;
        char* path;
        FileManager(void) {
            this->width = 864 / 1.25;
            this->height= 576 / 1.25;
            this->offsetX = 512 - (this->width / 2);
            this->offsetY = 384 - (this->height / 2);
            this->padding = 10; 
            this->frames = 1;
            this->thickness = 3;
            this->currentSelection = 0;
            this->files = readDirectory();
            this->path = nullptr;
            this->redraw(0b11111000);
        }
        void redraw(uint8_t description = 0b00111000) {
            // Bit 8 is the screen 
            // Bit 7 is the border 
            // Bit 6 is the background
            // Bit 5 is the title
            // Bit 4 is the options
            if ((description >> 7) & 1) this->clear_screen();
            if ((description >> 6) & 1) this->draw_border();
            if ((description >> 5) & 1) this->draw_background();
            if ((description >> 4) & 1) this->draw_title();
            if ((description >> 3) & 1) this->draw_options();
        }
    private:
        void clear_screen(void) {
            fill(0xb1a1c);
        }
        void draw_border(void) {
            for (int k = 0; k < frames; k++) {
                int x0 = offsetX + k * padding;
                int y0 = offsetY + k * padding;
                int w = width  - 2 * k * padding;
                int h = height - 2 * k * padding;
                for (int t = 0; t < thickness; t++) {
                    for (int x = 0; x < w; x++) draw_pixel(x0 + x, y0 + t, 0x00FFFFFF);
                    for (int x = 0; x < w; x++) draw_pixel(x0 + x, y0 + h - 1 - t, 0x00FFFFFF);
                    for (int y = 0; y < h; y++) draw_pixel(x0 + t, y0 + y, 0x00FFFFFF);
                    for (int y = 0; y < h; y++) draw_pixel(x0 + w - 1 - t, y0 + y, 0x00FFFFFF);
                }
            }
        }
        void draw_background(void) {
            int innerX = offsetX + (frames - 1) * padding + thickness;
            int innerY = offsetY + (frames - 1) * padding + thickness;
            int innerW = width  - 2 * ((frames - 1) * padding) - 2 * thickness;
            int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
            for (int y = 0; y < innerH; y++) {
                for (int x = 0; x < innerW; x++) draw_pixel(innerX + x, innerY + y, 0x00403a39);
            }
        }
        void draw_title(void) {
            draw_icon(offsetX + padding * frames + 4, offsetY + padding * frames + 4, 0);
            const char* titleString = "Select File";
            for (int i = 0; i < strlen(titleString); i++) draw_char(
                offsetX + padding * frames + 4 + 78 + (32 * i),
                offsetY + padding * frames + 24,
                titleString[i], 
                COLOR_W
            );   
        }
        // FIXME: The Directory is listed every time the FileManager is instanciated. It should only have to be listed once.
        void draw_options(void) {
            if (!files) return;
            int i = 0;
            while (files[i].filename != nullptr) {
                const char* optionString = files[i].filename;
                size_t len = strlen(optionString);
                // This loop will clear the icon border.
                for (int x = 0; x < 32; x++) {
                    for (int y = 0; y < 32; y++) {
                        draw_pixel(offsetX + padding * frames + 78 - 49 + x, offsetY + padding * frames + 106 + (35 * i) + y, 0x00403a39);
                    }
                }
                // Draw the icon.
                if (files[i].type == VFS_NODE_FILE) {
                    if (EndsWith(optionString, ".wav") == 1) {
                        draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 5, 1);
                    } else if (EndsWith(optionString, ".elf") == 1) {
                        draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 9, 1);
                    } else {
                        draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 6, 1);
                    }
                } else {
                    /*
                    if (memcmp(files[i].filename, path, strlen(files[i].filename)) == 0) {
                        draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 8, 1);
                    } else {
                        draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 7, 1);
                    }
                    */
                    draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 7, 1);
                }
                for (size_t j = 0; j < len; j++) {
                    draw_char(offsetX + padding * frames + 4 + 78 + (16 * j), offsetY + padding * frames + 106 + (35 * i) + 8, optionString[j], (i == currentSelection ? COLOR_B : COLOR_W), 2);
                }
                i++;
            }
        };
    public:
        void keyboard_callback(char key, bool shift, bool meta, unsigned char scancode) {
            bool redrawNeeded = false; // prevents other keys from redrawing.
            uint8_t redraw_description = 0;
            if (key == 's') {
                if (this->currentSelection + 1 < this->fileCount) {
                    this->currentSelection++;
                    redraw_description |= 0b00001000;
                    redrawNeeded = true;
                }
            } else if (key == 'w') {
                if (this->currentSelection > 0) {
                    this->currentSelection--;
                    redraw_description |= 0b00001000;
                    redrawNeeded = true;
                }
            } else if (key == '\n') {
                const char* selection = files[this->currentSelection].filename;
                if (files[this->currentSelection].type == VFS_NODE_DIR) {
                    if (strcmp(selection, "..") == 0) {
                        if (this->path == nullptr) {
                            // this code should never execute assuming readDirectory works correctly.
                            serial_write_string("Illegal Directory Change\n", true, FAIL);
                        } else {
                            char* _path = parent_path(this->path);
                            if (!_path) {
                                serial_write_string("Failed to get the parent directory.\n", true, FAIL);
                            } else {
                                free(this->path);
                                this->path = _path;
                                if (strcmp(this->path, "/") == 0) {
                                    free(this->path);
                                    this->path = nullptr;
                                }
                            }
                        }
                    } else {
                        // No Special Directory Names 
                        if (this->path == nullptr) {
                            int length = strlen(selection) + 2;
                            this->path = (char*)malloc(length);
                            if (this->path) (void)sprintf(this->path, "/%s", selection);
                        } else if (this->path != nullptr) {
                            int length = strlen(this->path) + strlen(selection) + 2;
                            char* _path = (char*)malloc(length);
                            if (_path) sprintf(_path, "%s/%s", this->path, selection);          
                            free(this->path);
                            this->path = (char*)malloc(length);
                            strcpy(this->path, _path);
                            free(_path);
                        }
                    }
                    this->currentSelection = 0; // Default selection to the first option, always. 
                    this->files = readDirectory(this->path != nullptr ? this->path : "/");
                    redraw_description |= 0b00111000;
                    redrawNeeded = true;
                } else if (files[this->currentSelection].type == VFS_NODE_FILE) {
                    if (EndsWith(files[this->currentSelection].filename, ".wav") == 1) {
                        openFile:
                            const char* fname = files[this->currentSelection].filename;
                            size_t len = strlen(fname);
                            char* path = (char*)malloc(len + 2);
                            if (!path) {
                                serial_write_string("Failed to allocate path buffer\n", FAIL);
                                return;
                            }
                            path[0] = '/';
                            memcpy(path + 1, fname, len + 1);
                            FILE* file = fopen(path, "r");
                            free(path);
                        if (!file) {
                            serial_write_string("Failed to open file.\n", false, FAIL);
                        } else {
                            uint32_t buffer_len = 52640;
                            char* buffer = (char*)malloc(buffer_len);
                            fread(buffer, 1, buffer_len, file);
                            struct wav_info_t* wav_info = read_wav_info(buffer, buffer_len);
                            play_wav(wav_info, 0);
                            free(buffer);
                        }
                        fclose(file);
                    } else if (EndsWith(files[this->currentSelection].filename, ".elf") == 1) {
                        openPath:
                            const char* fname = files[this->currentSelection].filename;
                            size_t len = strlen(fname);
                            char* path = (char*)malloc(len + 2);
                            if (!path) {
                                serial_write_string("Failed to allocate path buffer\n", FAIL);
                                return;
                            }
                            path[0] = '/';
                            memcpy(path + 1, fname, len + 1);
                            FILE* file = fopen(path, "r");
                            free(path);
                        // serial_write_string("User tried to run ELF file which is currently unsupported.\n");
                        uint32_t buffer_len = 1024;
                        char* buffer = (char*)malloc(buffer_len);
                        fread(buffer, 1, buffer_len, file);
                        // This needs to be worked on.
                        (void (*)())elf_load_file(buffer);
                        free(buffer);
                        fclose(file);
                    }
                }
            }
            if (redrawNeeded) this->redraw(redraw_description);
        }

        ~FileManager() {
            free(this->files);
        }

        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

static FileManager* fileManager = nullptr;

void fm_key(char key, bool shift, bool meta, unsigned char scancode) {
    if (fileManager != nullptr) {
        fileManager->keyboard_callback(key, shift, meta, scancode);
        return;
    }
};

void task1(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        serial_write_string("Task 1 running\n");
        for (volatile int k = 0; k < 1000000; ++k);
    }
}

void task2(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        serial_write_string("Task 2 running\n");
        for (volatile int k = 0; k < 1000000; ++k);
    }
}

// Create support for Programs and ELF next.

extern "C" void kernel_main(read_file load_floppy) {
    Cursor::enableCursor(0, 10);
    queryMemoryMap();
    PagingInstall();
    IDTInstall();
    ISRInstall();
    IRQInstall();
    // pit_init(100);
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
    // initTasking();
    // init_tasking();
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
        write_serial('\n');
    graphics_initalize_stage2();
    print("\n\n# Welcome to PumpkinOS!\n", 240);
    test_load_flp:
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
    serial_write_string("Starting test of AC97 Audio Codec...\n");
    waveTest:
        const uint32_t lengthFloppyBuffer = 52640;
        floppyBuffer = malloc(lengthFloppyBuffer);
        disablePaging();
        load_floppy("TEST.WAV", lowmem_floppy);
        enablePaging();
        memcpy(floppyBuffer, lowmem_floppy, lengthFloppyBuffer);
        // File
        FILE* file = fopen("/test.wav", "a");
        fwrite(floppyBuffer, 1, lengthFloppyBuffer, file);
        fclose(file);
        // Close
        initalize();
        struct wav_info_t* wav_info = read_wav_info(floppyBuffer, lengthFloppyBuffer);
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
        // Play WAV is the problem.
        for (int i = 0; i < 1; i++) {
            // play_wav(wav_info, lengthFloppyBuffer);
            play_wav(wav_info, 0);
            // timer_wait(18);
        }
        free(floppyBuffer);
        serial_write_string("AC97 Audio Codec test has ended.\n");
    procTestOne:
        floppyBuffer = malloc(1024);
        disablePaging();
        load_floppy("MAIN.ELF", lowmem_floppy);
        enablePaging();
        memcpy(floppyBuffer, lowmem_floppy, 1024);
        // File
        file = fopen("/main.elf", "a");
        fwrite(floppyBuffer, 1, 1024, file);
        fclose(file);
    procMan:
        mkdir("/hello", 0777);
        mkdir("/hello/world", 0777);
        mkdir("/hello/world/test", 0777);
        file = fopen("/hello/test.net", "a");
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
        terminal_delete();
        fileManager = new FileManager();
        kb_add_event(&fm_key);
    tasks:
        static uint8_t stack1[KSTACK_SIZE];
        static uint8_t stack2[KSTACK_SIZE];
        task_t *t1 = task_create(task1, NULL, stack1);
        task_t *t2 = task_create(task2, NULL, stack2);
        // link runqueue
        g_runqueue = t1;
        t1->next = t2;
        t2->next = t1;
        g_current = t1;
        g_current->state = TASK_RUNNING;
    // syscall(0, "loq.txt", 3, 4, 5, 6);
}