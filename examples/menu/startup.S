.syntax unified
.cpu arm7tdmi
.arm

.section .text.startup, "ax", %progbits
.global _start
.type _start, %function
.extern main

_start:
    ldr sp, =0x00203F00
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 1b
    ldr r0, =_sbss
    ldr r1, =_ebss
    mov r2, #0
2:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 2b
    bl main
3:
    b 3b

.size _start, . - _start

