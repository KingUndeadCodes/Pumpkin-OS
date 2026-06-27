#pragma once

#include "../../../dev/kb/kb.h"
#include "./surface.h"
#include "./types.h"

class KeyboardDelegate {
    public:
        virtual bool onKeyboard(char key, bool shift, bool meta, unsigned char scancode) = 0;
};

class MouseDelegate {
    public:
        virtual bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons) = 0;
};

class Window {
    public:
        KeyboardDelegate* keyboardDelegate;
        MouseDelegate* mouseDelegate;
        void setKeyboardDelegate(KeyboardDelegate* delegate);
        void setMouseDelegate(MouseDelegate* delegate);
        bool handleKeyboard(char key, bool shift, bool meta, unsigned char scancode);
        bool handleMouse(int x, int y, int dx, int dy, unsigned char buttons);
    public:
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        char* title;
        Surface* surface;
        Window(int width, int height, int offsetX, int offsetY, const char* title);
        ~Window();
        void* operator new(size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
        void* operator new[](size_t size) { return malloc(size); }
        void operator delete[](void* ptr) noexcept { free(ptr); }
};