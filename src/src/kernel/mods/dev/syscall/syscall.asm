; the `syscall` instruction is not available in 32-bit mode

[extern syscall_dispatch]    ; syscall_dispatch is a C function

section .text
global syscall_handler

; %define ENOSYS 38
; %define NEG_ENOSYS -38  ; Negative ENOSYS, as used in syscall returns

syscall_handler:
    ; push eax
    push gs ; push dword gs
    push fs ; push dword fs
    push es ; push dword es
    push ds ; push dword ds
    ; push dword -ENOSYS
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    ; push esp
    push eax
    call syscall_dispatch
    add esp, 4
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp
    ; pop dword -ENOSYS
    pop ds
    pop es
    pop fs
    pop gs
    iretd