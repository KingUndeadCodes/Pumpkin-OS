#include "./widgetdemo.h"

#define COLOR_W 0xFFFFFFFF
#define COLOR_BG 0xFF403a39
#define COLOR_TITLEBAR 0xFF2d2928
#define COLOR_DIVIDER 0xFF55504f

void WidgetDemo::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void WidgetDemo::utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale) {
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

void WidgetDemo::utility_draw_string(unsigned x, unsigned y, const char* str, unsigned color, unsigned scale) {
    for (unsigned i = 0; str[i] != '\0'; i++) {
        utility_draw_char(x + i * (8 * scale), y, str[i], color, scale);
    }
};

WidgetDemo::WidgetDemo(WindowManager* wm) {
    this->wm = wm;
    this->width = 380;
    this->height = 250;
    this->offsetX = 500;
    this->offsetY = 120;
    this->padding = 10;
    this->thickness = 3;

    this->button = new Button("Click Me", 0xFF605a59, [](void* userdata) {
        (void)userdata;
        serial_write_string("[WidgetDemo] Button clicked\n");
    }, nullptr);
    this->button->x = 170;
    this->button->y = 55;

    this->textInput = new TextInput(64, "Type here...");
    this->textInput->x = 170;
    this->textInput->y = 105;
    this->textInput->width = 190;
    this->textInput->height = 32;

    this->checkbox = new Checkbox(CheckboxStyleBox, false, [](bool checked, void* userdata) {
        (void)userdata;
        serial_write_string(checked ? "[WidgetDemo] Checkbox checked\n" : "[WidgetDemo] Checkbox unchecked\n");
    }, nullptr);
    this->checkbox->x = 170;
    this->checkbox->y = 153;

    this->toggle = new Checkbox(CheckboxStyleToggle, false, [](bool checked, void* userdata) {
        (void)userdata;
        serial_write_string(checked ? "[WidgetDemo] Toggle on\n" : "[WidgetDemo] Toggle off\n");
    }, nullptr);
    this->toggle->x = 170;
    this->toggle->y = 193;

    this->window = new Window(width, height, offsetX, offsetY, "Widget Demo");
    this->window->setKeyboardDelegate(this);
    this->window->setMouseDelegate(this);
    this->redraw();
    this->ref = WINGMAN_INVALID_WINDOW;
    if (this->wm != NULL) {
        this->ref = this->wm->add(this->window);
        if (this->ref != WINGMAN_INVALID_WINDOW) this->wm->focus(this->ref);
    }
};

WidgetDemo::~WidgetDemo() {
    delete this->button;
    delete this->textInput;
    delete this->checkbox;
    delete this->toggle;
    if (this->window != NULL) {
        delete this->window;
        this->window = NULL;
    }
};

void WidgetDemo::redraw(void) {
    this->draw_border();
    this->draw_background();
    this->draw_title();
    this->draw_widgets();
};

void WidgetDemo::draw_border(void) {
    for (int t = 0; t < thickness; t++) {
        for (int x = 0; x < width; x++) utility_draw_pixel(x, t, COLOR_W);
        for (int x = 0; x < width; x++) utility_draw_pixel(x, height - 1 - t, COLOR_W);
        for (int y = 0; y < height; y++) utility_draw_pixel(t, y, COLOR_W);
        for (int y = 0; y < height; y++) utility_draw_pixel(width - 1 - t, y, COLOR_W);
    }
};

void WidgetDemo::draw_background(void) {
    for (int y = thickness; y < height - thickness; y++) {
        for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, y, COLOR_BG);
    }
    int titleBarHeight = 40;
    for (int y = thickness; y < titleBarHeight; y++) {
        for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, y, COLOR_TITLEBAR);
    }
    for (int x = thickness; x < width - thickness; x++) utility_draw_pixel(x, titleBarHeight, COLOR_DIVIDER);
};

void WidgetDemo::draw_title(void) {
    utility_draw_string(padding, 12, "Widget Demo", COLOR_W, 2);
};

void WidgetDemo::draw_widgets(void) {
    utility_draw_string(padding, 67, "Button:", COLOR_W, 2);
    utility_draw_string(padding, 113, "Text:", COLOR_W, 2);
    utility_draw_string(padding, 159, "Checkbox:", COLOR_W, 2);
    utility_draw_string(padding, 199, "Toggle:", COLOR_W, 2);
    this->button->draw(this->window->surface, this->thickness);
    this->textInput->draw(this->window->surface, this->thickness);
    this->checkbox->draw(this->window->surface, this->thickness);
    this->toggle->draw(this->window->surface, this->thickness);
};

bool WidgetDemo::onKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    if (this->textInput->onKeyboard(key, shift, meta, scancode)) {
        this->textInput->draw(this->window->surface, this->thickness);
        return true;
    }
    return false;
};

bool WidgetDemo::onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    (void)dx;
    (void)dy;
    (void)buttons;
    if (!(pressedEdge & 1)) return false;

    // Click inside the field focuses it; click anywhere else in the
    // window unfocuses it, same as any normal text field.
    this->textInput->focused = this->textInput->contains(x, y);

    if (this->button->contains(x, y)) {
        if (this->button->onClick != NULL) this->button->onClick(this->button->userdata);
    } else if (this->checkbox->contains(x, y)) {
        this->checkbox->click(x, y);
    } else if (this->toggle->contains(x, y)) {
        this->toggle->click(x, y);
    }

    this->draw_widgets();
    return true;
};
