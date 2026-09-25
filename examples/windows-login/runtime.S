.syntax unified
.cpu arm7tdmi
.arm

.section .text.memcpy, "ax", %progbits
.global memcpy
.type memcpy, %function
memcpy:
    mov r3, r0
1:  cmp r2, #0
    ldrbne r12, [r1], #1
    strbne r12, [r3], #1
    subne r2, r2, #1
    bne 1b
    bx lr
.size memcpy, . - memcpy

.section .text.memset, "ax", %progbits
.global memset
.type memset, %function
memset:
    mov r3, r0
1:  cmp r2, #0
    strbne r1, [r3], #1
    subne r2, r2, #1
    bne 1b
    bx lr
.size memset, . - memset
