// tasking.cpp
#include "../port.cpp"
#include "tasking.h"

extern "C" void task_switch(uint32_t **old_esp_out, uint32_t *new_esp);

static task_t* pick_next(task_t *cur) {
    task_t *t = cur ? cur->next : g_runqueue;
    if (!t) t = g_runqueue;

    // find next READY
    task_t *start = t;
    while (t && t->state != TASK_READY) {
        t = t->next ? t->next : g_runqueue;
        if (t == start) return cur; // nothing else runnable
    }
    return t ? t : cur;
}

static int g_sched_lock = 0;
void sched_lock(void) { 
    g_sched_lock++; 
};
void sched_unlock(void) { 
    if (g_sched_lock) g_sched_lock--; 
}; 

uint32_t *scheduler_on_tick(uint32_t *current_esp) {
    if (g_sched_lock) return current_esp;

    if (!g_current) return current_esp;

    g_current->saved_esp = current_esp;

    task_t *next = pick_next(g_current);
    if (!next || next == g_current) return current_esp;

    g_current->state = TASK_READY;
    next->state = TASK_RUNNING;
    g_current = next;

    return g_current->saved_esp;
}

// extern void task_trampoline(void); // optional, see below

static task_t g_tasks[MAX_TASKS];
static int g_task_used[MAX_TASKS];

static task_t* task_alloc(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!g_task_used[i]) {
            g_task_used[i] = 1;
            // zero it
            for (size_t j = 0; j < sizeof(task_t); j++)
                ((uint8_t*)&g_tasks[i])[j] = 0;
            return &g_tasks[i];
        }
    }
    return NULL;
}

task_t* task_create(void (*entry)(void*), void *arg, void *stack_mem) {
    (void)arg;

    task_t *t = task_alloc();
    if (!t) return NULL;

    t->pid = g_next_pid++;
    t->state = TASK_READY;

    uint8_t *stack_top = (uint8_t*)stack_mem + KSTACK_SIZE;
    uint32_t *sp = (uint32_t*)stack_top;

    // TODO: build initial frame here matching your IRQ stub
    // For now just store something so it compiles:
    t->saved_esp = sp;

    return t;
}
