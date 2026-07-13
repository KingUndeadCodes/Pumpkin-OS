#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "syscall.h"
#include "../ramfs/ramfs.h"
#include "../serial/serial.h"
#include "../kb/kb.h"
#include "../tasking/tasking.h"

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_fn))
#define MAX_SYSCALL_FDS 16

typedef uint32_t syscall_t;

// fd 0 = stdin (keyboard), fd 1/2 = stdout/stderr (serial), fd >= 3 = real VFS files
static FILE* syscall_fd_table[MAX_SYSCALL_FDS];

static int alloc_syscall_fd(FILE* f) {
    for (int i = 3; i < MAX_SYSCALL_FDS; i++) {
        if (!syscall_fd_table[i]) {
            syscall_fd_table[i] = f;
            return i;
        }
    }
    return -1;
}

syscall_t sys_open(uint32_t path, uint32_t flags, uint32_t, uint32_t, uint32_t) {
    const char* mode = "r";
    if (flags == SYS_O_WRITE) mode = "w";
    else if (flags == SYS_O_APPEND) mode = "a";

    FILE* f = fopen((const char*)path, mode);
    if (!f) return (uint32_t)-1;

    int fd = alloc_syscall_fd(f);
    if (fd < 0) {
        fclose(f);
        return (uint32_t)-1;
    }
    return (uint32_t)fd;
}

// --- blocking stdin (fd 0) support, driven by the keyboard event system ---
static char*    stdin_buf_ptr;
static uint32_t stdin_buf_size;
static uint32_t stdin_buf_pos;
static volatile bool stdin_line_done;
static volatile bool stdin_reading = false;

bool stdin_is_reading(void) { return stdin_reading; }

static void stdin_kb_callback(char key, bool shift, bool meta, unsigned char scancode) {
    if (!key) return;
    if (key == '\n') {
        stdin_line_done = true;
        return;
    }
    if (stdin_buf_pos < stdin_buf_size - 1) {
        stdin_buf_ptr[stdin_buf_pos++] = key;
    }
}

static uint32_t stdin_read_line(char* buf, uint32_t size) {
    if (size == 0) return 0;

    stdin_buf_ptr = buf;
    stdin_buf_size = size;
    stdin_buf_pos = 0;
    stdin_line_done = false;
    stdin_reading = true;

    int id = kb_add_event(stdin_kb_callback);
    // int 0x80 is an interrupt gate, so the CPU cleared IF on entry; without
    // re-enabling it here the keyboard IRQ this loop is waiting on can never
    // fire and hlt would spin forever. iretd restores the caller's original
    // EFLAGS at the end of the syscall regardless of what we do with IF here.
    asm volatile("sti");
    while (!stdin_line_done) {
        asm volatile("hlt");
    }
    kb_remove_event(id);
    stdin_reading = false;

    buf[stdin_buf_pos] = '\0';
    return stdin_buf_pos;
}

syscall_t sys_read(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    if (fd == 0) {
        return stdin_read_line((char*)buf, size);
    }
    if (fd >= MAX_SYSCALL_FDS || !syscall_fd_table[fd]) return 0;
    return (uint32_t)fread((void*)buf, 1, size, syscall_fd_table[fd]);
}

syscall_t sys_write(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    if (fd == 1 || fd == 2) {
        const char* data = (const char*)buf;
        for (uint32_t i = 0; i < size; i++) {
            write_serial(data[i]);
        }
        return size;
    }
    if (fd >= MAX_SYSCALL_FDS || !syscall_fd_table[fd]) return 0;
    return (uint32_t)fwrite((void*)buf, 1, size, syscall_fd_table[fd]);
}

syscall_t sys_close(uint32_t fd, uint32_t, uint32_t, uint32_t, uint32_t) {
    if (fd >= MAX_SYSCALL_FDS || !syscall_fd_table[fd]) return (uint32_t)-1;
    fclose(syscall_fd_table[fd]);
    syscall_fd_table[fd] = NULL;
    return 0;
}

syscall_t sys_exit(uint32_t code, uint32_t, uint32_t, uint32_t, uint32_t) {
    (void)code;
    task_exit();
    return 0; // unreachable: task_exit() never returns
}

const syscall_fn syscall_table[] = {
    sys_open,
    sys_read,
    sys_write,
    sys_exit,
    sys_close,
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
