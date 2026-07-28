#include "tasking.h"
#include "../serial/serial.h"
#include "../../std/include/stdlib.h"
#include "../gdt/gdt.h"

// ----------------------
// Globals
// ----------------------
task_t* g_current = NULL;      // bootstrap task initially
task_t* g_runqueue = NULL;     // circular list tail pointer
uint32_t g_next_pid = 1;
// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- idle task"). Not part
// of g_runqueue -- pick_next() falls back to it directly, so it needs to
// be visible up here, ahead of pick_next()'s own definition below.
static task_t* g_idle_task = NULL;

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

// Pick next READY task after cur (round-robin). If nothing else in
// g_runqueue is READY, cur can only keep running if it's still actually
// runnable itself (state == TASK_RUNNING) -- a cur that just called
// task_block()/task_exit() is BLOCKED/DEAD at this point (that's what
// forced the reschedule that led here), and resuming it directly would
// silently defeat the block/exit. See docs/DOCS.md ("mods/dev/tasking/
// tasking.cpp -- idle task") for the bug this used to be before the idle
// task existed as an explicit fallback rather than a round-robin peer.
static task_t* pick_next(task_t* cur) {
    if (g_runqueue) {
        task_t* t = cur ? cur->next : runqueue_head();
        if (!t) t = runqueue_head();
        task_t* start = t;

        while (t && t->state != TASK_READY) {
            t = t->next;
            if (t == start) { t = NULL; break; } // nothing runnable in the queue
        }
        if (t) return t;
    }

    if (cur && cur->state == TASK_RUNNING) return cur;
    return g_idle_task;
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
// Idle task
// ----------------------
// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- idle task") for why
// this is a fallback pick_next() switches to directly, not a real
// round-robin participant: round-robin would give it equal footing with
// every other task, wasting every other tick on `hlt` even when there's
// real work queued elsewhere.
static void idle_task_fn(void* arg) {
    (void)arg;
    for (;;) asm volatile("hlt");
}

void tasking_spawn_idle(void* stack_mem) {
    g_idle_task = task_create(idle_task_fn, NULL, stack_mem, false);
}

// ----------------------
// Task/stack reaper
// ----------------------
// See docs/DOCS.md ("mods/dev/tasking/tasking.cpp — task/stack reaper")
// for why this is a dedicated task woken via task_wake(), not scheduler-
// or join-triggered: g_runqueue has no remove function and no prev
// pointer, so unlinking a dead node is real list surgery that doesn't
// belong in the timer IRQ path, and there's no join/wait primitive here
// to hang reaping off of.
static task_t* g_reaper_task = NULL;
static volatile int g_reap_pending = 0;

// Unlinks and frees every TASK_DEAD node in the runqueue. Caller must
// already hold sched_lock() -- this mutates g_runqueue's link structure
// and must not be preempted mid-splice.
static void task_reap_dead(void) {
    if (!g_runqueue) return;

    task_t* prev = g_runqueue;      // tail; prev->next == head
    task_t* cur = g_runqueue->next;

    // Bounded by MAX_TASKS, not by walking back to a captured start node:
    // once nodes get unlinked the list can shrink to a self-loop that
    // never again equals whatever pointer we started at.
    for (int i = 0; i < MAX_TASKS && g_runqueue != NULL; i++) {
        task_t* next = cur->next;
        if (cur->state == TASK_DEAD) {
            if (cur == cur->next) {
                g_runqueue = NULL; // was the sole remaining node
            } else {
                prev->next = next;
                if (cur == g_runqueue) g_runqueue = prev; // removed the tail
            }
            free(cur->stack_base); // no-op if NULL (statically-allocated stack)
            int idx = (int)(cur - g_tasks);
            g_task_used[idx] = 0;
        } else {
            prev = cur;
        }
        cur = next;
    }
}

static void reaper_task_fn(void* arg) {
    (void)arg;
    for (;;) {
        sched_lock();
        bool hasWork = (g_reap_pending != 0);
        if (hasWork) {
            g_reap_pending = 0;
            task_reap_dead();
        }
        sched_unlock();

        if (!hasWork) task_block();
    }
}

void tasking_spawn_reaper(void* stack_mem) {
    g_reaper_task = task_create(reaper_task_fn, NULL, stack_mem);
}

// ----------------------
// Task exit: mark dead and yield via timer vector
// ----------------------
extern "C" void task_exit(void) {
    // Don’t let nested scheduling happen while we mark state
    sched_lock();
    if (g_current) g_current->state = TASK_DEAD;
    g_reap_pending = 1;
    sched_unlock();

    // Reaper may not be blocked yet (e.g. hasn't run once at all) --
    // task_wake() is a no-op in that case, but g_reap_pending being
    // already set means it'll reap on its very next run regardless, so
    // no wakeup is ever truly lost, just possibly delayed.
    if (g_reaper_task) task_wake(g_reaper_task);

    // Force a reschedule using the timer vector (0x20 == 32)
    asm volatile("int $0x20");

    // Should never return; if it does, idle.
    for (;;) asm volatile("hlt");
}

// ----------------------
// Task blocking: mark blocked and force an immediate reschedule via the
// same software-timer-interrupt trick task_exit() uses above. Unlike a
// hlt-spin, `int $0x20` doesn't need EFLAGS.IF set first -- it's a
// software interrupt, not something waiting on a hardware one -- so the
// caller has no interrupt-enable precondition to get right. This only
// returns once some other context calls task_wake() on this exact task,
// since pick_next() already skips anything that isn't TASK_READY, so a
// still-BLOCKED task can never be rescheduled by accident.
// ----------------------
extern "C" void task_block(void) {
    sched_lock();
    if (g_current) g_current->state = TASK_BLOCKED;
    sched_unlock();

    asm volatile("int $0x20");
}

extern "C" void task_wake(task_t* t) {
    if (!t) return;
    unsigned long flags = enter_critical();
    if (t->state == TASK_BLOCKED) t->state = TASK_READY;
    exit_critical(flags);
}

// ----------------------
// Task creation: build an interrupt frame matching irq_common_stub
// IMPORTANT: This must match your idt.asm irq_common_stub pop order.
// ----------------------
task_t* task_create(void (*entry)(void*), void* arg, void* stack_mem,
                     bool enqueue, uint8_t ring, void* user_stack_top) {
    task_t* t = task_alloc();
    if (!t) return NULL;

    t->pid = g_next_pid++;
    t->state = TASK_READY;
    t->ring = ring;
    t->kernel_stack_top = (uint32_t)((uint8_t*)stack_mem + KSTACK_SIZE);

    uint32_t* sp = (uint32_t*)t->kernel_stack_top;

    // --- iret frame (popped by iret): eip, cs, eflags[, esp, ss] ---
    // A ring-3 target needs the 5-value form -- iret only pops esp/ss
    // when the popped CS's RPL differs from the current CPL, so a ring-0
    // task's 3-value frame (unchanged from before ring support existed)
    // is still exactly what it needs. See docs/DOCS.md ("mods/dev/tasking/
    // tasking.cpp -- ring-transition GDT/TSS").
    if (ring == 3) {
        *(--sp) = 0x33;                             // SS (ring3 data | RPL3)
        *(--sp) = (uint32_t)user_stack_top;          // ESP (ring3 user stack top)
        *(--sp) = 0x00000202;                        // EFLAGS (IF=1, IOPL=00)
        *(--sp) = 0x2B;                              // CS (ring3 code | RPL3)
        *(--sp) = (uint32_t)task_start_trampoline;    // EIP -> trampoline
    } else {
        *(--sp) = 0x00000202;                  // EFLAGS (IF=1)
        *(--sp) = 0x00000008;                  // CS (kernel code selector)
        *(--sp) = (uint32_t)task_start_trampoline; // EIP -> trampoline
    }

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
    uint32_t data_sel = (ring == 3) ? 0x33 : 0x10;
    *(--sp) = data_sel;                    // ds
    *(--sp) = data_sel;                    // es
    *(--sp) = data_sel;                    // fs
    *(--sp) = data_sel;                    // gs

    t->saved_esp = sp;

    if (enqueue) runqueue_push(t);
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
        GDTSetKernelStack(g_current->kernel_stack_top);
        return g_current->saved_esp;
    }

    // Save current
    g_current->saved_esp = current_esp;

    // If current died, mark it dead (already done in task_exit) and move on
    task_t* next = pick_next(g_current);
    if (!next || next == g_current) {
        // pick_next() already verified this is safe: either cur is still
        // TASK_RUNNING and there's nothing else to switch to, or cur *is*
        // g_idle_task and nothing woke up. Either way, no switch needed.
        return current_esp;
    }

    if (g_current->state == TASK_RUNNING)
        g_current->state = TASK_READY;

    next->state = TASK_RUNNING;
    g_current = next;
    // See docs/DOCS.md ("mods/dev/tasking/tasking.cpp -- ring-transition
    // GDT/TSS") -- must be set before any interrupt can catch a ring>0
    // task running; a no-op for ring-0 tasks (ESP0 is only consulted by
    // the CPU on a CPL-crossing trap).
    GDTSetKernelStack(g_current->kernel_stack_top);

    return g_current->saved_esp;
}
