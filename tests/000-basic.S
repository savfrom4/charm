.section .rodata

@ Used by test1
dead_beef:
.word 0xDEADBEEF
shifted_dead_beef:
.word 0x6F56DF77
cool_array:
.word 0x69
.word 0x48
.word 0x32
.word 0x16
cool_constant:
.word 4

@ Used by test2

memory:
.space 64*4*2, 0
memory_size:
.word 64*4*2

.section .text
.global test1
.type   test1,     %function
.global test2
.type   test2,     %function

@ Very basic test subroutine, if it doesn't pass this then we're doomed
@   Input: none
test1:
    push {r1, r2, r3, r4, lr}

    mov r0, #0x69 @ 105
    mov r1, #0x48 @ 72
    sub r0, r0, r1 @ 105 - 72 = 33
    mov r0, r0, lsl #1 @ 33 << 1 = 66
    beq fail @ stop if zero is set

    cmp r0, r1 @ Compare r0 against r1
    movhi r1, #0xEE @ if r0 > r1, move 0xEE into r1
    ldr r1, =dead_beef @ if r0 <= r1, move 0xDEADBEEF into r1
    ldr r1, [r1] 

    @ Since 66 < 72, r1 should be 0xDEADB00B
    mov r1, r1, lsr #1 @ Than means if we shift it left it should overflow
    ldr r0, =shifted_dead_beef @ And we should get this value
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

    mov r0, #1 @ congrats, we passed!
    pop {r1, r2, r3, r4, pc}

fail:
    mov r0, #0
    pop {r1, r2, r3, r4, pc}

@ This test is more advanced test of multiplication, stack and memory
@   Input: r0
test2:
    push {r2, r3, r4, lr}

    @ setup first loop
    mov r4, #64
loop1:
    mul r1, r0, r4 @ r1 = r0 * r4 (32 bit)
    mla r2, r0, r4, r1 @ r2 = r0 + r4 + r1 (32 bit)
    push {r1, r2}

    @ Finally loop again if possible
    sub r4, r4, #1
    bne loop1

    ldr r3, =memory
    mov r4, #64
loop2:
    pop {r3, r4}
    umull r1, r2, r3, r4  @ r1 (lo), r2 (hi) = r1 * r2
    stm r3!, {r1, r2}

    @ Finally loop again if possible
    sub r4, r4, #1
    bne loop2

    ldr r0, =memory
    ldr r1, =memory_size
    ldr r1, [r1]
    pop {r2, r3, r4, pc}