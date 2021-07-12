#include "text.h"

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
