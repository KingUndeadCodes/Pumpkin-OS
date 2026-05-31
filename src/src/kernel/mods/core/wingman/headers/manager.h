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
    public: 
        keyboard_handler(char key, bool shift, bool meta, unsigned char scancode);
    public:
        Surface* screen;
    public:
        WindowManager(/* Constraints constraints, Environment environment */ void);
        ~WindowManager();
        window_ref_t add(Window* window);
        int remove(window_ref_t ref);
        Window* get(window_ref_t ref);
        void composite();
        void clearScreen();
        uint32_t count();
        void* operator new(size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
};