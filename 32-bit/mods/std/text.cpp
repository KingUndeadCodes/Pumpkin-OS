#include "stdint.h"

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

static u32 strlen(char* string) {
    u32 len = 0;
    while (string[len] && string[len] != '\0') {len++;}
    return len;
};

void printf(char* string, u8 color = 15) {
    volatile char *vmem = (volatile char*)0xb8000;
    for (int i = 0; i < strlen(string); i++) {
       vmem[2 * i] = string[i];
       vmem[2 * i + 1] = color;
    }
};
