#include "./headers/manager.h"
#include "./headers/types.h"

#include "../../dev/serial/serial.h"
#include "../../dev/pit/pit.h"

// Height of the draggable band across a window's top edge. See docs/DOCS.md ("window dragging").
#define WINGMAN_DRAG_HANDLE_HEIGHT 30
#define WINGMAN_DRAG_REDRAW_INTERVAL_MS 33

color_t blend(color_t src, color_t dst) {
    uint8_t srcA = COLOR_A(src);
    if (srcA == 0xFF) return src;
    if (srcA == 0x00) return dst;
    uint8_t srcR = COLOR_R(src);
    uint8_t srcG = COLOR_G(src);
    uint8_t srcB = COLOR_B(src);
    uint8_t dstR = COLOR_R(dst);
    uint8_t dstG = COLOR_G(dst);
    uint8_t dstB = COLOR_B(dst);
    uint32_t invA = 255 - srcA;
    uint8_t outR = (uint8_t)(((srcR * srcA) + (dstR * invA)) / 255);
    uint8_t outG = (uint8_t)(((srcG * srcA) + (dstG * invA)) / 255);
    uint8_t outB = (uint8_t)(((srcB * srcA) + (dstB * invA)) / 255);
    return rgb(outR, outG, outB);
}

bool WindowManager::keyboard_handler(char key, bool shift, bool meta, unsigned char scancode) {
    if (focusedWindow != nullptr) {
        return focusedWindow->handleKeyboard(key, shift, meta, scancode);
    }
    return false;
};

// Shifts everything after ref down by one, shared by remove() and focus() so z-order stays a dense array.
static void zOrderRemove(window_ref_t* zOrder, uint32_t* zOrderCount, window_ref_t ref) {
    for (uint32_t i = 0; i < *zOrderCount; i++) {
        if (zOrder[i] == ref) {
            for (uint32_t j = i; j + 1 < *zOrderCount; j++) {
                zOrder[j] = zOrder[j + 1];
            }
            (*zOrderCount)--;
            return;
        }
    }
}

WindowManager::WindowManager(/* Constraints constraints, Environment environment */ void) {
    memcpy(&this->constraints, &defaultConstraints, sizeof(defaultConstraints));
    memcpy(&this->environment, &defaultEnvironment, sizeof(struct Environment));
    this->screen = new Surface(constraints.screenXSizePx, constraints.screenYSizePx);
    this->maxWindowCount = constraints.maxWindowCount;
    this->windowCount = 0;
    this->windows = (Window**)malloc(sizeof(Window*) * this->maxWindowCount);
    this->zOrder = (window_ref_t*)malloc(sizeof(window_ref_t) * this->maxWindowCount);
    this->zOrderCount = 0;
    this->focusedWindow = nullptr;
    this->lastButtons = 0;
    this->draggingWindow = WINGMAN_INVALID_WINDOW;
    this->dragOffsetX = 0;
    this->dragOffsetY = 0;
    this->lastDragRedrawMs = 0;
    this->dragLastRect = { 0, 0, 0, 0 };
    if (this->windows != nullptr) {
        for (uint32_t i = 0; i < this->maxWindowCount; i++) {
            this->windows[i] = nullptr;
        }
    }
    if (this->zOrder != nullptr) {
        for (uint32_t i = 0; i < this->maxWindowCount; i++) {
            this->zOrder[i] = WINGMAN_INVALID_WINDOW;
        }
    }
};

WindowManager::~WindowManager() {
    if (this->windows != NULL) {
        for (uint32_t i = 0; i < this->maxWindowCount; i++) {
            if (this->windows[i] != NULL) {
                delete this->windows[i];
                this->windows[i] = NULL;
            }
        }
        free(this->windows);
        this->windows = NULL;
    }
    if (this->zOrder != NULL) {
        free(this->zOrder);
        this->zOrder = NULL;
    }
    if (this->screen != NULL) {
        delete this->screen;
        this->screen = NULL;
    }
}

window_ref_t WindowManager::add(Window* window) {
    if (window == NULL) return WINGMAN_INVALID_WINDOW;
    if (this->windows == NULL || this->zOrder == NULL) return WINGMAN_INVALID_WINDOW;
    if (this->windowCount >= this->maxWindowCount) return WINGMAN_INVALID_WINDOW;
    for (uint32_t i = 0; i < this->maxWindowCount; i++) {
        if (this->windows[i] == NULL) {
            if (focusedWindow == nullptr) {
                focusedWindow = window;
            }
            this->windows[i] = window;
            this->windowCount++;
            this->zOrder[this->zOrderCount++] = (window_ref_t)i;
            return (window_ref_t)i;
        }
    }
    return WINGMAN_INVALID_WINDOW;
}

