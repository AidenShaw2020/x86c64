; SPDX-License-Identifier: GPL-2.0-or-later
; uint32_t fd_physical(seg, off): off in A/X, segment on cc65 stack.
; Compute the 20-bit address directly, avoiding general long arithmetic.
.export _fd_physical
.importzp sp, sreg, tmp1, tmp2, tmp3, tmp4
.import incsp2
.segment "CODE"
_fd_physical:
    sta tmp1
    stx tmp2
    ldy #1
    lda (sp),y
    lsr a
    lsr a
    lsr a
    lsr a
    sta sreg
    lda (sp),y
    asl a
    asl a
    asl a
    asl a
    sta tmp4
    dey
    lda (sp),y
    lsr a
    lsr a
    lsr a
    lsr a
    ora tmp4
    sta tmp4
    lda (sp),y
    asl a
    asl a
    asl a
    asl a
    clc
    adc tmp1
    sta tmp3
    lda tmp4
    adc tmp2
    tax
    lda sreg
    adc #0
    and #15
    sta sreg
    lda #0
    sta sreg+1
    lda tmp3
    jmp incsp2
