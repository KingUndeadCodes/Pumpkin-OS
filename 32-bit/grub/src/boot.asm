section .text

[global kernel_main]
[extern kernel_start]

kernel_main:
    call kernel_start
    
jmp $

%include "src/mods/dev/idt/idt.asm"
%include "src/mods/dev/paging/paging.asm"
%include "src/mods/std/tasking/tasking.asm"
