/*
 * Combination of stolen code morphed into one file.
 * CODE (7-21) || https://stackoverflow.com/questions/6139952/what-is-the-booting-process-for-arm
*/

.globl _start
 _start: b       reset
    ldr     pc, _undefined_instruction
    ldr     pc, _software_interrupt
    ldr     pc, _prefetch_abort
    ldr     pc, _data_abort
    ldr     pc, _not_used
    ldr     pc, _irq
    ldr     pc, _fiq

reset:
    mrs     r0,cpsr                 /* set the cpu to SVC32 mode        */
    bic     r0,r0,#0x1f             /* (superviser mode, M=10011)       */
    orr     r0,r0,#0x13
    msr     cpsr,r0
