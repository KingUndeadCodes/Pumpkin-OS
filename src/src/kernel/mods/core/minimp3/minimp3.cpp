// Single translation unit that instantiates the minimp3 implementation.
// https://github.com/lieff/minimp3 (public domain / CC0)

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#include "minimp3.h"
