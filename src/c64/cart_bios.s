; SPDX-License-Identifier: GPL-2.0-or-later
; Runs in the tape buffer $033C with IRQ masked. Magic Desk bank 8 contains
; the original 8KiB GLaBIOS. Stage each pair of pages at $0500 for REU DMA.
.segment "CODE"
    php
    sei
    lda $01
    pha
    ora #3 ; cc65 normally hides ROML with LORAM=0 ($01=$36).
    sta $01
    lda $02
    pha
    lda $03
    pha
    lda #8
    sta $de00
    lda #0
    sta $fb
    lda #$80
    sta $fc
    lda #16
    sta $02
    lda #$e0
    sta $03
chunk:
    ldy #0
copy_first:
    lda ($fb),y
    sta $0500,y
    iny
    bne copy_first
    inc $fc
copy_second:
    lda ($fb),y
    sta $0600,y
    iny
    bne copy_second
    inc $fc
    lda #0
    sta $df02
    sta $df04
    sta $df07
    sta $df09
    sta $df0a
    lda #5
    sta $df03
    lda $03
    sta $df05
    lda #15
    sta $df06
    lda #2
    sta $df08
    lda #$90
    sta $df01
    inc $03
    inc $03
    dec $02
    bne chunk
    lda #$80
    sta $de00
    pla
    sta $03
    pla
    sta $02
    pla
    sta $01
    plp
    rts
