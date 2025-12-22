%include "disk.asm"

drv_params_entry equ 0x5200

drive_parameters:
	mov  dl, [BOOT_DISK]	
	mov  ah, 0x08
	int  0x13
	jc   disk_error
	and  cl, 0x3f
	mov  [drv_params_entry], bl     ; Drive Type
	mov  [drv_params_entry+1], ch   ; Max. Cylinder
	mov  [drv_params_entry+2], cl   ; Max. Sector
	mov  [drv_params_entry+3], dh   ; Max. Head
	mov  [drv_params_entry+4], dl   ; N. Drives
	mov  dl, [BOOT_DISK]
	mov  [drv_params_entry+5], dl   ; Drive Number
    ret