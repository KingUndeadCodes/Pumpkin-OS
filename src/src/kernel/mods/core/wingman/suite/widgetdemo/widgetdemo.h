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
#include "../../headers/widgets/slider.h"
#include "../../headers/cursor.h"
#include "../../headers/draw.h"
#include "../../../../dev/serial/serial.h"
#include "../../../fontman/fontman.h"

// A standalone showcase window for every Wingman widget (button, text input, checkbox, toggle, slider).
class WidgetDemo : public KeyboardDelegate, public MouseDelegate {
    private:
        WindowManager* wm;
        window_ref_t ref;
        void draw_border(void);
        void draw_background(void);
        void draw_widgets(void);
        void closeWindow(void);
        static void closeTrampoline(void* userdata);
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
        Slider* slider;
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
