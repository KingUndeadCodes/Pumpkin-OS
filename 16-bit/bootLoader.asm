[org 0x7c00]

mov cx, 500

mov bp, 0x7c00
mov sp, bp

kernel equ 0x1000

mov bx, Found
call PrintString 

mov cx, 0fh
mov dx, 9999h
mov ah, 86h
int 15h

mov ah, 09h
mov cx, 1000h
mov al, 20h
mov bl, 17

Loop:
   int 0x10
   inc bl
   cmp bl, 07
   je TextEdit
   jmp Loop

TextEdit:
   mov ah, 0bh
   int 21h
   cmp al, 0
   mov ah, 0
   int 16h
   mov ah, 0x0e
   mov al, al
   int 0x10
   jmp TextEdit

%include "print.asm"

times 510-($-$$) db 0
dw 0xaa55
