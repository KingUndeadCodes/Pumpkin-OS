#ifndef __VGA_TABLE_H__
#include <stdint.h>

unsigned char vgaPalette[16][3] = {
    // Row 1
    {0, 0, 0},       // 0: Black
    {0, 0, 170},     // 1: Blue
    {0, 170, 0},     // 2: Green
    {0, 170, 170},   // 3: Cyan
    {170, 0, 0},     // 4: Red
    {170, 0, 170},   // 5: Magenta
    {170, 85, 0},    // 6: Brown
    {170, 170, 170}, // 7: Light Gray
    {85, 85, 85},    // 8: Dark Gray
    {85, 85, 255},   // 9: Light Blue
    {85, 255, 85},   // 10: Light Green
    {85, 255, 255},  // 11: Light Cyan
    {255, 85, 85},   // 12: Light Red
    {255, 85, 255},  // 13: Light Magenta
    {255, 255, 85},  // 14: Yellow
    {255, 255, 255}, // 15: White
};

#endif