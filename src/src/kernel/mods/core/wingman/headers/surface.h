#pragma once

#include "./types.h"

class WindowManager; // Forward Declare WindowManager class to allow for friends.

class Surface {
    private:
        int width;
        int height;
        color_t* pixels;
        friend class WindowManager; // Make WindowManager a friend class of surface for faster access to screen property instead of using putPixel all the time.
    public:
        Surface(int width, int height);
        ~Surface();
        int getWidth();
        int getHeight();
        void clear(color_t color);
        void clear(int x, int y, int w, int h, color_t color);
        void putPixel(int x, int y, color_t color);
        color_t getPixel(int x, int y);
        color_t* getBuffer();
        void* operator new(size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
        void* operator new[](size_t size) { return malloc(size); }
        void operator delete[](void* ptr) noexcept { free(ptr); }
    public:
        void putPixelUnsafe(int x, int y, color_t color);
};
