#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "./widget.h"

// See docs/DOCS.md ("mods/core/wingman/widgets/textinput.cpp" section).
class TextInput : public Widget {
    public:
        char* buffer;
        int maxLength;
        int length;
        char* placeholder;
        bool focused;
        TextInput(int maxLength, const char* placeholder = NULL);
        TextInput(int maxLength, const char* placeholder, int x, int y);
        TextInput(int maxLength, const char* placeholder, int x, int y, int width, int height);
        ~TextInput();
        void draw(Surface* surface, int thickness) const override;
        // Returns true if this consumed the key. Only meaningful while
        // `focused` is true -- the caller decides who's focused and
        // whether to call this at all.
        bool onKeyboard(char key, bool shift, bool meta, unsigned char scancode);
};
