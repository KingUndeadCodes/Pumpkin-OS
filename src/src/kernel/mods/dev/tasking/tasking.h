#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS   32
#define KSTACK_SIZE (32 * 1024)

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t* saved_esp;      // points to regs frame (top of irq_common_stub frame)
    struct task* next;        // circular runqueue
    task_state_t state;
    uint32_t pid;
} task_t;

// exported scheduler state
extern task_t* g_current;
extern task_t* g_runqueue;
extern uint32_t g_next_pid;

void tasking_init(void);
task_t* task_create(void (*entry)(void*), void* arg, void* stack_mem);

// called ONLY from IRQ handler (returns regs-frame pointer / esp)
uint32_t* scheduler_on_tick(uint32_t* current_esp);

extern "C" void task_exit(void);
extern "C" void task_start_trampoline(void);  // in tasking.asm