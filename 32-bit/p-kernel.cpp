/// Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

#include <stdint.h>

void system_print(char* str) {
  char* video_memory = (char*) 0xb8000;
  char string[] = "Welcome to my OS!";
  int length = sizeof(string) / sizeof(char);
  for (int i = 0; i < length; i++) {
     *video_memory = string[i];
     video_memory += 2;
  }
}

extern "C" void _start() {

   const char *name = "Flying Chicken || Revision 1.0.2";
   const bool found = 1;

   if (found == true) {
      // pass
   } else {
      // pass
   }

}
