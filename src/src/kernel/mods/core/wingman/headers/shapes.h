#pragma once

#include "./types.h"
#include "./surface.h"
#include "../../fontman/fontman.h"
#include "../../../std/include/math.h"

// See docs/DOCS.md ("mods/core/wingman/headers/shapes.h").

static inline uint8_t rounded_corner_coverage(double dx, double dy, int radius) {
    double distance = sqrt(dx * dx + dy * dy);
    double edge = (double)radius - distance + 0.5; // 1px-wide AA band at the arc boundary
    if (edge <= 0.0) return 0;
    if (edge >= 1.0) return 255;
    return (uint8_t)(edge * 255.0);
}

// See docs/DOCS.md ("mods/core/wingman/headers/shapes.h -- per-corner rounding") for why this takes 4 corner flags instead of always rounding all 4.
static inline void draw_rounded_rect_fill_corners(Surface* surface, int x, int y, int w, int h, int radius, color_t color, bool topLeft, bool topRight, bool bottomLeft, bool bottomRight) {
    if (w <= 0 || h <= 0) return;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    for (int row = 0; row < h; row++) {
        bool nearTop = row < radius;
        bool nearBottom = row >= h - radius;
        double cy = nearTop ? (double)radius : (double)(h - radius);
        for (int col = 0; col < w; col++) {
            bool nearLeft = col < radius;
            bool nearRight = col >= w - radius;
            uint8_t coverage = 255;
            bool roundedCorner = (nearTop && nearLeft && topLeft) || (nearTop && nearRight && topRight) ||
                                  (nearBottom && nearLeft && bottomLeft) || (nearBottom && nearRight && bottomRight);
            if (radius > 0 && roundedCorner) {
                double cx = nearLeft ? (double)radius : (double)(w - radius);
                double dx = (col + 0.5) - cx;
                double dy = (row + 0.5) - cy;
                coverage = rounded_corner_coverage(dx, dy, radius);
            }
            if (coverage == 0) continue;
            int px = x + col, py = y + row;
            if (coverage == 255) { surface->putPixelUnsafe(px, py, color); continue; }
            color_t under = surface->getPixel(px, py);
            surface->putPixelUnsafe(px, py, ttf_blend_over(under, color, coverage));
        }
    }
}

// See docs/DOCS.md ("mods/core/wingman/headers/shapes.h").
static inline void draw_rounded_rect_fill(Surface* surface, int x, int y, int w, int h, int radius, color_t color) {
    draw_rounded_rect_fill_corners(surface, x, y, w, h, radius, color, true, true, true, true);
}
