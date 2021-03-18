/// Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Going to add this at some point.
static const size_t WIDTH = 80;
static const size_t HEIGHT = 25;

// From: https://github.com/Absurdponcho/YoutubeOS/blob/1392693673d02b38bb291fead334a706d423ec51/IO.cpp
void outb(unsigned short port, unsigned char val){
  asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

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
