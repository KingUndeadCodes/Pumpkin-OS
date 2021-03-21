/// Copyright (C) 2020 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdint.h>

void printk(uint8_t color)
{
   volatile char *video_memory = (volatile char*)0xB8000;
   char string[] = "haha this text go brrrrr";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory++ = string[i];
      *video_memory++ = color;
   }
}

void outb(unsigned short port, unsigned char val)
{
   asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port)
{
   unsigned char returnVal;
   asm volatile ("inb %1, %0"
   : "=a"(returnVal)
   : "Nd"(port));
   return returnVal;
}

extern "C" void _start() {
   const bool found = true;
   if (found == true) {
      printk(15);
   } else {
      // pass
   }
}

