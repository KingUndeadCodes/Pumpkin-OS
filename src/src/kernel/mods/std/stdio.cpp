#include "../../dev/serial/serial.h"
#include "../../dev/vfs/vfs.h"
#include "./include/stdio.h"
#include <stdlib.h>
#include <string.h>

#define MAX_OPEN_FILES 32

typedef struct {
    int      in_use;
    vfs_node_t *node;
    size_t   position;
    int      flags;
    int      error;
} file_entry_t;

static file_entry_t file_table[MAX_OPEN_FILES];

// See docs/DOCS.md ("mods/std/stdio.cpp" section) for why the claim happens
// inside the same critical section as the scan.
static int alloc_fd() {
    unsigned long flags = enter_critical();
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!file_table[i].in_use) {
            file_table[i].in_use = 1;
            exit_critical(flags);
            return i;
        }
    }
    exit_critical(flags);
    return -1;
}

FILE* fopen(const char *filename, const char *mode) {
    int want_create = 0;
    int want_truncate = 0;
    int want_append = 0;
    if (strcmp(mode, "w") == 0 || strcmp(mode, "w+") == 0) {
        want_create = 1; 
        want_truncate = 1;
    } else if (strcmp(mode, "a") == 0 || strcmp(mode, "a+") == 0) {
        want_create = 1; 
        want_append = 1;
    }
    vfs_node_t *node = vfs_find(filename);
    if (!node && want_create) {
        node = vfs_create(filename, VFS_NODE_FILE, 0644);
        if (!node) return NULL;
    }
    if (!node) return NULL;
    if (node->open) node->open(node);
    int fd = alloc_fd();
    if (fd < 0) return NULL;
    file_table[fd].node = node;
    file_table[fd].position = want_append ? node->size : 0;
    file_table[fd].flags = 0;
    file_table[fd].error = 0;
    if (want_truncate && node->type == VFS_NODE_FILE) node->size = 0;
    FILE *f = (FILE*)malloc(sizeof(FILE));
    f->fd = fd;
    f->position = file_table[fd].position;
    f->flags = 0;
    f->error = 0;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    int fd = stream->fd;
    vfs_node_t *node = file_table[fd].node;
    if (node && node->close) node->close(node);
    unsigned long flags = enter_critical();
    file_table[fd].in_use = 0;
    file_table[fd].node = NULL;
    exit_critical(flags);
    free(stream);
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    int fd = stream->fd;
    vfs_node_t *node = file_table[fd].node;
    size_t new_pos = 0;
    if (whence == 0) new_pos = offset;
    else if (whence == 1) new_pos = file_table[fd].position + offset;
    else if (whence == 2) new_pos = node->size + offset;
    else return -1;
    if (new_pos > node->size) return -1;
    file_table[fd].position = new_pos;
    stream->position = new_pos;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    if (!stream) return 0;
    int fd = stream->fd;
    vfs_node_t *node = file_table[fd].node;
    if (!node || !node->read) return 0;
    size_t bytes = vfs_read(node, file_table[fd].position, size * count, ptr);
    file_table[fd].position += bytes;
    stream->position = file_table[fd].position;
    return bytes / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    if (!stream) return 0;
    int fd = stream->fd;
    vfs_node_t *node = file_table[fd].node;
    if (!node || !node->write) return 0;
    size_t bytes = vfs_write(node, file_table[fd].position, size * count, ptr);
    file_table[fd].position += bytes;
    stream->position = file_table[fd].position;
    if (file_table[fd].position > node->size)
        node->size = file_table[fd].position;
    return bytes / size;
}

int mkdir(const char *path, int permissions) {
    vfs_node_t* node = vfs_create(path, VFS_NODE_DIR, permissions);
    return node ? 0 : -1;
}