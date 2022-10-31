#ifndef __TASK_H__
#define __TASK_H__

#include "../../dev/paging/paging.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <text.h>

extern void initTasking();

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags, cr3;
} Registers;

typedef struct Task {
    Registers regs;
    struct Task *next;
    bool running;
    bool onHeap;
};
 
extern void initTasking();
extern void createTask(Task*, void(*)(), uint32_t, uint32_t*);

Task* fork(void(*func)());

extern "C" void yield(); // Switch task frontend
extern "C" void switchTask(Registers *a, Registers *b); // The function which actually switches

#endif /* __TASK_H__ */