; SPDX-License-Identifier: GPL-2.0-or-later
; Execute common register instructions as a batch on 6510. Unsupported
; opcodes (including all prefixes) return to the complete C interpreter
; before the opcode is consumed. Instruction budgets and flags are retained.
.export _fd_cpu_fast
.import _fd_cpu_fetch, _fd_cpu_alu
.import _fd_fetch_ip, _fd_fetch_bytes, _fd_fetch_remaining
.importzp sp, ptr3
.import pushax, incsp2
.macpack longbranch
.segment "BSS"
cpu: .res 2
budget: .res 2
count: .res 2
old_ip: .res 2
opcode: .res 1
reg_offset: .res 1
value: .res 2
immediate: .res 2
saved_cf: .res 1
alu_op: .res 1
width: .res 1
.segment "CODE"
_fd_cpu_fast:
    sta budget
    stx budget+1
    lda #0
    sta count
    sta count+1
    ldy #0
    lda (sp),y
    sta cpu
    iny
    lda (sp),y
    sta cpu+1
next:
    jsr cpu_pointer
    ldy #28
    lda (ptr3),y
    jne finish
    lda budget
    ora budget+1
    jeq finish
    ldy #24
    lda (ptr3),y
    sta old_ip
    iny
    lda (ptr3),y
    sta old_ip+1
    jsr fetch
    sta opcode
    jsr cpu_pointer
    ldy #28
    lda (ptr3),y
    jne finish
    lda opcode
    cmp #$40
    bcc accumulator
    cmp #$50
    jcc increment
    cmp #$b0
    bcc other
    cmp #$c0
    jcc mov_immediate
other:
    cmp #$90
    jeq nop_op
    cmp #$e2
    jeq loop
    cmp #$eb
    jeq jump
    cmp #$72
    jeq conditional
    cmp #$73
    jeq conditional
    cmp #$74
    jeq conditional
    cmp #$75
    jeq conditional
    jmp unsupported
accumulator:
    and #7
    cmp #4
    beq accumulator_go
    cmp #5
    jne unsupported
accumulator_go:
    jsr commit
    lda opcode
    and #1
    sta width
    lda opcode
    lsr a
    lsr a
    lsr a
    sta alu_op
    jsr fetch
    sta immediate
    lda #0
    sta immediate+1
    lda width
    beq :+
    jsr fetch
    sta immediate+1
:
    jsr cpu_pointer
    ldy #0
    lda (ptr3),y
    sta value
    lda #0
    sta value+1
    lda width
    beq :+
    iny
    lda (ptr3),y
    sta value+1
:
    jsr calculate
    lda alu_op
    cmp #7
    jeq next
    jsr cpu_pointer
    ldy #0
    lda value
    sta (ptr3),y
    lda width
    jeq next
    iny
    lda value+1
    sta (ptr3),y
    jmp next
increment:
    jsr commit
    lda opcode
    and #7
    asl a
    sta reg_offset
    tay
    lda (ptr3),y
    sta value
    iny
    lda (ptr3),y
    sta value+1
    ldy #26
    lda (ptr3),y
    and #1
    sta saved_cf
    lda #0
    sta alu_op
    sta immediate+1
    lda opcode
    and #8
    beq :+
    lda #5
    sta alu_op
:
    lda #1
    sta immediate
    sta width
    jsr calculate
    jsr cpu_pointer
    ldy reg_offset
    lda value
    sta (ptr3),y
    iny
    lda value+1
    sta (ptr3),y
    ldy #26
    lda (ptr3),y
    and #$fe
    ora saved_cf
    sta (ptr3),y
    jmp next
mov_immediate:
    jsr commit
    lda opcode
    and #7
    asl a
    sta reg_offset
    lda opcode
    cmp #$b8
    bcs mov_word
    and #3
    asl a
    sta reg_offset
    lda opcode
    and #4
    beq :+
    inc reg_offset
:
    jsr fetch
    sta value
    jsr cpu_pointer
    ldy reg_offset
    lda value
    sta (ptr3),y
    jmp next
mov_word:
    jsr fetch
    sta value
    jsr fetch
    sta value+1
    jsr cpu_pointer
    ldy reg_offset
    lda value
    sta (ptr3),y
    iny
    lda value+1
    sta (ptr3),y
    jmp next
nop_op:
    jsr commit
    jmp next
loop:
    jsr commit
    jsr fetch
    sta immediate
    jsr cpu_pointer
    ldy #2
    sec
    lda (ptr3),y
    sbc #1
    sta (ptr3),y
    sta value
    iny
    lda (ptr3),y
    sbc #0
    sta (ptr3),y
    ora value
    jeq next
    jmp relative
jump:
    jsr commit
    jsr fetch
    sta immediate
    jsr cpu_pointer
    jmp relative
conditional:
    jsr commit
    jsr fetch
    sta immediate
    jsr cpu_pointer
    ldy #26
    lda opcode
    cmp #$74
    bcs zero_test
    lda (ptr3),y
    and #1
    jmp test_condition
zero_test:
    lda (ptr3),y
    and #$40
test_condition:
    beq clear_condition
    lda opcode
    and #1
    jne next
    jmp relative
clear_condition:
    lda opcode
    and #1
    jeq next
relative:
    lda #0
    ldx immediate
    bpl :+
    lda #$ff
:
    sta immediate+1
    ldy #24
    clc
    lda (ptr3),y
    adc immediate
    sta (ptr3),y
    iny
    lda (ptr3),y
    adc immediate+1
    sta (ptr3),y
    jmp next
unsupported:
    ; Roll back only the opcode fetch, without discarding its resident page.
    ldy #24
    lda old_ip
    sta (ptr3),y
    sta _fd_fetch_ip
    iny
    lda old_ip+1
    sta (ptr3),y
    sta _fd_fetch_ip+1
    lda _fd_fetch_bytes
    bne :+
    dec _fd_fetch_bytes+1
:
    dec _fd_fetch_bytes
    inc _fd_fetch_remaining
    bne :+
    inc _fd_fetch_remaining+1
:
finish:
    lda count
    ldx count+1
    jmp incsp2
cpu_pointer:
    lda cpu
    sta ptr3
    lda cpu+1
    sta ptr3+1
    rts
fetch:
    lda cpu
    ldx cpu+1
    jmp _fd_cpu_fetch
commit:
    lda opcode
    ldy #29
    sta (ptr3),y
    ldy #31
    clc
    lda (ptr3),y
    adc #1
    sta (ptr3),y
    bcc :+
    iny
    lda (ptr3),y
    adc #0
    sta (ptr3),y
    bcc :+
    iny
    lda (ptr3),y
    adc #0
    sta (ptr3),y
    bcc :+
    iny
    lda (ptr3),y
    adc #0
    sta (ptr3),y
:
    inc count
    bne :+
    inc count+1
:
    lda budget
    bne :+
    dec budget+1
:
    dec budget
    rts
calculate:
    lda cpu
    ldx cpu+1
    jsr pushax
    lda alu_op
    ldx #0
    jsr pushax
    lda value
    ldx value+1
    jsr pushax
    lda immediate
    ldx immediate+1
    jsr pushax
    lda width
    ldx #0
    jsr _fd_cpu_alu
    sta value
    stx value+1
    rts
