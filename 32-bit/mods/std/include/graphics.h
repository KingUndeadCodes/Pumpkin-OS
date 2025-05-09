#ifndef __GRAPHICS_H
#define __GRAPHICS_H
#include <tasking.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "graphics/font.h"
#include "graphics/icons.h"

void graphics_initalize(void);
void terminal_write(const char* string);

#endif