#include "./explorer.h"

#define COLOR_R 0xFFFF0000
#define COLOR_G 0xFF00FF00
// See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- selection color").
#define COLOR_B 0xFFFF5C04
#define COLOR_W 0xFFFFFFFF
// See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- window chrome")
// -- matches MessageBox/WidgetDemo's own COLOR_TITLEBAR/COLOR_DIVIDER exactly.
#define COLOR_TITLEBAR 0xFF2d2928
#define COLOR_DIVIDER 0xFF55504f

/*
inline color_t rgb24_to_argb32(uint32_t color24) {
    return 0xFF000000 | (color24 & 0x00FFFFFF);
}
*/

void FileManager::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void FileManager::utility_draw_char(unsigned int x, unsigned int y, char c, unsigned int color, unsigned int scale = 4U) {
    const FontAtlas* atlas = ttf_font_get_atlas(scale);
    if (atlas == nullptr) {
        for (unsigned i = 0; i < 8; i++) {
            for (unsigned j = 0; j < 8; j++) {
                if (Font[(int)c][i] & (1 << j)) {
                    for (unsigned k = 0; k < scale; k++) {
                        for (unsigned l = 0; l < scale; l++) {
                            utility_draw_pixel(x + j * scale + l, y + i * scale + k, color);
                        }
                    }
                }
            }
        }
        return;
    }
    ttf_blit_glyph(atlas, c, (int)x, (int)y, color,
        [&](int px, int py, uint8_t alpha, uint32_t fg) {
            color_t under = this->window->surface->getPixel(px, py);
            utility_draw_pixel((unsigned)px, (unsigned)py, ttf_blend_over(under, fg, alpha));
        });
};

void FileManager::utility_draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            // const int scale = 2;
            for (unsigned k = 0; k < scale; k++) {
                for (unsigned l = 0; l < scale; l++) {
                    uint8_t color = Icons[icon][i][j];
                    if (color == 0x00) continue;
                    if (color == 0x10) utility_draw_pixel(x + j * scale + l, y + i * scale + k, 0x0); 
                    utility_draw_pixel(x + j * scale + l, y + i * scale + k, vgaPaletteConvertorRGB32[color]);
                }
            }
        }
    }
};

int FileManager::EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
};

FileEntity* FileManager::readDirectory(const char* _path) {
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
};

char* FileManager::parent_path(const char *path) {
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
};

void FileManager::freeFileList(FileEntity* list) {
    if (list == nullptr) return;
    for (int i = 0; list[i].filename != nullptr; i++) free(list[i].filename);
    free(list);
    return;
}

FileManager::FileManager(void) {
    this->width = 864 / 1.25;
    this->height= 576 / 1.25;
    this->offsetX = 512 - (this->width / 2);
    this->offsetY = 384 - (this->height / 2);
    this->padding = 10; 
    this->frames = 1;
    this->thickness = 3;
    this->currentSelection = 0;
    this->window = new Window(width, height, offsetX, offsetY, "File Manager");
    this->files = readDirectory();
    this->path = nullptr;
    this->redraw(0b11111000);
    this->window->setKeyboardDelegate(this);
    this->window->setMouseDelegate(this);
};

void FileManager::redraw(uint8_t description = 0b00111000) {
    // Bit 8 is the (unused)
    // Bit 7 is the border 
    // Bit 6 is the background
    // Bit 5 is the title
    // Bit 4 is the options
    if ((description >> 6) & 1) this->draw_border();
    if ((description >> 5) & 1) this->draw_background();
    if ((description >> 4) & 1) this->draw_title();
    if ((description >> 3) & 1) this->draw_options();
};

void FileManager::draw_border(void) {
    for (int k = 0; k < frames; k++) {
        int x0 = k * padding;
        int y0 = k * padding;
        int w = width - 2 * k * padding;
        int h = height - 2 * k * padding;
        constexpr auto borderColor = 0xFFFFFFFF;
        for (int t = 0; t < thickness; t++) {
            for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + t, borderColor);
            for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + h - 1 - t, borderColor);
            for (int y = 0; y < h; y++) utility_draw_pixel(x0 + t, y0 + y, borderColor);
            for (int y = 0; y < h; y++) utility_draw_pixel(x0 + w - 1 - t, y0 + y, borderColor);
        }
    }
};

