; SPDX-License-Identifier: GPL-2.0-or-later
.segment "CODE"
    .word start,start
    .byte $c3,$c2,$cd,$38,$30 ; CBM80 cold-start signature
start:
    sei
    ldx #$ff
    txs
    cld
    lda #$37
    sta $01
    jsr $fda3 ; IOINIT
    jsr $fd50 ; RAMTAS
    jsr $fd15 ; RESTOR
    jsr $ff5b ; CINT
    jsr $e453 ; BASIC vectors
    jsr $e3bf ; BASIC RAM
    sei
    ldx #0
copy_loader:
    lda loader,x
    sta $033c,x
    inx
    cpx #192
    bne copy_loader
    jmp $033c
loader:
.incbin "build/cart-copy.bin"
