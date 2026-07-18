#pragma once

#include "./window.h"
#include "./surface.h"
#include "./types.h"

// Wingman = Window General Manager

#define WINGMAN_INVALID_WINDOW (-1)
#define WINGMAN_DEFAULT_MAX_WINDOWS 10
#define WINGMAN_DEFAULT_COLOR_DEPTH 32
#define WINGMAN_DEFAULT_SCREEN_WIDTH 1024
#define WINGMAN_DEFAULT_SCREEN_HEIGHT 768
#define WINGMAN_DEFAULT_FOREGROUND rgb(255, 255, 255)
#define WINGMAN_DEFAULT_BACKGROUND rgb(11, 26, 28)

static struct Constraints defaultConstraints = {
    WINGMAN_DEFAULT_MAX_WINDOWS,
    WINGMAN_DEFAULT_COLOR_DEPTH,
    WINGMAN_DEFAULT_SCREEN_WIDTH,
    WINGMAN_DEFAULT_SCREEN_HEIGHT
};

static struct Environment defaultEnvironment = {
    WINGMAN_DEFAULT_FOREGROUND,
    WINGMAN_DEFAULT_BACKGROUND
};

class WindowManager {
    private:
        Constraints constraints;
        Environment environment;
        Window** windows;
        uint32_t maxWindowCount;
        uint32_t windowCount;
        // See docs/DOCS.md ("mods/core/wingman/headers/manager.h / manager.cpp" section).
        window_ref_t* zOrder;
        uint32_t zOrderCount;
    public:
        Window* focusedWindow;
        bool keyboard_handler(char key, bool shift, bool meta, unsigned char scancode);
        Surface* screen;
    public:
        WindowManager(/* Constraints constraints, Environment environment */ void);
        ~WindowManager();
        window_ref_t add(Window* window);
        int remove(window_ref_t ref);
        // See docs/DOCS.md ("mods/core/wingman/headers/manager.h / manager.cpp" section).
        void focus(window_ref_t ref);
        window_ref_t windowAt(int x, int y);
        Window* get(window_ref_t ref);
        void composite();
        void composite(Rect dirty);
        void clearScreen();
        void clearScreen(Rect dirty);
        uint32_t count();
        void* operator new(size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
};