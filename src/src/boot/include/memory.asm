[bits 16]

; mem_error:
;     mov bx, mem_error_msg
;     call print_string
;     jmp $
;     ret

; mmap_1_entry equ 0x5000
; mmap_2_entry equ 0x5100

; This is really all we can fit in the space we have.
; upper_mem_map:
;     mov ax, 0xe801
;     int 0x15
;     jc mem_error
;     mov [mmap_1_entry], ax          ; extended 1 (up to 15MB between 1MB and 16MB)
;     mov [mmap_1_entry + 2], bx      ; extended 2 (number of contiguous 64KB blocks between 16MB and 4GB)
;     ret

mmap_entry equ 0x5000       ; address to store the entries

memory_detection:
    xor ax, ax
    mov es, ax
    mov  di, 0x5004
    xor  ebx, ebx
    xor  bp, bp
    mov  edx, 0x0534D4150
    mov  eax, 0xE820
    mov  [es:di + 20], dword 1
    mov  ecx, 24
    int  0x15
    jc   short .failed
    mov  edx, 0x0534D4150       ; some BIOSes trash this register
    cmp  eax, edx
    jne  short .failed
    test ebx, ebx		
	je   short .failed
	jmp  short .middle
.loop:
    mov  eax, 0xe820		
	mov  [es:di + 20], dword 1	
	mov  ecx, 24		
	int  0x15
	jc   short .end		
	mov  edx, 0x0534D4150
.middle:
    jcxz .skip
	cmp  cl, 20		
	jbe  short .succesfull
	test byte [es:di + 20], 1	
	je   short .skip
.succesfull:
    mov  ecx, [es:di + 8]	
	or   ecx, [es:di + 12]	
	jz   .skip		
	inc  bp         ; good entry, so bp++
	add  di, 24
.skip:
    test ebx, ebx		
	jne  short .loop
.end:
    mov [mmap_entry], bp        ; store the entry count
	clc			
	ret
.failed:
    mov bx, mem_error_msg
    call print_string
    jmp $
    ret
    ; stc			; "function unsupported" error
	; ret

; %include "print.asm"

mem_error_msg: 
    db "MM Error", 0 ; Memory Map Fetch Error
