global task_switch
task_switch:
    ; args: [esp+4] = old_esp_out, [esp+8] = new_esp
    mov eax, [esp+4]
    mov edx, [esp+8]
    mov [eax], esp      ; save current esp into *old_esp_out
    mov esp, edx        ; load next esp
    ret