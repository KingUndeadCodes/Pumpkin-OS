section .text

set_up_stack:
    
    mov esp, 0x105000

section .bss

stack_begin: resb 4096 
stack_end:
