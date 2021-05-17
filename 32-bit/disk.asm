disk_load:
    mov ah, 0x02
    mov al, dh
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    mov dl, [BOOT_DISK]
    int 0x13
    jc disk_error
    cmp ah, 0   
    jne disk_error
    ret

BOOT_DISK: db 0

disk_error:
    mov ah, 0x0e
    mov al, 0x45
    int 0x10
    mov al, 0x72
    int 0x10
    mov al, 0x72
    int 0x10
    mov al, 0x6f
    int 0x10
    jmp $
