[org 0x7c00]

mov cx, 500

mov bp, 0x7c00
mov sp, bp

mov bx, SystemFound
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
   je Starting
   jmp Loop

Starting:
   mov ah, 0x0e
   mov bx, PleaseWait
   call PrintString
   int 0x10

jmp $

%include "print.asm"

times 510-($-$$) db 0

dw 0xaa55
