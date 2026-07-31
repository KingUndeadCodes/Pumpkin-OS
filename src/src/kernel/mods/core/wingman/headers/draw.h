#pragma once

#include "./types.h"
#include "./surface.h"

// Shared drawing primitives for anything rendering into a Surface.
// Prefixed `surface_` because mods/dev/vbe/vbe.h declares global draw_pixel/draw_char/
// draw_icon that write the raw framebuffer -- unprefixed names would silently overload them.
void surface_draw_pixel(Surface* surface, int x, int y, color_t color);
void surface_draw_char(Surface* surface, int x, int y, char c, color_t color, int scale);
void surface_draw_string(Surface* surface, int x, int y, const char* str, color_t color, int scale);
void surface_draw_icon(Surface* surface, int x, int y, int iconId, int scale);
