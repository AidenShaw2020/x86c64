; SPDX-License-Identifier: GPL-2.0-or-later
; cc65 fastcall: CPU pointer in A/X, byte result in A (X=0).
; FDCPU is 35 bytes: CS=18, IP=24. Never caches across pager eviction,
; segment changes, branches outside the next byte, or 16-bit IP wrap.
.export _fd_cpu_fetch
.import _fd_cpu_fetch_refill
.import _fd_fetch_cs, _fd_fetch_ip, _fd_fetch_remaining
.import _fd_fetch_generation, _fd_fetch_bytes, _fd_pager_generation
.import _fd_fetch_start_ip, _fd_fetch_start_bytes, _fd_fetch_length
.importzp ptr1, ptr2, tmp1, tmp2
.segment "CODE"
_fd_cpu_fetch:
    sta ptr1
    stx ptr1+1
    lda _fd_fetch_length
    ora _fd_fetch_length+1
    beq refill
    lda _fd_pager_generation
    cmp _fd_fetch_generation
    bne refill
    lda _fd_pager_generation+1
    cmp _fd_fetch_generation+1
    bne refill
    ldy #18
    lda (ptr1),y
    cmp _fd_fetch_cs
    bne refill
    iny
    lda (ptr1),y
    cmp _fd_fetch_cs+1
    bne refill
    ldy #24
    lda (ptr1),y
    cmp _fd_fetch_ip
    bne branch_in_page
    iny
    lda (ptr1),y
    cmp _fd_fetch_ip+1
    bne branch_in_page
    lda _fd_fetch_remaining
    ora _fd_fetch_remaining+1
    beq refill
    jmp resident
refill:
    lda ptr1
    ldx ptr1+1
    jmp _fd_cpu_fetch_refill
branch_in_page:
    ; A branch inside the cached window needs neither C nor a pager lookup.
    ldy #24
    sec
    lda (ptr1),y
    sbc _fd_fetch_start_ip
    sta tmp1
    iny
    lda (ptr1),y
    sbc _fd_fetch_start_ip+1
    sta tmp2
    cmp _fd_fetch_length+1
    bcc within_page
    bne refill
    lda tmp1
    cmp _fd_fetch_length
    bcs refill
within_page:
    clc
    lda _fd_fetch_start_bytes
    adc tmp1
    sta _fd_fetch_bytes
    lda _fd_fetch_start_bytes+1
    adc tmp2
    sta _fd_fetch_bytes+1
    sec
    lda _fd_fetch_length
    sbc tmp1
    sta _fd_fetch_remaining
    lda _fd_fetch_length+1
    sbc tmp2
    sta _fd_fetch_remaining+1
    ldy #24
    lda (ptr1),y
    sta _fd_fetch_ip
    iny
    lda (ptr1),y
    sta _fd_fetch_ip+1
resident:
    lda _fd_fetch_bytes
    sta ptr2
    lda _fd_fetch_bytes+1
    sta ptr2+1
    ldy #0
    lda (ptr2),y
    sta tmp1
    inc _fd_fetch_bytes
    bne :+
    inc _fd_fetch_bytes+1
:
    lda _fd_fetch_remaining
    bne :+
    dec _fd_fetch_remaining+1
:
    dec _fd_fetch_remaining
    inc _fd_fetch_ip
    bne :+
    inc _fd_fetch_ip+1
    bne :+
    lda #0
    sta _fd_fetch_remaining
    sta _fd_fetch_remaining+1
:
    ldy #24
    lda _fd_fetch_ip
    sta (ptr1),y
    iny
    lda _fd_fetch_ip+1
    sta (ptr1),y
    lda tmp1
    ldx #0
    rts
