#include "vbe.h"
#include "../serial/serial.h"
#include "../../std/include/graphics/icons.h"
#include "../../std/include/graphics/image_struct.h"
#include "../../std/include/graphics/image_background.h"
#include "../../std/include/graphics/font.h"
#include "vga_table.h"

volatile int currentBank = 0;
const int kilobytes = (VBE_DISPI_BANK_SIZE_KB * 256);
volatile uint32_t* video_memory = (volatile uint32_t*)0xA0000;

#define rgb(r, g, b) (((r) << 16) | ((g) << 8) | (b))

void fill(unsigned color) {
    for (unsigned y = 0; y < VBE_DISPI_MAX_YRES * 1; y++) {
        for (unsigned x = 0; x < VBE_DISPI_MAX_XRES * 1; x++) {
            draw_pixel(x, y, color);
        }
    }
}

/*
// Function simmlar to draw_char but scales the font by a factor of 4
void draw_char(unsigned x, unsigned y, char c, unsigned color) {
    // 8x8 pixel font
    for (unsigned i = 0; i < 8; i++) {
        for (unsigned j = 0; j < 8; j++) {
            if (Font[(int)c][i] & (1 << j)) {
                for (unsigned k = 0; k < 4; k++) {
                    for (unsigned l = 0; l < 4; l++) {
                        draw_pixel(x + j * 4 + l, y + i * 4 + k, color);
                    }
                }
            }
        }
    }
}

void draw_icon(unsigned x, unsigned y, unsigned icon) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            const int scale = 2;
            for (unsigned k = 0; k < scale; k++) {
                for (unsigned l = 0; l < scale; l++) {
                    if (Icons[icon][i][j] == 0x00) continue;
                    if (Icons[icon][i][j] == 0x10) draw_pixel(x + j * scale + l, y + i * scale + k, 0x0); 
                    draw_pixel(x + j * scale + l, y + i * scale + k, vgaPalette[Icons[icon][i][j]]);
                }
            }
        }
    }
}
*/

void draw_pixel(unsigned x, unsigned y, unsigned color) {
    uint32_t offset = x + y * SCREEN_X;
    uint32_t bank = offset / kilobytes; // (VBE_DISPI_BANK_SIZE_KB * 1000);
    uint32_t bank_offset = offset - bank * kilobytes; // (VBE_DISPI_BANK_SIZE_KB * 1000);
    if (bank != currentBank) {
        /*
        serial_write_string("Switching bank to ", false, NONE);
        serial_write_string(itoa(bank, 10), false, NONE);
        serial_write_string("\n", false, NONE);
        */
        BgaSetBank(bank);
        currentBank = bank;
    }
    video_memory[bank_offset] = color;
    return;
}

void init(void) {
    BgaSetVideoMode(SCREEN_X, SCREEN_Y, SCREEN_BPP, 0, 1);
    return;
}

/*
void test(void) {
    fill(rgb(11, 26, 28));
    // draw_icon(220, 0, 3);
    // draw_char(288, 0, 'H', COLOR_R);
    // draw_char(320, 0, 'e', COLOR_G);
    // draw_char(352, 0, 'l', COLOR_B);
    // draw_char(384, 0, 'l', rgb(11, 51, 50));
    // draw_char(416, 0, 'o', rgb(227, 71, 5));
}
*/

void BgaWriteRegister(unsigned short IndexValue, unsigned short DataValue) {
    outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
    outw(VBE_DISPI_IOPORT_DATA, DataValue);
}

unsigned short BgaReadRegister(unsigned short IndexValue) {
    outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
    return inw(VBE_DISPI_IOPORT_DATA);
}

int BgaIsAvailable(void) {
    return (BgaReadRegister(VBE_DISPI_INDEX_ID) == VBE_DISPI_ID4);
}

void BgaSetVideoMode(unsigned int Width, unsigned int Height, unsigned int BitDepth, int UseLinearFrameBuffer, int ClearVideoMemory) {
    BgaWriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    BgaWriteRegister(VBE_DISPI_INDEX_XRES, Width);
    BgaWriteRegister(VBE_DISPI_INDEX_YRES, Height);
    BgaWriteRegister(VBE_DISPI_INDEX_BPP, BitDepth);
    BgaWriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED |
        (UseLinearFrameBuffer == 1 ? VBE_DISPI_LFB_ENABLED : 0) |
        (ClearVideoMemory == 1 ? 0 : VBE_DISPI_NOCLEARMEM));
}

void BgaSetBank(unsigned short BankNumber) {
    BgaWriteRegister(VBE_DISPI_INDEX_BANK, BankNumber);
}