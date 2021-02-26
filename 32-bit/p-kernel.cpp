/// Copyright (c) 2020 KingUndeadCodes
/// Protected under MIT License which lays down the terms of use.


/// https://stackoverflow.com/questions/41519336/extended-ascii-characters-are-printed-in-yellow-instead-of-white-osdev

#define VIDEO_MEM 0xb8000

void putChar(char character, short col, short row, unsigned char attr) {
    volatile unsigned char* vid_mem = (unsigned char *) VIDEO_MEM;
    int offset = (row*80 + col)*2;
    vid_mem += offset;
    if(!attr) {
        attr = default_color;
    }
    *(unsigned short int *)vid_mem = (attr<<8)+character;
}

extern "C" void _start(){

   const char *name = "Flying Chicken || Revision 1.0.2";
   const bool found = 1;

   *((int*)0xb8001) = 0x0f;
   *((int*)0xb8003) = 0x0f;
   *((int*)0xb8005) = 0x0f;
   *((int*)0xb8007) = 0x0f;
   *((int*)0xb8009) = 0x0f;
   *((int*)0xb800b) = 0x0f;
   *((int*)0xb800d) = 0x0f;

   *((char*)0xb8000) = 'F';
   *((char*)0xb8002) = 'o';
   *((char*)0xb8004) = 'u';
   *((char*)0xb8006) = 'n';
   *((char*)0xb8008) = 'd';
   *((char*)0xb800a) = ':';
   *((char*)0xb800c) = ' ';


   if (found == true) {

      *((int*)0xb800f) = 0x2f;
      *((int*)0xb8011) = 0x2f;
      *((int*)0xb8013) = 0x2f;
      *((int*)0xb8015) = 0x2f;

      *((char*)0xb800e) = 'T';
      *((char*)0xb8010) = 'r';
      *((char*)0xb8012) = 'u';
      *((char*)0xb8014) = 'e';

   } else {

      *((int*)0xb800f) = 0x04f;
      *((int*)0xb8011) = 0x04f;
      *((int*)0xb8013) = 0x04f;
      *((int*)0xb8015) = 0x04f;
      *((int*)0xb8017) = 0x04f;

      *((char*)0xb800e) = 'F';
      *((char*)0xb8010) = 'a';
      *((char*)0xb8012) = 'l';
      *((char*)0xb8014) = 's';
      *((char*)0xb8016) = 'e';

   }

   return;
}
