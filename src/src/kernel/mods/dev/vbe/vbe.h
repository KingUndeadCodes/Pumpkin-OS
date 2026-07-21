#ifndef __VBE_H__
#define __VBE_H__

#include <stdint.h>
#include <stdbool.h>
#include "../port.cpp"

#define VBE_DISPI_BANK_ADDRESS          0xA0000
#define VBE_DISPI_BANK_SIZE_KB          64
#define VBE_DISPI_MAX_XRES              1024
#define VBE_DISPI_MAX_YRES              768
#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9
#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_VBE_ENABLED           0x40
#define VBE_DISPI_NOCLEARMEM            0x80
#define VBE_DISPI_LFB_ENABLED           0x01
#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000

#define VBE_DISPI_BPP_4 (0x04)
#define VBE_DISPI_BPP_8 (0x08)
#define VBE_DISPI_BPP_15 (0x0F)
#define VBE_DISPI_BPP_16 (0x10)
#define VBE_DISPI_BPP_24 (0x18)
#define VBE_DISPI_BPP_32 (0x20)

#define SCREEN_X VBE_DISPI_MAX_XRES
#define SCREEN_Y VBE_DISPI_MAX_YRES
#define SCREEN_BPP VBE_DISPI_BPP_32
#define SCREEN_PITCH (SCREEN_X * (SCREEN_BPP / 8))

#define COLOR_W 0x00FFFFFF

void draw_pixel(unsigned x, unsigned y, unsigned color);
unsigned int get_pixel(unsigned x, unsigned y);

void draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2);
void draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 4);

void pci_vbe_init(uint8_t bus, uint8_t device, uint8_t function);

void fill(unsigned color);
void scroll_framebuffer_up(unsigned lines, unsigned bg);
void init(void);
// void test(void); 

int BgaIsAvailable(void);
void BgaWriteRegister(unsigned short IndexValue, unsigned short DataValue);
void BgaSetVideoMode(unsigned int Width, unsigned int Height, unsigned int BitDepth, int UseLinearFrameBuffer, int ClearVideoMemory);
void BgaSetBank(unsigned short BankNumber);
unsigned short BgaReadRegister(unsigned short IndexValue);

// void draw_icon(unsigned x, unsigned y, unsigned icon);

#endif