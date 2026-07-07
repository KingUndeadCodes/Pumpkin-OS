#include "./message.h"

#define COLOR_W 0xFFFFFFFF
#define COLOR_BG 0xFF403a39
#define COLOR_TITLEBAR 0xFF2d2928
#define COLOR_DIVIDER 0xFF55504f

// See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
static inline uint32_t shade(uint32_t color, int delta) {
    int r = (int)COLOR_R(color) + delta;
    int g = (int)COLOR_G(color) + delta;
    int b = (int)COLOR_B(color) + delta;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

void MessageBox::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void MessageBox::utility_draw_char(unsigned int x, unsigned int y, char c, unsigned int color, unsigned int scale = 4U) {
    for (unsigned i = 0; i < 8; i++) {
        for (unsigned j = 0; j < 8; j++) {
            if (Font[(int)c][i] & (1 << j)) {
                for (unsigned k = 0; k < scale; k++) {
                    for (unsigned l = 0; l < scale; l++) {
                        utility_draw_pixel(x + j * scale + l, y + i * scale + k, color);
                    }
                }
            }
        }
    }
};

void MessageBox::utility_draw_icon(unsigned x, unsigned y, unsigned icon, float scale = 2.0f) {
    int size = (int)(32.0f * scale);
    for (int py = 0; py < size; py++) {
        int srcY = (int)(py / scale);
        if (srcY > 31) srcY = 31;
        for (int px = 0; px < size; px++) {
            int srcX = (int)(px / scale);
            if (srcX > 31) srcX = 31;
            uint8_t color = Icons[icon][srcY][srcX];
            if (color == 0x00) continue;
            if (color == 0x10) utility_draw_pixel(x + px, y + py, 0x0);
            utility_draw_pixel(x + px, y + py, vgaPaletteConvertorRGB32[color]);
        }
    }
};

MessageBox::MessageBox(WindowManager* wm, enum MessageBoxType dialogBoxType, const char* message, int icon = -1) {
    this->wm = wm;
    this->dialogBoxType = dialogBoxType;
    this->icon = icon;
    this->width = 500;
    this->height = 200;
    this->offsetX = 100;
    this->offsetY = 100;
    this->padding = 10;
    this->thickness = 3; // Border thickness
    this->buttons = NULL;
    this->buttonCount = 0;
    this->buttonCapacity = 0;
    this->buttonRowHeight = 40;
    this->buttonRowX = 2 * this->padding;
    this->buttonRowWidth = this->width - 2 * this->buttonRowX;
    // See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
    this->buttonSectionDividerY = this->height - this->thickness - this->padding - this->buttonRowHeight - (this->padding / 2);
    int sectionTop = this->buttonSectionDividerY + 1;
    int sectionBottom = this->height - this->thickness;
    this->buttonRowY = sectionTop + ((sectionBottom - sectionTop) - this->buttonRowHeight) / 2;
    if (message != NULL) {
        size_t messageLength = strlen(message);
        this->message = (char*)malloc(messageLength + 1);
        if (this->message != NULL) { strcpy(this->message, message); }
    } else {
        this->message = NULL;
    }
    this->window = new Window(width, height, offsetX, offsetY, "Message");
    this->window->setKeyboardDelegate(this);
    this->window->setMouseDelegate(this);
    this->redraw(0b11111100);
    this->ref = WINGMAN_INVALID_WINDOW;
    if (this->wm != NULL) {
        this->ref = this->wm->add(this->window);
        if (this->ref != WINGMAN_INVALID_WINDOW) this->wm->focus(this->ref);
    }
};

// See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
void MessageBox::layoutButtons(void) {
    if (this->buttonCount <= 0) return;
    int gap = this->padding;
    int totalGap = gap * (this->buttonCount - 1);
    int buttonWidth = (this->buttonRowWidth - totalGap) / this->buttonCount;
    int x = this->buttonRowX;
    for (int i = 0; i < this->buttonCount; i++) {
        Button* button = this->buttons[i];
        button->x = x;
        button->y = this->buttonRowY;
        button->width = buttonWidth;
        button->height = this->buttonRowHeight;
        x += buttonWidth + gap;
    }
};

int MessageBox::addButton(const char* label, uint32_t color, ButtonCallback onClick = NULL, void* userdata = NULL) {
    if (this->buttonCount >= MESSAGEBOX_MAX_BUTTONS) return -1;
    Button* button = new Button(label, color, onClick, userdata);
    if (button == NULL) return -1;
    Button** grown = (Button**)realloc(this->buttons, sizeof(Button*) * (this->buttonCount + 1));
    if (grown == NULL) {
        delete button;
        return -1;
    }
    this->buttons = grown;
    this->buttons[this->buttonCount] = button;
    this->buttonCount++;
    this->layoutButtons();
    this->draw_buttons();
    return this->buttonCount - 1;
};

void MessageBox::redraw(uint8_t description = 0b00111000) {
    // Bit 8 is the (unused)
    // Bit 7 is the border
    // Bit 6 is the background
    // Bit 5 is the title
    // Bit 4 is the body
    // Bit 3 is the options
    if ((description >> 6) & 1) this->draw_border();
    if ((description >> 5) & 1) this->draw_background();
    if ((description >> 4) & 1) this->draw_title();
    if ((description >> 3) & 1) this->draw_body();
    if ((description >> 2) & 1) this->draw_buttons();
};

void MessageBox::draw_border(void) {
    int k = 0;
    int x0 = k * padding;
    int y0 = k * padding;
    int w = width - 2 * k * padding;
    int h = height - 2 * k * padding;
    for (int t = 0; t < thickness; t++) {
        for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + t, COLOR_W);
        for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + h - 1 - t, COLOR_W);
        for (int y = 0; y < h; y++) utility_draw_pixel(x0 + t, y0 + y, COLOR_W);
        for (int y = 0; y < h; y++) utility_draw_pixel(x0 + w - 1 - t, y0 + y, COLOR_W);
    }
};

