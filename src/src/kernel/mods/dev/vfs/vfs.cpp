#include "vfs.h"
#include <string.h>
#include <stdlib.h>

#define VFS_MAX_MOUNTS 8
#define VFS_MAX_PATH   256   // path buffers should be bigger than a single name

static struct {
    char mount_path[VFS_MAX_PATH];  // store normalized absolute mount path
    vfs_node_t* root;
} mounts[VFS_MAX_MOUNTS];

static size_t mount_count = 0;
static vfs_node_t* vfs_root = NULL;

static void normalize_abs_path(const char* in, char* out, size_t out_sz) {
    // Produces:
    //  - always starts with '/'
    //  - no trailing '/' except root
    //  - collapses consecutive '/' into single '/'
    if (!out || out_sz == 0) return;
    out[0] = 0;

    if (!in || in[0] == 0) {
        strncpy(out, "/", out_sz);
        out[out_sz - 1] = 0;
        return;
    }

    size_t w = 0;

    // ensure leading '/'
    if (in[0] != '/') {
        out[w++] = '/';
    }

    char prev = 0;
    for (size_t r = 0; in[r] && w + 1 < out_sz; ++r) {
        char c = in[r];
        if (c == '/' && prev == '/') continue; // collapse //
        out[w++] = c;
        prev = c;
    }
    out[w] = 0;

    // trim trailing '/' except root
    size_t len = strlen(out);
    while (len > 1 && out[len - 1] == '/') {
        out[len - 1] = 0;
        len--;
    }

    // If it ended up empty, force "/"
    if (out[0] == 0) {
        strncpy(out, "/", out_sz);
        out[out_sz - 1] = 0;
    }
}

static int is_mount_prefix_match(const char* path, const char* mount_path) {
    // mount_path must match as a full path component prefix:
    // e.g. mount "/hello" matches "/hello" and "/hello/x"
    // but NOT "/hellothere"
    size_t len = strlen(mount_path);
    if (len == 0) return 0;
    if (strncmp(path, mount_path, len) != 0) return 0;

    char next = path[len];
    return (next == 0 || next == '/');
}

void vfs_init() {
    mount_count = 0;
    vfs_root = NULL;
    memset(mounts, 0, sizeof(mounts));
}

int vfs_mount(const char* path_in, vfs_node_t* root) {
    if (!root) return -1;
    if (mount_count >= VFS_MAX_MOUNTS) return -2;

    char path[VFS_MAX_PATH];
    normalize_abs_path(path_in, path, sizeof(path));

    // Mount root separately; do NOT also store "/" in mounts[]
    if (strcmp(path, "/") == 0) {
        if (vfs_root) return -3;
        vfs_root = root;
        return 0;
    }

    // Must have a root filesystem mounted to resolve mountpoints
    if (!vfs_root) return -4;

    // The mountpoint directory must exist and be a directory
    vfs_node_t* dir = vfs_find(path);
    if (!dir || dir->type != VFS_NODE_DIR) return -5;

    // See DOCS.md ("mods/dev/vfs/vfs.cpp" section) for why the limit/
    // double-mount checks are redone here, atomically with the write.
    unsigned long flags = enter_critical();
    if (mount_count >= VFS_MAX_MOUNTS || dir->mountpoint) {
        int rc = dir->mountpoint ? -6 : -2;
        exit_critical(flags);
        return rc;
    }

    dir->mountpoint = root;

    // Record in mount table for "longest prefix" selection
    strncpy(mounts[mount_count].mount_path, path, sizeof(mounts[mount_count].mount_path));
    mounts[mount_count].mount_path[sizeof(mounts[mount_count].mount_path) - 1] = 0;
    mounts[mount_count].root = root;
    mount_count++;
    exit_critical(flags);

    return 0;
}

