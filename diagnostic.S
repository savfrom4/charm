@ This is a special self-diagnostic utility that will detect
@ any issues and return a coresponding error code.
@   Error codes:
@   0:      no error
@   1:      expected condition flags weren't set
@   2:      the calculation result was invalid
@   255:    unknown failure

.section .text
.global run
.type   run,     %function

max_value:
.word 0xFFFFFFFF

constant1:
.word 6969

constant2:
.word 0x1AF2

fail_setflags:
    mov r0, #1
    pop {r1-ip, pc}

fail_value:
    mov r0, #2
    pop {r1-ip, pc}

run:
    push {r1-ip, lr}
    b data_processing

data_processing:
    mov r0, #31
    mov r1, #1

    @ and
    ands r0, r0, r1
    beq fail_setflags
    ands r0, r0, #0
    bne fail_setflags

    mov r0, #67
    mov r1, #1

    add r0, r0, #1 @ 68
    add r0, r0, r1 @ 69
    ldr r1, =max_value
    ldr r1, [r1]

    sub r0, r0, r1 @ wrap around to 70
    rsb r0, r0, r1 @ 0xFFFFFFF - 70 = 0xFFFFFFB9

    ldr r1, =constant1
    ldr r1, [r1]
    adds r0, r0, r1 @ lets push it to overflow

    bcc fail_setflags @ carry MUST be set
    beq fail_setflags @ zero must NOT be set

    ldr r1, =constant2
    ldr r1, [r1]

    cmp r0, r1 @ finally compare to constant
    bne fail_value

    mov r1, r0
    sub r1, r1, #1
    tst r0, r1 @ trick to check if is power of 2
    beq fail_value

    ldr r1, =constant2
    ldr r1, [r1]

    teq r0, r1 @ basically test if r0 == constant
    bne fail_value

    mov r0, #0x40000000
    movs r0, r0, lsl #1 @ r0 = 0x80000000, this must set negative flag
    bpl fail_setflags

    movs r0, r0, lsl #1 @ r0 = 0x0, this must set carry flag and Z = 0
    bne fail_setflags
    bcc fail_setflags

    mov r0, #0b11111111
    bic r0, r0, #0b00001111 @ this must clear bottom 4 bits
    cmp r0, #0b11110000
    bne fail_value

    mvn r0, #0xFFFFFFFF @ NOT 0xFFFFFFFF = 0
    bne fail_value

    mov r0, #0
    pop {r1-ip, pc}
