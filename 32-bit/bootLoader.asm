%include "disk.asm"

[bits 16]                   ; 16-bit
[org 0x7c00]

mov ah, 0x0		    ; clear screen (set text mode)
mov al, 0x3                 ; clear screen
int 0x10                    ; BIOS interupt

KERNEL_LOCATION equ 0x1000  ; Entry point for Kernel.

mov [BOOT_DISK], dl         ; Stores the boot disk number
xor ax, ax                  ; clear bits of ax
mov es, ax                  ; set es to 0
mov ds, ax                  ; set ds to 0
mov bp, 0x8000              ; stack base (0x8000)
mov sp, bp
mov bx, KERNEL_LOCATION     ; ES:BX is the location to read from, e.g. 0x00$
mov dh, 35                  ; read 20 sectors (blank sectors: empty_end)
call disk_load              ; call disk_load subroutine

    jmp 0:kernel_start

gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 0b10011010
    db 0b11001111
    db 0x0

gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 0b10010010
    db 0b11001111
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

shutdown:
    mov ax, 0x1000
    mov ax, ss
    mov sp, 0xf000
    mov ax, 0x5307
    mov bx, 0x0001
    mov cx, 0x0003
    int 0x15
    ret

kernel_start:
    mov ax, 0
    mov ss, ax
    mov sp, 0xFFFC

    mov ax, 0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    cli
    lgdt[gdt_descriptor]
    mov eax, cr0
    or al, 0x1
    mov cr0, eax
    jmp CODE_SEG:b32

[bits 32]

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

print32:
    pusha
    mov edx, VIDEO_MEMORY

.loop:
    mov al, [ebx]
    mov ah, WHITE_ON_BLACK
    cmp al, 0
    je .done
    mov [edx], ax
    add ebx, 1
    add edx, 2
    jmp .loop

.done:
    popa
    ret

b32:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov ebp, 0x90000          ; 32 bit stack base pointer
    mov esp, ebp

    jmp KERNEL_LOCATION       ; Kernel Load
    jmp shutdown              ; Shutdown if kernel load fails.

[SECTION signature start=0x7dfe]
dw 0aa55h
