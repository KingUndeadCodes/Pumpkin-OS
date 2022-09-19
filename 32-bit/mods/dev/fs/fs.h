#ifndef FS_H
#define FS_H

#include <text.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

// Should be in it's own
typedef void* test_t;

#define NULL 0
#define EOF '\0'   /* End of File */

struct Node {
    /* Index of the Previous Node */
    uint32_t n_prev;
    /* Node index */
    uint32_t n_index;
    /* Node data */
    char n_data[4096 - (sizeof(uint32_t) * 2)];
};

struct File {
    /* File Name */
    char f_name[255];
};

struct Node init_node(uint32_t* previous);
void write_file(struct File F, const char* data);
char* read_file(struct File F);
void init_file(struct File F);
void destroy_fs(void);
test_t FS_TEST();


// Hmmm... I wonder what this WILL be for.
// enum FLOPPY_INFO {
//     FLOPPY_SECTOR_LENGTH = 512,
//     FLOPPY_SECTORS_PER_TRACK = 18,
//     FLOPPY_TRACKS_PER_SIDE = 80,
//     FLOPPY_SIDES_PER_DISK = 2,
// };

// const short SECTORS_PER_NODE = 8;
// const short MAX_NODES = (FLOPPY_SECTORS_PER_TRACK * FLOPPY_TRACKS_PER_SIDE * FLOPPY_SIDES_PER_DISK) / SECTORS_PER_NODE; 

#endif