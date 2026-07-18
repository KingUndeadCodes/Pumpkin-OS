// Single translation unit that instantiates the stb_truetype implementation.
// https://github.com/nothings/stb (public domain / MIT). Vendored as a
// single header directly (not a submodule, unlike mods/ports/minimp3) since
// stb_truetype.h is a genuine single-file library -- pulling in the whole
// stb monorepo for one file would be needless.
#include <stdlib.h>
#include <string.h>
#include "../../std/include/math.h"

// Freestanding: no assert.h.
#define STBTT_assert(x) ((void)0)

// mods/std/math.cpp's complexFloor() returns 0 for any x <= 1 (including
// all negatives) -- not usable here. STBTT_ifloor/STBTT_iceil ARE reached
// by the real rasterizer path (stbtt_PackFontRange), confirmed by reading
// the vendored header directly, so this needs a real implementation, not a
// stub.
static int stbtt__ifloor_impl(double x) {
    int i = (int)x; // truncates toward zero
    return (x < 0.0 && (double)i != x) ? i - 1 : i;
}
static int stbtt__iceil_impl(double x) {
    int i = (int)x;
    return (x > 0.0 && (double)i != x) ? i + 1 : i;
}
// Also reached by the real rasterizer (stbtt__compute_crossings_x, part of
// scan-conversion) -- confirmed by reading the vendored header. sign-of-a
// convention (truncating division) matches libc fmod.
static double stbtt__fmod_impl(double a, double b) {
    if (b == 0.0) return 0.0;
    double q = a / b;
    long iq = (long)q;
    return a - (double)iq * b;
}

#define STBTT_ifloor(x)  stbtt__ifloor_impl(x)
#define STBTT_iceil(x)   stbtt__iceil_impl(x)
#define STBTT_fmod(x,y)  stbtt__fmod_impl(x,y)
#define STBTT_sqrt(x)    sqrt(x)
#define STBTT_fabs(x)    fabs(x)

// STBTT_pow/STBTT_cos/STBTT_acos are confirmed (by reading the vendored
// header) to only be reachable from stbtt_GetGlyphSDF/stbtt__solve_cubic --
// the signed-distance-field API, which this kernel never calls (only the
// stbtt_PackFontRange atlas-baking API is used). STBTT_pow/STBTT_cos route
// to the real implementations anyway since they already exist in
// math.cpp. STBTT_acos has no real implementation available (acos() is
// commented out, unimplemented, in mods/std/include/math.h) -- without an
// override here the header's own default (`acos(x)`) would reference a
// symbol that doesn't exist and fail to link, so this is a stub, not a
// correctness compromise: it's confirmed dead code for this kernel's usage.
#define STBTT_pow(x,y)   pow(x,y)
#define STBTT_cos(x)     cos(x)
#define STBTT_acos(x)    0.0

#define STBTT_malloc(size, u) ((void)(u), malloc(size))
#define STBTT_free(ptr, u)    ((void)(u), free(ptr))

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"
