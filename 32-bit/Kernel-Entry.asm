section .text
[bits 32]

[extern _start]
call _start

jmp $

section .rodata

%include "incbins.asm"
