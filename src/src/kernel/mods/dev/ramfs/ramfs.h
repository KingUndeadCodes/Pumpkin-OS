#ifndef RAMFS_H
#define RAMFS_H

#include "../vfs/vfs.h"
#include "../../dev/serial/serial.h"
#include <stdint.h>
#include <stddef.h>

#define RAMFS_MAX_FILES      32    // files per directory
#define RAMFS_MAX_DIRS       32    // subdirs per directory
#define RAMFS_MAX_NAME       64
#define RAMFS_MAX_FILE_SIZE  1024 * 128 // 4096   // bytes per file

// global pool sizes (tune to your memory constraints)
#ifndef RAMFS_MAX_TOTAL_DIRS
#define RAMFS_MAX_TOTAL_DIRS 128
#endif

#ifndef RAMFS_MAX_TOTAL_FILES
#define RAMFS_MAX_TOTAL_FILES (RAMFS_MAX_FILES * 16) // total file nodes available
#endif

typedef struct ramfs_dir ramfs_dir_t;
typedef struct ramfs_file ramfs_file_t;

struct ramfs_file {
    vfs_node_t node;
    ramfs_dir_t* parent;
    size_t size;
    uint8_t data[RAMFS_MAX_FILE_SIZE];
};

struct ramfs_dir {
    vfs_node_t node;
    ramfs_dir_t* parent;
    ramfs_dir_t* subdirs[RAMFS_MAX_DIRS];
    ramfs_file_t* files[RAMFS_MAX_FILES];
    size_t dir_count;
    size_t file_count;
};

static size_t ramfs_read(vfs_node_t* node, size_t offset, size_t size, void* buf);
static size_t ramfs_write(vfs_node_t* node, size_t offset, size_t size, const void* buf);
static int ramfs_readdir(vfs_node_t* node, size_t idx, struct dirent* out);
static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name);
static vfs_node_t* ramfs_create_node(vfs_node_t* parent, const char* name, int type, int permissions);
vfs_node_t* ramfs_init(void);
int ramfs_delete_node(vfs_node_t* node);

#endif