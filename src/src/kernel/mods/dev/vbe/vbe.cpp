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

// Writes straight through a plain uint32_t*, not draw_pixel()'s volatile per-pixel path --
// this is 786,432 stores for a full clear, and the framebuffer is plain mapped memory, not MMIO.
void fill(unsigned color) {
    uint32_t* fb = linearFramebuffer;
    uint32_t count = (uint32_t)SCREEN_X * (uint32_t)SCREEN_Y;
    for (uint32_t i = 0; i < count; i++) fb[i] = color;
}

// One memmove() for the shifted rows instead of a row-by-row get_pixel()/draw_pixel() loop.
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


// 8x8 pixel font, scaled by `scale`. Same plain-pointer write path as fill() above.
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

static uint32_t currentBackRegion = 1;   // region 0 is displayed at boot (Y_OFFSET=0)

void init(void) {
    BgaSetVideoMode(SCREEN_X, SCREEN_Y, SCREEN_BPP, 1, 1);
    // Offset writes must come after BgaSetVideoMode() -- its own ENABLE
    // transition resets VIRT_WIDTH/X_OFFSET/Y_OFFSET to 0 on QEMU. See
    // docs/DOCS.md ("mods/dev/vbe/vbe.cpp -- hardware double buffering").
    BgaWriteRegister(VBE_DISPI_INDEX_VIRT_HEIGHT, SCREEN_Y * VBE_FRAMEBUFFER_COUNT);
    BgaWriteRegister(VBE_DISPI_INDEX_X_OFFSET, 0);
    BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0);

    // Permanent boot-time self-check: an out-of-bounds Y_OFFSET write is
    // silently clamped back to 0 by the device rather than faulted, so
    // this is the only way to know the VRAM-size assumption actually
    // holds. See docs/DOCS.md (same section).
    BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, (unsigned short)SCREEN_Y);
    bool doubleBufferOk = (BgaReadRegister(VBE_DISPI_INDEX_Y_OFFSET) == (unsigned short)SCREEN_Y);
    BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0);  // restore region 0 before boot console draws
    serial_write_string(doubleBufferOk
        ? "VBE: double-buffer VRAM check OK\n"
        : "VBE: double-buffer VRAM check FAILED -- Y_OFFSET clamped, vbe_flip() will be a no-op\n",
        false, doubleBufferOk ? INFO : FAIL);

    uint32_t bar = pciConfigReadDword(pciBus, pciDevice, pciFunction, 0x10);  // Or BAR2/4 depending on device
    uint32_t phys_addr = bar & ~0xF;
    uint32_t allocationSize = SCREEN_X * SCREEN_Y * (SCREEN_BPP / 8) * VBE_FRAMEBUFFER_COUNT;
    uint32_t virtualAddress = 0xE0000000;
    map_physical_memory(phys_addr, (uintptr_t)virtualAddress, allocationSize, PAGE_PRESENT | PAGE_WRITABLE);
    linearFramebuffer = (uint32_t*)virtualAddress;
    currentBackRegion = 1;
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

// currentBackRegion is the half NOT currently scanned out -- safe to write into.
uint32_t* vbe_get_back_buffer(void) {
    uint32_t frameWords = (uint32_t)SCREEN_X * (uint32_t)SCREEN_Y;
    return linearFramebuffer + (size_t)currentBackRegion * frameWords;
}

// Atomically swaps which VRAM region is displayed via Y_OFFSET. The
// display hardware only picks this up at its own next frame boundary, so
// this single write is the entire flip -- no vsync wait needed.
void vbe_flip(void) {
    BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, (unsigned short)(currentBackRegion * SCREEN_Y));
    currentBackRegion = 1 - currentBackRegion;
}