#pragma once

#include "../../../dev/serial/serial.h"
#include "../../../dev/mouse/mouse.h"
#include "../../../dev/kb/kb.h"
#include "./manager.h"
#include "./surface.h"
#include "./window.h"
#include "./cursor.h"
#include "./types.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void initalizeWindowSystem(void);
// See docs/DOCS.md ("mods/core/wingman/wingman.cpp -- input queue / worker task").
void wingman_spawn_input_worker(void* stack_mem);