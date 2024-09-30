#ifndef __GRAPHICS_H
#define __GRAPHICS_H
#include <tasking.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "graphics/font.h"
#include "graphics/icons.h"
#include "graphics/image_struct.h"
#include "graphics/image_background.h"

namespace Screen {
    void Plot(int x, int y, int c = 0xF);
    void Fill(int c = 0x3);
    void DrawChar(char c, int x = NULL, int y = NULL);
    void DrawIcon(int e = 0, int x = 25, int y = 25);
    void DrawImage(void);
    void FromRangetoRange(int fromX, int toX, int fromY, int toY, int color);
}

namespace ViewTest {
    void HelloWorld(void);
    void SplashScreen(void);
    void CustomShapeTest(void);
    void CalendarTest(void);
};

enum Alignment: int {
    topLeading,
    top,
    topTrailing,
    leading,
    center,
    trailing,
    bottomLeading,
    bottom,
    bottomTrailing
};

enum Axis: int {
    horizontal,
    vertical
};

void setup_kb();

#endif