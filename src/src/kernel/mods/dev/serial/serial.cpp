#include "serial.h"
#include "../port.cpp"
#include "../cmos/cmos.h"

/*
static int init_serial() {
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03);
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);
    outb(PORT + 2, 0xC7);
    outb(PORT + 4, 0x0B);
    outb(PORT + 4, 0x1E);
    outb(PORT + 0, 0xAE);
    if(inb(PORT + 0) != 0xAE) return -1;
    outb(PORT + 4, 0x0F);
    return 0;
}
*/

inline int serial_received() { 
    return inb(PORT + 5) & 1; 
}

inline int is_transmit_empty() {
    return inb(PORT + 5) & 0x20;
}
 
char read_serial() {
    while (serial_received() == 0); 
    return inb(PORT);
}

void write_serial(char a) {
    while (is_transmit_empty() == 0);
    outb(PORT,a);
}

void __serial_write_string(const char* string) {
    for (int i = 0; i < strlen(string); i++) {
        write_serial(string[i]);
    }
}

void serial_write_time() {
    CMOSTime T = FetchCurrentCMOSTime();
    __serial_write_string("\033[1;33m");
    __serial_write_string(itoa(T.century*100 + T.year, 10));
    write_serial('-');
     __serial_write_string(itoa(T.month, 10));
    write_serial('-');
    __serial_write_string(itoa(T.month_day, 10));
    write_serial(' ');
    __serial_write_string(itoa(T.hours, 10));
    write_serial(':');
    __serial_write_string(itoa(T.minutes, 10));
    write_serial(':');
    __serial_write_string(itoa(T.seconds, 10));
    __serial_write_string("\033[0m");
    write_serial(' ');
    return;
}

void serial_write_string(const char* string, bool time_show = true, enum Types Type = INFO) {
    if (time_show == true) serial_write_time();
    switch (Type) {
        case INFO: __serial_write_string("\033[0;34mINFO\033[0m - "); break;
        case WARN: __serial_write_string("\033[1;33mWARN\033[0m - "); break;
        case FAIL: __serial_write_string("\033[0;31mFAIL\033[0m - "); break;
        default: break;
    }
    __serial_write_string(string);
}

// Adapts serial_write_string()'s signature to the LogDevice::log callback shape.
void serial_log_adapter(const char* message) {
    serial_write_string(message);
}

inline bool serial_token_is_int(int c) {
    bool isNumber = false;
    for (int i = 48; i < 58; i++) {
        if (c == i) {
            isNumber = true;
            break;
        }      
    } 
    return isNumber;
}

int vprintf_serial(const char* format, va_list list) {
    for (int i = 0; format[i]; ++i) {
        if (format[i] == '%') {
            switch ((char)format[++i]) {
                case 'c': {
                    char c[] = {(char)va_arg(list, int), '\0'};
                    __serial_write_string(c);
                    break;
                }
                case 's': {
                    __serial_write_string(va_arg(list, char*));
                    break;
                }
                case 'd':
                case 'i': {
                    __serial_write_string(itoa(va_arg(list, int), 10));
                    break;
                }
                case 'u':
                case 'o':
                case 'x':
                case 'X': {
                    __serial_write_string(itoa(va_arg(list, unsigned int), 10));
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(list, void*);
                    __serial_write_string("0x");
                    __serial_write_string(itoa((uintptr_t)ptr, 16));
                    break;
                }
                default: {
                    char c[] = {format[i], '\0'};
                    __serial_write_string(c);
                    break;
                }
            }
            continue;
        } else {
            char c[] = {format[i], '\0'};
            __serial_write_string(c);
        }
    }
    return 0;
};

__attribute__ ((format (printf, 3, 4))) int printf_serial(bool time_show, enum Types Type, const char* format, ...) {
    if (time_show) serial_write_time();
    switch (Type) {
        case INFO: __serial_write_string("\033[0;34mINFO\033[0m - "); break;
        case WARN: __serial_write_string("\033[1;33mWARN\033[0m - "); break;
        case FAIL: __serial_write_string("\033[0;31mFAIL\033[0m - "); break;
        default: break;
    }
    va_list list;
    va_start(list, format);
    int result = vprintf_serial(format, list);
    va_end(list);
    return result;
}