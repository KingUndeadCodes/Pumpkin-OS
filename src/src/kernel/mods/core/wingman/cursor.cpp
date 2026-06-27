#include "./headers/cursor.h"

static int mouse_x = 0;
static int mouse_y = 0;
static int cursor_id = 2;

static inline int clamp_int(int v, int min, int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
};

int get_mouse_x(void) { return mouse_x; }
int get_mouse_y(void) { return mouse_y; }
int get_cursor_id(void) { return cursor_id; }

void set_cursor_id(int cursor) { cursor_id = cursor; }

void redraw_cursor_special(WindowManager *wm) {
    // const color_t* buffer = wm->screen->getBuffer();
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    // Draw new cursor
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            const int px = mouse_x + j;
            const int py = mouse_y + i;
            if (px < 0 || py < 0 || px >= screenWidth || py >= screenHeight) continue;
            const char c = cursorArray[cursor_id][i][j];
            if (c == 2) draw_pixel(px, py, rgb(255, 255, 255));
            else if (c == 1) draw_pixel(px, py, rgb(0, 0, 0));
        }
    }
};

void redraw_cursor(WindowManager *wm, int x, int y) {
    const color_t* buffer = wm->screen->getBuffer();
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    x = clamp_int(x, 0, screenWidth  - CURSOR_W);
    y = clamp_int(y, 0, screenHeight - CURSOR_H);
    // Erase old cursor
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            const int ox = mouse_x + j;
            const int oy = mouse_y + i;
            if (ox < 0 || oy < 0 || ox >= screenWidth || oy >= screenHeight) { continue; }
            draw_pixel(ox, oy, buffer[oy * screenWidth + ox]);
        }
    }
    // Draw new cursor
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            const int px = x + j;
            const int py = y + i;
            if (px < 0 || py < 0 || px >= screenWidth || py >= screenHeight) continue;
            const char c = cursorArray[cursor_id][i][j];
            if (c == 2) draw_pixel(px, py, rgb(255, 255, 255));
            else if (c == 1) draw_pixel(px, py, rgb(0, 0, 0));
        }
    }
    mouse_x = x;
    mouse_y = y;
};
