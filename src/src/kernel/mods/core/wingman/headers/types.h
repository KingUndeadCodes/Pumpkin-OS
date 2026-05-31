#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef uint32_t color_t;
typedef uint32_t dimension_t;
typedef int32_t window_ref_t;

#define rgba(r, g, b, a) (color_t)((a << 24) | (r << 16) | (g << 8) | b)
#define rgb(r, g, b) (color_t)((0xFF << 24) | (r << 16) | (g << 8) | b)

#define RGB(r, g, b) rgb(r, g, b)
#define RGBA(r, g, b, a) rgba(r, g, b, a)

#define COLOR_A(c) ((uint8_t)(((c) >> 24) & 0xFF))
#define COLOR_R(c) ((uint8_t)(((c) >> 16) & 0xFF))
#define COLOR_G(c) ((uint8_t)(((c) >> 8) & 0xFF))
#define COLOR_B(c) ((uint8_t)((c) & 0xFF))

struct Constraints {
    uint32_t maxWindowCount;
    uint8_t colorDepth;
    dimension_t screenXSizePx;
    dimension_t screenYSizePx;
};

struct Environment {
    // char* font;
    color_t foregroundColor;
    color_t backgroundColor;
};