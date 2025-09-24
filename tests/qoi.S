@ Test implementation of qoi_decode written in ARMv5 assembly
@   Exports:
@   void*   qoi_decode  (const char* src, uint32_t src_size, uint32_t[3] info)

.section .text
.extern     malloc
.extern     __bswap_32
.global     qoi_decode
.type       qoi_decode,     %function

.macro swap32 reg
    push    { r0 }
    mov     r0, \reg
    bl      __bswap_32
    mov     \reg, r0
    pop     { r0 }
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

    ldm     r0!, { r4, r5 } @ read width/height
    swap32  r4
    swap32  r5

    @ store info
    stm     r2!, { r4, r5 }

    ldrb    r6, [r0]!       @ read channels
    str     r6, [r2]!       @ store info

    ldrb    r7, [r0]!       @ read colorspace
    bx      lr

subroutine_decode:
    sub     sp, sp, #256 @ alloc index array on stack (64*4)
    add     r1, r0, r1   @ ptr + size (end ptr)

    mov     r3, sp      @ save index array ptr

    @ alloc mem for read
    push    {r0, r1}

    mov     r1, #4      @ number of channels (we always return rgba32)
    mul     r0, r4, r5  @ w * h
    mul     r0, r1, r0  @ 4 * dim

    bl      malloc
    mov     r2, r0      @ r2 is now a data ptr

    pop     {r0, r1}
    push    {r2}        @ push on stack to save for later (r2 will be modified)

    loop:
        ldr     r7, [r0]!

        @ check if RGB
        cmp     r7, #QOI_OP_RGB
        beq     rgb

        @ check if RGBA
        cmp     r7, #QOI_OP_RGBA
        beq     rgba

        and     r8, r7, #QOI_MASK2

        @ check if INDEX
        cmp     r8, #QOI_OP_INDEX
        beq     index

        @ QOI_OP_RGB
        rgb:
            cmp         r5, #3

            @ this is taken when number of channels is 3
            ldmeq       r0!, {r7, r8, r9}
            moveq       r10, #0xFF

            @ this when number of channels is 4
            ldmne       r0!, {r7, r8, r9, r10}

            b           loop_end

        @ QOI_OP_RGBA
        rgba:
            ldm         r0!, {r7, r8, r9, r10}
            b           loop_end

        @ QOI_OP_INDEX
        index:
            add         r7, r7, r3  @ calculate address
            ldm         r7, {r7, r8, r9, r10}
            b           loop_end

        loop_end:
            stm     r2!, {r7, r8, r9, r10}
            bl      qoi_color_hash

            @ store index
            add     r11, r3, r12  @ calculate hash addr
            stm     r11, { r7, r8, r9, r10 }

            subs    r4, r4, r5  @ px_len = px_len - channels
            bne     loop

        @ return stack back
        pop     {r0}        @ pop the original data ptr
        add     sp, sp, #64
        bx      lr


@ direct port of QOI_COLOR_HASH macro with index wrapping built-in
@ also we sin by using r12 as return value... not that we are following callconv anyways...
qoi_color_hash:
    mov     r11, #3
    mul     r12, r11, r7     @ C.rgba.r*3

    mov     r11, #5
    mla     r12, r8, r11, r12 @ C.rgba.r*3 + C.rgba.g*5

    mov     r11, #7
    mla     r12, r9, r11, r12 @ C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7

    mov     r11, #11
    mla     r12, r10, r11, r12 @ C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11

    and     r12, r12, #63   @ & (64 - 1)
    bx      lr              @ r12 now holds the hash
