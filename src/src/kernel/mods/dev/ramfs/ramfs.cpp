#include "ramfs.h"
#include <stdlib.h>
#include <string.h>

typedef struct ramfs_block ramfs_block_t;
struct ramfs_block {
    ramfs_block_t* next;
    uint8_t data[RAMFS_BLOCK_SIZE];
};

typedef struct ramfs_dirent ramfs_dirent_t;
struct ramfs_dirent {
    vfs_node_t* node;
    ramfs_dirent_t* next;
};

// Stashed in vfs_node_t::fs_data. `parent` is NULL only for the root node,
// which is otherwise indistinguishable from any other directory.
typedef struct {
    vfs_node_t* parent;
    union {
        struct { // VFS_NODE_FILE
            ramfs_block_t* head;
            ramfs_block_t* tail;
            size_t block_count;
        } file;
        struct { // VFS_NODE_DIR
            ramfs_dirent_t* children;
            size_t child_count;
        } dir;
    };
} ramfs_node_data_t;

static int ramfs_readdir(vfs_node_t* node, size_t idx, struct dirent* out);
static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name);
static vfs_node_t* ramfs_create_node(vfs_node_t* parent, const char* name, int type, int permissions);

static void safe_strncpy(char *dst, const char *src, size_t n) {
    if (n == 0) return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

/* --- file block-list helpers --- */

static ramfs_block_t* ramfs_block_at(ramfs_node_data_t* d, size_t index) {
    ramfs_block_t* b = d->file.head;
    while (b && index--) b = b->next;
    return b;
}

// Appends blocks until the file has at least `needed_blocks`. Returns -1 if
// an allocation fails partway through (whatever got allocated is kept).
static int ramfs_grow_file(ramfs_node_data_t* d, size_t needed_blocks) {
    while (d->file.block_count < needed_blocks) {
        ramfs_block_t* nb = (ramfs_block_t*)malloc(sizeof(ramfs_block_t));
        if (!nb) return -1;
        nb->next = NULL;
        if (d->file.tail) d->file.tail->next = nb;
        else d->file.head = nb;
        d->file.tail = nb;
        d->file.block_count++;
    }
    return 0;
}

static void ramfs_free_file_blocks(ramfs_node_data_t* d) {
    ramfs_block_t* b = d->file.head;
    while (b) {
        ramfs_block_t* next = b->next;
        free(b);
        b = next;
    }
    d->file.head = d->file.tail = NULL;
    d->file.block_count = 0;
}

/* --- file read/write --- */

// See DOCS.md ("mods/dev/ramfs/ramfs.cpp" section) for why these guard
// their whole body, not just the block-list traversal/mutation.
static size_t ramfs_read(vfs_node_t* node, size_t offset, size_t size, void* buf) {
    if (!node || !buf) return 0;
    ramfs_node_data_t* d = (ramfs_node_data_t*)node->fs_data;
    unsigned long flags = enter_critical();
    if (offset >= node->size) { exit_critical(flags); return 0; }
    if (offset + size > node->size) size = node->size - offset;

    size_t remaining = size;
    uint8_t* out = (uint8_t*)buf;
    ramfs_block_t* blk = ramfs_block_at(d, offset / RAMFS_BLOCK_SIZE);
    size_t block_offset = offset % RAMFS_BLOCK_SIZE;

    while (remaining > 0 && blk) {
        size_t chunk = RAMFS_BLOCK_SIZE - block_offset;
        if (chunk > remaining) chunk = remaining;
        memcpy(out, blk->data + block_offset, chunk);
        out += chunk;
        remaining -= chunk;
        block_offset = 0;
        blk = blk->next;
    }
    size_t result = size - remaining;
    exit_critical(flags);
    return result;
}

static size_t ramfs_write(vfs_node_t* node, size_t offset, size_t size, const void* buf) {
    if (!node || !buf) return 0;
    ramfs_node_data_t* d = (ramfs_node_data_t*)node->fs_data;

    unsigned long flags = enter_critical();
    size_t needed_blocks = (offset + size + RAMFS_BLOCK_SIZE - 1) / RAMFS_BLOCK_SIZE;
    if (ramfs_grow_file(d, needed_blocks) < 0) {
        // Out of memory partway through growth -- clamp to what we actually got.
        size_t available = d->file.block_count * RAMFS_BLOCK_SIZE;
        if (offset >= available) { exit_critical(flags); return 0; }
        if (offset + size > available) size = available - offset;
    }

    size_t remaining = size;
    const uint8_t* in = (const uint8_t*)buf;
    ramfs_block_t* blk = ramfs_block_at(d, offset / RAMFS_BLOCK_SIZE);
    size_t block_offset = offset % RAMFS_BLOCK_SIZE;

    while (remaining > 0 && blk) {
        size_t chunk = RAMFS_BLOCK_SIZE - block_offset;
        if (chunk > remaining) chunk = remaining;
        memcpy(blk->data + block_offset, in, chunk);
        in += chunk;
        remaining -= chunk;
        block_offset = 0;
        blk = blk->next;
    }

    size_t written = size - remaining;
    if (offset + written > node->size) node->size = offset + written;
    exit_critical(flags);
    return written;
}

/* --- node allocation --- */

static vfs_node_t* ramfs_alloc_node(vfs_node_t* parent, const char* name, int type, int permissions) {
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(vfs_node_t));

    ramfs_node_data_t* d = (ramfs_node_data_t*)malloc(sizeof(ramfs_node_data_t));
    if (!d) { free(node); return NULL; }
    memset(d, 0, sizeof(ramfs_node_data_t));
    d->parent = parent;

    safe_strncpy(node->name, name, VFS_MAX_NAME);
    node->type = (vfs_node_type_t)type;
    node->permissions = permissions;
    node->size = 0;
    node->fs_data = d;

    if (type == VFS_NODE_DIR) {
        node->readdir = ramfs_readdir;
        node->finddir = ramfs_finddir;
        node->create  = ramfs_create_node;
    } else {
        node->read  = ramfs_read;
        node->write = ramfs_write;
    }
    return node;
}

