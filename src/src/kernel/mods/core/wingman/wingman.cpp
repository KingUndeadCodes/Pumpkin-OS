#include "../../dev/chorus/chorus.h"
#include "../../dev/syscall/syscall.h"
#include "../../dev/pit/pit.h"
#include "../../dev/tasking/tasking.h"
#include "../../dev/port.cpp"
#include "./suite/explorer/explorer.h"
#include "./suite/message/message.h"
#include "./suite/widgetdemo/widgetdemo.h"
#include "./headers/wingman.h"

static FileManager* fileManager = nullptr;
static WindowManager* wm = nullptr;
static size_t bufferSize = 0;
// Previous packet's button state, so a press-edge (0->1) can be computed once globally instead of every delegate re-firing on each held-down packet.
static unsigned char lastButtons = 0;
// Height of the draggable band across a window's top edge. See docs/DOCS.md ("window dragging").
#define WINGMAN_DRAG_HANDLE_HEIGHT 30
#define WINGMAN_DRAG_REDRAW_INTERVAL_MS 33
static window_ref_t draggingWindow = WINGMAN_INVALID_WINDOW;
static int dragOffsetX = 0;
static int dragOffsetY = 0;
static uint64_t lastDragRedrawMs = 0;
// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing") for why this exists.
static Rect dragLastRect = { 0, 0, 0, 0 };

// Ring-buffer capacity for events queued to the input worker task. See docs/DOCS.md ("input queue / worker task").
#define WINGMAN_INPUT_QUEUE_SIZE 32

enum WingmanInputEventType { WINGMAN_EVENT_MOUSE, WINGMAN_EVENT_KEY };

struct WingmanInputEvent {
    WingmanInputEventType type;
    int x, y, dx, dy;
    unsigned char buttons;
    char key;
    bool shift, meta;
    unsigned char scancode;
};

static WingmanInputEvent g_inputQueue[WINGMAN_INPUT_QUEUE_SIZE];
static volatile int g_inputQueueHead = 0;
static volatile int g_inputQueueTail = 0;
static task_t* g_inputWorkerTask = nullptr;

// See docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- hardware double buffering").
// Always a full-frame present now: real page-flipping means the back
// buffer's prior contents are two presents stale, not one, so a partial
// patch would leave the rest of the region wrong the instant it's shown.
// wm->screen is always a complete, current frame regardless of how little
// composite() actually recomputed, so copying all of it here is correct.
void redraw_screen(void) {
    if (wm == nullptr || wm->screen == nullptr) return;
    const color_t* buffer = wm->screen->getBuffer();
    color_t* back = (color_t*)vbe_get_back_buffer();
    memcpy(back, buffer, bufferSize);
    draw_cursor_into_buffer(back, wm->screen->getWidth(), wm->screen->getHeight());
    vbe_flip();
};