void FileManager::draw_background(void) {
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    for (int y = 0; y < innerH; y++) {
        for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, innerY + y, 0xFF403a39);
    }
    // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- window chrome").
    int titleBarHeight = 64;
    for (int y = innerY; y < titleBarHeight; y++) {
        for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, y, COLOR_TITLEBAR);
    }
    for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, titleBarHeight, COLOR_DIVIDER);
};

void FileManager::draw_title(void) {
    // Icon at scale 1 (32x32) so it comfortably fits inside the 64px title
    // band with room either side -- the previous default scale (2, 64x64)
    // overflowed past the divider into the body.
    int iconX = padding * frames + 4;
    int iconY = 18;
    utility_draw_icon(iconX, iconY, 8, 1); // 8 = Open Folder (icons.h)
    // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- current
    // path display"). this->path is nullptr at root.
    const char* titleString = (this->path != nullptr) ? this->path : "/";
    int charAdvance = ttf_font_char_advance(4);
    int textStartX = iconX + 48; // icon width (32) + gutter
    int innerX = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int availableWidth = (innerX + innerW) - textStartX;
    size_t maxChars = (charAdvance > 0) ? (size_t)(availableWidth / charAdvance) : 0;
    size_t len = strlen(titleString);
    if (len > maxChars) len = maxChars; // defensive clamp -- don't draw past the window edge
    for (size_t i = 0; i < len; i++) utility_draw_char(
        textStartX + (charAdvance * i),
        iconY,
        titleString[i],
        COLOR_W
    );
};

// See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- multi-column
// layout") for the column-major index math (column = idx/rowsPerColumn, row =
// idx%rowsPerColumn) shared with onKeyboard()/onMouseEvent(), and why this is
// bounded by page capacity now instead of drawing every entry unconditionally
// (the old unbounded loop could write past the window surface's pixel buffer
// for a large-enough directory -- Surface::putPixelUnsafe does no bounds check).
FileGridLayout FileManager::computeGridLayout(void) {
    FileGridLayout layout;
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    layout.listStartX = padding * frames + 78 - 49;
    layout.listStartY = padding * frames + 106;
    layout.columnWidth = 200;   // matches the original single-column mouse hit-box width
    layout.columnGutter = padding;
    layout.rowHeight = 35;
    int reservedBottom = 25; // room for the "Page N/M" indicator
    int availableHeight = (innerY + innerH) - layout.listStartY - reservedBottom;
    layout.rowsPerColumn = availableHeight / layout.rowHeight;
    if (layout.rowsPerColumn < 1) layout.rowsPerColumn = 1;
    int availableWidth = (innerX + innerW) - layout.listStartX;
    layout.columnsPerPage = availableWidth / (layout.columnWidth + layout.columnGutter);
    if (layout.columnsPerPage < 1) layout.columnsPerPage = 1;
    layout.capacityPerPage = layout.rowsPerColumn * layout.columnsPerPage;
    return layout;
}

// FIXME: The Directory is listed every time the FileManager is instanciated. It should only have to be listed once.
void FileManager::draw_options(void) {
    if (!files) return;
    FileGridLayout g = computeGridLayout();
    int charAdvance = ttf_font_char_advance(2);
    int page = this->currentSelection / g.capacityPerPage;
    int pageStart = page * g.capacityPerPage;
    int pageEnd = pageStart + g.capacityPerPage;
    if (pageEnd > this->fileCount) pageEnd = this->fileCount;
    for (int i = pageStart; i < pageEnd; i++) {
        int localIndex = i - pageStart;
        int column = localIndex / g.rowsPerColumn;
        int row = localIndex % g.rowsPerColumn;
        int x = g.listStartX + column * (g.columnWidth + g.columnGutter);
        int y = g.listStartY + row * g.rowHeight;
        const char* optionString = files[i].filename;
        size_t len = strlen(optionString);
        // Draw the icon.
        if (files[i].type == VFS_NODE_FILE) {
            int iconSelection = 0;
            if (EndsWith(optionString, ".wav") == 1) {
                iconSelection = 5;
            } else if (EndsWith(optionString, ".mp3") == 1) {
                iconSelection = 5;
            } else if (EndsWith(optionString, ".elf") == 1) {
                iconSelection = 9;
            } else if (EndsWith(optionString, ".lua")) {
                iconSelection = 10;
            } else {
                iconSelection = 6;
            }
            utility_draw_icon(x, y, iconSelection, 1);
        } else {
            utility_draw_icon(x, y, 7, 1);
        }
        // Defensive clamp -- keep the filename inside this column's own
        // footprint so it can't bleed into the next column or the gutter.
        int textOffset = 53; // matches the original icon-to-text x delta (92 - 39)
        size_t maxChars = (charAdvance > 0) ? (size_t)((g.columnWidth - textOffset) / charAdvance) : 0;
        size_t drawLen = len < maxChars ? len : maxChars;
        for (size_t j = 0; j < drawLen; j++) {
            utility_draw_char(x + textOffset + (charAdvance * j), y + 8, optionString[j], (i == currentSelection ? COLOR_B : COLOR_W), 2);
        }
    }
    // Page indicator, bottom-right of the content area.
    int totalPages = (this->fileCount + g.capacityPerPage - 1) / g.capacityPerPage;
    if (totalPages < 1) totalPages = 1;
    char pageBuf[32];
    sprintf(pageBuf, "Page %d/%d", page + 1, totalPages);
    int pbLen = (int)strlen(pageBuf);
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    int pbX = innerX + innerW - (charAdvance * pbLen) - padding;
    int pbY = innerY + innerH - 20;
    for (int k = 0; k < pbLen; k++) utility_draw_char(pbX + (charAdvance * k), pbY, pageBuf[k], COLOR_W, 2);
};

