/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

// types
typedef unsigned short int u16;
typedef unsigned char u8;
typedef unsigned int u32;
typedef signed char i8;
typedef short int i16;
typedef int i32;

// system vars
volatile char *video_memory = (volatile char*)0xB8000;

void println() {
   char string[] = "OK...";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory++ = string[i];
      *video_memory++ = 0x0f;
   };
};

extern "C" void _start() {
   println();
};
