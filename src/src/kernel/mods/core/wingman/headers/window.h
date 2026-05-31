#pragma once

#include "../../../dev/kb/kb.h"
#include "./surface.h"
#include "./types.h"

class Window {
    public:
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        char* title;
        Surface* surface;
        Window(int width, int height, int offsetX, int offsetY, const char* title);
        virtual ~Window();
        virtual void keyboard_callback(char key, bool shift, bool meta, unsigned char scancode);
        void* operator new(size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
        void* operator new[](size_t size) { return malloc(size); }
        void operator delete[](void* ptr) noexcept { free(ptr); }
    public:
        GlobalKbCallback fn;
        void assignKeyboardFunction(GlobalKbCallback function);
};