#include "../headers/widgets/checkbox.h"
#include "../headers/shapes.h"

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

Checkbox::Checkbox(CheckboxStyle style, bool initialChecked, CheckboxCallback onChange, void* userdata, int x, int y)
    : Checkbox(style, initialChecked, onChange, userdata) {
    this->x = x;
    this->y = y;
}

bool Checkbox::click(int px, int py) {
    if (!this->contains(px, py)) return false;
    this->checked = !this->checked;
    if (this->onChange != NULL) this->onChange(this->checked, this->userdata);
    return true;
}

void Checkbox::draw(Surface* surface, int thickness) const {
    int bx = this->x, by = this->y, bw = this->width, bh = this->height;
    if (this->style == CheckboxStyleToggle) {
        // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h") for why the toggle style uses bh/2, not the /5 rule.
        uint32_t trackColor = this->checked ? CHECKBOX_COLOR_ON : CHECKBOX_COLOR_OFF_TRACK;
        draw_rounded_rect_fill(surface, bx, by, bw, bh, bh / 2, trackColor);
        int thumbSize = bh - thickness * 2;
        if (thumbSize < 1) thumbSize = 1;
        int thumbY = by + thickness;
        int thumbX = this->checked ? (bx + bw - thickness - thumbSize) : (bx + thickness);
        draw_rounded_rect_fill(surface, thumbX, thumbY, thumbSize, thumbSize, thumbSize / 2, CHECKBOX_COLOR_THUMB);
    } else {
        // Box style: border + inset fill, both rounded the same amount minus the border thickness.
        int radius = bh / 5;
        uint32_t fillColor = this->checked ? CHECKBOX_COLOR_ON : CHECKBOX_COLOR_BG;
        draw_rounded_rect_fill(surface, bx, by, bw, bh, radius, CHECKBOX_COLOR_BORDER);
        draw_rounded_rect_fill(surface, bx + thickness, by + thickness, bw - thickness * 2, bh - thickness * 2, radius - thickness, fillColor);
    }
}
