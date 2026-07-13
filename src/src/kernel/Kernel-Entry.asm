[bits 32]

section .entry
[extern kernel_main]

[global start_kernel]
start_kernel:
    call kernel_main		        ; calls kernel function main()
    jmp $

section .text

%include "mods/dev/idt/idt.asm"
%include "mods/dev/paging/paging.asm"
%include "mods/dev/tasking/tasking.asm"
%include "mods/dev/syscall/syscall.asm"

section .data
section .rodata		    ; read only data