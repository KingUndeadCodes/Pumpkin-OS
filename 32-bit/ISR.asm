; http://www.osdever.net/tutorials/view/interrupts-exceptions-and-idts-part-2-exceptions

_isr0:
   pusha
   push gs
   push fs
   push ds
   push es
   
   extern _interrupt_0
   call _interrupt_0
   
   pop es
   pop ds
   pop fs
   pop gs
   popa
   iret

