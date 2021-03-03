disk_load:
  mov ah, 0x02 
  mov al, dh      ; Read DH sectors
  mov ch, 0x00    ; Select cylinder 0
  mov dh, 0x00    ; Select head 0
  mov cl, 0x02    ; Start reading from second sector ( i.e. after the boot sect$
  mov dl, [BOOT_DISK]
  int 0x13        ; BIOS interrupt
  jc disk_error   ; Jump if error ( i.e. carry flag set )
  cmp ah, 0       ; if AL ( sectors read ) != DH ( sectors expected )
  jne disk_error
  ret

BOOT_DISK: db 0

char_1 equ 'E'
char_2 equ 'r'
char_3 equ 'o'
char_4 equ 'B'
char_5 equ 't'
char_6 equ 'i'
char_7 equ 'n'
char_8 equ 'g'
char_9 equ ' '

disk_error:
  
  mov ah, 09h
  mov cx, 1000h
  mov al, 20h
  mov bl, 17

  mov ah, 0x0e

  mov al, char_1
  call char_print
  mov al, char_2
  call char_print
  mov al, char_2
  call char_print
  mov al, char_3
  call char_print
  mov al, char_2
  call char_print
  mov al, char_9
  call char_print
  mov al, char_4
  call char_print
  mov al, char_3
  call char_print
  mov al, char_3
  call char_print
  mov al, char_5
  call char_print
  mov al, char_6
  call print_char
  mov al, char_7
  call print_char
  mov al, char_8
  call print_char

  ; hlt
  jmp $

char_print:
  int 0x10
  ret
