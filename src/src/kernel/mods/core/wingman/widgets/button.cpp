#include "../headers/widgets/button.h"
#include "../headers/shapes.h"
#include "../../../dev/vbe/font.h"
#include "../../fontman/fontman.h"
#include <string.h>

#define BUTTON_COLOR_W 0xFFFFFFFF

Button::Button(const char* message, uint32_t color, ButtonCallback onClick, void* userdata) : Widget(WidgetTypeButton) {
    this->color = color;
    this->onClick = onClick;
    this->userdata = userdata;
    if (message != NULL) {
        size_t messageLength = strlen(message);
        this->message = (char*)malloc(messageLength + 1);
        if (this->message != NULL) { strcpy(this->message, message); }
    } else {
        this->message = NULL;
    }
    // See docs/DOCS.md ("mods/core/wingman/widgets/button.cpp" section)
    // for why this defaults to a content-based size instead of 0.
    constexpr int scale = 2;
    int labelLen = message != NULL ? (int)strlen(message) : 0;
    this->width = labelLen * (8 * scale) + 32;
    this->height = (8 * scale) + 24;
};

Button::~Button() {
    free(this->message);
};

// See docs/DOCS.md ("mods/core/wingman/widgets/button.cpp" section).
static inline uint32_t shade(uint32_t color, int delta) {
    int r = (int)COLOR_R(color) + delta;
    int g = (int)COLOR_G(color) + delta;
    int b = (int)COLOR_B(color) + delta;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static void drawChar(Surface* surface, unsigned x, unsigned y, char c, unsigned color, unsigned scale) {
    const FontAtlas* atlas = ttf_font_get_atlas(scale);
    if (atlas == nullptr) {
        for (unsigned i = 0; i < 8; i++) {
            for (unsigned j = 0; j < 8; j++) {
                if (Font[(int)c][i] & (1 << j)) {
                    for (unsigned k = 0; k < scale; k++) {
                        for (unsigned l = 0; l < scale; l++) {
                            surface->putPixelUnsafe(x + j * scale + l, y + i * scale + k, color);
                        }
                    }
                }
            }
        }
        return;
    }
    ttf_blit_glyph(atlas, c, (int)x, (int)y, color,
        [&](int px, int py, uint8_t alpha, uint32_t fg) {
            color_t under = surface->getPixel(px, py);
            surface->putPixelUnsafe(px, py, ttf_blend_over(under, fg, alpha));
        });
}

void Button::draw(Surface* surface, int thickness) const {
    constexpr uint32_t scale = 2;
    int bx = this->x, by = this->y, bw = this->width, bh = this->height;
    // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h") for the
    // radius-as-fraction-of-height rule shared across widgets.
    int radius = bh / 5;
    uint32_t highlight = shade(this->color, 35);
    uint32_t shadow = shade(this->color, -35);
    // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h") for the two-call pattern and the bevel-clip rationale below.
    draw_rounded_rect_fill(surface, bx, by, bw, bh, radius, BUTTON_COLOR_W);
    draw_rounded_rect_fill(surface, bx + thickness, by + thickness, bw - thickness * 2, bh - thickness * 2, radius - thickness, this->color);
    int bevelInset = radius > thickness ? radius : thickness;
    for (int x = bevelInset; x < bw - bevelInset; x++) surface->putPixelUnsafe(bx + x, by + thickness, highlight);
    for (int x = bevelInset; x < bw - bevelInset; x++) surface->putPixelUnsafe(bx + x, by + bh - thickness - 1, shadow);
    for (int y = bevelInset; y < bh - bevelInset; y++) surface->putPixelUnsafe(bx + thickness, by + y, highlight);
    for (int y = bevelInset; y < bh - bevelInset; y++) surface->putPixelUnsafe(bx + bw - thickness - 1, by + y, shadow);
    // Label, centered.
    int labelLen = strlen(this->message);
    int charWidth = ttf_font_char_advance(scale);
    int charHeight = 8 * scale;
    int textWidth = labelLen * charWidth;
    int textX = bx + (bw - textWidth) / 2;
    int textY = by + (bh - charHeight) / 2;
    for (int i = 0; i < labelLen; i++) {
        drawChar(surface, textX + i * charWidth, textY, this->message[i], BUTTON_COLOR_W, scale);
    }
}
