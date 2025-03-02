#include "./include/stdio.h"

typedef FILE* (*_fopen)(const char *filename, const char *mode);
typedef int (*_fclose)(FILE *stream);
typedef int (*_fseek)(FILE *stream, long offset, int whence);
typedef size_t (*_fread)(void *ptr, size_t size, size_t count, FILE *stream);
typedef size_t (*_fwrite)(const void *ptr, size_t size, size_t count, FILE *stream);

typedef struct {
    _fopen openFile;
    _fclose closeFile;
    _fseek seekFile;
    _fread readFile;
    _fwrite writeFile;
} FSNavigator;

static FSNavigator filesystem = {
    .openFile = ram_fopen,
    .closeFile = ram_fclose,
    .seekFile = ram_fseek,
    .readFile = ram_fread,
    .writeFile = ram_fwrite
};

FILE* fopen(const char *filename, const char *mode) {
    return filesystem.openFile(filename, mode);
};

int fclose(FILE *stream) {
    return filesystem.closeFile(stream);
};

int fseek(FILE *stream, long int offset, int whence) {
    return filesystem.seekFile(stream, offset, whence);
};

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    return filesystem.readFile(ptr, size, count, stream);
};

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    return filesystem.writeFile(ptr, size, count, stream);
};