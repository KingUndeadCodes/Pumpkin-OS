#ifndef __IO_FILE_H
#define __IO_FILE_H

#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;

typedef FILE*   (*_fopen_t)(const char *filename, const char *mode);
typedef int     (*_fclose_t)(FILE *stream);
typedef int     (*_fseek_t)(FILE *stream, long offset, int whence);
typedef size_t  (*_fread_t)(void *ptr, size_t size, size_t count, FILE *stream);
typedef size_t  (*_fwrite_t)(const void *ptr, size_t size, size_t count, FILE *stream);

/*
struct FILE {
    int      fd;         // File descriptor number
    void    *fs_data;    // Filesystem/device-specific data (inode, etc.)
    size_t   size;       // File size (if kno wn)
    size_t   position;   // Current position in file
    int      in_use;     // Used by file table
    int      mode;       // Open mode (read/write/append)
    int      error;      // Last error code

    _fclose_t _close;
    _fseek_t  _seek;
    _fread_t  _read;
    _fwrite_t _write;
};
*/

struct FILE {
    int      fd;         // File descriptor number (index into file table)
    size_t   position;   // Current position in file
    int      flags;      // Open mode, status, etc.
    int      error;      // Last error code
};


#endif