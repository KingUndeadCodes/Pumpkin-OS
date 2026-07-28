#include "./calculator.h"
#include "../../headers/shapes.h"

#define COLOR_W 0xFFFFFFFF
#define COLOR_BG 0xFF403a39
#define COLOR_TITLEBAR 0xFF2d2928
#define COLOR_DIVIDER 0xFF55504f

#define CALC_COLOR_DIGIT   0xFF605a59
#define CALC_COLOR_EQUALS  0xFF34c759
#define CALC_COLOR_CLEAR   0xFFFF0000
#define CALC_COLOR_DISPLAY_BG     0xFF1a1615
#define CALC_COLOR_DISPLAY_BORDER 0xFFFFFFFF

#define CALC_BUTTON_SIZE 50
#define CALC_GRID_GAP 8
#define CALC_GRID_COLS 4
#define CALC_GRID_ROWS 5
#define CALC_DISPLAY_HEIGHT 50
#define CALC_TITLEBAR_HEIGHT 40

struct CalcKey {
    const char* label;
    int row;
    int col;
};

// See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section) for the grid layout this implies.
static const CalcKey CALC_KEYS[CALCULATOR_KEY_COUNT] = {
    {"C", 0, 0},                         {"/", 0, 3},
    {"7", 1, 0}, {"8", 1, 1}, {"9", 1, 2}, {"*", 1, 3},
    {"4", 2, 0}, {"5", 2, 1}, {"6", 2, 2}, {"-", 2, 3},
    {"1", 3, 0}, {"2", 3, 1}, {"3", 3, 2}, {"+", 3, 3},
    {"0", 4, 0},              {".", 4, 2}, {"=", 4, 3},
};

// See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section) for why this exists locally instead of a shared atof().
static double calc_parse_double(const char* s) {
    int i = 0;
    bool negative = (s[0] == '-');
    if (negative) i++;
    double result = 0.0;
    while (s[i] >= '0' && s[i] <= '9') { result = result * 10.0 + (double)(s[i] - '0'); i++; }
    if (s[i] == '.') {
        i++;
        double frac = 0.1;
        while (s[i] >= '0' && s[i] <= '9') { result += (double)(s[i] - '0') * frac; frac *= 0.1; i++; }
    }
    return negative ? -result : result;
}

// See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section) for why this exists locally instead of sprintf's %f.
static void calc_format_number(double value, char* out, size_t outSize) {
    if (outSize == 0) return;
    bool negative = value < 0.0;
    if (negative) value = -value;
    // See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section) for why this is `long`, not `long long`.
    long intPart = (long)value;
    double frac = value - (double)intPart;

    char intBuf[24];
    int intLen = 0;
    if (intPart == 0) {
        intBuf[intLen++] = '0';
    } else {
        long tmp = intPart;
        while (tmp > 0 && intLen < (int)sizeof(intBuf)) {
            intBuf[intLen++] = (char)('0' + (tmp % 10));
            tmp /= 10;
        }
    }

    char fracBuf[8];
    int fracLen = 0;
    double f = frac;
    for (int i = 0; i < 6; i++) {
        f *= 10.0;
        int d = (int)f;
        if (d > 9) d = 9;
        fracBuf[fracLen++] = (char)('0' + d);
        f -= d;
    }
    while (fracLen > 0 && fracBuf[fracLen - 1] == '0') fracLen--;

    size_t pos = 0;
    if (negative && pos < outSize - 1) out[pos++] = '-';
    for (int i = intLen - 1; i >= 0 && pos < outSize - 1; i--) out[pos++] = intBuf[i];
    if (fracLen > 0 && pos < outSize - 1) {
        out[pos++] = '.';
        for (int i = 0; i < fracLen && pos < outSize - 1; i++) out[pos++] = fracBuf[i];
    }
    out[pos] = '\0';
}

void Calculator::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void Calculator::utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale) {
    const FontAtlas* atlas = ttf_font_get_atlas(scale);
    if (atlas == nullptr) {
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
        return;
    }
    ttf_blit_glyph(atlas, c, (int)x, (int)y, color,
        [&](int px, int py, uint8_t alpha, uint32_t fg) {
            color_t under = this->window->surface->getPixel(px, py);
            utility_draw_pixel((unsigned)px, (unsigned)py, ttf_blend_over(under, fg, alpha));
        });
};

