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

void update_mouse_position(int x, int y) {
    mouse_x = x;
    mouse_y = y;
};

// See DOCS.md ("mods/core/wingman/cursor.cpp / headers/cursor.h" section).
void draw_cursor_into_buffer(color_t* buffer, int bufferWidth, int bufferHeight) {
    if (buffer == NULL) return;
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    const int x = clamp_int(mouse_x, 0, bufferWidth - CURSOR_W);
    const int y = clamp_int(mouse_y, 0, bufferHeight - CURSOR_H);
    for (int i = 0; i < CURSOR_H; i++) {
        for (int j = 0; j < CURSOR_W; j++) {
            const int px = x + j;
            const int py = y + i;
            if (px < 0 || py < 0 || px >= bufferWidth || py >= bufferHeight) continue;
            const char c = cursorArray[cursor_id][i][j];
            if (c == 2) buffer[py * bufferWidth + px] = rgb(255, 255, 255);
            else if (c == 1) buffer[py * bufferWidth + px] = rgb(0, 0, 0);
        }
    }
};

// See DOCS.md ("mods/core/wingman/cursor.cpp / headers/cursor.h" section).
void redraw_cursor(WindowManager *wm, int x, int y) {
    const color_t* buffer = wm->screen->getBuffer();
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    x = clamp_int(x, 0, screenWidth  - CURSOR_W);
    y = clamp_int(y, 0, screenHeight - CURSOR_H);
    if (x == mouse_x && y == mouse_y) return;
    color_t* fb = (color_t*)0xE0000000;
    // Erase old cursor.
    for (int i = 0; i < CURSOR_H; i++) {
        const int oy = mouse_y + i;
        if (oy < 0 || oy >= screenHeight) continue;
        memcpy(&fb[oy * screenWidth + mouse_x], &buffer[oy * screenWidth + mouse_x], CURSOR_W * sizeof(color_t));
    }
    // Draw new cursor.
    color_t rowBuf[CURSOR_W];
    for (int i = 0; i < CURSOR_H; i++) {
        const int py = y + i;
        if (py < 0 || py >= screenHeight) continue;
        memcpy(rowBuf, &buffer[py * screenWidth + x], CURSOR_W * sizeof(color_t));
        for (int j = 0; j < CURSOR_W; j++) {
            const char c = cursorArray[cursor_id][i][j];
            if (c == 2) rowBuf[j] = rgb(255, 255, 255);
            else if (c == 1) rowBuf[j] = rgb(0, 0, 0);
        }
        memcpy(&fb[py * screenWidth + x], rowBuf, CURSOR_W * sizeof(color_t));
    }
    mouse_x = x;
    mouse_y = y;
};