int FileManager::fileIndexAt(int x, int y) {
    FileGridLayout g = computeGridLayout();
    int rectHeight = 32;
    int page = this->currentSelection / g.capacityPerPage;
    // Bail early if x is left of the list, or past the last column.
    if (x < g.listStartX) return -1;
    int localCol = (x - g.listStartX) / (g.columnWidth + g.columnGutter);
    if (localCol < 0 || localCol >= g.columnsPerPage) return -1;
    // Bail if the point landed in the gutter between columns, not on a column itself.
    int colInnerX = (x - g.listStartX) - localCol * (g.columnWidth + g.columnGutter);
    if (colInnerX > g.columnWidth) return -1;
    // Bail early if y is above the first row, or past the last row.
    if (y < g.listStartY) return -1;
    int row = (y - g.listStartY) / g.rowHeight;
    if (row < 0 || row >= g.rowsPerColumn) return -1;
    if (y > g.listStartY + (row * g.rowHeight) + rectHeight) return -1;
    int localIndex = localCol * g.rowsPerColumn + row;
    int index = page * g.capacityPerPage + localIndex;
    if (index < 0 || index >= this->fileCount) return -1;
    return index;
}

bool FileManager::onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    (void)dx;
    (void)dy;
    (void)buttons;
    // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- hover cursor").
    set_cursor_id(fileIndexAt(x, y) != -1 ? 2 : 0);

    if (pressedEdge & 1) {
        // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- selection-highlight redraw").
        uint8_t redraw_description = 0b00111000;
        int clicked = fileIndexAt(x, y);
        if (clicked == -1) return false;
        if (this->currentSelection == clicked) {
            bool redraw_needed = false;
            redraw_description = 0;
            if (fileClick(&redraw_needed, &redraw_description) == false) return false;
            if (redraw_needed == false) return false;
        } else {
            this->currentSelection = clicked;
        }
        // printf_serial(false, NONE, "Clicked: %s\n", files[this->currentSelection].filename);
        // Trigger the same logic as pressing Enter
        // (or just redraw the selection highlight for now)
        this->redraw(redraw_description);
        return true;
    };
    return false;
};

// See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp —
// running-ELF tracking") for why this lives here instead of in elf.cpp.
#define MAX_TRACKED_ELF_TASKS 16
struct RunningElfEntry {
    char filename[VFS_MAX_NAME];
    task_t* task;
    uint32_t pid; // See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- task/stack
                  // reaper"): once the reaper exists, task->state alone can't
                  // prove this entry still refers to the run we tracked -- the
                  // slot may have been reaped and recycled for an unrelated
                  // task. pid disambiguates.
};
static RunningElfEntry runningElfTasks[MAX_TRACKED_ELF_TASKS] = {};

