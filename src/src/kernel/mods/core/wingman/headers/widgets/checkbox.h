#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "./widget.h"

// See docs/DOCS.md ("mods/core/wingman/widgets/checkbox.cpp" section).
typedef void (*CheckboxCallback)(bool checked, void* userdata);

enum CheckboxStyle {
    CheckboxStyleBox,     // traditional square checkbox
    CheckboxStyleToggle,  // SwiftUI-style sliding switch
};

class Checkbox : public Widget {
    public:
        bool checked;
        CheckboxStyle style;
        CheckboxCallback onChange;
        void* userdata;
        Checkbox(CheckboxStyle style, bool initialChecked = false, CheckboxCallback onChange = NULL, void* userdata = NULL);
        Checkbox(CheckboxStyle style, bool initialChecked, CheckboxCallback onChange, void* userdata, int x, int y);
        void draw(Surface* surface, int thickness) const override;
        // Hit-tests, then toggles `checked` and fires `onChange` if hit.
        // Returns whether it was actually toggled.
        bool click(int px, int py);
};
