#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS   32
#define KSTACK_SIZE (32 * 1024)

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t* saved_esp;      // points to regs frame (top of irq_common_stub frame)
    struct task* next;        // circular runqueue
    task_state_t state;
    uint32_t pid;
    void* stack_base;         // malloc'd stack to free on reap; NULL for statically-allocated stacks (never freed)
    // See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- ring-transition GDT/TSS").
    uint8_t  ring;             // target CPL: 0 for every existing task, 3 for a ring-3 task
    uint32_t kernel_stack_top; // written into TSS.ESP0 whenever this task is current
} task_t;

// exported scheduler state
extern task_t* g_current;
extern task_t* g_runqueue;
extern uint32_t g_next_pid;

void tasking_init(void);
// enqueue=false builds the task_t/stack frame without pushing it onto
// g_runqueue -- see docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- idle
// task") for why the idle task is the one caller that needs this.
// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- ring-transition
// GDT/TSS") for ring/user_stack_top -- ring 0 (the default) is unchanged
// from before this existed; ring 3 needs user_stack_top set.
task_t* task_create(void (*entry)(void*), void* arg, void* stack_mem,
                     bool enqueue = true, uint8_t ring = 0, void* user_stack_top = NULL);

// called ONLY from IRQ handler (returns regs-frame pointer / esp)
uint32_t* scheduler_on_tick(uint32_t* current_esp);

extern "C" void task_exit(void);
extern "C" void task_start_trampoline(void);  // in tasking.asm

// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp — task_block()/task_wake()")
// for why this is the same immediate-reschedule trick task_exit() already
// uses, not a new mechanism.
extern "C" void task_block(void);
extern "C" void task_wake(task_t* t);

// See docs/DOCS.md ("p-kernel.cpp — kernel_main() task-creation race") for
// why kernel_main() needs to hold this across its own task_create() calls,
// not just the scheduler's internal use of it.
extern "C" void sched_lock(void);
extern "C" void sched_unlock(void);

// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp — task/stack reaper") for
// design. Call once at boot (kernel_main(), inside the same sched_lock()
// span the idle task is created in) with a statically-allocated stack --
// the reaper task itself never exits, so its own stack is never freed.
void tasking_spawn_reaper(void* stack_mem);

// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- idle task") for
// design. Call once at boot, before any other task_create() in the same
// span, with a statically-allocated stack -- the idle task never exits,
// so its own stack is never freed. Not part of round-robin rotation
// (task_create()'s enqueue=false); pick_next() falls back to it directly.
void tasking_spawn_idle(void* stack_mem);