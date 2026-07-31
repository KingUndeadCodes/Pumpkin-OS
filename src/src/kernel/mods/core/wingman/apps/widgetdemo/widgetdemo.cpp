#include "./widgetdemo.h"

#define COLOR_W 0xFFFFFFFF
#define COLOR_BG 0xFF403a39

#define WIDGETDEMO_TITLEBAR_HEIGHT 40

WidgetDemo::WidgetDemo(WindowManager* wm) : WingmanApp(wm) {
    this->width = 380;
    this->height = 290;
    this->offsetX = 500;
    this->offsetY = 120;
    this->padding = 10;
    this->thickness = 3;
    this->button = new Button("Click Me", 0xFF605a59, [](void* userdata) {
        (void)userdata;
        serial_write_string("[WidgetDemo] Button clicked\n");
    }, nullptr, 170, 55);
    this->textInput = new TextInput(64, "Type here...", 170, 105, 190, 32);
    this->checkbox = new Checkbox(CheckboxStyleBox, false, [](bool checked, void* userdata) {
        (void)userdata;
        serial_write_string(checked ? "[WidgetDemo] Checkbox checked\n" : "[WidgetDemo] Checkbox unchecked\n");
    }, nullptr, 170, 153);
    this->toggle = new Checkbox(CheckboxStyleToggle, false, [](bool checked, void* userdata) {
        (void)userdata;
        serial_write_string(checked ? "[WidgetDemo] Toggle on\n" : "[WidgetDemo] Toggle off\n");
    }, nullptr, 170, 193);
    this->slider = new Slider(0.0f, 100.0f, 50.0f, [](float value, void* userdata) {
        (void)userdata;
        char msg[48];
        sprintf(msg, "[WidgetDemo] Slider value: %d\n", (int)value);
        serial_write_string(msg);
    }, nullptr, 170, 233);
    this->window = new Window(width, height, offsetX, offsetY, "Widget Demo");
    this->window->titleBar.configure(WIDGETDEMO_TITLEBAR_HEIGHT, this->thickness, /*hasCloseButton=*/true, /*contentY=*/12);
    this->redraw();
    this->registerWindow();
};

WidgetDemo::~WidgetDemo() {
    delete this->button;
    delete this->textInput;
    delete this->checkbox;
    delete this->toggle;
    delete this->slider;
};

void WidgetDemo::redraw(void) {
    this->draw_border();
    this->draw_background();
    this->draw_widgets();
};

void WidgetDemo::draw_border(void) {
    for (int t = 0; t < thickness; t++) {
        for (int x = 0; x < width; x++) surface_draw_pixel(this->window->surface, x, t, COLOR_W);
        for (int x = 0; x < width; x++) surface_draw_pixel(this->window->surface, x, height - 1 - t, COLOR_W);
        for (int y = 0; y < height; y++) surface_draw_pixel(this->window->surface, t, y, COLOR_W);
        for (int y = 0; y < height; y++) surface_draw_pixel(this->window->surface, width - 1 - t, y, COLOR_W);
    }
};

void WidgetDemo::draw_background(void) {
    for (int y = thickness; y < height - thickness; y++) {
        for (int x = thickness; x < width - thickness; x++) surface_draw_pixel(this->window->surface, x, y, COLOR_BG);
    }
};

void WidgetDemo::draw_widgets(void) {
    surface_draw_string(this->window->surface, padding, 67, "Button:", COLOR_W, 2);
    surface_draw_string(this->window->surface, padding, 113, "Text:", COLOR_W, 2);
    surface_draw_string(this->window->surface, padding, 159, "Checkbox:", COLOR_W, 2);
    surface_draw_string(this->window->surface, padding, 199, "Toggle:", COLOR_W, 2);
    surface_draw_string(this->window->surface, padding, 239, "Slider:", COLOR_W, 2);
    this->button->draw(this->window->surface, this->thickness);
    this->textInput->draw(this->window->surface, this->thickness);
    this->checkbox->draw(this->window->surface, this->thickness);
    this->toggle->draw(this->window->surface, this->thickness);
    this->slider->draw(this->window->surface, this->thickness);
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
    // Runs every move, not just clicks, so plain hovering (not just clicking) updates the cursor.
    bool hoveringClickable = this->button->contains(x, y) || this->checkbox->contains(x, y) || this->toggle->contains(x, y) || this->slider->contains(x, y);
    set_cursor_id(hoveringClickable ? 2 : 0);
    // The slider needs to track drags even while the button is held past its own bounds.
    bool sliderChanged = this->slider->onMouse(x, y, buttons, pressedEdge);
    if (!(pressedEdge & 1)) {
        if (sliderChanged) this->redraw();
        return sliderChanged;
    }
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
    // Full redraw, not just draw_widgets() -- the field labels have no background clear of
    // their own, so alpha-blended glyph edges would re-blend on top of themselves each click.
    this->redraw();
    return true;
};