void Calculator::utility_draw_string(unsigned x, unsigned y, const char* str, unsigned color, unsigned scale) {
    int charAdvance = ttf_font_char_advance(scale);
    for (unsigned i = 0; str[i] != '\0'; i++) {
        utility_draw_char(x + i * charAdvance, y, str[i], color, scale);
    }
};

Calculator::Calculator(WindowManager* wm) {
    this->wm = wm;
    this->padding = 14;
    this->thickness = 3;

    int gridWidth = CALC_GRID_COLS * CALC_BUTTON_SIZE + (CALC_GRID_COLS - 1) * CALC_GRID_GAP;
    int gridHeight = CALC_GRID_ROWS * CALC_BUTTON_SIZE + (CALC_GRID_ROWS - 1) * CALC_GRID_GAP;
    this->width = gridWidth + this->padding * 2;
    this->height = CALC_TITLEBAR_HEIGHT + this->padding + CALC_DISPLAY_HEIGHT + CALC_GRID_GAP + gridHeight + this->padding;
    this->offsetX = 680;
    this->offsetY = 340;

    this->display[0] = '0';
    this->display[1] = '\0';
    this->displayLen = 1;
    this->storedValue = 0.0;
    this->pendingOp = 0;
    this->waitingForOperand = false;
    this->errorState = false;

    int gridTop = CALC_TITLEBAR_HEIGHT + this->padding + CALC_DISPLAY_HEIGHT + CALC_GRID_GAP;
    for (int i = 0; i < CALCULATOR_KEY_COUNT; i++) {
        uint32_t color = CALC_COLOR_DIGIT;
        if (CALC_KEYS[i].label[0] == '=') color = CALC_COLOR_EQUALS;
        else if (CALC_KEYS[i].label[0] == 'C') color = CALC_COLOR_CLEAR;
        this->keys[i] = new Button(CALC_KEYS[i].label, color, NULL, NULL);
        this->keys[i]->x = this->padding + CALC_KEYS[i].col * (CALC_BUTTON_SIZE + CALC_GRID_GAP);
        this->keys[i]->y = gridTop + CALC_KEYS[i].row * (CALC_BUTTON_SIZE + CALC_GRID_GAP);
        this->keys[i]->width = CALC_BUTTON_SIZE;
        this->keys[i]->height = CALC_BUTTON_SIZE;
    }

    this->window = new Window(width, height, offsetX, offsetY, "Calculator");
    this->window->setMouseDelegate(this);
    this->redraw();
    this->ref = WINGMAN_INVALID_WINDOW;
    if (this->wm != NULL) {
        this->ref = this->wm->add(this->window);
        if (this->ref != WINGMAN_INVALID_WINDOW) this->wm->focus(this->ref);
    }
};

Calculator::~Calculator() {
    for (int i = 0; i < CALCULATOR_KEY_COUNT; i++) delete this->keys[i];
    if (this->window != NULL) {
        delete this->window;
        this->window = NULL;
    }
};

void Calculator::redraw(void) {
    this->draw_border();
    this->draw_background();
    this->draw_title();
    this->draw_display();
    this->draw_keys();
};

void Calculator::draw_border(void) {
    for (int t = 0; t < thickness; t++) {
        for (int x = 0; x < width; x++) utility_draw_pixel(x, t, COLOR_W);
        for (int x = 0; x < width; x++) utility_draw_pixel(x, height - 1 - t, COLOR_W);
        for (int y = 0; y < height; y++) utility_draw_pixel(t, y, COLOR_W);
        for (int y = 0; y < height; y++) utility_draw_pixel(width - 1 - t, y, COLOR_W);
    }
};

void Calculator::draw_background(void) {
    for (int y = thickness; y < height - thickness; y++) {
        for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, y, COLOR_BG);
    }
    for (int y = thickness; y < CALC_TITLEBAR_HEIGHT; y++) {
        for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, y, COLOR_TITLEBAR);
    }
    for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, CALC_TITLEBAR_HEIGHT, COLOR_DIVIDER);
};

void Calculator::draw_title(void) {
    utility_draw_string(padding, 12, "Calculator", COLOR_W, 2);
};

