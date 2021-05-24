#include "../std/stdint.h"

static inline unsigned int strlen(char* str) {
    unsigned int len = 0;
    while (str[len] && str[len] != '\0') {len++;}
    return len;
}

inline void printf(char* string, u8 color = 15) {
    volatile char *video_memory = (volatile char*)0xb8000;
    long length = strlen(string);
    for (int i = 0; i < length; i++) {
       video_memory[2 * i] = string[i];
       video_memory[2 * i + 1] = color;
    }
}
