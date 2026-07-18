#include "../headers/widgets/textinput.h"
#include "../headers/shapes.h"
#include "../../../dev/vbe/font.h"
#include "../../fontman/fontman.h"
#include <string.h>

#define TEXTINPUT_COLOR_BG          0xFF1a1615
#define TEXTINPUT_COLOR_BG_FOCUSED  0xFF241f1e
#define TEXTINPUT_COLOR_BORDER      0xFFFFFFFF
#define TEXTINPUT_COLOR_TEXT        0xFFFFFFFF
#define TEXTINPUT_COLOR_PLACEHOLDER 0xFF7a7472
#define TEXTINPUT_COLOR_CARET       0xFFFFFFFF

TextInput::TextInput(int maxLength, const char* placeholder) : Widget(WidgetTypeTextInput) {
    this->maxLength = maxLength > 0 ? maxLength : 1;
    this->length = 0;
    this->focused = false;
    this->buffer = (char*)malloc(this->maxLength + 1);
    if (this->buffer != NULL) this->buffer[0] = '\0';
    if (placeholder != NULL) {
        size_t placeholderLength = strlen(placeholder);
        this->placeholder = (char*)malloc(placeholderLength + 1);
        if (this->placeholder != NULL) { strcpy(this->placeholder, placeholder); }
    } else {
        this->placeholder = NULL;
    }
};

TextInput::~TextInput() {
    free(this->buffer);
    free(this->placeholder);
};

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

void TextInput::draw(Surface* surface, int thickness) const {
    constexpr uint32_t scale = 2;
    int bx = this->x, by = this->y, bw = this->width, bh = this->height;
    int radius = bh / 5;
    uint32_t bg = this->focused ? TEXTINPUT_COLOR_BG_FOCUSED : TEXTINPUT_COLOR_BG;
    // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h").
    draw_rounded_rect_fill(surface, bx, by, bw, bh, radius, TEXTINPUT_COLOR_BORDER);
    draw_rounded_rect_fill(surface, bx + thickness, by + thickness, bw - thickness * 2, bh - thickness * 2, radius - thickness, bg);
    // Text, or placeholder when empty and unfocused, left-aligned and vertically centered.
    const char* shown = this->buffer;
    uint32_t textColor = TEXTINPUT_COLOR_TEXT;
    bool showingPlaceholder = (this->length == 0 && !this->focused && this->placeholder != NULL);
    if (showingPlaceholder) {
        shown = this->placeholder;
        textColor = TEXTINPUT_COLOR_PLACEHOLDER;
    }
    int charWidth = ttf_font_char_advance(scale);
    int charHeight = 8 * scale;
    int textY = by + (bh - charHeight) / 2;
    int shownLen = strlen(shown);
    // See docs/DOCS.md ("mods/core/wingman/widgets/textinput.cpp" section)
    // for why this is a trailing window, not the whole buffer.
    int textAreaWidth = bw - thickness * 4;
    int maxVisibleChars = textAreaWidth / charWidth;
    if (maxVisibleChars < 1) maxVisibleChars = 1;
    // Typed content scrolls (trailing window, keeps the caret in view);
    // the placeholder has no caret to chase, so it's just truncated to fit.
    int startIndex = 0;
    int visibleLen;
    if (showingPlaceholder) {
        visibleLen = (shownLen < maxVisibleChars) ? shownLen : maxVisibleChars;
    } else {
        startIndex = (shownLen > maxVisibleChars) ? (shownLen - maxVisibleChars) : 0;
        visibleLen = shownLen - startIndex;
    }
    for (int i = 0; i < visibleLen; i++) {
        drawChar(surface, bx + thickness * 2 + i * charWidth, textY, shown[startIndex + i], textColor, scale);
    }
    // Caret: a thin bar right after the last visible character, only while focused.
    if (this->focused) {
        int caretX = bx + thickness * 2 + visibleLen * charWidth;
        for (int y = thickness * 2; y < bh - thickness * 2; y++) {
            surface->putPixelUnsafe(caretX, by + y, TEXTINPUT_COLOR_CARET);
        }
    }
}

bool TextInput::onKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    (void)shift;
    (void)meta;
    (void)scancode;
    if (!this->focused) return false;
    if (key == '\b') {
        if (this->length > 0) {
            this->buffer[--this->length] = '\0';
            return true;
        }
        return false;
    }
    // Enter/tab/arrow-and-meta keys (KeyboardHandler reports arrows as
    // negative sentinel values) aren't handled here -- the caller decides
    // what they mean (submit, move focus, etc). No mid-string cursor
    // movement in this first version; typing always appends at the end.
    if (key == '\n' || key == '\t' || key < 0) return false;
    if (this->length < this->maxLength) {
        this->buffer[this->length++] = key;
        this->buffer[this->length] = '\0';
        return true;
    }
    return false;
}