void Calculator::draw_display(void) {
    constexpr int scale = 2;
    int dx = padding, dy = CALC_TITLEBAR_HEIGHT + padding;
    int dw = width - padding * 2, dh = CALC_DISPLAY_HEIGHT;
    int radius = dh / 6;
    // See docs/DOCS.md ("mods/core/wingman/headers/shapes.h").
    draw_rounded_rect_fill(window->surface, dx, dy, dw, dh, radius, CALC_COLOR_DISPLAY_BORDER);
    draw_rounded_rect_fill(window->surface, dx + thickness, dy + thickness, dw - thickness * 2, dh - thickness * 2, radius - thickness, CALC_COLOR_DISPLAY_BG);

    const char* shown = this->errorState ? "Error" : this->display;
    int shownLen = strlen(shown);
    int charWidth = ttf_font_char_advance(scale);
    int charHeight = 8 * scale;
    int textX = dx + dw - thickness * 2 - shownLen * charWidth;
    if (textX < dx + thickness * 2) textX = dx + thickness * 2;
    int textY = dy + (dh - charHeight) / 2;
    for (int i = 0; i < shownLen; i++) {
        utility_draw_char(textX + i * charWidth, textY, shown[i], COLOR_W, scale);
    }
};

void Calculator::draw_keys(void) {
    for (int i = 0; i < CALCULATOR_KEY_COUNT; i++) {
        this->keys[i]->draw(this->window->surface, this->thickness);
    }
};

void Calculator::applyPendingOp(double operand) {
    double result = 0.0;
    switch (this->pendingOp) {
        case '+': result = this->storedValue + operand; break;
        case '-': result = this->storedValue - operand; break;
        case '*': result = this->storedValue * operand; break;
        case '/':
            if (operand == 0.0) { this->errorState = true; return; }
            result = this->storedValue / operand;
            break;
        default: result = operand; break;
    }
    this->storedValue = result;
    this->setDisplayNumber(result);
};

void Calculator::setDisplayNumber(double value) {
    char buf[CALCULATOR_DISPLAY_MAX];
    calc_format_number(value, buf, sizeof(buf));
    size_t len = strlen(buf);
    if (len >= (size_t)CALCULATOR_DISPLAY_MAX) len = CALCULATOR_DISPLAY_MAX - 1;
    memcpy(this->display, buf, len);
    this->display[len] = '\0';
    this->displayLen = (int)len;
};

void Calculator::handleKey(char label) {
    if (label == 'C') {
        this->display[0] = '0';
        this->display[1] = '\0';
        this->displayLen = 1;
        this->storedValue = 0.0;
        this->pendingOp = 0;
        this->waitingForOperand = false;
        this->errorState = false;
        return;
    }
    // See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section) for why only C recovers from an error.
    if (this->errorState) return;

    if (label >= '0' && label <= '9') {
        if (this->waitingForOperand) { this->displayLen = 0; this->waitingForOperand = false; }
        if (this->displayLen == 1 && this->display[0] == '0') this->displayLen = 0;
        if (this->displayLen < CALCULATOR_DISPLAY_MAX - 1) {
            this->display[this->displayLen++] = label;
            this->display[this->displayLen] = '\0';
        }
        return;
    }

    if (label == '.') {
        if (this->waitingForOperand) {
            this->display[0] = '0';
            this->display[1] = '\0';
            this->displayLen = 1;
            this->waitingForOperand = false;
        }
        bool hasDot = false;
        for (int i = 0; i < this->displayLen; i++) if (this->display[i] == '.') hasDot = true;
        if (!hasDot && this->displayLen < CALCULATOR_DISPLAY_MAX - 1) {
            this->display[this->displayLen++] = '.';
            this->display[this->displayLen] = '\0';
        }
        return;
    }

    double operand = calc_parse_double(this->display);
    if (label == '=') {
        if (this->pendingOp != 0) this->applyPendingOp(operand);
        this->pendingOp = 0;
        this->waitingForOperand = true;
        return;
    }

    // +, -, *, /
    if (this->pendingOp != 0 && !this->waitingForOperand) {
        this->applyPendingOp(operand);
    } else {
        this->storedValue = operand;
    }
    this->pendingOp = label;
    this->waitingForOperand = true;
};

bool Calculator::onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    (void)dx;
    (void)dy;
    (void)buttons;
    bool hoveringKey = false;
    for (int i = 0; i < CALCULATOR_KEY_COUNT; i++) {
        if (this->keys[i]->contains(x, y)) { hoveringKey = true; break; }
    }
    set_cursor_id(hoveringKey ? 2 : 0);

    if (!(pressedEdge & 1)) return false;

    for (int i = 0; i < CALCULATOR_KEY_COUNT; i++) {
        if (this->keys[i]->contains(x, y)) {
            this->handleKey(CALC_KEYS[i].label[0]);
            this->redraw();
            return true;
        }
    }
    return false;
};
