/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include <stdint.h>

inline void println(uint8_t color = 10) {
    volatile char *video_memory = (volatile char*)0xb8000;
    char string[] = "[1] Found...";
    long length = sizeof(string) / sizeof(char);
    for (int i = 0; i < length; i++) {
       video_memory[2 * i] = string[i];
       video_memory[2 * i + 1] = color;
    };
};

extern "C" void _start() {
    println(5);
}
