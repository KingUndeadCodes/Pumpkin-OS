/*
 * Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
 * Protected under MIT License which lays down the terms of use.
*/

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
