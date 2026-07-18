#pragma once

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../headers/manager.h"
#include "../../headers/widgets/button.h"
#include "../../headers/widgets/textinput.h"
#include "../../headers/widgets/checkbox.h"
#include "../../headers/cursor.h"
#include "../../../../dev/serial/serial.h"
#include "../../../../dev/vbe/font.h"
#include "../../../fontman/fontman.h"

// See docs/DOCS.md ("mods/core/wingman/suite/widgetdemo/widgetdemo.cpp" section).
class WidgetDemo : public KeyboardDelegate, public MouseDelegate {
    private:
        void utility_draw_pixel(unsigned x, unsigned y, unsigned color);
        void utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 2);
        void utility_draw_string(unsigned x, unsigned y, const char* str, unsigned color, unsigned scale = 2);
    private:
        WindowManager* wm;
        window_ref_t ref;
        void draw_border(void);
        void draw_background(void);
        void draw_title(void);
        void draw_widgets(void);
    public:
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        int thickness;
        Window* window;
        Button* button;
        TextInput* textInput;
        Checkbox* checkbox;
        Checkbox* toggle;
        WidgetDemo(WindowManager* wm);
        ~WidgetDemo();
        void redraw(void);
        bool onKeyboard(char key, bool shift, bool meta, unsigned char scancode) override;
        bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) override;
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};
