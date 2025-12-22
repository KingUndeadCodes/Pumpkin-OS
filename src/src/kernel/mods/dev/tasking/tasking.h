#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../idt/idt.h"

//  Development Log:
//   > [Fri December 12th 2025 03:52 UTC] The current replacement is not functional. A complete rework is needed. 
//   > [Fri December 12th 2025 23:45 UTC] I'll have to rewrite this system.

// https://www.reddit.com/r/osdev/comments/jf1wgy/multitasking_tutorial/

#define KSTACK_SIZE 4096
#define MAX_TASKS 32

typedef enum { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_DEAD } task_state_t;

typedef struct task {
    uint32_t *saved_esp;       // points into that task's kernel stack at last preemption
    struct task *next;
    task_state_t state;
    uint32_t pid;
} task_t;

static task_t *g_current = NULL;
static task_t *g_runqueue = NULL;
static uint32_t g_next_pid = 1;

uint32_t *scheduler_on_tick(uint32_t *current_esp);
task_t* task_create(void (*entry)(void*), void *arg, void *stack_mem);