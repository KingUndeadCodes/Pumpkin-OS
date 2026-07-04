#ifndef RAMFS_H
#define RAMFS_H

#include "../vfs/vfs.h"
#include "../../dev/serial/serial.h"
#include <stdint.h>
#include <stddef.h>

// File data is stored as a linked list of fixed-size blocks, allocated on
// demand via malloc as data is written -- no per-file size cap beyond
// available heap. Directories hold a dynamically-sized linked list of
// children rather than a fixed-size array, so there's no per-directory
// fanout cap either. Internal layout (blocks, dirents, node data) lives
// entirely in ramfs.cpp; nothing outside this driver should depend on it.
#define RAMFS_BLOCK_SIZE 4096

vfs_node_t* ramfs_init(void);
int ramfs_delete_node(vfs_node_t* node);

#endif
