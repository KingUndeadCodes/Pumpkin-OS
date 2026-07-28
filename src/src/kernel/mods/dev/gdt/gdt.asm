global GDTLoad
[extern _GDTPointer]

GDTLoad:
    lgdt [_GDTPointer]
    jmp 0x08:.flush
.flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
