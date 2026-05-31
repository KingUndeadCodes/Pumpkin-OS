#ifndef ISS_MESSAGEBOX
#define ISS_MESSAGEBOX

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "../../headers/window.h"
#include "../../headers/surface.h"
#include "../../../../dev/serial/serial.h"
#include "../../../../dev/vbe/vga_table.h"
#include "../../../../std/include/graphics/font.h"
#include "../../../../std/include/graphics/icons.h"

enum MessageBoxType {
    DialogBoxWarning, 
    DialogBoxError,
    DialogBoxInformational
};

class MessageBox {
    private:
        void utility_draw_pixel(unsigned x, unsigned y, unsigned color);
        void utility_draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 4);
        void utility_draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2);
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
        MessageBox(enum MessageBoxType dialogBoxType, const char* message);
        void redraw(uint8_t description = 0b00111000);    
    private:
        void draw_border(void);
        void draw_background(void);
        void draw_title(void);
        void draw_body(void);
        void draw_options(void);
    public:
        // virtual void keyboard_callback(char key, bool shift, bool meta, unsigned char scancode);
        ~MessageBox();
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

#endif