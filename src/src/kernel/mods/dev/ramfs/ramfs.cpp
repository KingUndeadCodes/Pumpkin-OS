#include "ramfs.h"
#include <string.h>

// Pools (no malloc)
static ramfs_dir_t dirs_pool[RAMFS_MAX_TOTAL_DIRS];
static uint8_t dirs_used[RAMFS_MAX_TOTAL_DIRS];

static ramfs_file_t files_pool[RAMFS_MAX_TOTAL_FILES];
static uint8_t files_used[RAMFS_MAX_TOTAL_FILES];

/* helper: safe strncpy */
static void safe_strncpy(char *dst, const char *src, size_t n) {
    if (n == 0) return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

/* pool alloc/free helpers */
static ramfs_dir_t* alloc_dir(void) {
    for (size_t i = 0; i < RAMFS_MAX_TOTAL_DIRS; ++i) {
        if (!dirs_used[i]) {
            dirs_used[i] = 1;
            memset(&dirs_pool[i], 0, sizeof(ramfs_dir_t));
            return &dirs_pool[i];
        }
    }
    serial_write_string("ramfs: no free dir in pool\n");
    return NULL;
}

static void free_dir(ramfs_dir_t* d) {
    if (!d) return;
    size_t idx = d - dirs_pool;
    if (idx < RAMFS_MAX_TOTAL_DIRS) dirs_used[idx] = 0;
}

static ramfs_file_t* alloc_file(void) {
    for (size_t i = 0; i < RAMFS_MAX_TOTAL_FILES; ++i) {
        if (!files_used[i]) {
            files_used[i] = 1;
            memset(&files_pool[i], 0, sizeof(ramfs_file_t));
            return &files_pool[i];
        }
    }
    serial_write_string("ramfs: no free file in pool\n");
    return NULL;
}
static void free_file(ramfs_file_t* f) {
    if (!f) return;
    size_t idx = f - files_pool;
    if (idx < RAMFS_MAX_TOTAL_FILES) files_used[idx] = 0;
}

/* --- File ops --- */
static size_t ramfs_read(vfs_node_t* node, size_t offset, size_t size, void* buf) {
    if (!node || !buf) return 0;
    ramfs_file_t* file = (ramfs_file_t*)node;
    if (offset >= file->size) return 0;
    if (offset + size > file->size) size = file->size - offset;
    memcpy(buf, file->data + offset, size);
    return size;
}

static size_t ramfs_write(vfs_node_t* node, size_t offset, size_t size, const void* buf) {
    if (!node || !buf) return 0;
    ramfs_file_t* file = (ramfs_file_t*)node;

    if (offset >= RAMFS_MAX_FILE_SIZE) return 0;
    if (offset + size > RAMFS_MAX_FILE_SIZE) size = RAMFS_MAX_FILE_SIZE - offset;

    memcpy(file->data + offset, buf, size);
    if (offset + size > file->size) file->size = offset + size;
    node->size = file->size;
    return size;
}

/* --- Directory helpers --- */
static int ramfs_has_file(ramfs_dir_t* dir, const char* name) {
    for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (dir->files[i] && strcmp(dir->files[i]->node.name, name) == 0) return 1;
    }
    return 0;
}
static int ramfs_has_dir(ramfs_dir_t* dir, const char* name) {
    for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) {
        if (dir->subdirs[i] && strcmp(dir->subdirs[i]->node.name, name) == 0) return 1;
    }
    return 0;
}

/* --- Create --- */
static ramfs_file_t* ramfs_create_file(ramfs_dir_t* dir, const char* name, int permissions) {
    if (!dir || !name) {
        serial_write_string("ramfs: create_file invalid args\n");
        return NULL;
    }
    if (ramfs_has_file(dir, name) || ramfs_has_dir(dir, name)) {
        serial_write_string("ramfs: create_file duplicate\n");
        return NULL;
    }
    /* find free slot in directory */
    size_t slot = (size_t)-1;
    for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (dir->files[i] == NULL) { slot = i; break; }
    }
    if (slot == (size_t)-1) {
        serial_write_string("ramfs: create_file dir full\n");
        return NULL;
    }
    ramfs_file_t* file = alloc_file();
    if (!file) return NULL;

    safe_strncpy(file->node.name, name, RAMFS_MAX_NAME);
    file->node.type = VFS_NODE_FILE;
    file->node.permissions = permissions;
    file->node.read = ramfs_read;
    file->node.write = ramfs_write;
    file->node.finddir = NULL;
    file->node.readdir = NULL;
    file->node.create = NULL;
    file->parent = dir;
    file->size = 0;

    dir->files[slot] = file;
    dir->file_count++;
    return file;
}

