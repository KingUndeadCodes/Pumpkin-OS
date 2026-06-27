#pragma once

#include "../../../std/include/graphics/cursor.h"
#include "../../../dev/mouse/mouse.h"
#include "../../../dev/vbe/vbe.h"
#include "../../../dev/port.cpp"
#include "./wingman.h"
#include "./manager.h"

int get_mouse_x(void);
int get_mouse_y(void);
void set_cursor_id(int cursor);

void redraw_cursor_special(WindowManager *wm);
void redraw_cursor(WindowManager *wm, int x, int y);