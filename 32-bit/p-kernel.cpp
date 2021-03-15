/// Copyright (C) 2021 KingUndeadCodes (https://github.com/KingUndeadCodes)
/// Protected under MIT License which lays down the terms of use.

void print_s(char* str) {
   char* video_memory = (char*) 0xb8000;
   char string[] = "Welcome to Pumpkin OS! (Flying Chicken || Revision 1.0.2)";
   int length = sizeof(string) / sizeof(char);
   for (int i = 0; i < length; i++) {
      *video_memory = string[i];
      video_memory += 2;
   }
}

extern "C" void _start() {
   const bool found = 1;
   if (found == true) {
      // pass [ print_s will be here ]
   } else {
      // pass [ print_s will be here also ]
   }
}
