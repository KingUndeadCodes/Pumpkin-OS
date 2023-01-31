# Pumpkin OS

This is a Simple "toy" x86_64 OS.

### Features
- GRUB Support (still being worked on)
- Keyboard and Mouse Support
- Basic "Multitasking"
- Support for Various PCI Cards
    - `RTL8139` (basic support - still being worked on).

## Future
[ ] VESA Support \
[ ] File System \
[ ] AArch64 Support (Rewrite) <!-- I'll have to rewrite the whole OS at that point. -->

## Terms of Use
- Pumpkin-OS shall NOT be used to engage in Criminal Activity.
- Pumpkin-OS shall NOT be used to harm Animals or Humans.
- Pumpkin-OS is licensed under the [**MIT license**](/LICENSE).

<!--
    vesa_init:
        mov ax, 0x4F02
        mov bx, 0x4180
        int 0x10
        cmp ax, 0x4F02
        jne .vesa_fail
        ret
    .vesa_fail:
        ; Failed to Initialize VESA Graphics Adapter
        ; Defaulting to VGA Graphics Mode
        ret
-->