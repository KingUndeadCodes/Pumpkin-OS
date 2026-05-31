#ifndef _SERIAL_H
#define _SERIAL_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../port.cpp"

#define PORT 0x3f8          // COM1

enum Types {
    INFO,
    WARN,
    FAIL, 
    NONE,
};

// static int init_serial();
char read_serial();
void write_serial(char a);
void serial_write_string(const char* string, bool time_show = true, enum Types Type = INFO);

__attribute__ ((format (printf, 3, 4))) int printf_serial(bool time_show, enum Types Type, const char* format, ...);

#endif