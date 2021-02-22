#include <stdint.h>
 
enum video_type
{
    VIDEO_TYPE_NONE = 0x00,
    VIDEO_TYPE_COLOUR = 0x20,
    VIDEO_TYPE_MONOCHROME = 0x30,
};
 
uint16_t detect_bios_area_hardware(void)
{
    const uint16_t* bda_detected_hardware_ptr = (const uint16_t*) 0x410;
    return *bda_detected_hardware_ptr;
}
 
enum video_type get_bios_area_video_type(void)
{
    return (enum video_type) (detect_bios_area_hardware() & 0x30);
}

/// Copyright (c) 2020 KingUndeadCodes
/// Protected under MIT License which lays down the terms of use.

extern "C" void main(){

   const char *name = "Flying Chicken || Revision 1.0.0";
   const bool found = 1;

   if (found == true) {

      *((int*)0xb8001) = 0x0f;
      *((int*)0xb8003) = 0x0f;
      *((int*)0xb8005) = 0x0f;
      *((int*)0xb8007) = 0x0f;
      *((int*)0xb8009) = 0x0f;
      *((int*)0xb800b) = 0x0f;
      *((int*)0xb800d) = 0x0f;
      *((int*)0xb800f) = 0x02f;
      *((int*)0xb8011) = 0x02f;
      *((int*)0xb8013) = 0x22f;
      *((int*)0xb8015) = 0x02f;

      *((char*)0xb8000) = 'F';
      *((char*)0xb8002) = 'o';
      *((char*)0xb8004) = 'u';
      *((char*)0xb8006) = 'n';
      *((char*)0xb8008) = 'd';
      *((char*)0xb800a) = ':';
      *((char*)0xb800c) = ' ';
      *((char*)0xb800e) = 'T';
      *((char*)0xb8010) = 'r';
      *((char*)0xb8012) = 'u';
      *((char*)0xb8014) = 'e';

      return;

   } else {

      *((int*)0xb8001) = 0x0f;
      *((int*)0xb8003) = 0x0f;
      *((int*)0xb8005) = 0x0f;
      *((int*)0xb8007) = 0x0f;
      *((int*)0xb8009) = 0x0f;
      *((int*)0xb800b) = 0x0f;
      *((int*)0xb800d) = 0x0f;
      *((int*)0xb800f) = 0x04f;
      *((int*)0xb8011) = 0x04f;
      *((int*)0xb8013) = 0x04f;
      *((int*)0xb8015) = 0x04f;
      *((int*)0xb8017) = 0x04f;

      *((char*)0xb8000) = 'F';
      *((char*)0xb8002) = 'o';
      *((char*)0xb8004) = 'u';
      *((char*)0xb8006) = 'n';
      *((char*)0xb8008) = 'd';
      *((char*)0xb800a) = ':';
      *((char*)0xb800c) = ' ';
      *((char*)0xb800e) = 'F';
      *((char*)0xb8010) = 'a';
      *((char*)0xb8012) = 'l';
      *((char*)0xb8014) = 's';
      *((char*)0xb8016) = 'e';

      return;

   }
}
