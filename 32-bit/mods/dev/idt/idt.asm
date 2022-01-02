global IDTLoad
global IRQ0
global IRQ1
global IRQ2
global IRQ3
global IRQ4
global IRQ5
global IRQ6
global IRQ7
global IRQ8
global IRQ9
global IRQ10
global IRQ11
global IRQ12
global IRQ13
global IRQ14
global IRQ15
global ISR0
global ISR1
global ISR2
global ISR3
global ISR4
global ISR5
global ISR6
global ISR7
global ISR8
global ISR9
global ISR10
global ISR11
global ISR12
global ISR13
global ISR14
global ISR15
global ISR16
global ISR17
global ISR18
global ISR19
global ISR20
global ISR21
global ISR22
global ISR23
global ISR24
global ISR25
global ISR26
global ISR27
global ISR28
global ISR29
global ISR30
global ISR31

IDTLoad:
	[extern _IDTPointer]
	lidt [_IDTPointer]
	ret

IRQ0:
	cli
	push byte 0
	push byte 32
	jmp irq_common_stub

IRQ1:
	cli
	push byte 0
	push byte 33
	jmp irq_common_stub

IRQ2:
	cli
	push byte 0
	push byte 34
	jmp irq_common_stub


IRQ3:
	cli
	push byte 0
	push byte 35
	jmp irq_common_stub

IRQ4:
	cli
	push byte 0
	push byte 36
	jmp irq_common_stub

IRQ5:
	cli
	push byte 0
	push byte 37
	jmp irq_common_stub

IRQ6:
	cli
	push byte 0
	push byte 38
	jmp irq_common_stub

IRQ7:
	cli
	push byte 0
	push byte 39
	jmp irq_common_stub

IRQ8:
	cli
	push byte 0
	push byte 40
	jmp irq_common_stub

IRQ9:
	cli
	push byte 0
	push byte 41
	jmp irq_common_stub

IRQ10:
	cli
	push byte 0
	push byte 42
	jmp irq_common_stub

IRQ11:
        cli
        push byte 0
        push byte 43
        jmp irq_common_stub

IRQ12:
        cli
        push byte 0
        push byte 44
        jmp irq_common_stub

IRQ13:
        cli
        push byte 0
        push byte 45
        jmp irq_common_stub

IRQ14:
        cli
        push byte 0
        push byte 46
        jmp irq_common_stub

IRQ15:
        cli
        push byte 0
        push byte 47
        jmp irq_common_stub

[extern] _irq_handler

irq_common_stub:
	pusha
	push ds
	push es
	push fs
	push gs
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push eax
	mov eax, _irq_handler
	call eax
	pop eax
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	iret

ISR0:
	cli
	push byte 0
	push byte 0
	jmp isr_common_stub

ISR1:
	cli
	push byte 0
	push byte 1
	jmp isr_common_stub
ISR2:
	cli
	push byte 0
	push byte 2
	jmp isr_common_stub
ISR3:
	cli
	push byte 0
	push byte 3
	jmp isr_common_stub
ISR4:
	cli
	push byte 0
	push byte 4
	jmp isr_common_stub
ISR5:
	cli
	push byte 0
	push byte 5
	jmp isr_common_stub
ISR6:
	cli
	xchg bx, bx
	push byte 0
	push byte 6
	jmp isr_common_stub
ISR7:
	cli
	push byte 0
	push byte 7
	jmp isr_common_stub
ISR8:
	cli
	push byte 8
	jmp isr_common_stub
ISR9:
	cli
	push byte 0
	push byte 9
	jmp isr_common_stub
ISR10:
	cli
	push byte 10
	jmp isr_common_stub
ISR11:
	cli
	push byte 11
	jmp isr_common_stub
ISR12:
	cli
	push byte 12
	jmp isr_common_stub
ISR13:
	cli
	push byte 13
	jmp isr_common_stub
ISR14:
	cli
	push byte 14
	jmp isr_common_stub
ISR15:
	cli
	push byte 0
	push byte 15
	jmp isr_common_stub
ISR16:
	cli
	push byte 0
	push byte 16
	jmp isr_common_stub
ISR17:
	cli
	push byte 0
	push byte 17
	jmp isr_common_stub
ISR18:
	cli
	push byte 0
	push byte 18
	jmp isr_common_stub
ISR19:
	cli
	push byte 0
	push byte 19
	jmp isr_common_stub
ISR20:
	cli
	push byte 0
	push byte 20
	jmp isr_common_stub
ISR21:
	cli
	push byte 0
	push byte 21
	jmp isr_common_stub
ISR22:
	cli
	push byte 0
	push byte 22
	jmp isr_common_stub
ISR23:
	cli
	push byte 0
	push byte 23
	jmp isr_common_stub
ISR24:
	cli
	push byte 0
	push byte 24
	jmp isr_common_stub
ISR25:
	cli
	push byte 0
	push byte 25
	jmp isr_common_stub
ISR26:
	cli
	push byte 0
	push byte 26
	jmp isr_common_stub
ISR27:
	cli
	push byte 0
	push byte 27
	jmp isr_common_stub
ISR28:
	cli
	push byte 0
	push byte 28
	jmp isr_common_stub
ISR29:
	cli
	push byte 0
	push byte 29
	jmp isr_common_stub
ISR30:
	cli
	push byte 0
	push byte 30
	jmp isr_common_stub
ISR31:
	cli
	push byte 0
	push byte 31
	jmp isr_common_stub


extern _fault_handler


isr_common_stub:
	pusha
	push ds
	push es
	push fs
	push gs
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push eax
	mov eax, _fault_handler
	call eax
	pop eax
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	iret