/* --- directory operations --- */

// See DOCS.md ("mods/dev/ramfs/ramfs.cpp" section) for why the duplicate
// check and the splice are one atomic unit here.
static vfs_node_t* ramfs_create_node(vfs_node_t* parent_node, const char* name, int type, int permissions) {
    if (!parent_node || !name || parent_node->type != VFS_NODE_DIR) return NULL;
    ramfs_node_data_t* pd = (ramfs_node_data_t*)parent_node->fs_data;

    unsigned long flags = enter_critical();

    if (ramfs_finddir(parent_node, name)) {
        exit_critical(flags);
        serial_write_string("ramfs: create duplicate\n");
        return NULL;
    }

    vfs_node_t* node = ramfs_alloc_node(parent_node, name, type, permissions);
    if (!node) {
        exit_critical(flags);
        serial_write_string("ramfs: out of memory creating node\n");
        return NULL;
    }

    ramfs_dirent_t* entry = (ramfs_dirent_t*)malloc(sizeof(ramfs_dirent_t));
    if (!entry) {
        exit_critical(flags);
        free(node->fs_data);
        free(node);
        return NULL;
    }
    entry->node = node;
    entry->next = pd->dir.children;
    pd->dir.children = entry;
    pd->dir.child_count++;
    exit_critical(flags);
    return node;
}

static int ramfs_readdir(vfs_node_t* node, size_t idx, struct dirent* out) {
    if (!node || !out) return -1;
    ramfs_node_data_t* d = (ramfs_node_data_t*)node->fs_data;
    unsigned long flags = enter_critical();
    size_t i = 0;
    for (ramfs_dirent_t* e = d->dir.children; e; e = e->next, i++) {
        if (i == idx) {
            safe_strncpy(out->d_name, e->node->name, VFS_MAX_NAME);
            out->d_type = e->node->type;
            exit_critical(flags);
            return 0;
        }
    }
    exit_critical(flags);
    return -1;
}

static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    ramfs_node_data_t* d = (ramfs_node_data_t*)node->fs_data;
    unsigned long flags = enter_critical();
    for (ramfs_dirent_t* e = d->dir.children; e; e = e->next) {
        if (strcmp(e->node->name, name) == 0) {
            exit_critical(flags);
            return e->node;
        }
    }
    exit_critical(flags);
    return NULL;
}

/* --- delete --- */

int ramfs_delete_node(vfs_node_t* node) {
    if (!node) return -1;
    ramfs_node_data_t* d = (ramfs_node_data_t*)node->fs_data;
    vfs_node_t* parent_node = d->parent;
    if (!parent_node) return -1; // can't delete root

    unsigned long flags = enter_critical();

    if (node->type == VFS_NODE_DIR && d->dir.child_count != 0) {
        exit_critical(flags);
        return -1; // must be empty
    }

    ramfs_node_data_t* pd = (ramfs_node_data_t*)parent_node->fs_data;
    ramfs_dirent_t** link = &pd->dir.children;
    while (*link && (*link)->node != node) link = &(*link)->next;
    if (!*link) {
        exit_critical(flags);
        return -1;
    }

    ramfs_dirent_t* entry = *link;
    *link = entry->next;
    free(entry);
    if (pd->dir.child_count) pd->dir.child_count--;
    exit_critical(flags);

    if (node->type == VFS_NODE_FILE) ramfs_free_file_blocks(d);
    free(d);
    free(node);
    return 0;
}

/* --- init --- */

vfs_node_t* ramfs_init(void) {
    return ramfs_alloc_node(NULL, "/", VFS_NODE_DIR, 0755);
}
