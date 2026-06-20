#include "../../std/include/graphics/cursor.h"
#include "./suite/explorer/explorer.h"
#include "./suite/message/message.h"
#include "../../dev/mouse/mouse.h"
#include "./headers/wingman.h"
#include "../../dev/vbe/vbe.h"

static size_t bufferSize = 0;
static WindowManager* wm = nullptr;
static FileManager* fileManager = nullptr;
static int mouse_x = 0;
static int mouse_y = 0;
// static MessageBox* messageBox = nullptr;

static inline int clamp_int(int v, int min, int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
};

inline void redraw_screen(void) {
    color_t* buffer = wm->screen->getBuffer();
    memcpy((void*)0xE0000000, buffer, bufferSize);
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    if (wm != nullptr) {
        wm->keyboard_handler(key, shift, meta, scancode);
        wm->composite();
        redraw_screen();
        return;
    };
};

void mouseFunctionWindowManager(int x, int y, int dx, int dy, unsigned char buttons) {
    (void)dx;
    (void)dy;
    isInsideFocusedWindow:
        const Window* foucsedWindow = wm->focusedWindow;
        const int rectX = foucsedWindow->offsetX;
        const int rectY = foucsedWindow->offsetY;
        const int width = foucsedWindow->width;
        const int height = foucsedWindow->height;
        if (mouse_x >= rectX && mouse_x <= (rectX + width)) {
            // Check vertical boundaries (assuming screen coordinates where Y goes down)
            if (mouse_y >= rectY && mouse_y <= (rectY + height)) {
                const int _x = mouse_x - rectX;
                const int _y = mouse_y - rectY;
                foucsedWindow->handleMouse(_x, _y, dx, dy, buttons);
                wm->composite();
                redraw_screen();
            }
        }
    const color_t* buffer = wm->screen->getBuffer();
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    constexpr int CURSOR_ID = 2;
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
            const char c = cursorArray[CURSOR_ID][i][j];
            if (c == 2) draw_pixel(px, py, rgb(255, 255, 255));
            else if (c == 1) draw_pixel(px, py, rgb(0, 0, 0));
        }
    }
    mouse_x = x;
    mouse_y = y;
    return false;
}

void initalizeWindowSystem(void) {
    wm = new WindowManager();
    fileManager = new FileManager();
    // -
    /*
    messageBox = new MessageBox(DialogBoxError, "chorus: not initialized; call initalize() first.");
    Window* windowTest = new Window(1024, 768, 0, 0, "Test Window");
    windowTest->surface->clear(rgb(255, 0, 0));
    wm->add(messageBox->window);
    wm->add(windowTest);
    */
    // -
    wm->add(fileManager->window);
    kb_add_event(keyboardFunctionWindowManager);
    mouse_add_event(mouseFunctionWindowManager);
    wm->composite();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t);
    redraw_screen();
    return;
}