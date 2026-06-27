#include "./explorer.h"

#define COLOR_R 0xFFFF0000
#define COLOR_G 0xFF00FF00
#define COLOR_B 0xFF0000FF
#define COLOR_W 0xFFFFFFFF

/*
inline color_t rgb24_to_argb32(uint32_t color24) {
    return 0xFF000000 | (color24 & 0x00FFFFFF);
}
*/

void FileManager::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void FileManager::utility_draw_char(unsigned int x, unsigned int y, char c, unsigned int color, unsigned int scale = 4U) {
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
};

void FileManager::draw_title(void) {
    utility_draw_icon(padding * frames + 4, padding * frames + 4, 0);
    const char* titleString = "Select File";
    for (int i = 0; i < strlen(titleString); i++) utility_draw_char(
        padding * frames + 4 + 78 + (32 * i),
        padding * frames + 24,
        titleString[i], 
        COLOR_W
    );   
};

// FIXME: The Directory is listed every time the FileManager is instanciated. It should only have to be listed once.
void FileManager::draw_options(void) {
    if (!files) return;
    int i = 0;
    while (files[i].filename != nullptr) {
        const char* optionString = files[i].filename;
        size_t len = strlen(optionString);
        // This loop will clear the icon border.
        // TODO: Move this into utility_draw_icon? (Is it a good idea?)
        /*
        for (int x = 0; x < 32; x++) {
            for (int y = 0; y < 32; y++) {
                utility_draw_pixel(padding * frames + 78 - 49 + x, padding * frames + 106 + (35 * i) + y, 0xFF403a39);
            }
        }
        */
        // Draw the icon.
        if (files[i].type == VFS_NODE_FILE) {
            int iconSelection = 0;
            if (EndsWith(optionString, ".wav") == 1) {
                iconSelection = 5;
            } else if (EndsWith(optionString, ".elf") == 1) {
                iconSelection = 9;
            } else if (EndsWith(optionString, ".lua")) {
                iconSelection = 10;
            } else {
                iconSelection = 6;
            }
            utility_draw_icon(padding * frames + 78 - 49, padding * frames + 106 + (35 * i), iconSelection, 1);
        } else {
            // if (memcmp(files[i].filename, path, strlen(files[i].filename)) == 0) {
            //     draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 8, 1);
            // } else {
            //     draw_icon(offsetX + padding * frames + 78 - 49, offsetY + padding * frames + 106 + (35 * i), 7, 1);
            // }
            utility_draw_icon(padding * frames + 78 - 49, padding * frames + 106 + (35 * i), 7, 1);
        }
        for (size_t j = 0; j < len; j++) {
            utility_draw_char(padding * frames + 4 + 78 + (16 * j), padding * frames + 106 + (35 * i) + 8, optionString[j], (i == currentSelection ? COLOR_B : COLOR_W), 2);
        }
        i++;
    }
};

bool FileManager::onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons) {
    (void)dx;
    (void)dy;
    if (buttons == 1) {
        uint8_t redraw_description = 0b00001000;
        int listStartX = padding * frames + 78 - 49;
        int listStartY = padding * frames + 106;
        int rowHeight = 35;
        int rectWidth = 200;
        int rectHeight = 32;
        // Bail early if x is outside the list region
        if (x < listStartX || x > listStartX + rectWidth) return false;
        // Bail early if y is above the first entry
        if (y < listStartY) return false;
        int clicked = (y - listStartY) / rowHeight;
        // Validate: within bounds, and within the row's drawn height (not in the gap)
        if (clicked < 0 || clicked >= this->fileCount) return false;
        if (y > listStartY + (clicked * rowHeight) + rectHeight) return false;
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
            openFile:
                const char* fname = files[this->currentSelection].filename;
                size_t len = strlen(fname);
                char* path = (char*)malloc(len + 2);
                if (!path) {
                    serial_write_string("Failed to allocate path buffer\n", FAIL);
                    return false;
                }
                path[0] = '/';
                memcpy(path + 1, fname, len + 1);
                FILE* file = fopen(path, "r");
                free(path);
            if (!file) {
                serial_write_string("Failed to open file.\n", false, FAIL);
                return false;
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
                    return false;
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
    return true; // success
}

bool FileManager::onKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
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