### Pumpkin-OS ###

```
This is just a simple "toy" OS I make in my free time.
This project has ONLY been tested with a Virtual Machine (QEMU).
This OS uses BiOS and not UEFI so keep that in mind.
```

```diff
# Done:
+ Core / Bootloader
+ Design / Logo

# Doing:
! Kernel / Print function

# Haven't started:
- Kernel / Keyboard input
- Kernel / File System
- Kernel / Programming language ports.
- Kernel / Programming API's
```

```c
// Imaginary port of c

#include <pumpio.h>

int main() {
  pump_print("Hello, World!");
  return 0;
}
```

**Credits**:
- KingUndeadCodes  
  - Everything!