void MessageBox::draw_background(void) {
    int frames = 1;
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    for (int y = 0; y < innerH; y++) {
        for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, innerY + y, COLOR_BG);
    }
    // See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
    int titleBarHeight = 64;
    for (int y = 0; y < titleBarHeight; y++) {
        for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, innerY + y, COLOR_TITLEBAR);
    }
    for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, innerY + titleBarHeight, COLOR_DIVIDER);
};

void MessageBox::draw_title(void) {
    int iconType = 0;
    char* title = (char*)malloc(13);
    switch (this->dialogBoxType) {
        case DialogBoxInformational: { iconType = 0; strcpy(title, "Information"); break; }
        case DialogBoxError: { iconType = 1; strcpy(title, "Error"); break; }
        case DialogBoxWarning: { iconType = 2; strcpy(title, "Warning"); break; }
    };
    // See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
    if (this->icon >= 0 && this->icon < 12) iconType = this->icon;
    utility_draw_icon(8, 12, iconType, 1.5f);
    const char* titleString = (const char*)title;
    for (int i = 0; i < strlen(titleString); i++) {
        unsigned int xPos = 4 + 62 + (24 * i);
        char character = titleString[i];
        utility_draw_char(xPos, 24, character, COLOR_W, 3);
    }
    free(title);
};

void MessageBox::draw_body(void) {
    for (int i = 0; i < strlen(message); i++) {
        const int posX = 20 + (16 * (i % 30));
        const int posY = 80 + (16 * (int)(i / 30));
        utility_draw_char(posX, posY, message[i], COLOR_W, 2);
    }
    return;
};

