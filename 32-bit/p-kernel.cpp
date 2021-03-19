/// Copyright (C) 2020 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdbool.h>
#include <stdint.h>
#include <io.h>

// Print function
void printk(uint8_t color) {
   char* video_memory = (char*) 0xb8000;
   char string[] = "Welcome to Pumpkin OS! (Flying Chicken || Revision 1.0.2)";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory++ = string[i];
      *video_memory++ = color;
   }
}

// Main function
extern "C" void _start() {
   const bool found = true;
   if (found == true) {
      // pass
   } else {
      // pass
   }
}
