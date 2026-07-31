#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "./widget.h"

// See docs/DOCS.md ("mods/core/wingman/headers/widgets/button.h" section).
typedef void (*ButtonCallback)(void* userdata);

class Button : public Widget {
    public:
        char* message;
        uint32_t color;
        ButtonCallback onClick;
        void* userdata;
        Button(const char* message, uint32_t color, ButtonCallback onClick, void* userdata);
        Button(const char* message, uint32_t color, ButtonCallback onClick, void* userdata, int x, int y);
        ~Button();
        void draw(Surface* surface, int thickness) const override;
};
