global setjmp
global longjmp

; int setjmp(jmp_buf env)
; env layout: [ebx, esi, edi, ebp, esp, eip]
setjmp:
    mov eax, [esp+4]       ; env
    mov ecx, [esp]         ; caller's return address
    mov [eax+0],  ebx
    mov [eax+4],  esi
    mov [eax+8],  edi
    mov [eax+12], ebp
    mov [eax+16], esp
    mov [eax+20], ecx
    xor eax, eax
    ret

; void longjmp(jmp_buf env, int val)
longjmp:
    mov eax, [esp+4]       ; env
    mov edx, [esp+8]       ; val
    mov ebx, [eax+0]
    mov esi, [eax+4]
    mov edi, [eax+8]
    mov ebp, [eax+12]
    mov esp, [eax+16]
    mov ecx, [eax+20]      ; saved eip
    test edx, edx
    jnz .nonzero
    mov edx, 1
.nonzero:
    mov eax, edx
    jmp ecx
