#include "../headers/widgets/slider.h"
#include "../headers/shapes.h"

#define SLIDER_COLOR_TRACK 0xFF6e6862
#define SLIDER_COLOR_FILL  0xFF34c759
#define SLIDER_COLOR_THUMB 0xFFFFFFFF

static inline float slider_clamp_float(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Slider::Slider(float min, float max, float initialValue, SliderCallback onChange, void* userdata) : Widget(WidgetTypeSlider) {
    this->min = min;
    this->max = max;
    this->value = slider_clamp_float(initialValue, min, max);
    this->dragging = false;
    this->onChange = onChange;
    this->userdata = userdata;
    this->width = 160;
    this->height = 24;
};

// See docs/DOCS.md ("mods/core/wingman/widgets/slider.cpp" section) for the thumb-sizing/positioning rationale.
static inline int slider_thumb_x(const Slider* s) {
    int thumbSize = s->height;
    int trackStart = s->x + thumbSize / 2;
    int trackEnd = s->x + s->width - thumbSize / 2;
    float t = (s->max > s->min) ? (s->value - s->min) / (s->max - s->min) : 0.0f;
    return trackStart + (int)(t * (float)(trackEnd - trackStart)) - thumbSize / 2;
}

void Slider::draw(Surface* surface, int thickness) const {
    int bx = this->x, by = this->y, bw = this->width, bh = this->height;
    int thumbSize = bh;
    int trackHeight = bh / 3;
    int trackY = by + (bh - trackHeight) / 2;
    // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h").
    draw_rounded_rect_fill(surface, bx, trackY, bw, trackHeight, trackHeight / 2, SLIDER_COLOR_TRACK);
    int thumbX = slider_thumb_x(this);
    int fillWidth = (thumbX + thumbSize / 2) - bx;
    if (fillWidth > 0) {
        if (fillWidth > bw) fillWidth = bw;
        draw_rounded_rect_fill(surface, bx, trackY, fillWidth, trackHeight, trackHeight / 2, SLIDER_COLOR_FILL);
    }
    int thumbInset = thumbSize - thickness * 2;
    if (thumbInset < 1) thumbInset = 1;
    draw_rounded_rect_fill(surface, thumbX + thickness, by + thickness, thumbInset, thumbInset, thumbInset / 2, SLIDER_COLOR_THUMB);
};

bool Slider::onMouse(int px, int py, unsigned char buttons, unsigned char pressedEdge) {
    if (pressedEdge & 1 && this->contains(px, py)) this->dragging = true;
    if (!(buttons & 1)) this->dragging = false;
    if (!this->dragging) return false;

    int thumbSize = this->height;
    int trackStart = this->x + thumbSize / 2;
    int trackEnd = this->x + this->width - thumbSize / 2;
    float t = (trackEnd > trackStart) ? (float)(px - trackStart) / (float)(trackEnd - trackStart) : 0.0f;
    t = slider_clamp_float(t, 0.0f, 1.0f);
    float newValue = this->min + t * (this->max - this->min);
    if (newValue == this->value) return false;
    this->value = newValue;
    if (this->onChange != NULL) this->onChange(this->value, this->userdata);
    return true;
};
