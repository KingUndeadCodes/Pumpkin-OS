#include "./ramfs.h"

static char ram_disk[RAM_DISK_SIZE];
static RamFile files[MAX_FILES];

FILE *ram_fopen(const char *filename, const char *mode) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && strcmp(files[i].name, filename) == 0) {
            files[i].position = 0;
            return (FILE *)&files[i];
        }
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].in_use) {
            strncpy(files[i].name, filename, sizeof(files[i].name) - 1);
            files[i].data = ram_disk + (i * MAX_FILE_SIZE);
            files[i].size = 0;
            files[i].position = 0;
            files[i].in_use = 1;
            return (FILE *)&files[i];
        }
    }
    
    return NULL; // No space for new file
}

size_t ram_fread(void *ptr, size_t size, size_t count, FILE *stream) {
    RamFile *file = (RamFile *)stream;
    if (!file->in_use) return 0;
    
    size_t bytes_to_read = size * count;
    if (file->position + bytes_to_read > file->size)
        bytes_to_read = file->size - file->position;
    
    memcpy(ptr, file->data + file->position, bytes_to_read);
    file->position += bytes_to_read;
    return bytes_to_read / size;
}

size_t ram_fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    RamFile *file = (RamFile *)stream;
    if (!file->in_use) return 0;
    
    size_t bytes_to_write = size * count;
    if (file->position + bytes_to_write > MAX_FILE_SIZE)
        bytes_to_write = MAX_FILE_SIZE - file->position;
    
    memcpy(file->data + file->position, ptr, bytes_to_write);
    file->position += bytes_to_write;
    if (file->position > file->size)
        file->size = file->position;
    return bytes_to_write / size;
}

int ram_fseek(FILE *stream, long offset, int whence) {
    RamFile *file = (RamFile *)stream;
    if (!file->in_use) return -1;
    
    size_t new_pos;
    if (whence == SEEK_SET) new_pos = offset;
    else if (whence == SEEK_CUR) new_pos = file->position + offset;
    else if (whence == SEEK_END) new_pos = file->size + offset;
    else return -1;
    
    if (new_pos > file->size) return -1;
    file->position = new_pos;
    return 0;
}

int ram_fclose(FILE *stream) {
    return 0; // Nothing to do
}