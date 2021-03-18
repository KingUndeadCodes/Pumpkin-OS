/// Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdint.h>
#include <stddef.h>

// Going add this at some point.
static const size_t WIDTH = 80;
static const size_t HEIGHT = 25;

void printk(uint8_t color) {
   char* video_memory = (char*) 0xb8000;
   char string[] = "Welcome to Pumpkin OS! (Flying Chicken || Revision 1.0.2)";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory++ = string[i];
      *video_memory++ = color;
   }
}

extern "C" void _start(void) {
   const bool found = 1;
   if (found == true) {
      printk(15);
   } else {
      // pass
   }
}
