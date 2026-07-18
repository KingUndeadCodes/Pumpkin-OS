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

// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
struct Rect {
    int x, y, w, h;
};

// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
static inline bool rect_empty(Rect r) {
    return r.w <= 0 || r.h <= 0;
}

static inline Rect rect_intersect(Rect a, Rect b) {
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    Rect r = { x0, y0, x1 - x0, y1 - y0 };
    return r;
}

// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
static inline Rect rect_union(Rect a, Rect b) {
    int x0 = a.x < b.x ? a.x : b.x;
    int y0 = a.y < b.y ? a.y : b.y;
    int x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    Rect r = { x0, y0, x1 - x0, y1 - y0 };
    return r;
}