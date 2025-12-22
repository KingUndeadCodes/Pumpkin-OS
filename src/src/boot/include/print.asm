%ifndef PRINT_ASM
%define PRINT_ASM 1

; If `__NO_COLOR__` is defined then we do not include color related subroutines.
; This is useful for reducing the size of the bootloader and for debugging purposes.

; If `__STAGE_1__` is defined then we do not need the strings at the bottom of the file.

%ifndef __NO_COLOR__

section .bss

color resb 1
aux_color resb 1

section .text

init_print:
	; mov byte [aux_color], 77 ; Default highlight color (light gray on black)
    mov byte [color], 7 ; Initialize color to 1
    ret

%endif

print_string:
	push ax
	push bx

	mov ah, 0x0e
	.Loop:
	cmp [bx], byte 0
	je .Exit
		mov al, [bx]
		%ifndef __NO_COLOR__
        push bx
        mov bl, byte [color] ; White on black
		int 0x10
        pop bx
		%else
		int 0x10
		%endif
		inc bx
		jmp .Loop
	.Exit:

	pop ax
	pop bx
	ret

%ifndef __NO_COLOR__

print_string_highlight:
	mov ch, [aux_color]
	add ch, 77 	; inc ch
    mov byte [color], ch ; default should be 77
	; mov byte [color], 77
    call print_string
    mov byte [color], 0x07 ; Reset color to white on black
    ret

%endif

%ifndef __STAGE_1__

msg_header: db '[ PumpkinOS ]', 0x0a, 0x0d, 0x00 	; "[ PumpkinOS boot system ]"
msg_continue: db '* continue', 0x0a, 0x0d, 0x00 	; "* continue to PumpkinOS"
msg_shutdown: db '* shutdown', 0x0a, 0x0d, 0x00		; "* shutdown"
msg_about: db '* about', 0x00 						; "* about"

%endif

%endif