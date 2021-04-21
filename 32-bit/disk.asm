disk_load:
  mov ah, 0x02 
  mov al, dh          ; Read DH sectors
  mov ch, 0x00        ; Select cylinder 0
  mov dh, 0x00        ; Select head 0
  mov cl, 0x02        ; Start reading from second sector ( i.e. after the boot sect$
  mov dl, [BOOT_DISK]
  int 0x13            ; BIOS interrupt
  jc disk_error       ; Jump if error ( i.e. carry flag set )
  cmp ah, 0           ; if AL ( sectors read ) != DH ( sectors expected )
  jne disk_error
  ret

BOOT_DISK: db 0

char_1 equ 'E'
char_2 equ 'r'
char_3 equ 'o'

disk_error:
  mov ah, 0x0e

  mov al, char_1
  int 0x10
  mov al, char_2
  int 0x10
  mov al, char_2
  int 0x10
  mov al, char_3
  int 0x10
  mov al, char_2
  int 0x10

  jmp $