// See docs/DOCS.md (same section) -- dirty-rect compositing still narrows
// the blend work in WindowManager::composite(Rect); the present step
// itself can no longer be partial once real page-flipping is in play.
inline void redraw_screen_rect(Rect rect) {
    (void)rect;
    redraw_screen();
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    // Don't also deliver keystrokes to Wingman while a task owns stdin -- see docs/DOCS.md ("stdin_is_reading() / stdin exclusivity").
    if (stdin_is_reading()) return;
    if (wm != nullptr) {
        if (wm->keyboard_handler(key, shift, meta, scancode)) {
            // Only the focused window's rect needs recompositing for a keyboard-driven change.
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
    // Accumulates the minimal region that actually changed this event, unioned in as work is found below.
    Rect dirtyRect = { 0, 0, 0, 0 };
    bool hasDirty = false;
    auto markDirty = [&](Rect r) {
        dirtyRect = hasDirty ? rect_union(dirtyRect, r) : r;
        hasDirty = true;
        needsRedraw = true;
    };
    // Clicking a background window raises it to focus.
    if (pressedEdge & 1) {
        window_ref_t hitRef = wm->windowAt(mouse_x, mouse_y);
        Window* hitWindow = wm->get(hitRef);
        if (hitWindow != nullptr && hitWindow != wm->focusedWindow) {
            wm->focus(hitRef);
            markDirty({ hitWindow->offsetX, hitWindow->offsetY, hitWindow->width, hitWindow->height });
        }
        // The close/minimize/maximize trio must not also start a window drag.
        bool inCloseButtonZone = hitWindow != nullptr && (mouse_x - hitWindow->offsetX) < hitWindow->titleBar.closeButtonZoneWidth();
        if (hitWindow != nullptr && !inCloseButtonZone && mouse_y - hitWindow->offsetY < WINGMAN_DRAG_HANDLE_HEIGHT) {
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

    // Reset each event; a delegate sets it back if still hovering something clickable.
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
        // handleMouse() itself intercepts clicks on the title bar's close button before this reaches the delegate.
        Window* foucsedWindow = wm->focusedWindow;
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
    // Only recomposite when something actually changed; otherwise just keep the cursor in sync.
    if (needsRedraw) {
        wm->composite(dirtyRect);
        // handleMouse() may have just blocked for a while (e.g. the MP3
        // playback branch in explorer.cpp), during which the real mouse
        // kept moving -- re-read its current position rather than reusing
        // x/y, which are still whatever they were when this event started.
        update_mouse_position(mouse_get_x(), mouse_get_y());
        redraw_screen_rect(dirtyRect);
    } else {
        // See docs/DOCS.md ("mods/core/wingman/wingman.cpp -- cursor
        // duplication during drag") for why this reads the driver's live
        // position instead of the possibly-stale x/y parameters, same as
        // the needsRedraw branch above.
        redraw_cursor(wm, mouse_get_x(), mouse_get_y());
    }
    return;
}

// Producer side of the queue drained by the input worker task.
void queueMouseEventForWingman(int x, int y, int dx, int dy, unsigned char buttons) {
    unsigned long flags = enter_critical();
    int next = (g_inputQueueHead + 1) % WINGMAN_INPUT_QUEUE_SIZE;
    if (next != g_inputQueueTail) {
        WingmanInputEvent& e = g_inputQueue[g_inputQueueHead];
        e.type = WINGMAN_EVENT_MOUSE;
        e.x = x; e.y = y; e.dx = dx; e.dy = dy; e.buttons = buttons;
        g_inputQueueHead = next;
    }
    exit_critical(flags);
    if (g_inputWorkerTask) task_wake(g_inputWorkerTask);
}

void queueKeyEventForWingman(char key, bool shift, bool meta, unsigned char scancode) {
    unsigned long flags = enter_critical();
    int next = (g_inputQueueHead + 1) % WINGMAN_INPUT_QUEUE_SIZE;
    if (next != g_inputQueueTail) {
        WingmanInputEvent& e = g_inputQueue[g_inputQueueHead];
        e.type = WINGMAN_EVENT_KEY;
        e.key = key; e.shift = shift; e.meta = meta; e.scancode = scancode;
        g_inputQueueHead = next;
    }
    exit_critical(flags);
    if (g_inputWorkerTask) task_wake(g_inputWorkerTask);
}

static void wingman_input_worker_fn(void* arg) {
    while (true) {
        while (g_inputQueueTail == g_inputQueueHead) task_block();
        unsigned long flags = enter_critical();
        WingmanInputEvent e = g_inputQueue[g_inputQueueTail];
        g_inputQueueTail = (g_inputQueueTail + 1) % WINGMAN_INPUT_QUEUE_SIZE;
        exit_critical(flags);
        if (e.type == WINGMAN_EVENT_MOUSE) mouseFunctionWindowManager(e.x, e.y, e.dx, e.dy, e.buttons);
        else keyboardFunctionWindowManager(e.key, e.shift, e.meta, e.scancode);
    }
}

void wingman_spawn_input_worker(void* stack_mem) {
    sched_lock();
    g_inputWorkerTask = task_create(wingman_input_worker_fn, nullptr, stack_mem);
    sched_unlock();
}

void initalizeWindowSystem(void) {
    wm = new WindowManager();
    fileManager = new FileManager(wm);
    // chorus_initalize() sets up the AC97 DMA ring and isn't called automatically at boot -- shown so the user can trigger it manually.
    MessageBox* messageBox = new MessageBox(wm, DialogBoxInformational, "chorus: not initialized; call initalize() first.", 5);
    messageBox->addButton("Initialize", 0xFF605a59, [](void) {
        serial_write_string("Initializing AC97 Audio Codec...\n");
        chorus_initalize();
    });
    messageBox->addButton("Ignore", rgb(255, 0, 0));
    new WidgetDemo(wm);
    kb_add_event(queueKeyEventForWingman);
    mouse_add_event(queueMouseEventForWingman);
    set_cursor_id(0);
    wm->composite();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t);
    redraw_screen();
    return;
}