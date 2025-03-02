#ifndef _RAMFS_H
#define _RAMFS_H

#include "../../std/include/io/file.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define RAM_DISK_SIZE (1024 * 1024) // 1MB RAM disk
#define MAX_FILES 16
#define MAX_FILE_SIZE (RAM_DISK_SIZE / MAX_FILES)

typedef struct {
    char name[32];
    char *data;
    size_t size;
    size_t position;
    int in_use;
} RamFile;

FILE *ram_fopen(const char *filename, const char *mode);
size_t ram_fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t ram_fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int ram_fseek(FILE *stream, long offset, int whence);
int ram_fclose(FILE *stream);

#endif