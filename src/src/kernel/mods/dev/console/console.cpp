// #include "../vbe/vga_table.h"
#include "../serial/serial.h"
#include "console.h"
#include "../cmos/cmos.h"
#include "../vbe/vbe.h"
#include "../pit/pit.h"
#include "../kb/kb.h"

/*
/// Set Pixel<x, y> to the color c
void DrawPixel(int x, int y, int c = 0xF) {
    unsigned char* Pixel = (unsigned char*)0xA0000 + 320 * y + x;
    *Pixel = c;
}
*/

// https://wiki.osdev.org/Terminals
// https://wiki.osdev.org/Creating_A_Shell

/*
typedef struct {
    unsigned char r, g, b;
} RGBColor;

RGBColor convertStruct(uint32_t c) {
    RGBColor color;
    color.r = (c >> 16) & 0xFF;
    color.g = (c >> 8) & 0xFF;
    color.b = c & 0xFF;
    return color;
}

uint32_t convertInteger(RGBColor color) {
    return (color.r << 16) | (color.g << 8) | color.b;
}

RGBColor calculateGradientColor(RGBColor color1, RGBColor color2, int percentage) {
    RGBColor result;
    result.r = (unsigned char)((color1.r * (100 - percentage) + color2.r * percentage + 50) / 100);
    result.g = (unsigned char)((color1.g * (100 - percentage) + color2.g * percentage + 50) / 100);
    result.b = (unsigned char)((color1.b * (100 - percentage) + color2.b * percentage + 50) / 100);
    return result;
}
*/

namespace VBEScreen {
    const int fontScale = 2;

    // See docs/DOCS.md ("Font Rendering System" -- "Integration: the 6 non-panic call sites").
    void draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned bg) {
        for (unsigned i = 0; i < 8 * fontScale; i++) {
            for (unsigned j = 0; j < 8 * fontScale; j++) {
                draw_pixel(x + j, y + i, bg);
            }
        }

        for (unsigned i = 0; i < 8; i++) {
            for (unsigned j = 0; j < 8; j++) {
                if (Font[(int)c][i] & (1 << j)) {
                    for (unsigned k = 0; k < fontScale; k++) {
                        for (unsigned l = 0; l < fontScale; l++) {
                            draw_pixel(x + j * fontScale + l, y + i * fontScale + k, color);
                        }
                    }
                }
            }
        }
    }

    void draw_string(const char* str, unsigned x, unsigned y, unsigned color, unsigned bg) {
        for (unsigned i = 0; str[i] != '\0'; i++) {
            draw_char(x + i * (8 * fontScale), y, str[i], color, bg);
        }
    }

    /*
    void draw_char_gradiant(unsigned x, unsigned y, char c, unsigned color1, unsigned color2, unsigned bg) {
        RGBColor rgb1 = convertStruct(color1);
        RGBColor rgb2 = convertStruct(color2);

        for (unsigned i = 0; i < 8 * fontScale; i++) {
            for (unsigned j = 0; j < 8 * fontScale; j++) {
                draw_pixel(x + j, y + i, bg);
            }
        }

        for (unsigned i = 0; i < 8; i++) {
            int percent = (i * 100) / 7;
            RGBColor gradient = calculateGradientColor(rgb1, rgb2, percent);
            unsigned color = convertInteger(gradient);

            for (unsigned j = 0; j < 8; j++) {
                if (Font[(unsigned char)c][i] & (1 << j)) {
                    for (unsigned k = 0; k < fontScale; k++) {
                        for (unsigned l = 0; l < fontScale; l++) {
                            draw_pixel(x + j * fontScale + l, y + i * fontScale + k, color);
                        }
                    }
                }
            }
        }
    }
    */
}

class Terminal {
public:
    static const int screenWidth = 1024;
    static const int screenHeight = 768;
    static const int charWidth = 8 * VBEScreen::fontScale;
    static const int charHeight = 8 * VBEScreen::fontScale;
    static const int defaultForegroundColor = COLOR_W;
    static const int defaultBackgroundColor = 0xb1a1c;
    static const int defaultLineSpacing = 2;

private:
    char* buffer;
    int bufferSize;
    int bufferIndex;
    int cursorX;
    int cursorY;
    int lineSpacing;
    int currentTextForegroundColor;
    int currentTextBackgroundColor;

    // Delegates to vbe.cpp's memmove()-based scroll instead of walking pixels itself.
    void scroll() {
        int lineHeight = charHeight + lineSpacing;
        scroll_framebuffer_up((unsigned)lineHeight, (unsigned)currentTextBackgroundColor);
        cursorY -= lineHeight;
    }

    inline char checkCharacter(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 255;
    }

public:
    Terminal(int bufferSize, int cursorX, int cursorY, int lineSpacing = defaultLineSpacing)
        : bufferSize(bufferSize), cursorX(cursorX), cursorY(cursorY), lineSpacing(lineSpacing) {
        buffer = (char*)malloc(bufferSize);
        bufferIndex = 0;
        currentTextForegroundColor = defaultForegroundColor;
        currentTextBackgroundColor = defaultBackgroundColor;
    }

