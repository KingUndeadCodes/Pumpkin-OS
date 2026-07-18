#include "../../dev/chorus/chorus.h"
#include "../../dev/syscall/syscall.h"
#include "../../dev/pit/pit.h"
#include "./suite/explorer/explorer.h"
#include "./suite/message/message.h"
#include "./suite/widgetdemo/widgetdemo.h"
#include "./headers/wingman.h"

static FileManager* fileManager = nullptr;
static WindowManager* wm = nullptr;
static size_t bufferSize = 0;
// See docs/DOCS.md ("mods/core/wingman/wingman.cpp" section) for outputBuffer/lastButtons.
static color_t* outputBuffer = nullptr;
static unsigned char lastButtons = 0;
// See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window dragging") section.
#define WINGMAN_DRAG_HANDLE_HEIGHT 30
#define WINGMAN_DRAG_REDRAW_INTERVAL_MS 33
static window_ref_t draggingWindow = WINGMAN_INVALID_WINDOW;
static int dragOffsetX = 0;
static int dragOffsetY = 0;
static uint64_t lastDragRedrawMs = 0;
// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing") for why this exists.
static Rect dragLastRect = { 0, 0, 0, 0 };

inline void redraw_screen(void) {
    color_t* buffer = wm->screen->getBuffer();
    memcpy(outputBuffer, buffer, bufferSize);
    draw_cursor_into_buffer(outputBuffer, wm->screen->getWidth(), wm->screen->getHeight());
    memcpy((void*)0xE0000000, outputBuffer, bufferSize);
};

// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
inline void redraw_screen_rect(Rect rect) {
    if (wm == nullptr || wm->screen == nullptr) return;
    const int screenWidth = wm->screen->getWidth();
    const int screenHeight = wm->screen->getHeight();
    Rect screenRect = { 0, 0, screenWidth, screenHeight };
    rect = rect_intersect(rect, screenRect);
    if (rect_empty(rect)) return;
    const color_t* buffer = wm->screen->getBuffer();
    color_t* fb = (color_t*)0xE0000000;
    for (int y = rect.y; y < rect.y + rect.h; y++) {
        int rowOffset = y * screenWidth + rect.x;
        memcpy(&fb[rowOffset], &buffer[rowOffset], (size_t)rect.w * sizeof(color_t));
    }
    constexpr int CURSOR_W = 17;
    constexpr int CURSOR_H = 24;
    Rect cursorRect = { get_mouse_x(), get_mouse_y(), CURSOR_W, CURSOR_H };
    if (!rect_empty(rect_intersect(cursorRect, rect))) {
        draw_cursor_into_buffer(fb, screenWidth, screenHeight);
    }
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — stdin exclusivity").
    if (stdin_is_reading()) return;
    if (wm != nullptr) {
        if (wm->keyboard_handler(key, shift, meta, scancode)) {
            // See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
            Window* focused = wm->focusedWindow;
            Rect dirty = { focused->offsetX, focused->offsetY, focused->width, focused->height };
            wm->composite(dirty);
            redraw_screen_rect(dirty);
        }
    };
};

