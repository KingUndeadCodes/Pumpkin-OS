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

// Blits the sprite straight into the presented frame buffer -- called once per present, not per move.
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

// See docs/DOCS.md ("mods/core/wingman/cursor.cpp -- hardware double
// buffering") for why this only updates position and delegates to
// redraw_screen() instead of patching the framebuffer directly.
void redraw_cursor(WindowManager *wm, int x, int y) {
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    x = clamp_int(x, 0, screenWidth  - CURSOR_W);
    y = clamp_int(y, 0, screenHeight - CURSOR_H);
    if (x == mouse_x && y == mouse_y) return;
    mouse_x = x;
    mouse_y = y;
    redraw_screen();
};
