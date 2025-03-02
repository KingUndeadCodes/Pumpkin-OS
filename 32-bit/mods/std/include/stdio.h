#ifndef __STDIO_H
#define __STDIO_H

#include "../../dev/ramfs/ramfs.h"
#include "./io/file.h"
#include <stddef.h>
#include <string.h>

FILE* fopen(const char *filename, const char *mode);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int fclose(FILE *stream);

/*
int fprintf(FILE *stream, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
char* fgets(char *s, int n, FILE *stream);
int fputs(const char *s, FILE *stream);
long int ftell(FILE *stream);
void rewind(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
*/

#endif