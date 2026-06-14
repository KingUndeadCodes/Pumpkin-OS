#ifndef ISS_EXPLORER
#define ISS_EXPLORER

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../../../dev/elf/elf.h"
#include "../../../../dev/chorus/wav.h"
#include "../../../../dev/ramfs/ramfs.h"
#include "../../../../dev/vbe/vga_table.h"
#include "../../../../std/include/graphics/font.h"
#include "../../../../std/include/graphics/icons.h"

struct FileEntity {
    // struct FileEntity* children;
    // int childCount;
    char* filename; 
    int type;
};

// ISS - Integrated Software Suite

class FileManager : public KeyboardDelegate {
    private:
        void utility_draw_pixel(unsigned x, unsigned y, unsigned color);
        void utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 4);
        void utility_draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2);
    private:
        int EndsWith(const char *str, const char *suffix);
        FileEntity* readDirectory(const char* _path = "/");
        char* parent_path(const char *path);
        void freeFileList(FileEntity* list);
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
        Window* window;
        FileEntity* files;
        char* path;
        FileManager(void);
        void redraw(uint8_t description = 0b00111000);
    private: // Make this protected.
        void draw_border(void);
        void draw_background(void);
        void draw_title(void);
        void draw_options(void);
    public:
        void onKeyboard(char key, bool shift, bool meta, unsigned char scancode) override;
        ~FileManager();
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

#endif