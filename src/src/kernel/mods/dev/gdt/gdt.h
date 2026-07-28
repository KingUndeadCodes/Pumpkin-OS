#pragma once
#include <stdint.h>

// See docs/DOCS.md ("mods/dev/gdt/gdt.cpp -- ring-transition GDT/TSS").
void GDTInstall(void);
void TSSInstall(void);
void GDTSetKernelStack(uint32_t esp0);
