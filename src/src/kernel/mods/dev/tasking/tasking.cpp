#include "tasking.h"
#include "../serial/serial.h"

// ----------------------
// Globals
// ----------------------
task_t* g_current = NULL;      // bootstrap task initially
task_t* g_runqueue = NULL;     // circular list tail pointer
uint32_t g_next_pid = 1;

// ----------------------
// Task storage
// ----------------------
static task_t g_tasks[MAX_TASKS];
static int g_task_used[MAX_TASKS];

static task_t* task_alloc(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!g_task_used[i]) {
            g_task_used[i] = 1;
            // zero task
            for (size_t j = 0; j < sizeof(task_t); j++)
                ((uint8_t*)&g_tasks[i])[j] = 0;
            return &g_tasks[i];
        }
    }
    return NULL;
}

// ----------------------
// Runqueue helpers
// g_runqueue is TAIL of a circular list.
// head is g_runqueue->next
// ----------------------
static void runqueue_push(task_t* t) {
    if (!g_runqueue) {
        g_runqueue = t;
        t->next = t;
    } else {
        t->next = g_runqueue->next; // head
        g_runqueue->next = t;       // tail -> new
        g_runqueue = t;             // new tail
    }
}

static task_t* runqueue_head(void) {
    return g_runqueue ? g_runqueue->next : NULL;
}

// pick next READY task after cur (round-robin)
static task_t* pick_next(task_t* cur) {
    if (!g_runqueue) return cur;

    task_t* t = cur ? cur->next : runqueue_head();
    if (!t) t = runqueue_head();
    task_t* start = t;

    while (t && t->state != TASK_READY) {
        t = t->next;
        if (t == start) return cur; // nothing runnable
    }
    return t ? t : cur;
}

// ----------------------
// Scheduler lock
// ----------------------
// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp" section) for why the
// increment/decrement need their own guard here.
static int g_sched_lock = 0;
extern "C" void sched_lock(void) {
    unsigned long flags = enter_critical();
    g_sched_lock++;
    exit_critical(flags);
}
extern "C" void sched_unlock(void) {
    unsigned long flags = enter_critical();
    if (g_sched_lock) g_sched_lock--;
    exit_critical(flags);
}

// ----------------------
// Init
// ----------------------
void tasking_init(void) {
    // clear task table usage
    for (int i = 0; i < MAX_TASKS; i++) g_task_used[i] = 0;

    // bootstrap "current task" representing kernel_main context
    g_current = task_alloc();
    g_current->pid = 0;
    g_current->state = TASK_RUNNING;
    g_current->saved_esp = NULL;

    g_runqueue = NULL;
}

// ----------------------
// Task exit: mark dead and yield via timer vector
// ----------------------
extern "C" void task_exit(void) {
    // Don’t let nested scheduling happen while we mark state
    sched_lock();
    if (g_current) g_current->state = TASK_DEAD;
    sched_unlock();

    // Force a reschedule using the timer vector (0x20 == 32)
    asm volatile("int $0x20");

    // Should never return; if it does, idle.
    for (;;) asm volatile("hlt");
}

// ----------------------
// Task creation: build an interrupt frame matching irq_common_stub
// IMPORTANT: This must match your idt.asm irq_common_stub pop order.
// ----------------------
task_t* task_create(void (*entry)(void*), void* arg, void* stack_mem) {
    task_t* t = task_alloc();
    if (!t) return NULL;

    t->pid = g_next_pid++;
    t->state = TASK_READY;

    uint32_t* sp = (uint32_t*)((uint8_t*)stack_mem + KSTACK_SIZE);

    // --- iret frame (popped by iret): eip, cs, eflags ---
    *(--sp) = 0x00000202;                  // EFLAGS (IF=1)
    *(--sp) = 0x00000008;                  // CS (kernel code selector)
    *(--sp) = (uint32_t)task_start_trampoline; // EIP -> trampoline

    // --- the stub discards 8 bytes: int_no + err_code (in that order as seen by regs*) ---
    // Your IRQ stubs push: err_code first, then int_no (push 0; push 32). :contentReference[oaicite:0]{index=0}
    // So memory layout at regs* is: int_no, err_code.
    *(--sp) = 0;                           // err_code
    *(--sp) = 32;                          // int_no (timer vector)

    // --- pusha frame for popa: edi, esi, ebp, esp, ebx, edx, ecx, eax (popped in reverse) ---
    // Your stub does: popa, so stack must contain exactly what popa expects.
    // We want EAX=entry, ECX=arg for the trampoline.
    *(--sp) = (uint32_t)entry;             // eax (used by trampoline)
    *(--sp) = (uint32_t)arg;               // ecx (used by trampoline)
    *(--sp) = 0;                           // edx
    *(--sp) = 0;                           // ebx
    *(--sp) = 0;                           // esp (ignored by popa)
    *(--sp) = 0;                           // ebp
    *(--sp) = 0;                           // esi
    *(--sp) = 0;                           // edi

    // --- segment regs (popped: gs, fs, es, ds) ---
    *(--sp) = 0x10;                        // ds
    *(--sp) = 0x10;                        // es
    *(--sp) = 0x10;                        // fs
    *(--sp) = 0x10;                        // gs

    t->saved_esp = sp;

    runqueue_push(t);                      // ✅ enqueue
    return t;
}

// ----------------------
// Called from IRQ0 only (through _irq_handler returning esp)
// ----------------------
uint32_t* scheduler_on_tick(uint32_t* current_esp) {
    if (g_sched_lock) return current_esp;
    if (!g_current)   return current_esp;

    // First tick: capture bootstrap kernel context and jump into first runnable task
    if (g_current->saved_esp == NULL) {
        g_current->saved_esp = current_esp;

        task_t* first = runqueue_head();
        if (!first) return current_esp;

        g_current = first;
        g_current->state = TASK_RUNNING;
        return g_current->saved_esp;
    }

    // Save current
    g_current->saved_esp = current_esp;

    // If current died, mark it dead (already done in task_exit) and move on
    task_t* next = pick_next(g_current);
    if (!next || next == g_current) {
        // If nothing else is READY, just keep running current (or idle task if you made one)
        return current_esp;
    }

    if (g_current->state == TASK_RUNNING)
        g_current->state = TASK_READY;

    next->state = TASK_RUNNING;
    g_current = next;

    return g_current->saved_esp;
}
