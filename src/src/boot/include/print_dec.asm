%ifndef PRINT_DEC_ASM
%define PRINT_DEC_ASM 1

print_dec:
    push bx
    push cx
    push dx
    mov cx, 5
    mov si, divisors
.next:
    mov dx, 0
    mov ax, bx
    mov bx, [si]
    div bx
    add al, '0'
    mov ah, 0x0E
    int 0x10
    mov bx, dx
    add si, 2
    loop .next
    pop dx
    pop cx
    pop bx
    ret

divisors:
    dw 10000, 1000, 100, 10, 1

%endif