#pragma once

#include "../../../std/include/graphics/cursor.h"
#include "../../../dev/mouse/mouse.h"
#include "../../../dev/vbe/vbe.h"
#include "../../../dev/port.cpp"
#include "./wingman.h"
#include "./manager.h"

int get_mouse_x(void);
int get_mouse_y(void);
int get_cursor_id(void);
void set_cursor_id(int cursor);

// See docs/DOCS.md ("mods/core/wingman/cursor.cpp / headers/cursor.h" section).
void update_mouse_position(int x, int y);
void draw_cursor_into_buffer(color_t* buffer, int bufferWidth, int bufferHeight);
void redraw_cursor(WindowManager *wm, int x, int y);