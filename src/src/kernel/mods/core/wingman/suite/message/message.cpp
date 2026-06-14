#include "./message.h"

#define COLOR_R 0xFFFF0000
#define COLOR_G 0xFF00FF00
#define COLOR_B 0xFF0000FF
#define COLOR_W 0xFFFFFFFF

void MessageBox::utility_draw_pixel(unsigned x, unsigned y, unsigned color) {
    this->window->surface->putPixelUnsafe(x, y, color);
};

void MessageBox::utility_draw_char(unsigned int x, unsigned int y, char c, unsigned int color, unsigned int scale = 4U) {
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

void MessageBox::utility_draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            // const int scale = 2;
            for (unsigned k = 0; k < scale; k++) {
                for (unsigned l = 0; l < scale; l++) {
                    uint8_t color = Icons[icon][i][j];
                    if (color == 0x00) continue;
                    if (color == 0x10) utility_draw_pixel(x + j * scale + l, y + i * scale + k, 0x0); 
                    utility_draw_pixel(x + j * scale + l, y + i * scale + k, vgaPaletteConvertorRGB32[color]);
                }
            }
        }
    }
};

MessageBox::MessageBox(enum MessageBoxType dialogBoxType, const char* message) {
    this->dialogBoxType = dialogBoxType;
    this->width = 500;
    this->height = 200;
    this->offsetX = 100;
    this->offsetY = 100;
    this->padding = 10; 
    this->thickness = 3; // Border thickness
    if (message != NULL) {
        size_t messageLength = strlen(message);
        this->message = (char*)malloc(messageLength + 1);
        if (this->message != NULL) { strcpy(this->message, message); }
    } else {
        this->message = NULL;
    }
    this->window = new Window(width, height, offsetX, offsetY, "Message");
    this->redraw(0b11111000);
};

void MessageBox::redraw(uint8_t description = 0b00111000) {
    // Bit 8 is the (unused) 
    // Bit 7 is the border 
    // Bit 6 is the background
    // Bit 5 is the title
    // Bit 4 is the body
    // Bit 3 is the options
    if ((description >> 6) & 1) this->draw_border();
    if ((description >> 5) & 1) this->draw_background();
    if ((description >> 4) & 1) this->draw_title();
    if ((description >> 3) & 1) this->draw_body();
    if ((description >> 2) & 1) this->draw_options();
};

void MessageBox::draw_border(void) {
    int k = 0;
    int x0 = k * padding;
    int y0 = k * padding;
    int w = width - 2 * k * padding;
    int h = height - 2 * k * padding;
    for (int t = 0; t < thickness; t++) {
        for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + t, 0xFFFFFFFF);
        for (int x = 0; x < w; x++) utility_draw_pixel(x0 + x, y0 + h - 1 - t, 0xFFFFFFFF);
        for (int y = 0; y < h; y++) utility_draw_pixel(x0 + t, y0 + y, 0xFFFFFFFF);
        for (int y = 0; y < h; y++) utility_draw_pixel(x0 + w - 1 - t, y0 + y, 0xFFFFFFFF);
    }
};

void MessageBox::draw_background(void) {
    int frames = 1;
    int innerX = (frames - 1) * padding + thickness;
    int innerY = (frames - 1) * padding + thickness;
    int innerW = width - 2 * ((frames - 1) * padding) - 2 * thickness;
    int innerH = height - 2 * ((frames - 1) * padding) - 2 * thickness;
    for (int y = 0; y < innerH; y++) {
        for (int x = 0; x < innerW; x++) utility_draw_pixel(innerX + x, innerY + y, 0xFF403a39);
    }
};

void MessageBox::draw_title(void) {
    int iconType = 0; 
    char* title = (char*)malloc(13);
    switch (this->dialogBoxType) {
        case DialogBoxInformational: { iconType = 0; strcpy(title, "Information"); break; }
        case DialogBoxError: { iconType = 1; strcpy(title, "Error"); break; }  
        case DialogBoxWarning: { iconType = 2; strcpy(title, "Warning"); break; }  
    };
    utility_draw_icon(8, 4, iconType, 2);
    const char* titleString = (const char*)title;
    for (int i = 0; i < strlen(titleString); i++) {
        unsigned int xPos = 4 + 78 + (24 * i);
        char character = titleString[i];
        utility_draw_char(xPos, 24, character, COLOR_W, 3);
    }
    free(title);
};

void MessageBox::draw_body(void) {
    for (int i = 0; i < strlen(message); i++) {
        const int posX = 20 + (16 * (i % 30));
        const int posY = 80 + (16 * (int)(i / 30));
        utility_draw_char(posX, posY, message[i], COLOR_W, 2);
    }
    return;
};

void MessageBox::draw_options(void) {
    
};

MessageBox::~MessageBox() {
    free(this->message);
    delete this->window;
    return;
};