int WindowManager::remove(window_ref_t ref) {
    if (ref < 0) return -1;
    if ((uint32_t)ref >= this->maxWindowCount) return -1;
    if (this->windows == NULL) return -1;
    if (this->windows[ref] == NULL) return -1;
    if (this->focusedWindow == this->windows[ref]) this->focusedWindow = nullptr;
    if (this->zOrder != NULL) zOrderRemove(this->zOrder, &this->zOrderCount, ref);
    delete this->windows[ref];
    this->windows[ref] = NULL;
    if (this->windowCount > 0) this->windowCount--;
    return 0;
}

void WindowManager::focus(window_ref_t ref) {
    Window* window = this->get(ref);
    if (window == nullptr) return;
    this->focusedWindow = window;
    if (this->zOrder != NULL) {
        zOrderRemove(this->zOrder, &this->zOrderCount, ref);
        this->zOrder[this->zOrderCount++] = ref;
    }
}

window_ref_t WindowManager::windowAt(int x, int y) {
    if (this->windows == NULL || this->zOrder == NULL) return WINGMAN_INVALID_WINDOW;
    for (int i = (int)this->zOrderCount - 1; i >= 0; i--) {
        window_ref_t ref = this->zOrder[i];
        Window* window = this->windows[ref];
        if (window == NULL) continue;
        if (x >= window->offsetX && x <= window->offsetX + window->width &&
            y >= window->offsetY && y <= window->offsetY + window->height) {
            return ref;
        }
    }
    return WINGMAN_INVALID_WINDOW;
}

Window* WindowManager::get(window_ref_t ref) {
    if (ref < 0) return NULL;
    if ((uint32_t)ref >= this->maxWindowCount) return NULL;
    if (this->windows == NULL) return NULL;
    return this->windows[ref];
}

uint32_t WindowManager::count() { return this->windowCount; }

void WindowManager::clearScreen(void) {
    if (this->screen == NULL) return;
    this->screen->clear(this->environment.backgroundColor);
}

void WindowManager::clearScreen(Rect dirty) {
    if (this->screen == NULL) return;
    this->screen->clear(dirty.x, dirty.y, dirty.w, dirty.h, this->environment.backgroundColor);
}

// Only clears and re-blends the given rect, not the whole screen -- see docs/DOCS.md ("Rect / dirty-rect compositing").
void WindowManager::composite(Rect dirty) {
    if (this->screen == NULL) return;
    Rect screenRect = { 0, 0, (int)this->constraints.screenXSizePx, (int)this->constraints.screenYSizePx };
    dirty = rect_intersect(dirty, screenRect);
    if (rect_empty(dirty)) return;
    clearScreen(dirty);
    if (this->zOrder == NULL) return;
    const int screenWidth = this->screen->width;
    for (uint32_t i = 0; i < this->zOrderCount; i++) {
        window_ref_t ref = this->zOrder[i];
        Window* window = this->windows[ref];
        if (window == NULL) continue;
        if (window->surface == NULL) continue;
        if (window->surface->pixels == NULL) continue;
        Surface* surface = window->surface;
        Rect windowRect = { window->offsetX, window->offsetY, surface->getWidth(), surface->getHeight() };
        Rect region = rect_intersect(windowRect, dirty);
        if (rect_empty(region)) continue;
        // Must run after the dirty-rect check above, or every pass redraws every window's title band.
        draw_title_bar(surface, window->width, window->titleBar, window->title);
        for (int screenY = region.y; screenY < region.y + region.h; screenY++) {
            int localY = screenY - window->offsetY;
            int rowBase = screenY * screenWidth;
            for (int screenX = region.x; screenX < region.x + region.w; screenX++) {
                int localX = screenX - window->offsetX;
                color_t src = surface->getPixel(localX, localY);
                color_t dst = this->screen->pixels[rowBase + screenX];
                this->screen->pixels[rowBase + screenX] = blend(src, dst);
            }
        }
    }
}

void WindowManager::composite() {
    Rect full = { 0, 0, (int)this->constraints.screenXSizePx, (int)this->constraints.screenYSizePx };
    composite(full);
}

