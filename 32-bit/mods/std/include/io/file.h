#ifndef __IO_FILE_H
#define __IO_FILE_H

#include <stddef.h>

#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

typedef struct {
    char name[32];
    char *data;
    size_t size;
    size_t position;
    int in_use;
} FILE;

#endif