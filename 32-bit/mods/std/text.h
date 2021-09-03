#pragma once
#ifndef _TEXT_H
#define _TEXT_H
#include <stdint.h>
#include <stddef.h>

enum {
    COLOR_BLACK = 0x0,
    COLOR_BLUE = 0x1,
    COLOR_GREEN = 0x2,
    COLOR_CYAN = 0x3,
    COLOR_RED = 0x4,
    COLOR_PURPLE = 0x5,
    COLOR_BROWN = 0x6,
    COLOR_GRAY = 0x7,
    COLOR_DARK_GRAY = 0x8,
    COLOR_LIGHT_BLUE = 0x9,
    COLOR_LIGHT_GREEN = 0xA,
    COLOR_LIGHT_CYAN = 0xB,
    COLOR_LIGHT_RED = 0xC,
    COLOR_LIGHT_PURPLE = 0xD,
    COLOR_YELLOW = 0xE,
    COLOR_WHITE = 0xF,
};

const static size_t COLS = 80;
const static size_t ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

namespace Cursor {
   void disbaleCursor();
   void enableCursor(uint8_t cursor_start, uint8_t cursor_end);
   void moveCursor(int x, int y);
   uint16_t getCursor(void);
}

void printf(const char* string, uint8_t color = 15);

#endif
