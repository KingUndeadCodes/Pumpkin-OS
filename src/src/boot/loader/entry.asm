[bits 32]
section .text
    global _start

KERNEL_LOCATION equ 0x400000

[extern kernel_main]
[extern g_read_file_ptr]

_start:
    ; Here, you could set up the stack or call main() if needed
    call kernel_main
    push dword [g_read_file_ptr]    ; read_file function pointer, set by main.cpp's kernel_main()
    mov eax, KERNEL_LOCATION        ; kernel entry point
    jmp eax
    jmp $

; times 363 db 0
; times 479 db 0
; times 731 db 0
; times 600 db 0

section .text
section .data
section .rodata		                ; read only data
