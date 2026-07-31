#ifndef ISS_EXPLORER
#define ISS_EXPLORER

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../headers/manager.h"
#include "../../headers/cursor.h"
#include "../../../../dev/elf/elf.h"
#include "../../../../dev/chorus/wav.h"
#include "../../../../dev/chorus/mp3.h"
#include "../../../../dev/ramfs/ramfs.h"
#include "../../../../dev/pci/drivers/ac97.h"
#include "../../../fontman/fontman.h"
#include "../../headers/draw.h"
#include "../../headers/app.h"

struct FileEntity {
    // struct FileEntity* children;
    // int childCount;
    char* filename;
    int type;
};

// Column-major grid, paginated -- replaced an unbounded single-column list that
// could write past the window's pixel buffer for a large enough directory.
struct FileGridLayout {
    int listStartX;
    int listStartY;
    int columnWidth;
    int columnGutter;
    int rowHeight;
    int rowsPerColumn;
    int columnsPerPage;
    int capacityPerPage;
};

// ISS - Integrated Software Suite

class FileManager : public WingmanApp {
    private:
        int EndsWith(const char *str, const char *suffix);
        FileEntity* readDirectory(const char* _path = "/");
        char* parent_path(const char *path);
        void freeFileList(FileEntity* list);
        FileGridLayout computeGridLayout(void);
        // Returns the file index at (x, y), or -1 if none -- shared by
        // onMouseEvent()'s click handling and its hover-cursor check.
        int fileIndexAt(int x, int y);
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
        FileManager(WindowManager* wm);
        void redraw(uint8_t description = 0b00111000);
    private: // Make this protected.
        void draw_border(void);
        void draw_background(void);
        void draw_options(void);
        // Mirrors this->path (or "/" at root) into the window's title bar.
        void updateTitle(void);
    private:
        bool fileClick(bool* redrawNeeded, uint8_t* redraw_description);
    public:
        bool onKeyboard(char key, bool shift, bool meta, unsigned char scancode) override;
        bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) override;
        ~FileManager();
};

#endif