void MessageBox::draw_buttons(void) {
    constexpr uint32_t scale = 2;
    // See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
    for (int y = 0; y < this->buttonRowHeight; y++) {
        for (int x = 0; x < this->buttonRowWidth; x++) {
            utility_draw_pixel(this->buttonRowX + x, this->buttonRowY + y, COLOR_BG);
        }
    }
    // Divider above the row, mirroring the one under the title bar.
    int dividerY = this->buttonSectionDividerY;
    for (int x = thickness; x < width - thickness; x++) {
        utility_draw_pixel(x, dividerY, COLOR_DIVIDER);
    }
    for (int b = 0; b < this->buttonCount; b++) {
        Button* button = this->buttons[b];
        int bx = button->x, by = button->y, bw = button->width, bh = button->height;
        uint32_t highlight = shade(button->color, 35);
        uint32_t shadow = shade(button->color, -35);
        // Fill, leaving room for the outer border.
        for (int y = thickness; y < bh - thickness; y++) {
            for (int x = thickness; x < bw - thickness; x++) utility_draw_pixel(bx + x, by + y, button->color);
        }
        // Outer border, matching the window's own.
        for (int t = 0; t < thickness; t++) {
            for (int x = 0; x < bw; x++) utility_draw_pixel(bx + x, by + t, COLOR_W);
            for (int x = 0; x < bw; x++) utility_draw_pixel(bx + x, by + bh - 1 - t, COLOR_W);
            for (int y = 0; y < bh; y++) utility_draw_pixel(bx + t, by + y, COLOR_W);
            for (int y = 0; y < bh; y++) utility_draw_pixel(bx + bw - 1 - t, by + y, COLOR_W);
        }
        // Inner bevel (lighter top/left, darker bottom/right) for a raised look.
        for (int x = thickness; x < bw - thickness; x++) utility_draw_pixel(bx + x, by + thickness, highlight);
        for (int x = thickness; x < bw - thickness; x++) utility_draw_pixel(bx + x, by + bh - thickness - 1, shadow);
        for (int y = thickness; y < bh - thickness; y++) utility_draw_pixel(bx + thickness, by + y, highlight);
        for (int y = thickness; y < bh - thickness; y++) utility_draw_pixel(bx + bw - thickness - 1, by + y, shadow);
        // Label, centered.
        int labelLen = strlen(button->message);
        int charWidth = 8 * scale;
        int charHeight = 8 * scale;
        int textWidth = labelLen * charWidth;
        int textX = bx + (bw - textWidth) / 2;
        int textY = by + (bh - charHeight) / 2;
        for (int i = 0; i < labelLen; i++) {
            utility_draw_char(textX + i * charWidth, textY, button->message[i], COLOR_W, scale);
        }
    }
};

// See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
void MessageBox::dismiss(void) {
    if (this->wm != NULL && this->ref != WINGMAN_INVALID_WINDOW) {
        this->wm->remove(this->ref);
    }
    this->window = NULL;
    delete this;
};

bool MessageBox::onKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    (void)shift;
    (void)meta;
    (void)scancode;
    if (key == '\n') {
        if (this->buttonCount > 0 && this->buttons[0]->onClick != NULL) {
            this->buttons[0]->onClick(this->buttons[0]->userdata);
        }
        this->dismiss();
        return true;
    }
    return false;
};

bool MessageBox::onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    (void)dx;
    (void)dy;
    (void)buttons;
    Button* hovered = NULL;
    for (int i = 0; i < this->buttonCount; i++) {
        Button* button = this->buttons[i];
        if (x >= button->x && x < button->x + button->width &&
            y >= button->y && y < button->y + button->height) {
            hovered = button;
            break;
        }
    }
    // See DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
    set_cursor_id(hovered != NULL ? 2 : 0);
    if (!(pressedEdge & 1) || hovered == NULL) return false;
    if (hovered->onClick != NULL) hovered->onClick(hovered->userdata);
    this->dismiss();
    return true;
};

MessageBox::~MessageBox() {
    free(this->message);
    if (this->buttons != NULL) {
        for (int i = 0; i < this->buttonCount; i++) delete this->buttons[i];
        free(this->buttons);
    }
    if (this->window != NULL) {
        delete this->window;
        this->window = NULL;
    }
    return;
};
