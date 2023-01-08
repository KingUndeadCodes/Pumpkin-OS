#ifndef __GRAPHICS_H
#define __GRAPHICS_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

namespace Screen {
    void Fill(int c = 0x3);
    void DrawChar(char c, int x = NULL, int y = NULL);
    void DrawIcon(int e = 0, int x = 25, int y = 25);
}

#endif