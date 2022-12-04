# Pumpkin OS

Simple "toy" OS I develop in my free time.

## Important Points
### Boot
- The OS uses BIOS and not UEFI
- The OS uses it's own boot loader and is not multiboot complient. 
<!-- Need to add more things -->
<!-- 

TTY Feature soon. 

#define MAX_TTY_SPACE (ROWS*COLS)

char TTY001[2000];

// Slice Text to fit within 2000 characters
void Echo(char* Text, char* TTY);
-->

## Roadmap
```diff
# Done:
+ Misc   / Bootloader
+ Kernel / Basic Text I/O
+ Kernel / PC Speaker Support
+ Kernel / Date and Time Support
+ Ports  / Ported a SHA256 Hashing Library

# Doing:
! Kernel / Polishing PCI code
! Kernel / Polishing Multitasking
! Kernel / Improving C Library
! Kernel / RTL8139 (Networking Card) Support

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

<!--
```xml
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="1420" height="1080" viewBox="-70.5 -70.5 391 391" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
<rect x="30" y="20" rx="20" ry="20" width="150" height="150" style="fill:lightblue;stroke:black;stroke-width:5;opacity:0.5"/>
<text x="50" y="100">Memory Block 0</text>
<path transform="rotate(90 50 50)" id="svg_3" d="m0,50l50,-50l50,50l-25,0l0,50l-50,0l0,-50l-25,0z" stroke-linecap="null" stroke-linejoin="null" stroke-dasharray="null" stroke-width="0" fill="lightgreen"/>
```
-->