static bool isElfAlreadyRunning(const char* filename) {
    for (int i = 0; i < MAX_TRACKED_ELF_TASKS; i++) {
        if (!runningElfTasks[i].task) continue;
        if (strcmp(runningElfTasks[i].filename, filename) != 0) continue;
        bool sameTask = runningElfTasks[i].task->pid == runningElfTasks[i].pid;
        if (sameTask && runningElfTasks[i].task->state != TASK_DEAD) return true;
        runningElfTasks[i].task = NULL; // finished, or slot recycled -- reclaim either way
    }
    return false;
}

static void trackElfTask(const char* filename, task_t* task) {
    for (int i = 0; i < MAX_TRACKED_ELF_TASKS; i++) {
        bool sameTask = runningElfTasks[i].task &&
            runningElfTasks[i].task->pid == runningElfTasks[i].pid;
        if (sameTask && runningElfTasks[i].task->state != TASK_DEAD) continue;
        strncpy(runningElfTasks[i].filename, filename, VFS_MAX_NAME - 1);
        runningElfTasks[i].filename[VFS_MAX_NAME - 1] = '\0';
        runningElfTasks[i].task = task;
        runningElfTasks[i].pid = task->pid;
        return;
    }
    // No free slot to track it in -- worst case this specific launch just
    // won't be guarded against a concurrent re-launch, nothing crashes.
}

