/// Copyright (C) 2020 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdbool.h>
#include <stdint.h>


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


// Input \ Output
void outb(unsigned short port, unsigned char val){
  asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port){
  unsigned char returnVal;
  asm volatile ("inb %1, %0"
  : "=a"(returnVal)
  : "Nd"(port));
  return returnVal;
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
