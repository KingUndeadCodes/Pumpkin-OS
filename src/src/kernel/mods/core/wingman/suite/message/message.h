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
#include "../../headers/draw.h"
#include "../../../../dev/serial/serial.h"
#include "../../../fontman/fontman.h"

enum MessageBoxType {
    DialogBoxWarning,
    DialogBoxError,
    DialogBoxInformational
};

// Fixed-size button array cap -- no dialog in this codebase needs more than 3.
#define MESSAGEBOX_MAX_BUTTONS 3

class MessageBox : public KeyboardDelegate, public MouseDelegate {
    private:
        WindowManager* wm;
        window_ref_t ref;
        // Explicit icon override (0-11); -1 means "derive it from dialogBoxType instead".
        int icon;
        Button** buttons;
        int buttonCount;
        int buttonCapacity;
        // Shared row geometry every button's rect is derived from.
        int buttonRowX;
        int buttonRowY;
        int buttonRowWidth;
        int buttonRowHeight;
        // Y of the divider line above the button row.
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
        void draw_body(void);
        void draw_buttons(void);
        void dismiss(void);
        static void closeTrampoline(void* userdata);
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
