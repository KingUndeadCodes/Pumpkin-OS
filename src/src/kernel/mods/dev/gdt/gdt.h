#pragma once
#include <stdint.h>

// GDTInstall() loads the kernel-built GDT; TSSInstall() sets up the ring>0 stack-switch TSS.
void GDTInstall(void);
void TSSInstall(void);
void GDTSetKernelStack(uint32_t esp0);
