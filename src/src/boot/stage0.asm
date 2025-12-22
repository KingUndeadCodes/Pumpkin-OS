[org 0x7c00]
[bits 16]

; [ PumpkinOS ]
; * continue
; * shutdown
; * about

; section .fsjump

    jmp short start
    nop

; https://raw.githubusercontent.com/nanobyte-dev/nanobyte_os/refs/heads/master/src/bootloader/stage1/boot.asm
; section .fsheaders

    bdb_oem:                    db "abcdefgh"           ; 8 bytes
    bdb_bytes_per_sector:       dw 512
    bdb_sectors_per_cluster:    db 1
    bdb_reserved_sectors:       dw 18                   ; The entirety of the first track (18 sectors) is reserved by the boot loader. This value was previously 1.
    bdb_fat_count:              db 2
    bdb_dir_entries_count:      dw 0E0h
    bdb_total_sectors:          dw 2880                 ; 2880 * 512 = 1.44MB
    bdb_media_descriptor_type:  db 0F0h                 ; F0 = 3.5" floppy disk
    bdb_sectors_per_fat:        dw 9                    ; 9 sectors/fat
    bdb_sectors_per_track:      dw 18
    bdb_heads:                  dw 2
    bdb_hidden_sectors:         dd 0
    bdb_large_sector_count:     dd 0

    ebr_drive_number:           db 0                    ; 0x00 floppy, 0x80 hdd, useless
                                db 0                    ; reserved
    ebr_signature:              db 29h
    ebr_volume_id:              db 12h, 34h, 56h, 78h   ; serial number, value doesn't matter
    ebr_volume_label:           db 'PUMPKIN OS '        ; 11 bytes, padded with spaces
    ebr_system_id:              db 'FAT12   '           ; 8 bytes

section .bss

data resb 1

section .text

global start

start:

mov [BOOT_DISK], dl         ; Stores the boot disk number

cli
xor ax, ax                  ; clear bits of ax
mov es, ax                  ; set es to 0
mov ds, ax                  ; set ds to 0
mov ss, ax                  ; set ss to 0
mov sp, 0x7C00              ; stack base (0x7C00)
sti

mov byte [data], 1          ; Initialize data to 0

; mov ah, 0x00              ; Redundent, line 18 (xor ax, ax) alraedy sets ah to zero.
mov al, 0x13 ; 0x13
int 0x10

call init_print

options:
.header:
    ; Print the header message
    mov bx, msg_header
    call print_string
.continue:
    ; Print the continue message
    cmp byte [data], 1
    mov bx, msg_continue
    je .continue_alt
    call print_string
    jmp .shutdown
.continue_alt:
    call print_string_highlight
.shutdown:
    ; Print the shutdown message
    cmp byte [data], 2
    mov bx, msg_shutdown
    je .shutdown_alt
    call print_string
    jmp .about
.shutdown_alt:
    call print_string_highlight
.about:
    ; Print the about message
    cmp byte [data], 3
    mov bx, msg_about
    je .about_alt
    call print_string
    je .menu
.about_alt:
    call print_string_highlight
.menu:
    ; mov ah, 0bh
    ; int 21h
    mov ah, 0x00
    int 0x16 ; Read keyboard input
    cmp ah, 0x50
    je .down
    cmp ah, 0x1C
    je .select
    cmp ah, 0x48
    je .up
    ; jmp .jump_back
.up:
    cmp byte [data], 1
    jle .jump_back
    dec byte [data]
    jmp .jump_back
.down:
    cmp byte [data], 3
    jge .jump_back
    inc byte [data]
    jmp .jump_back
.select:
    cmp byte [data], 1
    je continue
    cmp byte [data], 2
    je shutdown
    cmp byte [data], 3
    je about
    ; jmp .jump_back   ; This was removed to save storage space, be careful not to add any other options (other than 1, 2, 3) in the future.
.jump_back:
    mov ah, 0x02       ; Set cursor position
    mov bh, 0x00       ; Page number (usually 0)
    mov dh, 0x00       ; Row (Y)
    mov dl, 0x00       ; Column (X)
    int 0x10
    jmp .header

continue:
    ; Revert to the original video mode
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ; Read from disk and jump to stage 1
    mov bx, STAGE_1_LOCATION
    mov ah, 0x02        ; BIOS read function
    mov al, 0x01		; read two sectors
	mov ch, 0x00		; from cylinder 0
	mov cl, 0x02		; from sector 2 (counting from 1)
	mov dh, 0x00		; from head 0
    mov dl, [BOOT_DISK]  ; Get the boot disk number
    call disk_load
    ; Jump to the loaded stage 1
    jmp 0:STAGE_1_LOCATION
    ; ret
shutdown:
    mov ax, 0x1000
    mov ax, ss
    mov sp, 0xf000
    mov ax, 0x5307
    mov bx, 0x0001
    mov cx, 0x0003
    int 0x15
    ret
about:
    ; nop
    inc byte [aux_color]
    jmp options.jump_back
    ; ret

jmp $

%include "print.asm"
%include "disk.asm"

STAGE_1_LOCATION equ 0x7e00

times 510-($-$$) db 0 ; NOTE: There are four bytes left.
dw 0xaa55