    ~Terminal() {
        if (buffer) free(buffer);
    }

    void backspace() {
        if (bufferIndex == 0) return;
        // char lastCharacter = buffer[bufferIndex - 1];
        buffer[--bufferIndex] = '\0';
        cursorX -= charWidth;
        if (cursorX < 0) {
            cursorX = screenWidth - charWidth;
            cursorY -= charHeight + lineSpacing;
        }
        VBEScreen::draw_char(cursorX, cursorY, ' ', currentTextForegroundColor, currentTextBackgroundColor);
    }

    void write(const char* str) {
        for (int i = 0; str[i] != '\0'; i++) {
            char c = str[i];
            if (c == '\n') {
                cursorX = 0;
                cursorY += charHeight + lineSpacing;
                if (cursorY + charHeight >= screenHeight) scroll();
                buffer[bufferIndex++] = c;
            } else if (c == '\b') {
                backspace();
            } else if (c == '\t') {
                continue;
            } else if (c == '\\' && str[i + 1] == '(') {
                int color = 0;
                bool valid = true;
                for (int j = 0; j < 6; j++) {
                    char hex = str[i + 2 + j];
                    char val = checkCharacter(hex);
                    if (val == 255) { valid = false; break; }
                    color = (color << 4) | val;
                }
                if (valid && str[i + 8] == ')') {
                    currentTextForegroundColor = color;
                    i += 8;
                    continue;
                }
            } else {
                if (bufferIndex < bufferSize) {
                    buffer[bufferIndex++] = c;
                    VBEScreen::draw_char(cursorX, cursorY, c, currentTextForegroundColor, currentTextBackgroundColor);
                    cursorX += charWidth;
                    if (cursorX + charWidth >= screenWidth) {
                        cursorX = 0;
                        cursorY += charHeight + lineSpacing;
                        if (cursorY + charHeight >= screenHeight) scroll();
                    }
                }
            }
        }
    }

    void setTextForegroundColor(int color) {
        currentTextForegroundColor = color;
    }

    void *operator new(size_t size) { return malloc(size); }
    void *operator new[](size_t size) { return malloc(size); }
    void operator delete(void *p) { free(p); }
    void operator delete[](void *p) { free(p); }
};

static Terminal* terminal = nullptr;
static int associatedKeyboardEvent = -1;
static bool suppressCharacterOutput = false;

static void terminalWriteCharacter(char key, bool shift, bool meta, unsigned char scancode) {
    if (!terminal || suppressCharacterOutput || scancode > 0x3A) return;
    switch (key) {
        case '\b': terminal->backspace(); break;
        case '\n': terminal->write("\n"); break;
        case '\t': terminal->write("\t"); break;
        default: {
            if (key == 0x1B) return; // Escape key
            if (key >= 'a' && key <= 'z' && shift) key -= 32;
            else if (key >= 'A' && key <= 'Z' && !shift) key += 32;
            char buf[2] = { key, '\0' };
            terminal->write(buf);
        }
    }
}

void terminal_delete(void) {
    if (terminal) {
        delete terminal;
        terminal = nullptr;
    }
    if (associatedKeyboardEvent != -1) kb_remove_event(associatedKeyboardEvent);
}

void terminal_write(const char* str) {
    if (terminal) terminal->write(str);
}

void graphics_initalize_stage1() {
    init();
    // See docs/DOCS.md ("Font Rendering System") -- must run after
    // initialize_memory_pool() (already true here, called
    // from kernel_main() before this function) so malloc is available, and
    // before Terminal/any Wingman widget exists so every real draw-char
    // call site always sees a baked (or explicitly-invalid-with-fallback)
    // atlas.
    ttf_font_init();
    fill(Terminal::defaultBackgroundColor);
    terminal = new Terminal(1024 * 1024, 0, 0);
    terminal->setTextForegroundColor(COLOR_W);
}

void graphics_initalize_stage2() {
    terminal->write("\n\n# Welcome to \\(FF5C04)PumpkinOS\\(FFFFFF)!\n");
    // terminal->write("Type \\(C666D8)helpme\\(FFFFFF) if you need assistance.\n");
    /*
    VBEScreen::draw_char_gradiant(0 * VBEScreen::fontScale, 0, 'H', COLOR_R, COLOR_B, Terminal::defaultBackgroundColor);
    VBEScreen::draw_char_gradiant(8 * VBEScreen::fontScale, 0, 'e', COLOR_G, COLOR_B, Terminal::defaultBackgroundColor);
    VBEScreen::draw_char_gradiant(16 * VBEScreen::fontScale, 0, 'l', COLOR_G, COLOR_B, Terminal::defaultBackgroundColor);
    VBEScreen::draw_char_gradiant(24 * VBEScreen::fontScale, 0, 'l', COLOR_G, COLOR_B, Terminal::defaultBackgroundColor);
    */
    associatedKeyboardEvent = kb_add_event(&terminalWriteCharacter);
}
