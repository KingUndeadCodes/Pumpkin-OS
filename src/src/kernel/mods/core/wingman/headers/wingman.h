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
// Always a full-frame present -- real page-flipping means a partial patch would leave the rest wrong.
void redraw_screen(void);
// Starts the task that drains queueMouseEventForWingman()/queueKeyEventForWingman()'s ring buffer.
void wingman_spawn_input_worker(void* stack_mem);