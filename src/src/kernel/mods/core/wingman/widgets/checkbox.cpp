#include "../headers/widgets/checkbox.h"

#define CHECKBOX_COLOR_BORDER    0xFFFFFFFF
#define CHECKBOX_COLOR_BG        0xFF1a1615
#define CHECKBOX_COLOR_ON        0xFF34c759
#define CHECKBOX_COLOR_OFF_TRACK 0xFF6e6862
#define CHECKBOX_COLOR_THUMB     0xFFFFFFFF

Checkbox::Checkbox(CheckboxStyle style, bool initialChecked, CheckboxCallback onChange, void* userdata) : Widget(WidgetTypeCheckbox) {
    this->style = style;
    this->checked = initialChecked;
    this->onChange = onChange;
    this->userdata = userdata;
    if (style == CheckboxStyleToggle) {
        this->width = 48;
        this->height = 24;
    } else {
        this->width = 24;
        this->height = 24;
    }
};

bool Checkbox::click(int px, int py) {
    if (!this->contains(px, py)) return false;
    this->checked = !this->checked;
    if (this->onChange != NULL) this->onChange(this->checked, this->userdata);
    return true;
}

void Checkbox::draw(Surface* surface, int thickness) const {
    int bx = this->x, by = this->y, bw = this->width, bh = this->height;
    if (this->style == CheckboxStyleToggle) {
        // Track: full widget rect, color signals state -- no border, matching
        // SwiftUI's borderless pill track.
        uint32_t trackColor = this->checked ? CHECKBOX_COLOR_ON : CHECKBOX_COLOR_OFF_TRACK;
        for (int y = 0; y < bh; y++) {
            for (int x = 0; x < bw; x++) surface->putPixelUnsafe(bx + x, by + y, trackColor);
        }
        // Thumb: inset by `thickness`, slid to whichever side matches `checked`.
        int thumbSize = bh - thickness * 2;
        if (thumbSize < 1) thumbSize = 1;
        int thumbY = by + thickness;
        int thumbX = this->checked ? (bx + bw - thickness - thumbSize) : (bx + thickness);
        for (int y = 0; y < thumbSize; y++) {
            for (int x = 0; x < thumbSize; x++) surface->putPixelUnsafe(thumbX + x, thumbY + y, CHECKBOX_COLOR_THUMB);
        }
    } else {
        // Fill signals state; outer border matches Button/TextInput's look.
        uint32_t fillColor = this->checked ? CHECKBOX_COLOR_ON : CHECKBOX_COLOR_BG;
        for (int y = thickness; y < bh - thickness; y++) {
            for (int x = thickness; x < bw - thickness; x++) surface->putPixelUnsafe(bx + x, by + y, fillColor);
        }
        for (int t = 0; t < thickness; t++) {
            for (int x = 0; x < bw; x++) surface->putPixelUnsafe(bx + x, by + t, CHECKBOX_COLOR_BORDER);
            for (int x = 0; x < bw; x++) surface->putPixelUnsafe(bx + x, by + bh - 1 - t, CHECKBOX_COLOR_BORDER);
            for (int y = 0; y < bh; y++) surface->putPixelUnsafe(bx + t, by + y, CHECKBOX_COLOR_BORDER);
            for (int y = 0; y < bh; y++) surface->putPixelUnsafe(bx + bw - 1 - t, by + y, CHECKBOX_COLOR_BORDER);
        }
    }
}
