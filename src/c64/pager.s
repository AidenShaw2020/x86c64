; SPDX-License-Identifier: GPL-2.0-or-later
; cc65 fastcall resident-page access. Page loads/replacement, I/O errors,
; page boundaries and CGA dirty tracking stay in the C fallback.
; Frame layout: page=0, valid=2, dirty=3, data=4, size=516 (C compile guard).
.export _fd_mem_get, _fd_mem_write, _fd_mem_read_word, _fd_mem_write_word
.import _fd_mem_get_slow, _fd_mem_write_slow
.import _fd_mem_read_word_slow, _fd_mem_write_word_slow
.import _fd_cache_frame, _fd_cache_frames, _fd_pc_map
.importzp sp, sreg, ptr1, ptr2, ptr4, tmp1, tmp2, tmp3, tmp4
.import incsp4
.macpack longbranch
.segment "BSS"
word_value: .res 2
requested_page: .res 2
scan_count: .res 1
.segment "CODE"
; uint32 address in A/X/sreg; result int in A/X.
_fd_mem_get:
    sta tmp1
    stx tmp2
    lda sreg
    and #15
    sta tmp3
    jsr readable
    bcc absent_byte
    jsr resident
    bcc get_slow
    ldy #0
    lda (ptr1),y
    ldx #0
    rts
absent_byte:
    lda #255
    ldx #0
    rts
get_slow:
    lda tmp3
    sta sreg
    lda #0
    sta sreg+1
    lda tmp1
    ldx tmp2
    jmp _fd_mem_get_slow
; Value in A, uint32 address on software stack.
_fd_mem_write:
    sta tmp4
    jsr address_from_stack
    lda _fd_pc_map
    beq :+
    lda tmp3
    cmp #10
    bcs write_slow  ; Includes CGA, preserving dirty-row bookkeeping in C.
:
    jsr resident
    bcc write_slow
    ldy #0
    lda tmp4
    sta (ptr1),y
    ldy #3
    lda #1
    sta (ptr2),y
    jmp success
write_slow:
    lda tmp4
    ldx #0
    jmp _fd_mem_write_slow
; Output pointer in A/X, uint32 address on software stack.
_fd_mem_read_word:
    sta ptr4
    stx ptr4+1
    jsr address_from_stack
    jsr page_edge
    bcs read_word_slow
    jsr readable
    bcc absent_word
    jsr resident
    bcc read_word_slow
    ldy #0
    lda (ptr1),y
    sta tmp4
    iny
    lda (ptr1),y
    sta (ptr4),y
    dey
    lda tmp4
    sta (ptr4),y
    jmp success
absent_word:
    lda #255
    ldy #0
    sta (ptr4),y
    iny
    sta (ptr4),y
    jmp success
read_word_slow:
    lda ptr4
    ldx ptr4+1
    jmp _fd_mem_read_word_slow
; Word in A/X, uint32 address on software stack.
_fd_mem_write_word:
    sta word_value
    stx word_value+1
    jsr address_from_stack
    jsr page_edge
    bcs write_word_slow
    lda _fd_pc_map
    beq :+
    lda tmp3
    cmp #10
    bcs write_word_slow
:
    jsr resident
    bcc write_word_slow
    ldy #0
    lda word_value
    sta (ptr1),y
    iny
    lda word_value+1
    sta (ptr1),y
    ldy #3
    lda #1
    sta (ptr2),y
success:
    lda #0
    tax
    jmp incsp4
write_word_slow:
    lda word_value
    ldx word_value+1
    jmp _fd_mem_write_word_slow
address_from_stack:
    ldy #0
    lda (sp),y
    sta tmp1
    iny
    lda (sp),y
    sta tmp2
    iny
    lda (sp),y
    and #15  ; Physical bus wraps at 20 bits.
    sta tmp3
    rts
page_edge:
    lda tmp1
    cmp #255
    bne not_edge
    lda tmp2
    lsr a
    rts  ; Carry is bit 8 of the byte offset.
not_edge:
    clc
    rts
; Accept precisely the existing PC memory map, without long arithmetic.
readable:
    lda _fd_pc_map
    beq mapped
    lda tmp3
    cmp #10
    bcc mapped
    cmp #15
    beq bios
    cmp #13
    beq option_rom
    cmp #11
    bne hole
    lda tmp2
    cmp #$80
    bcc hole
    cmp #$c0
    bcc mapped
hole:
    clc
    rts
bios:
    lda tmp2
    cmp #$e0
    rts
option_rom:
    lda tmp2
    cmp #8
    bcs hole
mapped:
    sec
    rts
; C=1: ptr1 is the byte address, ptr2 the current Frame. C=0: use C pager.
; tmp1/2/3, tmp4 and ptr4 are preserved for callers/fallback ABI.
resident:
    lda tmp3
    lsr a
    sta requested_page+1
    lda tmp2
    ror a
    sta requested_page
    lda _fd_cache_frame
    sta ptr2
    lda _fd_cache_frame+1
    sta ptr2+1
    ora ptr2
    beq scan
    jsr matches
    bcs found
scan:
    lda #<_fd_cache_frames
    sta ptr2
    lda #>_fd_cache_frames
    sta ptr2+1
    lda #8
    sta scan_count
scan_frame:
    jsr matches
    bcs select_frame
    clc
    lda ptr2
    adc #4  ; sizeof(Frame)=516=$0204
    sta ptr2
    lda ptr2+1
    adc #2
    sta ptr2+1
    dec scan_count
    bne scan_frame
    clc
    rts
select_frame:
    lda ptr2
    sta _fd_cache_frame
    lda ptr2+1
    sta _fd_cache_frame+1
found:
    clc
    lda ptr2
    adc #4
    sta ptr1
    lda ptr2+1
    adc #0
    sta ptr1+1
    clc
    lda ptr1
    adc tmp1
    sta ptr1
    lda tmp2
    and #1
    adc ptr1+1
    sta ptr1+1
    sec
    rts
matches:
    ldy #2
    lda (ptr2),y
    beq missing
    ldy #0
    lda requested_page
    cmp (ptr2),y
    bne missing
    iny
    lda requested_page+1
    cmp (ptr2),y
    bne missing
    sec
    rts
missing:
    clc
    rts
