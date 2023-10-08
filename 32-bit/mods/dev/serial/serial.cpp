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

void serial_terminal_start(void) {
    const uint16_t MAX_COMMAND_LENGTH = 256;
    char* command = malloc(MAX_COMMAND_LENGTH * sizeof(char)); 
    int pointer = 0;
    serial_write_string("$ ", false, NONE);
    while (true) {
        command[pointer++] = read_serial();
        // printf("%c", command[pointer - 1]);
        if (command[pointer - 1] == 27) {
            if (read_serial() == '[') {
                switch (read_serial()) {
                    case 'A': break; // up
                    case 'B': break; // down
                    case 'C': break; // right
                    case 'D': break; // left
                    default:  break; // other
                }
                command[pointer - 1] = 0;
                pointer -= 1;
                continue; 
            }
        } 
        write_serial(command[pointer - 1]);
        if (command[pointer - 1] == 127) {
            if (pointer - 1 == 0) {
                pointer--;
                continue;
            }
            pointer -= 1;
            command[pointer] = 0;
            pointer -= 1;
            serial_write_string("\b", false, NONE);
            continue;
        }
        else if (command[pointer - 1] == 13) {
            char* command_copy = command;
            char* token = strtok(command_copy, " ");
            if (token != NULL && strcmp(token, "r") == 0) {
                // reading address
                char* returned_string = (char*)malloc(128);
                uint16_t port = (uint16_t)atoi(strtok(NULL, " "));
                strcpy(returned_string, "[R] ");
                strcat(returned_string, itoa(port, 10));
                strcat(returned_string, " : ");
                int rvalue = (int)inb(port);
                serial_write_string((const char*)returned_string, false, NONE);
                serial_write_string(itoa(rvalue, 10), false, NONE);
                free(returned_string);
            } else if (token != NULL && strcmp(token, "w") == 0) {
                // writing address
                char* returned_string = (char*)malloc(128);
                uint16_t port = (uint16_t)atoi(strtok(NULL, " "));
                uint8_t value = (uint8_t)atoi(strtok(NULL, " "));
                strcpy(returned_string, "[W] ");
                strcat(returned_string, itoa(port, 10));
                strcat(returned_string, ", ");
                strcat(returned_string, itoa(value, 10));
                strcat(returned_string, " : ");
                /* int rvalue = outb(port, value); */
                outb(port, value);
                serial_write_string((const char*)returned_string, false, NONE);
                serial_write_string(itoa(0, 10), false, NONE);
                free(returned_string);
            } 
            // else if (token != NULL && strcmp(token, "s") == 0) {
            //     serial_write_string("swapping address: ", false, NONE);
            //     serial_write_string("incomplete", false, NONE);
            // } 
            else if (token != NULL && strcmp(token, "h") == 0) {
                serial_write_string("[h]         | help\n", false, NONE);
                serial_write_string("[r] [a]     | read\n", false, NONE);
                serial_write_string("[s] [a] [a] | swap\n", false, NONE);
                serial_write_string("[w] [a] [v] | write", false, NONE);
            } else {
                serial_write_string("command not found: ", false, NONE);
                serial_write_string(command, false, NONE);
            }
            serial_write_string("\n$ ", false, NONE);
            for (int j = 0; j < 256; j++) command[j] = 0;
            pointer = 0;
            continue;
        }
    }
    // write_serial('\n');
}