#include "../include/tasking.h"

// https://wiki.osdev.org/Paging
// https://wiki.osdev.org/Cooperative_Multitasking

uint32_t prev_stack = 0x200000;
static Task *runningTask;
static Task mainTask;
static Task otherTask;

void fork(Task* task, void(*func)()) {
    createTask(task,func,runningTask->regs.eflags,(uint32_t*)runningTask->regs.cr3);
    task->next = runningTask->next;
    runningTask->next = task;
    return;
}

Task* fork(void(*func)()) {
    Task* t = (Task*)malloc(sizeof(Task));
    fork(t, func);
    t->onHeap = true;
    return t;
}

static void otherMain() {
    print("Hello, Multitasking World!", COLOR_GRAY);
    yield();
}

void initTasking() {
    // Get EFLAGS and CR3
    asm volatile("movl %%cr3, %%eax; movl %%eax, %0;":"=m"(mainTask.regs.cr3)::"%eax");
    asm volatile("pushfl; movl (%%esp), %%eax; movl %%eax, %0; popfl;":"=m"(mainTask.regs.eflags)::"%eax");
    createTask(&otherTask, otherMain, mainTask.regs.eflags, (uint32_t*)mainTask.regs.cr3);
    mainTask.next = &otherTask;
    otherTask.next = &mainTask;
    runningTask = &mainTask;
    print(" - Tasking Initialized!", (uint8_t)COLOR_GREEN | COLOR_BLACK << 4);
}

void quit() {
    runningTask->running = false;
    Task* current = runningTask;
    while (1) {
        if (current->next == runningTask) {
            current->next = runningTask->next;
            break;
        }
        current = current->next;
    }
    if (runningTask->onHeap) free(runningTask);
    yield();
}

void process_end(void) {
    quit();
    for (;;);
}

void* allocateStack() {
    prev_stack = prev_stack + 0x1000;    
    *(uint32_t*)(prev_stack - 4) = (uint32_t)&process_end;
    return (void*)prev_stack;
}

void createTask(Task *task, void (*main)(), uint32_t flags, uint32_t *pagedir) {
    task->regs.eax = 0;
    task->regs.ebx = 0;
    task->regs.ecx = 0;
    task->regs.edx = 0;
    task->regs.esi = 0;
    task->regs.edi = 0;
    task->regs.eflags = flags;
    task->regs.eip = (uint32_t)main;
    task->regs.cr3 = (uint32_t)pagedir;
    task->regs.esp = (uint32_t)allocateStack() - 4;
    task->running = true;
    task->onHeap = false;
    task->next = 0;
}
 
void yield() {
    Task *last = runningTask;
    runningTask = runningTask->next;
    switchTask(&last->regs, &runningTask->regs);
}