void mouseFunctionWindowManager(int x, int y, int dx, int dy, unsigned char buttons) {
    const unsigned char pressedEdge = buttons & (unsigned char)~lastButtons;
    lastButtons = buttons;
    const int mouse_x = mouse_get_x();
    const int mouse_y = mouse_get_y();
    bool needsRedraw = false;
    // See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
    Rect dirtyRect = { 0, 0, 0, 0 };
    bool hasDirty = false;
    auto markDirty = [&](Rect r) {
        dirtyRect = hasDirty ? rect_union(dirtyRect, r) : r;
        hasDirty = true;
        needsRedraw = true;
    };
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp" section) for refocus-on-click.
    if (pressedEdge & 1) {
        window_ref_t hitRef = wm->windowAt(mouse_x, mouse_y);
        Window* hitWindow = wm->get(hitRef);
        if (hitWindow != nullptr && hitWindow != wm->focusedWindow) {
            wm->focus(hitRef);
            markDirty({ hitWindow->offsetX, hitWindow->offsetY, hitWindow->width, hitWindow->height });
        }
        // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window dragging") section.
        if (hitWindow != nullptr && mouse_y - hitWindow->offsetY < WINGMAN_DRAG_HANDLE_HEIGHT) {
            draggingWindow = hitRef;
            dragOffsetX = mouse_x - hitWindow->offsetX;
            dragOffsetY = mouse_y - hitWindow->offsetY;
            dragLastRect = { hitWindow->offsetX, hitWindow->offsetY, hitWindow->width, hitWindow->height };
        }
    }
    const bool wasDragging = (draggingWindow != WINGMAN_INVALID_WINDOW);
    // Captured before the clear below so the drag-end block can still
    // reach the window that was just released.
    Window* draggedWindowBeforeRelease = wasDragging ? wm->get(draggingWindow) : nullptr;
    if (!(buttons & 1)) draggingWindow = WINGMAN_INVALID_WINDOW;
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window dragging") for
    // why a drag ending forces a redraw regardless of the throttle below.
    if (wasDragging && draggingWindow == WINGMAN_INVALID_WINDOW && draggedWindowBeforeRelease != nullptr) {
        Window* w = draggedWindowBeforeRelease;
        Rect currentRect = { w->offsetX, w->offsetY, w->width, w->height };
        markDirty(rect_union(dragLastRect, currentRect));
    }

    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp" section) for the cursor-id reset.
    set_cursor_id(0);
    if (draggingWindow != WINGMAN_INVALID_WINDOW) {
        Window* dragged = wm->get(draggingWindow);
        if (dragged != nullptr) {
            int newX = mouse_x - dragOffsetX;
            int newY = mouse_y - dragOffsetY;
            // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window
            // dragging") for why this clamps to keep the whole window rect
            // on screen, not just the point under the cursor.
            int maxX = wm->screen->getWidth() - dragged->width;
            int maxY = wm->screen->getHeight() - dragged->height;
            if (maxX < 0) maxX = 0;
            if (maxY < 0) maxY = 0;
            if (newX < 0) newX = 0;
            if (newX > maxX) newX = maxX;
            if (newY < 0) newY = 0;
            if (newY > maxY) newY = maxY;
            if (newX != dragged->offsetX || newY != dragged->offsetY) {
                dragged->offsetX = newX;
                dragged->offsetY = newY;
                // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window
                // dragging") for why the redraw itself is throttled here.
                uint64_t now = timer_ticks;
                if (now - lastDragRedrawMs >= WINGMAN_DRAG_REDRAW_INTERVAL_MS) {
                    lastDragRedrawMs = now;
                    Rect currentRect = { newX, newY, dragged->width, dragged->height };
                    markDirty(rect_union(dragLastRect, currentRect));
                    dragLastRect = currentRect;
                }
            }
        }
    } else {
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
                        markDirty({ rectX, rectY, width, height });
                    }
                }
            }
        }
    }
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp" section) for this split.
    if (needsRedraw) {
        wm->composite(dirtyRect);
        // handleMouse() may have just blocked for a while (e.g. the MP3
        // playback branch in explorer.cpp), during which the real mouse
        // kept moving -- re-read its current position rather than reusing
        // x/y, which are still whatever they were when this event started.
        update_mouse_position(mouse_get_x(), mouse_get_y());
        redraw_screen_rect(dirtyRect);
    } else {
        redraw_cursor(wm, x, y);
    }
    return;
}

void initalizeWindowSystem(void) {
    wm = new WindowManager();
    fileManager = new FileManager();
    wm->add(fileManager->window);
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp" section) for MessageBox wiring here.
    MessageBox* messageBox = new MessageBox(wm, DialogBoxInformational, "chorus: not initialized; call initalize() first.", 5);
    messageBox->addButton("Initialize", 0xFF605a59, [](void) { 
        serial_write_string("Initializing AC97 Audio Codec...\n");
        chorus_initalize();
    });
    messageBox->addButton("Ignore", rgb(255, 0, 0));
    new WidgetDemo(wm);
    kb_add_event(keyboardFunctionWindowManager);
    mouse_add_event(mouseFunctionWindowManager);
    set_cursor_id(0);
    wm->composite();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t);
    outputBuffer = (color_t*)malloc(bufferSize);
    redraw_screen();
    return;
}