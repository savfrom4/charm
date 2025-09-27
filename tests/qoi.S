@ Test implementation of qoi_decode written in ARMv5 assembly
@   Exports:
@   void*   qoi_decode  (const char* src, uint32_t src_size, uint32_t[3] info)

.section .text
.extern     malloc
.extern     __bswap_32
.global     qoi_decode
.type       qoi_decode,     %function

@ named registers
var_data_ptr        .req    r0
var_data_len        .req    r1
var_info_ptr        .req    r2

var_image_width     .req    r3
var_image_height    .req    r4
var_image_channels  .req    r5
var_color_r         .req    r6
var_color_g         .req    r7
var_color_b         .req    r8
var_color_a         .req    r9

.equ QOI_HEAD_SIZE,     14
.equ QOI_MAGIC,         0x716F6966

.equ QOI_MASK2,         0xc0
.equ QOI_OP_RGB,        0xfe
.equ QOI_OP_RGBA,       0xfe
.equ QOI_OP_INDEX,      0x00
.equ QOI_OP_DIFF,       0x40
.equ QOI_OP_LUMA,       0x80
.equ QOI_OP_RUN,        0xc0
.equ QOI_PADDING,       0x08

.macro swap32 reg
    push    { r0, lr }
    mov     r0, \reg
    bl      __bswap_32
    mov     \reg, r0
    pop     { r0, lr }
.endm

@ direct port of QOI_COLOR_HASH macro with index wrapping built-in
@ also we sin by using r12 as return value... not that we are following callconv anyways...
.macro rgba_hash
    mov     r11, #3
    mul     r12, r11, var_color_r   @ C.rgba.r*3

    mov     r11, #5
    mla     r12, var_color_g, r11, r12 @ C.rgba.r*3 + C.rgba.g*5

    mov     r11, #7
    mla     r12, var_color_b, r11, r12 @ C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7

    mov     r11, #11
    mla     r12, var_color_a, r11, r12 @ C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11

    and     r12, r12, #63   @ & (64 - 1)
.endm

.macro rgba_load reg
    ldrb    var_color_r,    [\reg], #1 @ r
    ldrb    var_color_g,    [\reg], #1 @ g
    ldrb    var_color_b,    [\reg], #1 @ b
    ldrb    var_color_a,    [\reg], #1 @ a
.endm

.macro rgba_store reg
    strb    var_color_r,    [\reg], #1 @ r
    strb    var_color_g,    [\reg], #1 @ g
    strb    var_color_b,    [\reg], #1 @ b
    strb    var_color_a,    [\reg], #1 @ a
.endm

qoi_decode:
    push    { r1-r12, lr }

    @ check if any of the provided arguments are 0
    orr     r3, var_data_ptr, var_data_len
    orrs    r3, r3, var_info_ptr
    beq     fail

    @ check if we have enough bytes for qoi header
    cmp     var_data_len, #QOI_HEAD_SIZE
    blo     fail

    bl      read_header
    bl      decode

    pop     { r1-r12, pc }

fail:
    mov     r0, #0
    pop     { r1-r12, pc }


read_header:
    @ load qoi magic
    ldr     r3, =QOI_MAGIC

    @ read qoi magic
    ldr     r4, [var_data_ptr], #4
    swap32  r4
    cmp     r4, r3
    bne     fail

    ldm     var_data_ptr!, { var_image_width, var_image_height } @ read width/height
    swap32  var_image_width
    swap32  var_image_height
    stm     var_info_ptr!, { var_image_width, var_image_height } @ store info

    ldrb    var_image_channels, [var_data_ptr], #1 @ read channels
    str     var_image_channels, [var_info_ptr], #4 @ store info

    bx      lr

decode:
var_data_end_ptr    .req    r1
var_decoded_ptr     .req    r2
var_data_run        .req    r3
var_index_array_ptr .req    r4
    push    {var_data_ptr, var_data_end_ptr, lr}

    @ calculate size
    mov     r1, #4                                  @ num of channels
    mul     r0, var_image_width, var_image_height   @ w * h
    mul     r0, r1, r0  @ 4 * dim

    @ alloc mem for read
    bl      malloc
    mov     var_decoded_ptr, r0

    pop     {var_data_ptr, var_data_end_ptr, lr}
    push    {var_decoded_ptr}   @ r2 will be advanced so store initial value

    sub     sp, sp, #256                                        @ alloc index array on stack (64*4)
    add     var_data_end_ptr, var_data_ptr, var_data_len        @ calculate end ptr
    sub     var_data_end_ptr, var_data_end_ptr, #QOI_PADDING    @ - padding
    mov     var_data_run, #0                                    @ current run (done here since r3 is free by now)
    mov     var_index_array_ptr, sp                             @ save index array ptr (done here since r4 is free by now)

    loop:
        cmp     var_data_run, #0    @ check if run is 0
        subne   var_data_run, #1    @ run --;
        bne     loop_end

        ldrb    r11, [r0], #1

        @ if QOI_OP_RGB
        cmp     r11, #QOI_OP_RGB
        beq     rgb

        @ if QOI_OP_RGBA
        cmp     r11, #QOI_OP_RGBA
        beq     rgba

        and     r12, r11, #QOI_MASK2

        @ if QOI_OP_INDEX
        cmp     r12, #QOI_OP_INDEX
        beq     index

        @ if QOI_OP_DIFF
        cmp     r12, #QOI_OP_DIFF
        beq     diff

        @ if QOI_OP_LUMA
        cmp     r12, #QOI_OP_LUMA
        beq     luma

        @ if QOI_OP_RUN
        cmp     r12, #QOI_OP_RUN
        beq     index

        @ QOI_OP_RGB
        rgb:
            ldrb        var_color_r, [var_data_ptr], #1  @ r
            ldrb        var_color_g, [var_data_ptr], #1  @ g
            ldrb        var_color_b, [var_data_ptr], #1  @ b
            mov         var_color_a, #0xFF  @ a

            b           loop_end

        @ QOI_OP_RGBA
        rgba:
            rgba_load   var_data_ptr
            b           loop_end

        @ QOI_OP_INDEX
        index:
            add         r11, r11, var_index_array_ptr
            rgba_load   r11
            b           loop_end

        diff:
            @ calculate r
            mov         r11, r11, lsr #4
            and         r11, r11, #0x03
            sub         r11, r11, #2
            add         var_color_r, var_color_r, r11

            @ calculate g
            mov         r11, r11, lsr #2
            and         r11, r11, #0x03
            sub         r11, r11, #2
            add         var_color_r, var_color_r, r11

            @ calculate b
            and         r11, r11, #0x03
            sub         r11, r11, #2
            add         var_color_r, var_color_r, r11

            b           loop_end

        luma:
            @ stub
            ldrb        r11, [var_data_ptr], #1  @ just read for now
            b           loop_end

        @ QOI_OP_RUN
        run:
            and         var_data_run, r11, #0x3F
            b           loop_end

        loop_end:
            rgba_store  var_decoded_ptr

            @ store index
            rgba_hash
            add         r11, var_index_array_ptr, r12  @ calculate hash ptr
            rgba_store  r11

            cmp         var_data_ptr, var_data_end_ptr
            blo         loop

        add     sp, sp, #256
        pop     {r0}    @ pop our decoded ptr and store in r0 (return)
        bx      lr
