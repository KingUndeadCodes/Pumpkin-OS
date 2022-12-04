#ifndef _SERIAL_H
#define _SERIAL_H
#include <string.h>
#include <stdlib.h>
#include "../port.cpp"

#define PORT 0x3f8          // COM1

// static int init_serial();
// char read_serial();
// void write_serial(char a);
void serial_write_string(const char* string, bool time_show = true);

#endif