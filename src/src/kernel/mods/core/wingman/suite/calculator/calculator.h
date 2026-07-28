#pragma once

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../headers/manager.h"
#include "../../headers/widgets/button.h"
#include "../../headers/cursor.h"
#include "../../../../dev/vbe/font.h"
#include "../../../fontman/fontman.h"

// See docs/DOCS.md ("mods/core/wingman/suite/calculator/calculator.cpp" section).
#define CALCULATOR_KEY_COUNT 17
#define CALCULATOR_DISPLAY_MAX 24

class Calculator : public MouseDelegate {
    private:
        void utility_draw_pixel(unsigned x, unsigned y, unsigned color);
        void utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 2);
        void utility_draw_string(unsigned x, unsigned y, const char* str, unsigned color, unsigned scale = 2);
    private:
        WindowManager* wm;
        window_ref_t ref;
        Button* keys[CALCULATOR_KEY_COUNT];
        char display[CALCULATOR_DISPLAY_MAX];
        int displayLen;
        double storedValue;
        char pendingOp;
        bool waitingForOperand;
        bool errorState;
        void handleKey(char label);
        void applyPendingOp(double operand);
        void setDisplayNumber(double value);
        void draw_border(void);
        void draw_background(void);
        void draw_title(void);
        void draw_display(void);
        void draw_keys(void);
    public:
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        int thickness;
        Window* window;
        Calculator(WindowManager* wm);
        ~Calculator();
        void redraw(void);
        bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) override;
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};
