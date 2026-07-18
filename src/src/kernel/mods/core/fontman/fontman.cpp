#include "fontman.h"
#include "../../std/include/stdlib.h"
#include "../../dev/serial/serial.h"

extern "C" const uint8_t _binary_font_ttf_start[];
extern "C" const uint8_t _binary_font_ttf_end[];

static FontAtlas g_atlases[3]; // index 0/1/2 -> scale 2/3/4
static const int kCellSizes[3] = { 16, 24, 32 }; // == 8 * scale

static int tier_for_scale(unsigned scale) {
    switch (scale) {
        case 2: return 0;
        case 3: return 1;
        case 4: return 2;
        default: return -1;
    }
}

static bool bake_tier(int tier, const uint8_t* fontData) {
    FontAtlas& atlas = g_atlases[tier];
    atlas.cellSize = kCellSizes[tier];
    atlas.atlasW = 256;
    atlas.atlasH = (tier == 2) ? 256 : 128; // headroom for 95 ASCII glyphs at 16-32px
    atlas.bitmap = (uint8_t*)malloc((size_t)atlas.atlasW * (size_t)atlas.atlasH);
    if (atlas.bitmap == nullptr) {
        printf_serial(false, FAIL, "[ttf_font] atlas malloc failed for tier %d\n", tier);
        return false;
    }

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, atlas.bitmap, atlas.atlasW, atlas.atlasH, 0, 1, nullptr)) {
        printf_serial(false, FAIL, "[ttf_font] stbtt_PackBegin failed for tier %d\n", tier);
        free(atlas.bitmap);
        atlas.bitmap = nullptr;
        return false;
    }
    if (!stbtt_PackFontRange(&pc, fontData, 0, (float)atlas.cellSize, TTF_FIRST_CHAR, TTF_NUM_CHARS, atlas.chardata)) {
        printf_serial(false, FAIL, "[ttf_font] stbtt_PackFontRange failed for tier %d (atlas too small?)\n", tier);
        stbtt_PackEnd(&pc);
        free(atlas.bitmap);
        atlas.bitmap = nullptr;
        return false;
    }
    stbtt_PackEnd(&pc);

    // Fixed baseline: derive from the font's own vertical metrics scaled to
    // this cell size, so every glyph in the tier sits on the same row --
    // ink position varies per glyph, the cell origin doesn't (the whole
    // point of a monospace grid).
    stbtt_fontinfo info;
    stbtt_InitFont(&info, fontData, 0);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    float scalePx = stbtt_ScaleForPixelHeight(&info, (float)atlas.cellSize);
    atlas.baselineRow = (int)(ascent * scalePx + 0.5f);

    // Metrically-monospace check: every draw-string loop in this codebase
    // advances by a fixed 8*scale pixels per character regardless of what
    // the font's real advance width says. A font that's only *visually*
    // monospace would silently mis-space with no other signal, so check
    // once, here, and log (not fail) if it doesn't hold.
    int firstAdvance = 0;
    bool uniform = true;
    for (int i = 0; i < TTF_NUM_CHARS; i++) {
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&info, TTF_FIRST_CHAR + i, &advance, &lsb);
        if (i == 0) firstAdvance = advance;
        else if (advance != firstAdvance) { uniform = false; break; }
    }
    if (!uniform) {
        printf_serial(false, FAIL, "[ttf_font] WARNING: font is not metrically monospace (tier %d) -- text will mis-space, since every layout call site assumes a fixed 8*scale advance\n", tier);
    }

    // Real pixel advance width, scaled the same way the baseline is (above).
    // This is narrower than cellSize for Cousine, which is exactly why the
    // old "advance by cellSize per character" call sites read as loosely
    // spaced -- see ttf_font_char_advance().
    atlas.advanceWidth = (int)(firstAdvance * scalePx + 0.5f);
    if (atlas.advanceWidth < 1) atlas.advanceWidth = 1;

    atlas.valid = true;
    printf_serial(false, INFO, "[ttf_font] tier %d baked ok (cell=%d atlas=%dx%d advance=%d)\n", tier, atlas.cellSize, atlas.atlasW, atlas.atlasH, atlas.advanceWidth);
    return true;
}

void ttf_font_init() {
    size_t fontSize = (size_t)(_binary_font_ttf_end - _binary_font_ttf_start);
    if (fontSize == 0) {
        printf_serial(false, FAIL, "[ttf_font] embedded font data is empty (incbin problem?), skipping bake entirely\n");
        return;
    }
    for (int tier = 0; tier < 3; tier++) {
        if (!bake_tier(tier, _binary_font_ttf_start)) {
            printf_serial(false, FAIL, "[ttf_font] tier %d bake failed, falling back to bitmap Font[] for that scale\n", tier);
        }
    }
}

const FontAtlas* ttf_font_get_atlas(unsigned scale) {
    int tier = tier_for_scale(scale);
    if (tier < 0 || !g_atlases[tier].valid) return nullptr;
    return &g_atlases[tier];
}

int ttf_font_char_advance(unsigned scale) {
    int tier = tier_for_scale(scale);
    if (tier < 0 || !g_atlases[tier].valid) return (int)(8 * scale);
    return g_atlases[tier].advanceWidth;
}
