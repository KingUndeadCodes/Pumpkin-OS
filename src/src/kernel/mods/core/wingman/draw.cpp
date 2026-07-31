#include <stdint.h>
#include "./headers/draw.h"
#include "./data/icons.h"
#include "../../dev/vbe/font.h"
#include "../../dev/vbe/vga_table.h"
#include "../fontman/fontman.h"

void surface_draw_pixel(Surface* surface, int x, int y, color_t color) {
    surface->putPixelUnsafe(x, y, color);
}

void surface_draw_char(Surface* surface, int x, int y, char c, color_t color, int scale) {
    const FontAtlas* atlas = ttf_font_get_atlas(scale);
    // No baked atlas for this scale -- fall back to the 8x8 bitmap font.
    if (atlas == nullptr) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (Font[(int)c][i] & (1 << j)) {
                    for (int k = 0; k < scale; k++) {
                        for (int l = 0; l < scale; l++) {
                            surface_draw_pixel(surface, x + j * scale + l, y + i * scale + k, color);
                        }
                    }
                }
            }
        }
        return;
    }
    ttf_blit_glyph(atlas, c, x, y, color,
        [&](int px, int py, uint8_t alpha, uint32_t fg) {
            color_t under = surface->getPixel(px, py);
            surface_draw_pixel(surface, px, py, ttf_blend_over(under, fg, alpha));
        });
}

void surface_draw_string(Surface* surface, int x, int y, const char* str, color_t color, int scale) {
    int charAdvance = ttf_font_char_advance(scale);
    for (int i = 0; str[i] != '\0'; i++) {
        surface_draw_char(surface, x + i * charAdvance, y, str[i], color, scale);
    }
}

void surface_draw_icon(Surface* surface, int x, int y, int iconId, int scale) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            for (int k = 0; k < scale; k++) {
                for (int l = 0; l < scale; l++) {
                    uint8_t color = Icons[iconId][i][j];
                    if (color == 0x00) continue;
                    if (color == 0x10) surface_draw_pixel(surface, x + j * scale + l, y + i * scale + k, 0x0);
                    surface_draw_pixel(surface, x + j * scale + l, y + i * scale + k, vgaPaletteConvertorRGB32[color]);
                }
            }
        }
    }
}
