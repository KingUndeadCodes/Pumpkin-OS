#ifndef _SERIAL_H
#define _SERIAL_H
#include <string.h>
#include <stdlib.h>
#include "../port.cpp"

#define PORT 0x3f8          // COM1

enum Types {
    // DEBUG,
    INFO,
    // INFORMATION,
    WARN,
    // WARNING,
    // ERROR, 
    FAIL, 
    NONE,
    // FAILURE
};

// static int init_serial();
// char read_serial();
// void write_serial(char a);
void serial_write_string(const char* string, bool time_show = true, enum Types Type = INFO);

#endif