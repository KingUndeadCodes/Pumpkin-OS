#include <stdint.h>
#include <string.h>
#include "../ramfs/ramfs.h"
#include "../serial/serial.h"

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_fn))

typedef uint32_t syscall_t;
typedef uint32_t (*syscall_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

// map<syscall_t, FILE>

syscall_t sys_open(uint32_t path, uint32_t flags, uint32_t, uint32_t, uint32_t) {
    // ram_fopen((char *)path, flags);
    char* path_str = (char*)path;
    strcat(path_str, ">\0");
    serial_write_string(strcat("Serial Open <", (const char*)path));
    return 0;
}

syscall_t sys_read(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    serial_write_string("Serial Read");
    return 0;
}

syscall_t sys_write(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    serial_write_string("Serial Write");
    return 0;
}

syscall_t sys_exit(uint32_t code, uint32_t, uint32_t, uint32_t, uint32_t) {
    serial_write_string("Serial Exit");
    return 0;
}

const syscall_fn syscall_table[] = {
    sys_open,
    sys_read,
    sys_write,
    sys_exit,
};

/*
mov eax, 0x0 ; syscall number
mov ebx, 0x0 ; arg1
mov ecx, 0x0 ; arg2
mov edx, 0x0 ; arg3
mov esi, 0x0 ; arg4
mov edi, 0x0 ; arg5
int 0x80
*/

extern "C" uint32_t syscall_dispatch(uint32_t syscall_number, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    if (syscall_number >= SYSCALL_COUNT) {
        serial_write_string("Invalid syscall (syscall: ");
        serial_write_string(itoa(syscall_number, 10), false, NONE);
        serial_write_string(") <", false, NONE);
        uint32_t argvs[] = {arg1, arg2, arg3, arg4, arg5};
        for (uint32_t i = 0; i < 5; i++) {
            serial_write_string(itoa(argvs[i], 10), false, NONE);
            if (i < 4) serial_write_string(", ", false, NONE);
        }
        serial_write_string(">\n", false, NONE);
        return -1; // Invalid syscall
    } else {
        return syscall_table[syscall_number](arg1, arg2, arg3, arg4, arg5);
    }
}
