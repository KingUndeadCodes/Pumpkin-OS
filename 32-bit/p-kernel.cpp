/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

#include <stdint.h>

void WriteCharacter(unsigned char c, unsigned char forecolour, unsigned char backcolour, int x, int y) {
     uint16_t attrib = (backcolour << 4) | (forecolour & 0x0F);
     volatile uint16_t * where;
     where = (volatile uint16_t *)0xB8000 + (y * 80 + x) ;
     *where = c | (attrib << 8);
}

void println() {
   volatile char *video_memory = (volatile char*)0xB8000;
   char string[] = "Pumpkin OS - 4-14-2021 - MIT";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory++ = string[i];
      *video_memory++ = 0x0f;
   };
};

extern "C" void _start() {
   println();
};
