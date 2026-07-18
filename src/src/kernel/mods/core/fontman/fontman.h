#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../../ports/truetype/vendor/stb_truetype.h"

// See docs/DOCS.md ("Font Rendering System") for why this bakes
// fixed-size atlases once at boot instead of rasterizing live: -O0 +
// x87-only floats (-mno-sse -mno-sse2 -mno-mmx -mfpmath=387) makes
// stb_truetype's rasterizer real, non-trivial CPU cost, worth paying
// exactly once per size tier, never per glyph per redraw.
#define TTF_FIRST_CHAR 32   // ' '
#define TTF_NUM_CHARS  95   // through '~' (126)

struct FontAtlas {
    uint8_t* bitmap;                        // atlasW * atlasH, 8bpp coverage, owned (malloc'd)
    int atlasW;
    int atlasH;
    int cellSize;                           // == 8 * scale for this tier (16 / 24 / 32)
    int baselineRow;                        // fixed baseline row within the cell, same for every glyph
    int advanceWidth;                       // real monospace glyph advance (px) -- see ttf_font_char_advance()
    stbtt_packedchar chardata[TTF_NUM_CHARS];
    bool valid;                             // false => callers must fall back to the old Font[] path
};

// Bakes all size tiers. Call exactly once, from graphics_initalize_stage1()
// (after initialize_memory_pool() so malloc is available, before any
// Terminal/Wingman widget exists so every real caller always sees a baked,
// or explicitly-invalid-with-fallback, atlas).
void ttf_font_init();

// scale is 2/3/4, matching every draw_char/utility_draw_char/drawChar call
// site's existing scale parameter. Returns nullptr if that tier failed to
// bake (or ttf_font_init() hasn't run) -- callers must fall back to Font[].
const FontAtlas* ttf_font_get_atlas(unsigned scale);

// Real per-character horizontal advance (px) for this tier's baked TTF
// glyphs -- the font's actual monospace advance width (verified uniform at
// bake time in bake_tier()), which is narrower than the 8*scale bitmap-grid
// cell every draw-string call site used to advance by unconditionally, and
// is what was causing text to read as loosely/generously spaced. Returns
// 8*scale (the bitmap Font[] glyph's own fixed width) if that tier's atlas
// isn't baked, so a caller can always use this single value as the stride
// regardless of which glyph path (TTF or Font[] fallback) draw_char itself
// ends up taking for a given character.
int ttf_font_char_advance(unsigned scale);

// Alpha-over blend of a single foreground color against an existing pixel,
// using an 8-bit coverage value from the atlas. No dependency on Wingman's
// headers/types.h -- fontman is its own mods/core/ component, a peer of
// wingman rather than a dependent of it, even though wingman is
// currently fontman's only GUI-side consumer.
static inline uint32_t ttf_blend_over(uint32_t under, uint32_t fg, uint8_t alpha) {
    uint32_t ia = 255u - alpha;
    uint8_t ur = (uint8_t)(under >> 16), ug = (uint8_t)(under >> 8), ub = (uint8_t)under;
    uint8_t fr = (uint8_t)(fg >> 16),    fgc = (uint8_t)(fg >> 8),   fb = (uint8_t)fg;
    uint8_t r = (uint8_t)(((uint32_t)fr * alpha + (uint32_t)ur * ia) / 255u);
    uint8_t g = (uint8_t)(((uint32_t)fgc * alpha + (uint32_t)ug * ia) / 255u);
    uint8_t b = (uint8_t)(((uint32_t)fb * alpha + (uint32_t)ub * ia) / 255u);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Shared blit core. Templated on the plot callback so it works uniformly
// for a free-function framebuffer write (vbe.cpp/VBEScreen) or a Surface
// member call (Wingman widgets) with no virtual dispatch (this project
// builds -fno-rtti). plot(px, py, alpha, fg) is called once per covered
// dest pixel.
//
// IMPORTANT: pc.xoff/pc.yoff are SIGNED floats (frequently negative --
// left overhang, accents, etc). Every intermediate here (originX, originY,
// dstRow, dstCol) is `int`, never `unsigned`, all the way through, and
// gets clipped to [0, cellSize) BEFORE the caller's (usually unsigned)
// cellOriginX/cellOriginY is ever added in. This is the direct fix for the
// bug that killed the prior attempt at this feature: an unsigned
// wraparound on a negative xoff/yoff caused a page fault. See docs/TODO.md
// ("True font rendering") for that history.
template <typename PlotFn>
inline void ttf_blit_glyph(const FontAtlas* atlas, char c, int cellOriginX, int cellOriginY,
                            uint32_t fg, PlotFn&& plot) {
    if (atlas == nullptr || !atlas->valid) return;
    if (c < TTF_FIRST_CHAR || c >= TTF_FIRST_CHAR + TTF_NUM_CHARS) return;

    const stbtt_packedchar& pc = atlas->chardata[c - TTF_FIRST_CHAR];
    int glyphW = (int)pc.x1 - (int)pc.x0;
    int glyphH = (int)pc.y1 - (int)pc.y0;
    if (glyphW <= 0 || glyphH <= 0) return; // e.g. space -- no ink, nothing to blit

    int originX = (int)(pc.xoff >= 0.0f ? pc.xoff + 0.5f : pc.xoff - 0.5f);
    int originY = atlas->baselineRow + (int)(pc.yoff >= 0.0f ? pc.yoff + 0.5f : pc.yoff - 0.5f);

    for (int row = 0; row < glyphH; row++) {
        int dstRow = originY + row;
        if (dstRow < 0 || dstRow >= atlas->cellSize) continue;
        const uint8_t* srcRow = atlas->bitmap + (size_t)(pc.y0 + row) * (size_t)atlas->atlasW + pc.x0;
        for (int col = 0; col < glyphW; col++) {
            int dstCol = originX + col;
            if (dstCol < 0 || dstCol >= atlas->cellSize) continue;
            uint8_t a = srcRow[col];
            if (a == 0) continue;
            plot(cellOriginX + dstCol, cellOriginY + dstRow, a, fg);
        }
    }
}