// Cached before the call, not re-read after: handleKeyboard() can delete the
// focused window (e.g. MessageBox's Enter-to-dismiss), which would leave
// focusedWindow null and a re-read here a NULL deref.
bool WindowManager::handleKeyboardEvent(char key, bool shift, bool meta, unsigned char scancode, Rect* dirty) {
    Window* focused = this->focusedWindow;
    if (focused == nullptr) return false;
    Rect focusedRect = { focused->offsetX, focused->offsetY, focused->width, focused->height };
    if (!this->keyboard_handler(key, shift, meta, scancode)) return false;
    this->composite(focusedRect);
    if (dirty != nullptr) *dirty = focusedRect;
    return true;
}

bool WindowManager::handleMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, Rect* dirty) {
    const unsigned char pressedEdge = buttons & (unsigned char)~this->lastButtons;
    this->lastButtons = buttons;
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
        window_ref_t hitRef = this->windowAt(x, y);
        Window* hitWindow = this->get(hitRef);
        if (hitWindow != nullptr && hitWindow != this->focusedWindow) {
            this->focus(hitRef);
            markDirty({ hitWindow->offsetX, hitWindow->offsetY, hitWindow->width, hitWindow->height });
        }
        // The close/minimize/maximize trio must not also start a window drag.
        bool inCloseButtonZone = hitWindow != nullptr && (x - hitWindow->offsetX) < hitWindow->titleBar.closeButtonZoneWidth();
        if (hitWindow != nullptr && !inCloseButtonZone && y - hitWindow->offsetY < WINGMAN_DRAG_HANDLE_HEIGHT) {
            this->draggingWindow = hitRef;
            this->dragOffsetX = x - hitWindow->offsetX;
            this->dragOffsetY = y - hitWindow->offsetY;
            this->dragLastRect = { hitWindow->offsetX, hitWindow->offsetY, hitWindow->width, hitWindow->height };
        }
    }
    const bool wasDragging = (this->draggingWindow != WINGMAN_INVALID_WINDOW);
    // Captured before the clear below so the drag-end block can still
    // reach the window that was just released.
    Window* draggedWindowBeforeRelease = wasDragging ? this->get(this->draggingWindow) : nullptr;
    if (!(buttons & 1)) this->draggingWindow = WINGMAN_INVALID_WINDOW;
    // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window dragging") for
    // why a drag ending forces a redraw regardless of the throttle below.
    if (wasDragging && this->draggingWindow == WINGMAN_INVALID_WINDOW && draggedWindowBeforeRelease != nullptr) {
        Window* w = draggedWindowBeforeRelease;
        Rect currentRect = { w->offsetX, w->offsetY, w->width, w->height };
        markDirty(rect_union(this->dragLastRect, currentRect));
    }
    if (this->draggingWindow != WINGMAN_INVALID_WINDOW) {
        Window* dragged = this->get(this->draggingWindow);
        if (dragged != nullptr) {
            int newX = x - this->dragOffsetX;
            int newY = y - this->dragOffsetY;
            // See docs/DOCS.md ("mods/core/wingman/wingman.cpp — window
            // dragging") for why this clamps to keep the whole window rect
            // on screen, not just the point under the cursor.
            int maxX = this->screen->getWidth() - dragged->width;
            int maxY = this->screen->getHeight() - dragged->height;
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
                if (now - this->lastDragRedrawMs >= WINGMAN_DRAG_REDRAW_INTERVAL_MS) {
                    this->lastDragRedrawMs = now;
                    Rect currentRect = { newX, newY, dragged->width, dragged->height };
                    markDirty(rect_union(this->dragLastRect, currentRect));
                    this->dragLastRect = currentRect;
                }
            }
        }
    } else {
        // handleMouse() itself intercepts clicks on the title bar's close button before this reaches the delegate.
        Window* focused = this->focusedWindow;
        if (focused != nullptr) {
            const int rectX = focused->offsetX;
            const int rectY = focused->offsetY;
            const int width = focused->width;
            const int height = focused->height;
            if (x >= rectX && x <= (rectX + width)) {
                // Check vertical boundaries (assuming screen coordinates where Y goes down)
                if (y >= rectY && y <= (rectY + height)) {
                    const int _x = x - rectX;
                    const int _y = y - rectY;
                    if (focused->handleMouse(_x, _y, dx, dy, buttons, pressedEdge)) {
                        markDirty({ rectX, rectY, width, height });
                    }
                }
            }
        }
    }
    if (needsRedraw) {
        this->composite(dirtyRect);
        if (dirty != nullptr) *dirty = dirtyRect;
    }
    return needsRedraw;
}