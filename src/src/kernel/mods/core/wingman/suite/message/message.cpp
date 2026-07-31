#include "./message.h"

#define COLOR_W 0xFFFFFFFF
#define COLOR_BG 0xFF403a39
#define COLOR_DIVIDER 0xFF55504f

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
    // Y of the divider line above the button row -- buttons are then centered in the space below it.
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
    int iconType = 0;
    const char* titleText = "Information";
    switch (this->dialogBoxType) {
        case DialogBoxInformational: { iconType = 0; titleText = "Information"; break; }
        case DialogBoxError: { iconType = 1; titleText = "Error"; break; }
        case DialogBoxWarning: { iconType = 2; titleText = "Warning"; break; }
    };
    if (this->icon >= 0 && this->icon < 12) iconType = this->icon;
    // contentY=18 matches Explorer's icon+text layout for the same 64px band.
    this->window->titleBar.configure(
        64,
        this->thickness,
        true, /*hasCloseButton */ 
        18, /*contentY */
        /*hasIcon=*/true, /*iconId=*/iconType, /*iconScale=*/1, /*textScale=*/3);
    this->window->setTitle(titleText);
    this->window->setOnCloseRequested(&MessageBox::closeTrampoline, this);
    this->window->setKeyboardDelegate(this);
    this->window->setMouseDelegate(this);
    this->redraw(0b11101100);
    this->ref = WINGMAN_INVALID_WINDOW;
    if (this->wm != NULL) {
        this->ref = this->wm->add(this->window);
        if (this->ref != WINGMAN_INVALID_WINDOW) this->wm->focus(this->ref);
    }
};

// Splits buttonRowWidth evenly across however many buttons exist, re-run whenever the count changes.
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
    // Bit 4 is the body
    // Bit 3 is the options
    // No title bit -- WindowManager::composite() draws the band/icon/title every pass now.
    if ((description >> 6) & 1) this->draw_border();
    if ((description >> 5) & 1) this->draw_background();
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
        for (int x = 0; x < w; x++) surface_draw_pixel(this->window->surface, x0 + x, y0 + t, COLOR_W);
        for (int x = 0; x < w; x++) surface_draw_pixel(this->window->surface, x0 + x, y0 + h - 1 - t, COLOR_W);
        for (int y = 0; y < h; y++) surface_draw_pixel(this->window->surface, x0 + t, y0 + y, COLOR_W);
        for (int y = 0; y < h; y++) surface_draw_pixel(this->window->surface, x0 + w - 1 - t, y0 + y, COLOR_W);
    }
};

void MessageBox::draw_background(void) {
    int frames = 1;
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    for (int y = 0; y < innerH; y++) {
        for (int x = 0; x < innerW; x++) surface_draw_pixel(this->window->surface, innerX + x, innerY + y, COLOR_BG);
    }
};

// charsPerLine is derived from the box's real text-area width and the font's real advance,
// not a hardcoded count -- a fixed guess drifts out of sync whenever either one changes.
void MessageBox::draw_body(void) {
    int charAdvance = ttf_font_char_advance(2);
    int margin = 2 * this->padding;
    int textAreaWidth = this->width - 2 * margin;
    int charsPerLine = textAreaWidth / charAdvance;
    if (charsPerLine < 1) charsPerLine = 1;

    int len = strlen(this->message);
    int row = 0;
    int lineStart = 0;
    while (lineStart < len) {
        int remaining = len - lineStart;
        int budget = (remaining < charsPerLine) ? remaining : charsPerLine;
        int lineLen = budget;
        bool skipBreakChar = false;
        // A literal newline forces a break wherever it falls, even short of budget.
        for (int i = 0; i < budget; i++) {
            if (this->message[lineStart + i] == '\n') { lineLen = i; skipBreakChar = true; break; }
        }
        // Otherwise, if the line doesn't fit as-is, back up to the last
        // space within budget so words don't get split mid-word.
        if (lineLen == budget && remaining > charsPerLine) {
            for (int i = budget; i > 0; i--) {
                if (this->message[lineStart + i - 1] == ' ') { lineLen = i - 1; skipBreakChar = true; break; }
            }
        }
        for (int col = 0; col < lineLen; col++) {
            surface_draw_char(this->window->surface, margin + charAdvance * col, 80 + 16 * row, this->message[lineStart + col], COLOR_W, 2);
        }
        lineStart += lineLen;
        if (skipBreakChar && lineStart < len) lineStart++; // skip the space/newline we broke on
        row++;
    }
};

void MessageBox::draw_buttons(void) {
    // Clear the row first -- addButton() can call this again after the layout shifted.
    for (int y = 0; y < this->buttonRowHeight; y++) {
        for (int x = 0; x < this->buttonRowWidth; x++) {
            surface_draw_pixel(this->window->surface, this->buttonRowX + x, this->buttonRowY + y, COLOR_BG);
        }
    }
    // Divider above the row, mirroring the one under the title bar.
    int dividerY = this->buttonSectionDividerY;
    for (int x = thickness; x < width - thickness; x++) {
        surface_draw_pixel(this->window->surface, x, dividerY, COLOR_DIVIDER);
    }
    for (int b = 0; b < this->buttonCount; b++) {
        this->buttons[b]->draw(this->window->surface, this->thickness);
    }
};

// Removes the window from the WindowManager, then frees this instance -- callers must not touch `this` after.
void MessageBox::dismiss(void) {
    if (this->wm != NULL && this->ref != WINGMAN_INVALID_WINDOW) {
        this->wm->remove(this->ref);
    }
    this->window = NULL;
    delete this;
};

// Registered with Window::setOnCloseRequested() so the title bar's close button reaches dismiss().
void MessageBox::closeTrampoline(void* userdata) {
    ((MessageBox*)userdata)->dismiss();
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
        if (this->buttons[i]->contains(x, y)) {
            hovered = this->buttons[i];
            break;
        }
    }
    // Runs every move, not just clicks, so plain hovering (not just clicking) updates the cursor.
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