static ramfs_dir_t* ramfs_create_dir(ramfs_dir_t* parent, const char* name, int permissions) {
    if (!parent || !name) return NULL;
    if (ramfs_has_dir(parent, name) || ramfs_has_file(parent, name)) {
        serial_write_string("ramfs: create_dir duplicate\n");
        return NULL;
    }
    size_t slot = (size_t)-1;
    for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) {
        if (parent->subdirs[i] == NULL) { slot = i; break; }
    }
    if (slot == (size_t)-1) {
        serial_write_string("ramfs: create_dir parent full\n");
        return NULL;
    }
    ramfs_dir_t* dir = alloc_dir();
    if (!dir) return NULL;

    safe_strncpy(dir->node.name, name, RAMFS_MAX_NAME);
    dir->node.type = VFS_NODE_DIR;
    dir->node.permissions = permissions;
    dir->node.readdir = ramfs_readdir;
    dir->node.finddir = ramfs_finddir;
    dir->node.create = ramfs_create_node;
    dir->node.read = NULL;
    dir->node.write = NULL;
    dir->parent = parent;
    dir->dir_count = 0;
    dir->file_count = 0;

    parent->subdirs[slot] = dir;
    parent->dir_count++;
    return dir;
}

/* --- vfs wrappers --- */
static int ramfs_readdir(vfs_node_t* node, size_t idx, struct dirent* out) {
    if (!node || !out) return -1;
    ramfs_dir_t* dir = (ramfs_dir_t*)node;
    /* enumerate subdirs first */
    size_t d = 0;
    for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) {
        if (dir->subdirs[i]) {
            if (d == idx) {
                safe_strncpy(out->d_name, dir->subdirs[i]->node.name, RAMFS_MAX_NAME);
                out->d_type = VFS_NODE_DIR;
                return 0;
            }
            d++;
        }
    }
    size_t f = 0;
    for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (dir->files[i]) {
            if (d + f == idx) {
                safe_strncpy(out->d_name, dir->files[i]->node.name, RAMFS_MAX_NAME);
                out->d_type = VFS_NODE_FILE;
                return 0;
            }
            f++;
        }
    }
    return -1;
}

static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    ramfs_dir_t* dir = (ramfs_dir_t*)node;
    for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) {
        if (dir->subdirs[i] && strcmp(dir->subdirs[i]->node.name, name) == 0)
            return &dir->subdirs[i]->node;
    }
    for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (dir->files[i] && strcmp(dir->files[i]->node.name, name) == 0)
            return &dir->files[i]->node;
    }
    return NULL;
}

static vfs_node_t* ramfs_create_node(vfs_node_t* parent_node, const char* name, int type, int permissions) {
    if (!parent_node || !name) return NULL;
    ramfs_dir_t* parent = (ramfs_dir_t*)parent_node;
    if (type == VFS_NODE_FILE) {
        ramfs_file_t* f = ramfs_create_file(parent, name, permissions);
        return f ? &f->node : NULL;
    } else if (type == VFS_NODE_DIR) {
        ramfs_dir_t* d = ramfs_create_dir(parent, name, permissions);
        return d ? &d->node : NULL;
    }
    return NULL;
}

/* --- delete --- */
int ramfs_delete_node(vfs_node_t* node) {
    if (!node) return -1;
    if (node->type == VFS_NODE_FILE) {
        ramfs_file_t* file = (ramfs_file_t*)node;
        ramfs_dir_t* parent = file->parent;
        if (!parent) return -1;
        /* find index */
        size_t idx = (size_t)-1;
        for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) {
            if (parent->files[i] == file) { idx = i; break; }
        }
        if (idx == (size_t)-1) return -1;
        parent->files[idx] = NULL;
        if (parent->file_count) parent->file_count--;
        free_file(file);
        return 0;
    } else if (node->type == VFS_NODE_DIR) {
        ramfs_dir_t* dir = (ramfs_dir_t*)node;
        ramfs_dir_t* parent = dir->parent;
        if (!parent) return -1;
        /* ensure empty */
        for (size_t i = 0; i < RAMFS_MAX_FILES; ++i) if (dir->files[i]) return -1;
        for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) if (dir->subdirs[i]) return -1;
        /* find index in parent */
        size_t idx = (size_t)-1;
        for (size_t i = 0; i < RAMFS_MAX_DIRS; ++i) {
            if (parent->subdirs[i] == dir) { idx = i; break; }
        }
        if (idx == (size_t)-1) return -1;
        parent->subdirs[idx] = NULL;
        if (parent->dir_count) parent->dir_count--;
        free_dir(dir);
        return 0;
    }
    return -1;
}

/* --- init --- */
vfs_node_t* ramfs_init(void) {
    memset(dirs_used, 0, sizeof(dirs_used));
    memset(files_used, 0, sizeof(files_used));
    /* allocate root dir from pool directly */
    ramfs_dir_t* root = alloc_dir();
    if (!root) return NULL;
    safe_strncpy(root->node.name, "/", RAMFS_MAX_NAME);
    root->node.type = VFS_NODE_DIR;
    root->node.permissions = 0755;
    root->node.readdir = ramfs_readdir;
    root->node.finddir = ramfs_finddir;
    root->node.create = ramfs_create_node;
    root->node.read = NULL;
    root->node.write = NULL;
    root->node.size = 0;
    root->parent = NULL;
    root->dir_count = 0;
    root->file_count = 0;
    return &root->node;
}
