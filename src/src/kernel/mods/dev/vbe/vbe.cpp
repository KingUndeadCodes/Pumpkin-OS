#include "vbe.h"
#include "../pci/pci.h"
#include "../serial/serial.h"
#include "../paging/paging.h"
#include "../../core/wingman/headers/icons.h"
#include "font.h"
#include "./vga_table.h"
#include <string.h>
// #include "vga_table.h"

static uint8_t pciBus = 0;
static uint8_t pciDevice = 0;
static uint8_t pciFunction = 0;
static uint32_t* linearFramebuffer = (uint32_t*)VBE_DISPI_LFB_PHYSICAL_ADDRESS;

#define rgb(r, g, b) (((r) << 16) | ((g) << 8) | (b))

// See docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- bulk framebuffer operations").
void fill(unsigned color) {
    uint32_t* fb = linearFramebuffer;
    uint32_t count = (uint32_t)SCREEN_X * (uint32_t)SCREEN_Y;
    for (uint32_t i = 0; i < count; i++) fb[i] = color;
}

// See docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- bulk framebuffer operations").
void scroll_framebuffer_up(unsigned lines, unsigned bg) {
    if (lines == 0) return;
    uint32_t* fb = linearFramebuffer;
    if (lines >= (unsigned)SCREEN_Y) {
        fill(bg);
        return;
    }
    uint32_t moveRows = (uint32_t)SCREEN_Y - lines;
    size_t moveBytes = (size_t)moveRows * (size_t)SCREEN_X * sizeof(uint32_t);
    memmove(fb, fb + (size_t)lines * (size_t)SCREEN_X, moveBytes);
    uint32_t* bottom = fb + (size_t)moveRows * (size_t)SCREEN_X;
    uint32_t bottomCount = lines * (uint32_t)SCREEN_X;
    for (uint32_t i = 0; i < bottomCount; i++) bottom[i] = bg;
}

void pci_vbe_init(uint8_t bus, uint8_t device, uint8_t function) {
    pciBus = bus;
    pciDevice = device;
    pciFunction = function;
}


// See docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- bulk framebuffer operations").
// 8x8 pixel font, scaled by `scale`.
void draw_char(unsigned x, unsigned y, char c, unsigned color, unsigned scale = 4) {
    uint32_t* fb = linearFramebuffer;
    for (unsigned i = 0; i < 8; i++) {
        for (unsigned j = 0; j < 8; j++) {
            if (!(Font[(int)c][i] & (1 << j))) continue;
            unsigned blockX = x + j * scale;
            unsigned blockY = y + i * scale;
            for (unsigned k = 0; k < scale; k++) {
                uint32_t* row = fb + (blockY + k) * (unsigned)SCREEN_X + blockX;
                for (unsigned l = 0; l < scale; l++) row[l] = color;
            }
        }
    }
}

void draw_icon(unsigned x, unsigned y, unsigned icon, unsigned scale = 2) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            // const int scale = 2;
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

void draw_pixel(unsigned x, unsigned y, unsigned color) {
    volatile uint32_t* fb = (volatile uint32_t*)linearFramebuffer;
    uint32_t offset = x + y * SCREEN_X;
    fb[offset] = color;
    return;
}

unsigned int get_pixel(unsigned x, unsigned y) {
    volatile uint32_t* fb = (volatile uint32_t*)linearFramebuffer;
    uint32_t offset = x + y * SCREEN_X;
    return linearFramebuffer[offset];
}

uint32_t pciConfigReadDword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    // Ensure offset is DWORD aligned (0x00, 0x04, 0x08, ...)
    uint16_t low = pciConfigReadWord(bus, device, function, offset);
    uint16_t high = pciConfigReadWord(bus, device, function, offset + 2);
    return ((uint32_t)high << 16) | low;
}

void init(void) {
    BgaSetVideoMode(SCREEN_X, SCREEN_Y, SCREEN_BPP, 1, 1);
    uint32_t bar = pciConfigReadDword(pciBus, pciDevice, pciFunction, 0x10);  // Or BAR2/4 depending on device
    uint32_t phys_addr = bar & ~0xF;
    uint32_t allocationSize = SCREEN_X * SCREEN_Y * (SCREEN_BPP / 8);
    uint32_t virtualAddress = 0xE0000000;
    map_physical_memory(phys_addr, (uintptr_t)virtualAddress, allocationSize, PAGE_PRESENT | PAGE_WRITABLE);
    linearFramebuffer = (uint32_t*)virtualAddress;
    return;
}

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