#ifndef __BOOTSYSLIB_H__
#define __BOOTSYSLIB_H__ 1

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

// Output functions
void outb(uint16_t port, uint8_t data);
// Input functions
uint8_t inb(uint16_t port);

/*
namespace Cursor {
    void disbaleCursor();
    void enableCursor(uint8_t cursor_start, uint8_t cursor_end);
    void moveCursor(int x, int y);
    uint16_t getCursor(void);
};
*/

size_t strlen(const char* str);
void print(const char* string, uint8_t color = 15);
void reverse(char s[]);
const char* itoa_signed(int n, int base, char* dest);

#endif