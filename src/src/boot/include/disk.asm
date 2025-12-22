%ifndef DISK_ASM
%define DISK_ASM 1

; AH = 0x02 - Read Sectors From Drive
; AL = Sectors To Read Count
; CH = Cylinder Number
; CL = Sector Number
; DH = Head Number
; DL = Drive Number
; ES:BX = Buffer Address
disk_load:
    ; pusha
    ; Do NOT overwrite dl — it is already set by the caller
    ; Assume ah=0x02, al=#sectors, ch=cylinder, cl=sector, dh=head, dl=drive, es:bx=buffer
    ; mov ah, 0x02
    int 0x13
    jc disk_error
    call loaded_sectors
    ret

BOOT_DISK: db 0x00

loaded_sectors:
    xor ah, ah
    mov bx, ax
    call print_dec
    mov bx, disk_sectors_loaded
    call print_string
    ret

disk_error:
    mov ah, 0x00
    int 0x13
    xor ebx, ebx
    mov bl, ah
    call print_dec
    mov bx, disk_error_msg
    call print_string
    ret

disk_sectors_loaded: db " sectors loaded.", 0x0a, 0x0d, 0x00
disk_error_msg: db "Disk error", 0x0a, 0x0d, 0x00

%include "print_dec.asm"
%endif