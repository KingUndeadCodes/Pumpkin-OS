# Pumpkin OS

Simple "toy" OS I develop in my free time.

## Important Points
### Boot
- The OS uses BIOS and not UEFI
- The OS uses it's own boot loader and is not multiboot complient. 
<!-- Need to add more things -->

## Roadmap
```diff
# Done:
+ Core   / Bootloader
+ Kernel / Basic Text I/O
+ Kernel / PC Speaker Support
+ Kernel / Date and Time Support

# Improving:
! Kernel / RTL8139 (Networking Card) Support
! Kernel / PCI code

# Doing:
! Kernel / Polishing Multitasking
! Kernel / Improving C Library

# Doing Next:
+++ Kernel / Ethernet Support

# Haven't started:
- Kernel / Text Scrolling
- Kernel / Programming language ports (*)
- Kernel / Programming API's
- Misc   / Basic Website and Documentation
```

## Future

### Lua
I wan't Pumpkin OS to have support for Lua.\
I am currently in the process of getting this to work.\
New functions will have to be added to the Standard Library.
### Ext2
In wan't to replace the current file system with a real file system in the near future.\
I think Ext2 is the best option.

## Terms of Use
- Pumpkin OS shall NOT be used to engage in Criminal Activity.
- Pumpkin OS shall NOT be used to harm Animals or Humans.
- Pumpkin OS is licensed under the [**MIT license**](/LICENSE).