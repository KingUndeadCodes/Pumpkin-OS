#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

// See docs/DOCS.md ("mods/core/wingman/headers/widgets/button.h" section).
typedef void (*ButtonCallback)(void* userdata);

class Button {
    public:
        char* message;
        uint32_t color;
        ButtonCallback onClick;
        void* userdata;
        int x;
        int y;
        int width;
        int height;
        Button(const char* message, uint32_t color, ButtonCallback onClick, void* userdata);
        ~Button();
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};
