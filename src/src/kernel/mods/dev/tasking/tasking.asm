global task_start_trampoline
extern task_exit

; On first run, the task's registers come from your fake pusha frame.
; We set:
;   EAX = entry function pointer
;   ECX = arg pointer
;
; So trampoline can do: entry(arg)
task_start_trampoline:
    push ecx        ; push arg
    call eax        ; call entry
    add esp, 4

    ; if task returns, exit cleanly
    call task_exit

.hang:
    hlt
    jmp .hang
