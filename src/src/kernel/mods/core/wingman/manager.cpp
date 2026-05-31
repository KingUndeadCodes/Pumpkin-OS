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

WindowManager::keyboard_handler(char key, bool shift, bool meta, unsigned char scancode) {
    for (uint32_t i = 0; i < this->maxWindowCount; i++) {
        Window* window = this->windows[i];
        if (window == NULL) continue;
        if (window->fn == NULL) continue;
        window->fn(key, shift, meta, scancode);
    };
    return;
};

WindowManager::WindowManager(/* Constraints constraints, Environment environment */ void) {
    memcpy(&this->constraints, &defaultConstraints, sizeof(defaultConstraints));
    memcpy(&this->environment, &defaultEnvironment, sizeof(struct Environment));
    this->screen = new Surface(constraints.screenXSizePx, constraints.screenYSizePx);
    this->maxWindowCount = constraints.maxWindowCount;
    this->windowCount = 0;
    this->windows = (Window**)malloc(sizeof(Window*) * this->maxWindowCount);
    if (this->windows != nullptr) {
        for (uint32_t i = 0; i < this->maxWindowCount; i++) {
            this->windows[i] = nullptr;
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
    if (this->screen != NULL) {
        delete this->screen;
        this->screen = NULL;
    }
}

window_ref_t WindowManager::add(Window* window) {
    if (window == NULL) return WINGMAN_INVALID_WINDOW;
    if (this->windows == NULL) return WINGMAN_INVALID_WINDOW;
    if (this->windowCount >= this->maxWindowCount) return WINGMAN_INVALID_WINDOW;
    for (uint32_t i = 0; i < this->maxWindowCount; i++) {
        if (this->windows[i] == NULL) {
            this->windows[i] = window;
            this->windowCount++;
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
    delete this->windows[ref];
    this->windows[ref] = NULL;
    if (this->windowCount > 0) this->windowCount--;
    return 0;
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

/*
void WindowManager::composite() {
    if (this->screen == NULL) return;
    clearScreen();
    for (uint32_t i = 0; i < this->maxWindowCount; i++) {
        Window* window = this->windows[i];
        if (window == NULL) continue;
        if (window->surface == NULL) continue;
        Surface* surface = window->surface;
        const int height = surface->getHeight();
        const int width = surface->getWidth();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int screenX = window->offsetX + x;
                int screenY = window->offsetY + y;
                color_t src = surface->getPixel(x, y);
                color_t dst = this->screen->getPixel(screenX, screenY);
                this->screen->putPixel(screenX, screenY, blend(src, dst));
            }
        }
    }
}
*/

void WindowManager::composite() {
    if (this->screen == NULL) return;
    clearScreen();
    for (uint32_t i = 0; i < this->maxWindowCount; i++) {
        Window* window = this->windows[i];
        if (window == NULL) continue;
        if (window->surface == NULL) continue;
        if (window->surface->pixels == NULL) continue;
        Surface* surface = window->surface;
        const int screenWidth = this->screen->width;
        const int height = surface->getHeight();
        const int width = surface->getWidth();
        for (int y = 0; y < height; y++) {
            int screenY = window->offsetY + y;
            for (int x = 0; x < width; x++) {
                int screenX = window->offsetX + x;
                color_t src = surface->getPixel(x, y);
                color_t dst = this->screen->getPixel(screenX, screenY);
                this->screen->pixels[screenY * screenWidth + screenX] = blend(src, dst);
                // this->screen->putPixel(screenX, screenY, blend(src, dst));
            }
        }
    }
}