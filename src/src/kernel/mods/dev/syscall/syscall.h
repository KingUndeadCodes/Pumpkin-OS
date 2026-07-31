#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stdint.h>

typedef uint32_t (*syscall_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

enum {
    SYS_O_READ   = 0,
    SYS_O_WRITE  = 1,
    SYS_O_APPEND = 2,
};

uint32_t sys_open(uint32_t path, uint32_t flags, uint32_t, uint32_t, uint32_t);
uint32_t sys_read(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t);
uint32_t sys_write(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t);
uint32_t sys_exit(uint32_t code, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t sys_close(uint32_t fd, uint32_t, uint32_t, uint32_t, uint32_t);

extern "C" uint32_t syscall_dispatch(uint32_t syscall_number, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

// True for the duration of stdin_read_line()'s blocking wait, so other keyboard
// consumers (e.g. Wingman) know not to also process the same keystrokes.
bool stdin_is_reading(void);

#endif