#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "./widget.h"

// See docs/DOCS.md ("mods/core/wingman/widgets/slider.cpp" section).
typedef void (*SliderCallback)(float value, void* userdata);

class Slider : public Widget {
    public:
        float min;
        float max;
        float value;
        bool dragging;
        SliderCallback onChange;
        void* userdata;
        Slider(float min, float max, float initialValue, SliderCallback onChange = NULL, void* userdata = NULL);
        Slider(float min, float max, float initialValue, SliderCallback onChange, void* userdata, int x, int y);
        void draw(Surface* surface, int thickness) const override;
        // See docs/DOCS.md ("mods/core/wingman/widgets/slider.cpp" section) for why this must run on every mouse event, not just clicks.
        bool onMouse(int px, int py, unsigned char buttons, unsigned char pressedEdge);
};
