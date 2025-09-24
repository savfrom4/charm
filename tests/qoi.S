@ Test implementation of qoi_decode written in ARMv5 assembly
@   Exports:
@   void*   qoi_decode  (const char* src, uint32_t src_size, uint32_t[3] info)

.section .text
.extern     malloc
.global     qoi_decode
.type       qoi_decode,     %function

.macro swap32 reg
    mov     r12, \reg, lsr #24
    ldr     r11, =0x00FF0000
    and     \reg, \reg, r11
    orr     \reg, \reg, r12
    mov     r12, \reg, lsr #8
    ldr     r11, =0xFF00FF00
    and     \reg, \reg, r11
    orr     \reg, \reg, r12, lsl #8
    mov     r12, \reg, lsl #24
    ldr     r11, =0x0000FF00
    and     \reg, \reg, r11
    orr     \reg, \reg, r12
.endm

.equ QOI_HEAD_SIZE,     14
.equ QOI_MAGIC,         0x716F6966

.equ QOI_MASK2,         0xc0
.equ QOI_OP_RGB,        0xfe
.equ QOI_OP_RGBA,       0xfe
.equ QOI_OP_INDEX,      0x00
.equ QOI_OP_DIFF,       0x40
.equ QOI_OP_LUMA,       0x80
.equ QOI_OP_RUN,        0xc0

qoi_decode:
    push    { r1-r11, lr }

    @ check if any of the provided arguments are 0
    orr     r3, r0, r1
    orrs    r3, r3, r2
    beq     fail

    @ check if we have enough bytes for qoi header
    swap32  r1
    cmp     r1, #QOI_HEAD_SIZE
    blo     fail

    bl      subroutine_read_header
    bl      subroutine_decode

    pop     { r1-r11, pc} 

    fail:
        mov     r0, #0      @ Failed
        pop     { r1-r11, pc } 


subroutine_read_header:
    @ load qoi magic
    ldr     r5, =QOI_MAGIC

    @ read qoi magic
    ldr     r4, [r0]!
    cmp     r4, r5
    bne     fail

    ldm     r0!, { r4, r6 } @ read width/height
    swap32  r4
    swap32  r6
    @ store info
    stm     r2!, { r4, r6 }

    ldrb    r5, [r0]!       @ read channels
    swap32  r5
    str     r5, [r2]!       @ store info

    mul     r4, r7, r4      @ height * width
    mul     r4, r5, r4      @ channels * size
    ldrb    r6, [r0]!       @ read colorspace
    swap32  r6

    bx      lr

subroutine_decode:
    @ alloc index array on stack
    sub     sp, sp, #64
    add     r1, r0, r1   @ ptr + size (end ptr)

    @ alloc mem for read
    push    {r0}
    mov     r0, r4
    bl      malloc
    mov     r2, r0      @ r2 is now a data ptr
    pop     {r0}
    push    {r2}        @ push on stack to save for later (r2 will be modified)

    loop:
        ldr     r7, [r0]!

        @ check if RGB
        cmp     r7, #QOI_OP_RGB
        beq     rgb

        @ check if RGBA
        cmp     r7, #QOI_OP_RGBA
        beq     rgba

        rgb:
            cmp         r5, #3
            @ this is taken when number of channels is 3
            ldmeq       r0!, {r7, r8, r9}
            stmeq       r2!, {r7, r8, r9}
            
            @ this when number of channels is 4
            movne       r10, #255
            ldmne       r0!, {r7, r8, r9, r10}
            stmne       r2!, {r7, r8, r9, r10}
            b           loop_end

        rgba:
            ldm         r0!, {r7, r8, r9, r10}
            stm         r2!, {r7, r8, r9, r10}
            b           loop_end

    loop_end:
        subs    r4, r4, r5  @ px_len = px_len - channels
        bne     loop

        @ return stack back
        pop     {r0}        @ pop the original data ptr
        add     sp, sp, #64
        bx      lr

@ in qoi, all numbers are big endian. we are little-endian, so convert
subroutine_swap_32:
    and     r1, r0, #0xFF000000
    lsr     r1, r1, #24

    and     r2, r0, #0x00FF0000
    lsr     r2, r2, #8

    and     r3, r0, #0x0000FF00
    lsl     r3, r3, #8

    and     r4, r0, #0x000000FF
    lsl     r4, r4, #24

    ORR     r1, r1, r2
    ORR     r1, r1, r3
    ORR     r1, r1, r4
    bx      lr