section .text
[bits 32]

[extern _start]
call _start

jmp $

%include "mods/dev/idt/idt.asm"
