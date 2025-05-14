// #include "../dev/vbe/vga_table.h"
#include "../dev/serial/serial.h"
#include "../dev/audio/speaker.h"
#include "./include/graphics.h"
#include "../dev/cmos/cmos.h"
#include "../dev/vbe/vbe.h"
#include "../dev/pit/pit.h"
#include "../dev/kb/kb.h"
#include <text.h>

/*
/// Set Pixel<x, y> to the color c
void DrawPixel(int x, int y, int c = 0xF) {
    unsigned char* Pixel = (unsigned char*)0xA0000 + 320 * y + x;
    *Pixel = c;
}
*/

// https://wiki.osdev.org/Terminals
// https://wiki.osdev.org/Creating_A_Shell

namespace VBEScreen {
    const int fontScale = 2; // Must be divisible by 2
    // Function simmlar to draw_char but scales the font by a factor of 4
    void draw_char(unsigned x, unsigned y, char c, unsigned color) {
        // 8x8 pixel font
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
    void draw_string(const char* str, unsigned x, unsigned y, unsigned color) {
        for (unsigned i = 0; i < strlen(str); i++) {
            draw_char(x + i * (8 * fontScale), y, str[i], color);
        }
    }
}

class Terminal {

    public:
        
        static const int defaultForegroundColor = COLOR_W;
        static const int defaultBackgroundColor = 0xb1a1c;

    private:

        char* buffer;
        int bufferSize;
        int bufferIndex;
        int cursorX;
        int cursorY;
        int lineSpacing;

        int currentTextForegroundColor = Terminal::defaultForegroundColor;
        int currentTextBackgroundColor = Terminal::defaultBackgroundColor;

        // Write a function that given a bufferIndex, would modify the cursorX and cursorY with the correct position of that characters index, account for newlines, assuming a resolution of 1024x768
        void setCursor(int bufferIndex) {
            // Reset cursor positions
            this->cursorX = 0;
            this->cursorY = 0;
        
            // Iterate through the buffer up to the given index
            for (int i = 0; i < bufferIndex; i++) {
                if (this->buffer[i] == '\n') {
                    // Move to the next line
                    this->cursorX = 0;
                    this->cursorY += (8 * VBEScreen::fontScale) + this->lineSpacing;
                } else {
                    // If the cursor exceeds the screen width, move to the next line
                    this->cursorX += (8 * VBEScreen::fontScale);
                    if (this->cursorX + (8 * VBEScreen::fontScale) >= 1024) {
                        this->cursorX = 0;
                        this->cursorY += (8 * VBEScreen::fontScale) + this->lineSpacing;
                    }
                }
                // clearCharacter();
            }
        }

        void clearCharacter() {
            for (int i = 0; i < (8 * VBEScreen::fontScale); i++) {
                for (int j = 0; j < (8 * VBEScreen::fontScale); j++) {
                    draw_pixel(this->cursorX + j, this->cursorY + i, 0xb1a1c);
                }
            }
        }

        inline char checkCharacter(char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 255;
        }

    public:
        Terminal(int bufferSize, int cursorX, int cursorY, int lineSpacing = 2) {
            this->bufferSize = bufferSize;
            this->buffer = (char*)malloc(bufferSize);
            this->bufferIndex = 0;
            this->cursorX = cursorX;
            this->cursorY = cursorY;
            this->lineSpacing = lineSpacing;
        }

        /*
        ~Terminal() {
            free(this->buffer);
        }
        */

        int getLineSpacing() {
            return this->lineSpacing;
        }

        void backspace(void) {
            /*
            char character[2] = {this->buffer[this->bufferIndex - 1], '\0'};
            serial_write_string("\n", false, NONE);
            serial_write_string(character, false, NONE);
            */
            if (this->bufferIndex == 0) {
                // beep(950, 1);
                return;
            }
            char deletedChar = this->buffer[--this->bufferIndex];
            this->buffer[this->bufferIndex] = '\0';
            setCursor(this->bufferIndex);
            clearCharacter();
        }

