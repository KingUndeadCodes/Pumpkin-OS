[org 0x7e00]
[bits 16]

%define __NO_COLOR__
%define __STAGE_1__

KERNEL_LOCATION equ 0x8000

; mov ah, 0x0e
; mov al, 'a'
; int 0x10

load_kernel:
    pusha
    mov ax, KERNEL_LOCATION >> 4
    mov es, ax
    mov bx, KERNEL_LOCATION & 0xF     ; Set ES:BX = KERNEL_LOCATION (e.g. 0x8200)
    mov al, 16                        ; Read 13 sectors
    mov ch, 0x00                      ; Cylinder 0
    mov cl, 0x03                      ; Sector 3 (starts counting at 1)
    mov dh, 0x00                      ; Head 0
    mov ah, 0x02                      ; BIOS read function
    mov dl, [BOOT_DISK]               ; Load the boot disk number
    call disk_load
    ; Clear Screen
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    call drive_parameters
    call memory_detection ; upper_mem_map
    ; ...
    popa
    jmp 0:kernel_start

jmp $

%include "disk_info.asm"
%include "memory.asm"
%include "disk.asm"
%include "print.asm"
%include "gdt.asm"

kernel_start:
    mov ax, 0
    mov ss, ax
    mov sp, 0xFFFC

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1 ; Some people put `al` here
    mov cr0, eax
    jmp CODE_SEG:b32


[bits 32]

VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

b32:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; setup stack
    mov ebp, 0x90000
    mov esp, ebp

    ; enable A20 Line
    in al, 0x92
    or al, 0x02
    out 0x92, al

    jmp KERNEL_LOCATION
    ; jmp $

times 512-($-$$) db 0x00 ; NOTE: There are 34 bytes left.