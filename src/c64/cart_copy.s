; SPDX-License-Identifier: GPL-2.0-or-later
.include "build/cart-payload.inc"
.segment "CODE"
    lda #<PAYLOAD_START
    sta $fd
    lda #>PAYLOAD_START
    sta $fe
    lda #<PAYLOAD_SIZE
    sta $02
    lda #>PAYLOAD_SIZE
    sta $03
    lda #0
    sta $fb
    lda #$80
    sta $fc
    lda #1
    sta $04
    sta $de00
copy:
    ldy #0
    lda ($fb),y
    sta ($fd),y
    inc $fd
    bne :+
    inc $fe
:
    inc $fb
    bne count
    inc $fc
    lda $fc
    cmp #$a0
    bne count
    lda #$80
    sta $fc
    inc $04
    lda $04
    sta $de00
count:
    lda $02
    bne :+
    dec $03
:
    dec $02
    lda $02
    ora $03
    bne copy
    lda #$80
    sta $de00
    cli
    jmp PAYLOAD_ENTRY
