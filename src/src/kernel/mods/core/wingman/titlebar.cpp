#include <string.h>
#include <stdint.h>
#include "./headers/titlebar.h"
#include "./headers/shapes.h"
#include "./headers/draw.h"
#include "../fontman/fontman.h"

// Consider moving all of this into Window because it's specific to window.cpp

#define COLOR_TITLEBAR         0xFF2d2928
#define COLOR_DIVIDER          0xFF55504f
#define COLOR_W                0xFFFFFFFF

#define CLOSE_BUTTON_SIZE      20
#define CLOSE_BUTTON_MARGIN    10
#define CLOSE_BUTTON_RADIUS    7
#define CLOSE_BUTTON_FILL      0xFFFF5F57
#define CLOSE_BUTTON_BORDER    0xFFE0443E
#define MINIMIZE_BUTTON_FILL   0xFFFFBD2E
#define MINIMIZE_BUTTON_BORDER 0xFFDEA123
#define MAXIMIZE_BUTTON_FILL   0xFF28C840
#define MAXIMIZE_BUTTON_BORDER 0xFF1AAB29
#define CONTENT_GAP            8
// See docs/DOCS.md ("mods/core/wingman/headers/titlebar.h / mods/core/wingman/window.h -- TitleBar owned by Window" section).
#define ICON_ADVANCE           48

TitleBar::TitleBar() {}

void TitleBar::configure(
    int height,
    int thickness,
    bool hasCloseButton,
    int contentY,                      
    bool hasIcon,
    int iconId,
    int iconScale,
    int textScale
) {
    this->_height = height;
    this->_thickness = thickness;
    this->_hasCloseButton = hasCloseButton;
    this->_contentY = contentY;
    this->_hasIcon = hasIcon;
    this->_iconId = iconId;
    this->_iconScale = iconScale;
    this->_textScale = textScale;
}

int TitleBar::height() const { return this->_height; }
bool TitleBar::hasCloseButton() const { return this->_hasCloseButton; }

bool TitleBar::closeButtonContains(int x, int y) const {
    if (!this->_hasCloseButton) return false;
    int x0 = this->_thickness + CLOSE_BUTTON_MARGIN;
    int y0 = (this->_height - CLOSE_BUTTON_SIZE) / 2;
    return x >= x0 && x < x0 + CLOSE_BUTTON_SIZE && y >= y0 && y < y0 + CLOSE_BUTTON_SIZE;
}

int TitleBar::closeButtonZoneWidth() const {
    if (!this->_hasCloseButton) return 0;
    return this->_thickness + CLOSE_BUTTON_MARGIN + 3 * CLOSE_BUTTON_SIZE;
}

// See docs/DOCS.md ("mods/core/wingman/headers/titlebar.h / mods/core/wingman/window.h -- TitleBar owned by Window" section).
void draw_title_bar(Surface* surface, int windowWidth, const TitleBar& titleBar, const char* title) {
    if (titleBar._height <= 0) return;
    int t = titleBar._thickness;
    for (int y = t; y < titleBar._height; y++) {
        for (int x = t; x < windowWidth - t; x++) {
            surface_draw_pixel(surface, x, y, COLOR_TITLEBAR);
        }
    }
    for (int x = t; x < windowWidth - t; x++) surface_draw_pixel(surface, x, titleBar._height, COLOR_DIVIDER);
    int contentX = t + CLOSE_BUTTON_MARGIN;
    if (titleBar._hasCloseButton) {
        int y0 = (titleBar._height - CLOSE_BUTTON_SIZE) / 2;
        int cy = y0 + CLOSE_BUTTON_SIZE / 2;
        int closeCx = contentX + CLOSE_BUTTON_SIZE / 2;
        draw_circle_fill(surface, closeCx, cy, CLOSE_BUTTON_RADIUS + 1, CLOSE_BUTTON_BORDER);
        draw_circle_fill(surface, closeCx, cy, CLOSE_BUTTON_RADIUS, CLOSE_BUTTON_FILL);
        int minimizeCx = contentX + CLOSE_BUTTON_SIZE + CLOSE_BUTTON_SIZE / 2;
        draw_circle_fill(surface, minimizeCx, cy, CLOSE_BUTTON_RADIUS + 1, MINIMIZE_BUTTON_BORDER);
        draw_circle_fill(surface, minimizeCx, cy, CLOSE_BUTTON_RADIUS, MINIMIZE_BUTTON_FILL);
        int maximizeCx = contentX + 2 * CLOSE_BUTTON_SIZE + CLOSE_BUTTON_SIZE / 2;
        draw_circle_fill(surface, maximizeCx, cy, CLOSE_BUTTON_RADIUS + 1, MAXIMIZE_BUTTON_BORDER);
        draw_circle_fill(surface, maximizeCx, cy, CLOSE_BUTTON_RADIUS, MAXIMIZE_BUTTON_FILL);
        contentX += 3 * CLOSE_BUTTON_SIZE + CONTENT_GAP;
    }
    if (titleBar._hasIcon) {
        surface_draw_icon(surface, contentX, titleBar._contentY, titleBar._iconId, titleBar._iconScale);
        contentX += ICON_ADVANCE;
    }
    if (title == nullptr) return;
    int charAdvance = ttf_font_char_advance(titleBar._textScale);
    int availableWidth = (windowWidth - t) - contentX;
    // Clamp negative widths so the size_t division below can't wrap to a huge value.
    if (availableWidth < 0) availableWidth = 0;
    size_t maxChars = (charAdvance > 0) ? (size_t)(availableWidth / charAdvance) : 0;
    size_t len = strlen(title);
    if (len > maxChars) len = maxChars;
    for (size_t i = 0; i < len; i++) surface_draw_char(surface, contentX + (charAdvance * i), titleBar._contentY, title[i], COLOR_W, titleBar._textScale);
}
