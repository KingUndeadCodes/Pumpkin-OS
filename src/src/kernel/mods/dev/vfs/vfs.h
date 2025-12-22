#ifndef VFS_H
#define VFS_H

#include "../../dev/serial/serial.h"
#include <stddef.h>

#define VFS_MAX_NAME 64

typedef enum {
    VFS_NODE_FILE,
    VFS_NODE_DIR
} vfs_node_type_t;

struct vfs_node;
typedef struct vfs_node vfs_node_t;

typedef size_t (*vfs_read_t)(vfs_node_t*, size_t, size_t, void*);
typedef size_t (*vfs_write_t)(vfs_node_t*, size_t, size_t, const void*);
typedef int    (*vfs_open_t)(vfs_node_t*);
typedef int    (*vfs_close_t)(vfs_node_t*);
typedef int    (*vfs_readdir_t)(vfs_node_t*, size_t, struct dirent*);
typedef vfs_node_t* (*vfs_finddir_t)(vfs_node_t*, const char*);
typedef vfs_node_t* (*vfs_create_t)(vfs_node_t*, const char*, int, int);

struct vfs_node {
    char name[VFS_MAX_NAME];
    vfs_node_type_t type;
    int permissions; // 0644, etc.
    size_t size;
    void* fs_data;

    vfs_read_t    read;
    vfs_write_t   write;
    vfs_open_t    open;
    vfs_close_t   close;
    vfs_readdir_t readdir;
    vfs_finddir_t finddir;
    vfs_create_t  create;

    vfs_node_t* mountpoint;
};

struct dirent {
    char d_name[VFS_MAX_NAME];
    int  d_type;
};

void vfs_init();
int  vfs_mount(const char* path, vfs_node_t* root);
vfs_node_t* vfs_find(const char* path);
vfs_node_t* vfs_create(const char* path, int type, int permissions);
int  vfs_open(vfs_node_t* node);
int  vfs_close(vfs_node_t* node);
size_t vfs_read(vfs_node_t* node, size_t offset, size_t size, void* buf);
size_t vfs_write(vfs_node_t* node, size_t offset, size_t size, const void* buf);
int  vfs_readdir(vfs_node_t* node, size_t index, struct dirent* out);
vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name);

#endif