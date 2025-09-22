.section .rodata

dead_boob:
.word 0xDEADB00B

garbage_boob:
.word 0x6F56D805

cool_constant:
.word 4
cool_array:
.word 0x69
.word 0x48
.word 0x32
.word 0x16

.section .text
.global sanity_check
.type   sanity_check,     %function

@ Very basic test, if it doesn't pass this then we're doomed
sanity_check:
    push {r1, r2, r3, r4, lr}

    mov r0, #0x69 @ 105
    mov r1, #0x48 @ 72
    sub r0, r0, r1 @ 105 - 72 = 33
    mov r0, r0, lsl #1 @ 33 << 1 = 66
    beq fail @ stop if zero is set

    cmp r0, r1 @ Compare r0 against r1
    movhi r1, #0xEE @ if r0 > r1, move 0xB00B into r1
    ldr r1, =dead_boob @ if r0 <= r1, move 0xDEADB00B into r1
    ldr r1, [r1] 

    @ Since 66 < 72, r1 should be 0xDEADB00B
    mov r1, r1, lsr #1 @ Than means if we shift it left it should overflow
    ldr r0, =garbage_boob @ And we should get this value
    ldr r0, [r0]  
    cmp r1, r0 @ compare
    bne fail @ stop if zero is set

    tst r1, #1 @ Check if its even or odd
    beq fail @ stop if zero is set
   
    ldr r0, =cool_constant  @ Let's test some nasty ldrs
    ldr r0, [r0]  @ Let's test some nasty ldrs
    cmp r0, #4
    bhi fail @ stop if zero is set

    ldr r4, =cool_array @ Load array ptr
    ldm r4!, {r0, r1, r2, r3} @ Write back is set, that means it will be 
    cmp r0, #0x69
    bne fail @ stop if zero is set
    cmp r1, #0x48
    bne fail @ stop if zero is set
    cmp r2, #0x32
    bne fail @ stop if zero is set
    cmp r3, #0x16
    bne fail @ stop if zero is set

    ldr r0, =cool_array @ Load array ptr
    add r0, #16
    cmp r4, r0 @ r4 should be  =cool_array + 16 (4*4)
    bne fail @ stop if zero is set

    mov r0, #69 @ congrats, we passed!
    pop {r1, r2, r3, r4, pc}

fail:
    mov r0, #68
    pop {r1, r2, r3, r4, pc}