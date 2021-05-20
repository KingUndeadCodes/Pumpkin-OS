#include <stdint.h>

enum COLORS {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_PURPLE = 5,
    COLOR_BROWN = 6,
    COLOR_GRAY = 7,
    COLOR_DARK_GRAY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_PURPLE = 13,
    COLOR_YELLOW = 14,
    COLOR_WHITE = 15,
};

static inline unsigned int strlen(char* str) {
    unsigned int len = 0;
    while (str[len] && str[len] != '\0') {len++;}
    return len;
}

inline void printf(char* string, uint8_t color = 15) {
    volatile char *video_memory = (volatile char*)0xb8000;
    long length = strlen(string);
    for (int i = 0; i < length; i++) {
       video_memory[2 * i] = string[i];
       video_memory[2 * i + 1] = color;
    }
}
