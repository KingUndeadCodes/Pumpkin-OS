#ifndef ISS_MESSAGEBOX
#define ISS_MESSAGEBOX

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../headers/manager.h"
#include "../../headers/cursor.h"
#include "../../headers/widgets/button.h"
#include "../../../../dev/serial/serial.h"
#include "../../../../dev/vbe/vga_table.h"
#include "../../../../std/include/graphics/font.h"
#include "../../../../std/include/graphics/icons.h"

enum MessageBoxType {
    DialogBoxWarning,
    DialogBoxError,
    DialogBoxInformational
};

// See docs/DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
#define MESSAGEBOX_MAX_BUTTONS 3

class MessageBox : public KeyboardDelegate, public MouseDelegate {
    private:
        void utility_draw_pixel(unsigned x, unsigned y, unsigned color);
        void utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 4);
        void utility_draw_icon(unsigned x, unsigned y, unsigned icon, float scale = 2.0f);
    private:
        WindowManager* wm;
        window_ref_t ref;
        // See docs/DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
        int icon;
        Button** buttons;
        int buttonCount;
        int buttonCapacity;
        // Shared row geometry every button's rect is derived from.
        int buttonRowX;
        int buttonRowY;
        int buttonRowWidth;
        int buttonRowHeight;
        // See docs/DOCS.md ("mods/core/wingman/suite/message/message.h / message.cpp" section).
        int buttonSectionDividerY;
        void layoutButtons(void);
    public:
        enum MessageBoxType dialogBoxType;
        int width;
        int height;
        int offsetX;
        int offsetY;
        int padding;
        int thickness;
        char* message;
        Window* window;
        MessageBox(WindowManager* wm, enum MessageBoxType dialogBoxType, const char* message, int icon = -1);
        void redraw(uint8_t description = 0b00111000);
        int addButton(const char* label, uint32_t color, ButtonCallback onClick = NULL, void* userdata = NULL);
    private:
        void draw_border(void);
        void draw_background(void);
        void draw_title(void);
        void draw_body(void);
        void draw_buttons(void);
        void dismiss(void);
    public:
        bool onKeyboard(char key, bool shift, bool meta, unsigned char scancode) override;
        bool onMouseEvent(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) override;
        ~MessageBox();
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

#endif
