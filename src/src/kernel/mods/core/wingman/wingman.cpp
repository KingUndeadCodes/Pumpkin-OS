#include "../../dev/chorus/chorus.h"
#include "./suite/explorer/explorer.h"
#include "./suite/message/message.h"
#include "./headers/wingman.h"

static FileManager* fileManager = nullptr;
static WindowManager* wm = nullptr;
static size_t bufferSize = 0;
// See DOCS.md ("mods/core/wingman/wingman.cpp" section) for outputBuffer/lastButtons.
static color_t* outputBuffer = nullptr;
static unsigned char lastButtons = 0;

inline void redraw_screen(void) {
    color_t* buffer = wm->screen->getBuffer();
    memcpy(outputBuffer, buffer, bufferSize);
    draw_cursor_into_buffer(outputBuffer, wm->screen->getWidth(), wm->screen->getHeight());
    memcpy((void*)0xE0000000, outputBuffer, bufferSize);
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    if (wm != nullptr) {
        if (wm->keyboard_handler(key, shift, meta, scancode)) {
            wm->composite();
            redraw_screen();
        }
    };
};

void mouseFunctionWindowManager(int x, int y, int dx, int dy, unsigned char buttons) {
    const unsigned char pressedEdge = buttons & (unsigned char)~lastButtons;
    lastButtons = buttons;
    const int mouse_x = mouse_get_x();
    const int mouse_y = mouse_get_y();
    bool needsRedraw = false;
    // See DOCS.md ("mods/core/wingman/wingman.cpp" section) for refocus-on-click.
    if (pressedEdge & 1) {
        window_ref_t hitRef = wm->windowAt(mouse_x, mouse_y);
        if (hitRef != WINGMAN_INVALID_WINDOW && wm->get(hitRef) != wm->focusedWindow) {
            wm->focus(hitRef);
            needsRedraw = true;
        }
    }
    // See DOCS.md ("mods/core/wingman/wingman.cpp" section) for the cursor-id reset.
    set_cursor_id(0);
    const Window* foucsedWindow = wm->focusedWindow;
    if (foucsedWindow != nullptr) {
        const int rectX = foucsedWindow->offsetX;
        const int rectY = foucsedWindow->offsetY;
        const int width = foucsedWindow->width;
        const int height = foucsedWindow->height;
        if (mouse_x >= rectX && mouse_x <= (rectX + width)) {
            // Check vertical boundaries (assuming screen coordinates where Y goes down)
            if (mouse_y >= rectY && mouse_y <= (rectY + height)) {
                const int _x = mouse_x - rectX;
                const int _y = mouse_y - rectY;
                if (foucsedWindow->handleMouse(_x, _y, dx, dy, buttons, pressedEdge)) {
                    needsRedraw = true;
                }
            }
        }
    }
    // See DOCS.md ("mods/core/wingman/wingman.cpp" section) for this split.
    if (needsRedraw) {
        wm->composite();
        // handleMouse() may have just blocked for a while (e.g. elf_run()
        // running a program to completion), during which the real mouse
        // kept moving -- re-read its current position rather than reusing
        // x/y, which are still whatever they were when this event started.
        update_mouse_position(mouse_get_x(), mouse_get_y());
        redraw_screen();
    } else {
        redraw_cursor(wm, x, y);
    }
    return;
}

void initalizeWindowSystem(void) {
    wm = new WindowManager();
    fileManager = new FileManager();
    wm->add(fileManager->window);
    // See DOCS.md ("mods/core/wingman/wingman.cpp" section) for MessageBox wiring here.
    MessageBox* messageBox = new MessageBox(wm, DialogBoxInformational, "chorus: not initialized; call initalize() first.", 5);
    messageBox->addButton("Initialize", 0xFF605a59, [](void) { 
        serial_write_string("Initializing AC97 Audio Codec...\n");
        chorus_initalize();
    });
    messageBox->addButton("Ignore", rgb(255, 0, 0));
    kb_add_event(keyboardFunctionWindowManager);
    mouse_add_event(mouseFunctionWindowManager);
    set_cursor_id(0);
    wm->composite();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t);
    outputBuffer = (color_t*)malloc(bufferSize);
    redraw_screen();
    return;
}