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
        // `buttons` is the raw current button state (still useful for
        // drag/hold checks); `pressedEdge` has only the bits that
        // transitioned from up to down on this exact packet, so a single
        // physical click is observed exactly once regardless of how many
        // packets arrive while the button stays held.
        virtual bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) = 0;
};

class Window {
    public:
        KeyboardDelegate* keyboardDelegate;
        MouseDelegate* mouseDelegate;
        void setKeyboardDelegate(KeyboardDelegate* delegate);
        void setMouseDelegate(MouseDelegate* delegate);
        bool handleKeyboard(char key, bool shift, bool meta, unsigned char scancode);
        bool handleMouse(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge);
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