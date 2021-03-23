; Logs -> ||
; @date initial: 2021/3/23
; @name initial: King Undead
; @text initial:
;   - Basic 32-bit bootloader.
; ||

[bits 16]
[org 0x7c00]

mov ah, 0x0
mov al, 0x3
int 0x10

KERNEL_LOCATION equ 0x1000

mov [BOOT_DISK], dl         ; Stores the boot disk number
xor ax, ax                  ; clear bits of ax
mov es, ax                  ; set es to 0
mov ds, ax                  ; set ds to 0
mov bp, 0x8000              ; stack base
mov sp, bp
mov bx, KERNEL_LOCATION     ; ES:BX is the location to read from, e.g. 0x00$
mov dh, 35                  ; read 20 sectors (blank sectors: empty_end)
call disk_load

    jmp 0:kernel_start

gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0

gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

Found db 'Found: True', 0

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
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:b32

%include "disk.asm"

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

    mov ebp, 0x2000
    mov esp, ebp

    jmp KERNEL_LOCATION

    jmp $

[SECTION signature start=0x7dfe]
dw 0aa55h

