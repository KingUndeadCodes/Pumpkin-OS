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

disk_error:
  
  mov ah, 09h
  mov cx, 1000h
  mov al, 20h
  mov bl, 17

  mov ah, 0x0e

  mov al, 'E'
  int 0x10
  mov al, 'r'
  int 0x10
  mov al, 'r'
  int 0x10
  mov al, 'o'
  int 0x10
  mov al, 'r'
  int 0x10
  mov al, ' '
  int 0x10
  mov al, 'B'
  int 0x10
  mov al, 'o'
  int 0x10
  mov al, 'o'
  int 0x10
  mov al, 't'
  int 0x10
  mov al, 'i'
  int 0x10
  mov al, 'n'
  int 0x10
  mov al, 'g'
  int 0x10
  
  jmp $