static vfs_node_t* vfs_find_mount(const char* path_in, const char** rel_path_out) {
    if (!rel_path_out) return NULL;
    *rel_path_out = NULL;

    if (!vfs_root || !path_in) return NULL;

    // Support relative paths by treating them as "/<path>"
    const char* path = path_in;
    char tmp[VFS_MAX_PATH];
    if (path_in[0] != '/') {
        // build "/<relative>"
        tmp[0] = '/';
        strncpy(tmp + 1, path_in, sizeof(tmp) - 2);
        tmp[sizeof(tmp) - 1] = 0;
        path = tmp;
    }

    vfs_node_t* best = vfs_root;
    size_t best_len = 1;          // "/" length
    *rel_path_out = (path[0] == '/') ? path + 1 : path;

    // Find the longest mount_path that matches as a full component prefix
    unsigned long flags = enter_critical();
    for (size_t i = 0; i < mount_count; ++i) {
        if (mounts[i].mount_path[0] == 0 || !mounts[i].root) continue;

        if (is_mount_prefix_match(path, mounts[i].mount_path)) {
            size_t len = strlen(mounts[i].mount_path);
            if (len > best_len) {
                best = mounts[i].root;
                best_len = len;
                *rel_path_out = path + len;
                if (**rel_path_out == '/') (*rel_path_out)++;
            }
        }
    }
    exit_critical(flags);

    return best;
}

vfs_node_t* vfs_find(const char* path) {
    if (!path || !vfs_root) return NULL;

    const char* rel = NULL;
    vfs_node_t* node = vfs_find_mount(path, &rel);
    if (!node || !rel) return NULL;

    // If path is exactly the mount root, rel may be ""
    if (*rel == 0) return node;

    // Walk components
    while (*rel) {
        if (node->type != VFS_NODE_DIR) return NULL;
        if (!node->finddir) return NULL;

        char part[VFS_MAX_NAME];
        size_t i = 0;

        // parse one component
        while (*rel && *rel != '/') {
            if (i >= VFS_MAX_NAME - 1) {
                // component too long -> reject instead of truncating
                return NULL;
            }
            part[i++] = *rel++;
        }
        part[i] = 0;

        while (*rel == '/') rel++; // skip repeated slashes

        node = node->finddir(node, part);
        if (!node) return NULL;

        // If this node is a mountpoint directory, jump into mounted root
        if (node->mountpoint) node = node->mountpoint;
    }

    return node;
}

vfs_node_t* vfs_create(const char* path_in, int type, int permissions) {
    if (!path_in || !vfs_root) return NULL;

    // Normalize, and support relative paths.
    char path[VFS_MAX_PATH];
    normalize_abs_path(path_in, path, sizeof(path));

    // Reject creating "/" itself
    if (strcmp(path, "/") == 0) return NULL;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];

    const char* slash = strrchr(path, '/');
    if (!slash) return NULL;

    // name = after last slash
    strncpy(name, slash + 1, sizeof(name));
    name[sizeof(name) - 1] = 0;
    if (name[0] == 0) return NULL;

    // parent_path = before last slash
    if (slash == path) {
        // parent is root "/"
        strncpy(parent_path, "/", sizeof(parent_path));
        parent_path[sizeof(parent_path) - 1] = 0;
    } else {
        size_t plen = (size_t)(slash - path);
        if (plen >= sizeof(parent_path)) return NULL;
        memcpy(parent_path, path, plen);
        parent_path[plen] = 0;
    }

    vfs_node_t* parent = vfs_find(parent_path);
    if (!parent || parent->type != VFS_NODE_DIR || !parent->create) return NULL;

    return parent->create(parent, name, type, permissions);
}

int vfs_open(vfs_node_t* node) {
    (void)node;
    return 0;
}

int vfs_close(vfs_node_t* node) {
    (void)node;
    return 0;
}

size_t vfs_read(vfs_node_t* node, size_t offset, size_t size, void* buf) {
    if (node && node->read) return node->read(node, offset, size, buf);
    return 0;
}

size_t vfs_write(vfs_node_t* node, size_t offset, size_t size, const void* buf) {
    if (node && node->write) return node->write(node, offset, size, buf);
    return 0;
}

int vfs_readdir(vfs_node_t* node, size_t index, struct dirent* out) {
    if (node && node->readdir) return node->readdir(node, index, out);
    return -1;
}

vfs_node_t* vfs_finddir(vfs_node_t* node, const char* name) {
    if (node && node->finddir) return node->finddir(node, name);
    return NULL;
}
