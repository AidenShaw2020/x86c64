; SPDX-License-Identifier: GPL-2.0-or-later
; cc65 fastcall ABI, binary 8/16-bit 8086 ALU and flags.
; The C implementation remains the portable reference.
.export _fd_cpu_alu, _fd_cpu_szp
.importzp sp, ptr1, tmp1, tmp2, tmp3, tmp4
.import incsp4, incsp8
.segment "BSS"
arg_a: .res 2
arg_b: .res 2
result: .res 2
is_word: .res 1
operation: .res 1
carry_in: .res 1
.segment "RODATA"
parity: .byte 4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4
.segment "CODE"
; CPU and value are on the software stack; word is in A/X.
_fd_cpu_szp:
    sta tmp3
    txa
    ora tmp3
    sta tmp3
    ldy #0
    lda (sp),y
    sta tmp1
    iny
    lda (sp),y
    sta tmp2
    iny
    lda (sp),y
    sta ptr1
    iny
    lda (sp),y
    sta ptr1+1
    jsr apply_szp
    jmp incsp4
; tmp1/2=value, tmp3=word, ptr1=CPU. Preserve flags outside SZP.
apply_szp:
    ldy #26
    lda (ptr1),y
    and #$3b
    sta tmp4
    lda tmp1
    ora tmp2
    bne :+
    lda tmp4
    ora #$40
    sta tmp4
:
    lda tmp3
    beq byte_sign
    lda tmp2
    jmp sign
byte_sign:
    lda tmp1
sign:
    bpl :+
    lda tmp4
    ora #$80
    sta tmp4
:
    lda tmp1
    lsr a
    lsr a
    lsr a
    lsr a
    eor tmp1
    and #15
    tax
    lda parity,x
    ora tmp4
    ldy #26
    sta (ptr1),y
    rts
; b, a, op, CPU on software stack; word in A/X.
_fd_cpu_alu:
    sta is_word
    txa
    ora is_word
    sta is_word
    ldy #0
    lda (sp),y
    sta arg_b
    iny
    lda (sp),y
    sta arg_b+1
    iny
    lda (sp),y
    sta arg_a
    iny
    lda (sp),y
    sta arg_a+1
    iny
    lda (sp),y
    sta operation
    ldy #6
    lda (sp),y
    sta ptr1
    iny
    lda (sp),y
    sta ptr1+1
    ldy #26
    lda (ptr1),y
    and #1
    sta carry_in
    lda (ptr1),y
    and #$ee
    sta (ptr1),y
    iny
    lda (ptr1),y
    and #$f7
    sta (ptr1),y
    lda operation
    cmp #1
    beq logical_or
    cmp #4
    beq logical_and
    cmp #6
    beq logical_xor
    cmp #2
    beq add_carry
    cmp #0
    beq add_plain
    jmp subtract
logical_or:
    lda arg_a
    ora arg_b
    sta result
    lda arg_a+1
    ora arg_b+1
    jmp logical_high
logical_and:
    lda arg_a
    and arg_b
    sta result
    lda arg_a+1
    and arg_b+1
    jmp logical_high
logical_xor:
    lda arg_a
    eor arg_b
    sta result
    lda arg_a+1
    eor arg_b+1
logical_high:
    sta result+1
    jmp flags_szp
add_plain:
    clc
    bcc add_low
add_carry:
    lda carry_in
    lsr a
add_low:
    lda arg_a
    adc arg_b
    sta result
    lda is_word
    beq addition_carry
    lda arg_a+1
    adc arg_b+1
    sta result+1
addition_carry:
    bcc arithmetic_flags
    jmp set_carry
subtract:
    lda operation
    cmp #3
    beq subtract_carry
    sec
    bcs subtract_low
subtract_carry:
    lda carry_in
    eor #1
    lsr a
subtract_low:
    lda arg_a
    sbc arg_b
    sta result
    lda is_word
    beq subtraction_carry
    lda arg_a+1
    sbc arg_b+1
    sta result+1
subtraction_carry:
    bcs arithmetic_flags
set_carry:
    ldy #26
    lda (ptr1),y
    ora #1
    sta (ptr1),y
arithmetic_flags:
    lda arg_a
    eor arg_b
    eor result
    and #16
    ldy #26
    ora (ptr1),y
    sta (ptr1),y
    ldx #0
    lda is_word
    beq :+
    inx
:
    lda arg_a,x
    eor arg_b,x
    ldy operation
    cpy #3
    bcs :+
    eor #$ff
:
    sta tmp1
    lda arg_a,x
    eor result,x
    and tmp1
    bpl flags_szp
    ldy #27
    lda (ptr1),y
    ora #8
    sta (ptr1),y
flags_szp:
    lda is_word
    sta tmp3
    bne :+
    lda #0
    sta result+1
:
    lda result
    sta tmp1
    lda result+1
    sta tmp2
    jsr apply_szp
    lda result
    ldx result+1
    jmp incsp8
