#include "./headers/manager.h"
#include "./headers/types.h"

#include "../../dev/serial/serial.h"

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

// See docs/DOCS.md ("mods/core/wingman/headers/manager.h / manager.cpp" section).
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

// See docs/DOCS.md ("mods/core/wingman/headers/types.h -- Rect / dirty-rect compositing").
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