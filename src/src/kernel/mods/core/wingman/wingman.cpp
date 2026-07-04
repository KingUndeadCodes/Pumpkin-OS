#include "./suite/explorer/explorer.h"
#include "./suite/message/message.h"
#include "./headers/wingman.h"

static FileManager* fileManager = nullptr;
static WindowManager* wm = nullptr;
static bool mouseEnabled = false;
static size_t bufferSize = 0;
// PS/2 delivers a packet at a fairly high rate, not just on state changes,
// so a single physical click can arrive as several packets with buttons=1.
// Tracking the previous packet's button state here lets us compute a
// press-edge (0->1 this packet) once, globally, before any window/delegate
// ever sees it -- instead of every MouseDelegate treating "buttons == 1"
// as its own fresh click and re-firing on every packet held down.
static unsigned char lastButtons = 0;

inline void redraw_screen(void) {
    color_t* buffer = wm->screen->getBuffer();
    memcpy((void*)0xE0000000, buffer, bufferSize);
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    if (wm != nullptr) {
        if (wm->keyboard_handler(key, shift, meta, scancode)) {
            wm->composite();
            redraw_screen();
            if (mouseEnabled) redraw_cursor_special(wm);
        }
    };
};

void mouseFunctionWindowManager(int x, int y, int dx, int dy, unsigned char buttons) {
    (void)dx;
    (void)dy;
    mouseEnabled = true;
    const unsigned char pressedEdge = buttons & (unsigned char)~lastButtons;
    lastButtons = buttons;
    isInsideFocusedWindow:
        const Window* foucsedWindow = wm->focusedWindow;
        const int rectX = foucsedWindow->offsetX;
        const int rectY = foucsedWindow->offsetY;
        const int width = foucsedWindow->width;
        const int height = foucsedWindow->height;
        const int mouse_x = mouse_get_x();
        const int mouse_y = mouse_get_y();
        if (mouse_x >= rectX && mouse_x <= (rectX + width)) {
            // Check vertical boundaries (assuming screen coordinates where Y goes down)
            if (mouse_y >= rectY && mouse_y <= (rectY + height)) {
                const int _x = mouse_x - rectX;
                const int _y = mouse_y - rectY;
                /*
                uint64_t _time = rdtsc();
                printf_serial(false, NONE, "%d\n", _time - time);
                time = rdtsc();
                */
                if (foucsedWindow->handleMouse(_x, _y, dx, dy, buttons, pressedEdge)) {
                    wm->composite();
                    redraw_screen();
                }
            }
        }
    redraw_cursor(wm, x, y);
    return;
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
    MessageBox* messageBox = new MessageBox(DialogBoxWarning, "chorus: not initialized; call initalize() first.");
    // -
    wm->add(fileManager->window);
    wm->add(messageBox->window);
    kb_add_event(keyboardFunctionWindowManager);
    mouse_add_event(mouseFunctionWindowManager);
    set_cursor_id(0);
    wm->composite();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t);
    redraw_screen();
    return;
}