        void write(const char* string) {
            for (int i = 0; string[i] != '\0'; i++) {
                const char c = string[i];
                if (c == '\n') {
                    this->cursorX = 0;
                    this->cursorY += (8 * VBEScreen::fontScale) + this->lineSpacing; // Move to the next line
                    this->buffer[this->bufferIndex++] = c;
                } else if (c == '\b') {
                    this->backspace(); // Backspace
                } else if (c == '\t') {
                    continue; // Tab
                } 
                else {
                    if (this->bufferIndex < this->bufferSize) {
                        if (c == '\\') {
                            // Handle this as a special case
                            // \(HEX_COLOR) where HEX_COLOR is a 6 digit hex color code. At any point the format is incorrect ignore the color change and default to write the character inclosed and all previous characters.
                            if (string[i + 1] == '(') {
                                int color = 0;
                                int index = 0;
                                for (int j = 0; j < 6; j++) {
                                    if (checkCharacter(string[i + 2 + j]) == 255) {
                                        break; // Invalid character, ignore the color change
                                    } else {
                                        color = (color << 4) | checkCharacter(string[i + 2 + j]);
                                        index++;
                                    }
                                }
                                if (string[i + 2 + 8] != ')') {
                                    continue; // Invalid format, ignore the color change
                                } else {
                                    this->setTextForegroundColor(color);
                                    i += 10; // Skip the color code
                                }
                                continue;
                            }
                        }
                        this->buffer[this->bufferIndex++] = c;
                        VBEScreen::draw_char(this->cursorX, this->cursorY, c, currentTextForegroundColor);
                        this->cursorX += (8 * VBEScreen::fontScale); // Move to the next character position
                        if (this->cursorX + (8 * VBEScreen::fontScale) >= 1024) { // If the cursor goes off the screen, move to the next line
                            this->cursorX = 0;
                            this->cursorY += (8 * VBEScreen::fontScale) + this->lineSpacing; // Move to the next line
                        }
                    }
                }
            }
        }
        
        void setTextForegroundColor(int color) {
            this->currentTextForegroundColor = color;
        }

        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

typedef struct TerminalPreferences {
    bool suppressCharacterOutput;
} __attribute__((packed)); 

static Terminal* terminal = NULL;
static TerminalPreferences terminalPreferences = { .suppressCharacterOutput = false };

void terminalWriteCharacter(char key, bool shift, bool meta, unsigned char scancode) {
    if (!terminal) {
        serial_write_string("Terminal is NULL\n", false, NONE);
        return;
    }
    if (terminalPreferences.suppressCharacterOutput) {
        return; // Suppress character output
    }
    if (scancode > 0x3A) return;
    switch (key) {
        case '\b': terminal->backspace(); break;
        case '\n': terminal->write("\n"); break;
        case '\t': terminal->write("\t"); break;
        default: {
            if (key == 0x1B) { return; } // Ignore escape key
            if (key >= 'a' && key <= 'z') {
                if (shift) { key -= 32; } // Convert to uppercase
            } else if (key >= 'A' && key <= 'Z') {
                if (!shift) { key += 32; } // Convert to lowercase
            }
            char* string = (char*)malloc(2);
            string[0] = key;
            string[1] = '\0';
            terminal->write(string);
            free(string);
            break;
        }
    }
}

void terminal_write(const char* string) {
    if (!terminal) {
        // serial_write_string("Terminal is NULL\n", false, NONE);
        return;
    }
    terminal->write(string);
    return;
}

void graphics_initalize_stage1(void) {
    init();
    fill(Terminal::defaultBackgroundColor);
    terminal = new Terminal((1024 * 1024), 0, 0);
    terminal->setTextForegroundColor(COLOR_W);
    return;
}

void graphics_initalize_stage2(void) {
    terminal->write("\n\n# Welcome to \\(FF5C0400)PumpkinOS\\(FFFFFF00)!\n");
    kb_add_event(&terminalWriteCharacter);
    return;
}