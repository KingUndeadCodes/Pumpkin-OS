#include "../../dev/chorus/chorus.h"
#include "../../dev/syscall/syscall.h"
#include "../../dev/pit/pit.h"
#include "../../dev/tasking/tasking.h"
#include "../../dev/port.cpp"
#include "./apps/explorer/explorer.h"
#include "./apps/message/message.h"
#include "./apps/widgetdemo/widgetdemo.h"
#include "./headers/wingman.h"

static WindowManager* wm = nullptr;
static size_t bufferSize = 0;

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

static void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    // Don't also deliver keystrokes to Wingman while a task owns stdin -- see docs/DOCS.md ("stdin_is_reading() / stdin exclusivity").
    if (stdin_is_reading()) return;
    if (wm == nullptr) return;
    Rect dirty;
    if (wm->handleKeyboardEvent(key, shift, meta, scancode, &dirty)) {
        redraw_screen_rect(dirty);
    }
};

static void mouseFunctionWindowManager(int x, int y, int dx, int dy, unsigned char buttons) {
    if (wm == nullptr) return;
    const int mouse_x = mouse_get_x();
    const int mouse_y = mouse_get_y();
    // Reset each event; a delegate sets it back if still hovering something clickable.
    set_cursor_id(0);
    Rect dirty;
    if (wm->handleMouseEvent(mouse_x, mouse_y, dx, dy, buttons, &dirty)) {
        // handleMouseEvent() may have just blocked for a while (e.g. the MP3
        // playback branch in explorer.cpp), during which the real mouse
        // kept moving -- re-read its current position rather than reusing
        // x/y, which are still whatever they were when this event started.
        update_mouse_position(mouse_get_x(), mouse_get_y());
        redraw_screen_rect(dirty);
    } else {
        // See docs/DOCS.md ("mods/core/wingman/wingman.cpp -- cursor
        // duplication during drag") for why this reads the driver's live
        // position instead of the possibly-stale x/y parameters.
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
    new FileManager(wm);
    // chorus_initalize() sets up the AC97 DMA ring and isn't called automatically at boot -- shown so the user can trigger it manually.
    MessageBox* messageBox = new MessageBox(wm, DialogBoxInformational, "chorus: not initialized; call initalize() first.", 5);
    messageBox->addButton("Initialize", 0xFF605a59, [](void* userdata) {
        (void)userdata;
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