bool FileManager::fileClick(bool* redrawNeeded, uint8_t* redraw_description) {
    const char* selection = files[this->currentSelection].filename;
    if (files[this->currentSelection].type == VFS_NODE_DIR) {
        if (strcmp(selection, "..") == 0) {
            if (this->path == nullptr) {
                // this code should never execute assuming readDirectory works correctly.
                serial_write_string("Illegal Directory Change\n", true, FAIL);
                return false;
            } else {
                char* _path = parent_path(this->path);
                if (!_path) {
                    serial_write_string("Failed to get the parent directory.\n", true, FAIL);
                    return false;
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
                size_t length = strlen(selection) + 2; // "/" + name + '\0'
                char* newPath = (char*)malloc(length);
                if (!newPath) {
                    serial_write_string("Failed to allocate path.\n", true, FAIL);
                    return false;
                }
                sprintf(newPath, "/%s", selection);
                this->path = newPath;
            } else {
                size_t length = strlen(this->path) + strlen(selection) + 2; // old + "/" + name + '\0'
                char* newPath = (char*)malloc(length);
                if (!newPath) {
                    serial_write_string("Failed to allocate path.\n", true, FAIL);
                    return false;
                }
                sprintf(newPath, "%s/%s", this->path, selection);
                free(this->path);
                this->path = newPath;
            }
        }
        this->currentSelection = 0; // Default selection to the first option, always. 
        FileEntity* newFiles = readDirectory(this->path != nullptr ? this->path : "/");
        if (newFiles) {
            freeFileList(this->files);
            this->files = newFiles;
        } else {
            serial_write_string("Failed to load new directory.\n", true, FAIL);
            return false;
        }
        *(redraw_description) |= 0b00111000;
        *(redrawNeeded) = true;
    } else if (files[this->currentSelection].type == VFS_NODE_FILE) {
        if (EndsWith(files[this->currentSelection].filename, ".wav") == 1) {
            const char* fname = files[this->currentSelection].filename;
            size_t len = strlen(fname);
            char* path = (char*)malloc(len + 2);
            if (!path) {
                serial_write_string("Failed to allocate path buffer\n", FAIL);
                return false;
            }
            path[0] = '/';
            memcpy(path + 1, fname, len + 1);
            vfs_node_t* node = vfs_find(path);
            uint32_t buffer_len = node ? (uint32_t)node->size : 0;
            FILE* file = fopen(path, "r");
            free(path);
            if (!file || buffer_len == 0) {
                serial_write_string("Failed to open WAV file.\n", false, FAIL);
                if (file) fclose(file);
                return false;
            }
            char* buffer = (char*)malloc(buffer_len);
            fread(buffer, 1, buffer_len, file);
            fclose(file);
            struct wav_info_t* wav_info = read_wav_info(buffer, buffer_len);
            play_wav(wav_info, 0);
            free(buffer);
        } else if (EndsWith(files[this->currentSelection].filename, ".mp3") == 1) {
            const char* fname = files[this->currentSelection].filename;
            size_t len = strlen(fname);
            char* path = (char*)malloc(len + 2);
            if (!path) {
                serial_write_string("Failed to allocate path buffer\n", FAIL);
                return false;
            }
            path[0] = '/';
            memcpy(path + 1, fname, len + 1);
            vfs_node_t* node = vfs_find(path);
            uint32_t buffer_len = node ? (uint32_t)node->size : 0;
            FILE* file = fopen(path, "r");
            free(path);
            if (!file || buffer_len == 0) {
                serial_write_string("Failed to open MP3 file.\n", false, FAIL);
                if (file) fclose(file);
                return false;
            }
            char* buffer = (char*)malloc(buffer_len);
            fread(buffer, 1, buffer_len, file);
            fclose(file);
            struct mp3_info_t* mp3_info = read_mp3_info((uint8_t*)buffer, buffer_len);
            if (mp3_info) {
                play_mp3(mp3_info, 0);
                // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- MP3 playback buffer lifetime").
                while (AC97IsPlaying()) {
                    asm volatile("hlt");
                }
            } else {
                serial_write_string("Failed to parse MP3 file\n", false, FAIL);
            }
            free(buffer);
        } else if (EndsWith(files[this->currentSelection].filename, ".elf") == 1) {
            openPath:
                const char* fname = files[this->currentSelection].filename;
                if (isElfAlreadyRunning(fname)) {
                    serial_write_string("ELF: already running, ignoring launch request.\n", false, FAIL);
                    return false;
                }
                size_t len = strlen(fname);
                char* path = (char*)malloc(len + 2);
                if (!path) {
                    serial_write_string("Failed to allocate path buffer\n", FAIL);
                    return false;
                }
                path[0] = '/';
                memcpy(path + 1, fname, len + 1);
                vfs_node_t* node = vfs_find(path);
                uint32_t buffer_len = node ? (uint32_t)node->size : 0;
                FILE* file = fopen(path, "r");
                free(path);
            if (!file || buffer_len == 0) {
                serial_write_string("Failed to open ELF file.\n", false, FAIL);
                if (file) fclose(file);
                return false;
            }
            char* buffer = (char*)malloc(buffer_len);
            fread(buffer, 1, buffer_len, file);
            fclose(file);
            void* entry = elf_load_file(buffer, buffer_len);
            if (entry) {
                // See docs/DOCS.md ("mods/dev/elf/elf.cpp — elf_spawn()")
                // for why buffer is intentionally NOT freed here on success.
                task_t* task = elf_spawn(entry);
                if (task) trackElfTask(fname, task);
                else free(buffer);
            } else {
                serial_write_string("Failed to load ELF (no entry point)\n", false, FAIL);
                free(buffer);
            }
        }
    }
    return true; // success
}

bool FileManager::onKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    bool redrawNeeded = false; // prevents other keys from redrawing.
    uint8_t redraw_description = 0;
    if (key == 's') {
        if (this->currentSelection + 1 < this->fileCount) {
            this->currentSelection++;
            // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp -- selection-highlight redraw").
            redraw_description |= 0b00111000;
            redrawNeeded = true;
        }
    } else if (key == 'w') {
        if (this->currentSelection > 0) {
            this->currentSelection--;
            redraw_description |= 0b00111000;
            redrawNeeded = true;
        }
    } else if (key == 'd') {
        // See docs/DOCS.md ("mods/core/wingman/suite/explorer/explorer.cpp --
        // multi-column layout") -- +/- rowsPerColumn jumps a whole column,
        // and crossing a page's last column naturally rolls into the next
        // page's first column since page is just column/columnsPerPage.
        FileGridLayout g = computeGridLayout();
        int next = this->currentSelection + g.rowsPerColumn;
        if (next >= this->fileCount) next = this->fileCount - 1;
        if (next != this->currentSelection && next >= 0) {
            this->currentSelection = next;
            redraw_description |= 0b00111000;
            redrawNeeded = true;
        }
    } else if (key == 'a') {
        FileGridLayout g = computeGridLayout();
        int prev = this->currentSelection - g.rowsPerColumn;
        if (prev < 0) prev = 0;
        if (prev != this->currentSelection) {
            this->currentSelection = prev;
            redraw_description |= 0b00111000;
            redrawNeeded = true;
        }
    } else if (key == '\n') {
        if (fileClick(&redrawNeeded, &redraw_description) == false) return false;
    }
    if (redrawNeeded) {
        this->redraw(redraw_description);
    }
    return redrawNeeded;
}

FileManager::~FileManager() {
    freeFileList(this->files);
    free(this->path);
    delete this->window;
    return;
}