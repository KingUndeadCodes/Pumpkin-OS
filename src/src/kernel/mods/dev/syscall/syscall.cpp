#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "syscall.h"
#include "../ramfs/ramfs.h"
#include "../serial/serial.h"
#include "../kb/kb.h"
#include "../tasking/tasking.h"
#include "../paging/paging.h"

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_fn))
#define MAX_SYSCALL_FDS 16
#define SYS_OPEN_MAX_PATH 256

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

// See docs/DOCS.md ("mods/dev/syscall/syscall.cpp -- ring-3 syscall gate
// (Phase 1)"). Ring 0/1 callers are already trusted and unaffected; a
// no-op for them, same as before this existed.
static bool syscall_buf_ok(uint32_t buf, uint32_t size) {
    if (g_current->ring != 3) return true;
    return is_user_accessible((uintptr_t)buf, (size_t)size);
}

syscall_t sys_open(uint32_t path, uint32_t flags, uint32_t, uint32_t, uint32_t) {
    if (g_current->ring == 3) {
        if (!is_user_accessible((uintptr_t)path, SYS_OPEN_MAX_PATH)) return (uint32_t)-1;
        bool terminated = false;
        for (uint32_t i = 0; i < SYS_OPEN_MAX_PATH; i++) {
            if (((const char*)path)[i] == '\0') { terminated = true; break; }
        }
        if (!terminated) return (uint32_t)-1;
    }

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
// See docs/DOCS.md ("mods/dev/syscall/syscall.cpp — stdin keyboard
// ownership") for why this is a small LIFO stack of readers rather than
// one global: with real task blocking (tasking Phase 3), more than one
// task can legitimately be mid-sys_read on stdin at once. Whichever task
// most recently started reading owns the keyboard exclusively -- the
// same nesting a stack of modal dialogs would have -- until it
// completes, at which point ownership reverts to whichever task (if any)
// was reading before it.
#define MAX_STDIN_DEPTH 8

struct StdinFrame {
    task_t*  waiter;
    char*    buf;
    uint32_t size;
    uint32_t pos;
};

static StdinFrame stdin_stack[MAX_STDIN_DEPTH];
static int stdin_stack_depth = 0;
static int stdin_kb_event_id = -1;

bool stdin_is_reading(void) { return stdin_stack_depth > 0; }

static void stdin_kb_callback(char key, bool shift, bool meta, unsigned char scancode) {
    if (!key) return;
    if (stdin_stack_depth == 0) return;
    StdinFrame* top = &stdin_stack[stdin_stack_depth - 1];
    if (key == '\n') {
        if (top->waiter) task_wake(top->waiter);
        return;
    }
    if (top->pos < top->size - 1) {
        top->buf[top->pos++] = key;
    }
}

static uint32_t stdin_read_line(char* buf, uint32_t size) {
    if (size == 0) return 0;
    if (stdin_stack_depth >= MAX_STDIN_DEPTH) return 0;

    StdinFrame* frame = &stdin_stack[stdin_stack_depth];
    frame->buf = buf;
    frame->size = size;
    frame->pos = 0;
    frame->waiter = g_current;
    stdin_stack_depth++;

    // Only register once, when the stack goes empty -> non-empty -- every
    // frame shares the one stdin_kb_callback registration (kb_add_event()
    // is already idempotent for a repeat callback), so it must stay
    // registered as long as *any* frame is waiting, not just the top one.
    if (stdin_stack_depth == 1) {
        stdin_kb_event_id = kb_add_event(stdin_kb_callback);
    }

    // task_block() yields to another READY task (real concurrency) instead
    // of hlt-spinning until any interrupt fires, and only returns once
    // stdin_kb_callback calls task_wake() on us from the keyboard IRQ once
    // Enter is pressed -- no separate "done" flag to poll anymore. Only
    // the top frame ever receives '\n', so only the top frame's task can
    // ever be woken -- we're guaranteed to still be on top when this
    // returns, since nothing pushed after us could have completed first.
    task_block();

    stdin_stack_depth--;
    if (stdin_stack_depth == 0) {
        kb_remove_event(stdin_kb_event_id);
        stdin_kb_event_id = -1;
    }

    buf[frame->pos] = '\0';
    return frame->pos;
}

syscall_t sys_read(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    if (!syscall_buf_ok(buf, size)) return (uint32_t)-1;
    if (fd == 0) {
        return stdin_read_line((char*)buf, size);
    }
    if (fd >= MAX_SYSCALL_FDS || !syscall_fd_table[fd]) return 0;
    return (uint32_t)fread((void*)buf, 1, size, syscall_fd_table[fd]);
}

syscall_t sys_write(uint32_t fd, uint32_t buf, uint32_t size, uint32_t, uint32_t) {
    if (!syscall_buf_ok(buf, size)) return (uint32_t)-1;
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
