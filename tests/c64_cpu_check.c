/* SPDX-License-Identifier: GPL-2.0-or-later
 * Runs independently generated ALU vectors through the actual C64 assembly
 * paths. Native tests alone cannot validate these architecture-specific paths.
 */
#include <stdio.h>
#include <conio.h>
#include "../src/core/x86.h"
#include "../src/core/pager.h"
#include "../src/c64/reu.h"
typedef struct { uint16_t a,b,flags,result,expected_flags; uint8_t op,word; } Vector;
#include "cpu_vectors.h"
FDCPU check_cpu;
uint16_t check_index;
uint32_t check_signature;
uint8_t check_fail;
static int failed_read=-1,failed_write=-1;
static int checked_read(uint16_t page,uint8_t *data) {
    return page==failed_read ? -1 : fd_reu_read(page,data);
}
static int checked_write(uint16_t page,const uint8_t *data) {
    return page==failed_write ? -1 : fd_reu_write(page,data);
}
static uint8_t pager_checks(void) {
    uint16_t i,word;
    uint32_t address;
    static const uint32_t edges[]={0,1,0x1fe,0x1ff,0x200,0x9ffff,0xa0000,0xb7fff,
        0xb8000,0xbbfff,0xbc000,0xcffff,0xd0000,0xd07ff,0xd0800,0xfdfff,0xfe000,0xffffe,0xfffff,0x100000};
    /* Compare byte/word values across selected-frame hits, misses and wraps. */
    fd_pager_init(checked_read,checked_write);
    for(i=0;i<sizeof edges/sizeof edges[0];++i) {
        address=edges[i];
        if(fd_mem_write_word(address,0x1234) || fd_mem_read_word(address,&word) || word!=0x1234) return 13;
        if(fd_mem_write(address,0x56) || fd_mem_get(address)!=0x56) return 13;
        if(fd_mem_write_word(address,0xabcd) || fd_mem_read_word(address,&word) || word!=0xabcd ||
            fd_mem_get(address)!=0xcd || fd_mem_get(address+1)!=0xab) return 13;
    }
    if(fd_pager_flush()) return 14;
    fd_pager_init(checked_read,checked_write);
    for(i=0;i<8;++i) if(fd_mem_write_word((uint32_t)i*512,0xab00|i)) return 14;
    /* Force the assembler hit write and verify dirty eviction preserves it. */
    if(fd_mem_get(0)!=0 || fd_mem_write_word(0,0x5678)) return 14;
    failed_write=0;
    if(fd_mem_get(4096)!=-1 || fd_mem_get(0)!=0x78 || fd_pager_flush()!=-1) return 14;
    failed_write=-1;
    if(fd_pager_flush()) return 14;
    fd_pager_init(checked_read,checked_write);
    if(fd_mem_read_word(0,&word) || word!=0x5678) return 14;
    failed_read=8;
    if(fd_mem_get(4096)!=-1 || fd_mem_read_word(4096,&word)!=-1 || fd_mem_write_word(4096,1)!=-1 ||
        fd_mem_write(4096,1)!=-1 || fd_mem_get(0)!=0x78) return 15;
    failed_read=-1;
    /* ROM protection and holes must work on both cache hits and cold misses. */
    if(fd_mem_write_word(0xfe000,0x4567) || fd_mem_write_word(0xd0000,0x89ab)) return 16;
    fd_mem_pc_map(1);
    if(fd_mem_read_word(0xfe000,&word) || word!=0x4567 || fd_mem_write_word(0xfe000,0) ||
        fd_mem_read_word(0xfe000,&word) || word!=0x4567 || fd_mem_write(0xfe000,0) || fd_mem_get(0xfe000)!=0x67) return 16;
    if(fd_mem_read_word(0xd0000,&word) || word!=0x89ab || fd_mem_write_word(0xd0000,0) ||
        fd_mem_read_word(0xd0000,&word) || word!=0x89ab) return 16;
    for(i=0;i<sizeof edges/sizeof edges[0];++i) {
        address=edges[i]&0xfffffUL;
        if(address>=0xa0000 && address<0xfe000 && !(address>=0xb8000 && address<0xbc000) &&
            !(address>=0xd0000 && address<0xd0800)) {
            if(fd_mem_get(address)!=255 || fd_mem_write(address,0) || fd_mem_get(address)!=255) return 17;
        }
    }
    /* A word spanning a hole/ROM boundary must not fabricate its other byte. */
    if(fd_mem_read_word(0xfdfff,&word) || word!=0x67ff) return 17;
    fd_video_dirty=0;
    if(fd_mem_write_word(0xb809f,0x0720) || fd_video_dirty!=3) return 18;
    fd_video_dirty=0;
    if(fd_mem_write(0xb80a0,0x41) || fd_video_dirty!=2) return 18;
    if(fd_mem_write_word(0x1ffe,0x1234)) return 18;
    fd_video_dirty=0;
    if(fd_mem_write_word(0x1ffe,0x5678) || fd_video_dirty) return 18;
    fd_mem_pc_map(0);
    return fd_pager_flush() ? 18 : 0;
}
static uint8_t fast_checks(void) {
    uint16_t i; uint8_t opcode;
    static const uint8_t moves[]={0xb8,0x34,0x12,0xb4,0x56,0xb1,0x9a,0xb7,0xbc,0xbf,0x0f,0x7c,0xf4};
    static const uint8_t loop[]={0x40,0xe2,0xfd,0xf4};
    static const uint8_t prefix[]={0x26,0xa1,0xff,0x01,0xf4};
    for(i=0;i<sizeof moves;++i) fd_mem_write(0x7c00+i,moves[i]);
    fd_cpu_init(&check_cpu);
    while(fd_cpu_run(&check_cpu,2,0)==FD_X86_BUDGET) {}
    if(check_cpu.status!=FD_X86_HALT || check_cpu.r[AX]!=0x5634 || check_cpu.r[CX]!=0x9a ||
        check_cpu.r[BX]!=0xbc00 || check_cpu.r[DI]!=0x7c0f || check_cpu.instructions!=6) return 4;
    /* INC/DEC preserve carry, wrap registers, and retain exact batch limits. */
    for(opcode=0x40;opcode<0x50;++opcode) {
        fd_mem_write(0x7c00,opcode); fd_cpu_init(&check_cpu);
        check_cpu.r[opcode&7]=(opcode&8)?0x8000:0x7fff; check_cpu.flags=3;
        if(fd_cpu_run(&check_cpu,1,0)!=FD_X86_BUDGET || check_cpu.instructions!=1 ||
            check_cpu.r[opcode&7]!=((opcode&8)?0x7fff:0x8000) || !(check_cpu.flags&FD_CF) ||
            !(check_cpu.flags&FD_OF)) return 5;
    }
    for(i=0;i<sizeof loop;++i) fd_mem_write(0x7c00+i,loop[i]);
    fd_cpu_init(&check_cpu); check_cpu.r[CX]=20; check_cpu.instructions=0xfffffffeUL;
    if(fd_cpu_run(&check_cpu,17,0)!=FD_X86_BUDGET || check_cpu.r[AX]!=9 || check_cpu.r[CX]!=12 ||
        check_cpu.instructions!=15 || check_cpu.ip!=0x7c01) return 6;
    if(fd_cpu_run(&check_cpu,64,0)!=FD_X86_HALT || check_cpu.r[AX]!=20 || check_cpu.r[CX]!=0) return 6;
    /* Prefix fallback, an unaligned segment, and word crossing a cache page. */
    for(i=0;i<sizeof prefix;++i) fd_mem_write(0x7c00+i,prefix[i]);
    fd_mem_write_word(0x121f,0x1234); fd_cpu_init(&check_cpu); check_cpu.s[ES]=0x102;
    if(fd_cpu_run(&check_cpu,8,0)!=FD_X86_HALT || check_cpu.r[AX]!=0x1234) return 7;
    /* Physical 20-bit bus wrap differs from 16-bit IP wrap. */
    fd_mem_write(0xfffff,0xb8); fd_mem_write(0,0x34); fd_mem_write(1,0x12); fd_mem_write(2,0xf4);
    fd_cpu_init(&check_cpu); check_cpu.s[CS]=0xffff; check_cpu.ip=15;
    if(fd_cpu_run(&check_cpu,8,0)!=FD_X86_HALT || check_cpu.r[AX]!=0x1234) return 8;
    fd_mem_write(0xffff,0xb8); fd_mem_write(0,0x78); fd_mem_write(1,0x56); fd_mem_write(2,0xf4);
    fd_cpu_init(&check_cpu); check_cpu.ip=0xffff;
    if(fd_cpu_run(&check_cpu,8,0)!=FD_X86_HALT || check_cpu.r[AX]!=0x5678 || check_cpu.ip!=3) return 9;
    /* Same-page self modification and eviction must never reuse stale code. */
    fd_mem_write(0x7c00,0x40); fd_cpu_init(&check_cpu); fd_cpu_run(&check_cpu,1,0);
    fd_mem_write(0x7c00,0x48); check_cpu.ip=0x7c00;
    if(fd_cpu_run(&check_cpu,1,0)!=FD_X86_BUDGET || check_cpu.r[AX]!=0) return 10;
    fd_pager_flush();
    for(i=0;i<16;++i) fd_mem_get((uint32_t)i*512);
    check_cpu.ip=0x7c00;
    if(fd_cpu_run(&check_cpu,1,0)!=FD_X86_BUDGET || check_cpu.r[AX]!=0xffff) return 11;
    /* Taken/not-taken JC/JNC/JZ/JNZ, forward and backward displacement. */
    for(opcode=0x72;opcode<=0x75;++opcode) for(i=0;i<2;++i) {
        fd_mem_write(0x7c00,opcode); fd_mem_write(0x7c01,0xfe);
        fd_cpu_init(&check_cpu); check_cpu.flags=2|(i?(opcode<0x74?FD_CF:FD_ZF):0);
        fd_cpu_run(&check_cpu,1,0);
        if(check_cpu.ip!=((i!=(opcode&1))?0x7c00:0x7c02)) return 12;
    }
    return 0;
}
int main(void) {
    const Vector *v; uint16_t result;
    clrscr(); puts("FD64 / 6510 CPU REGRESSION");
    if(!fd_reu_probe()) { check_fail=1; goto finished; }
    fd_reu_clear(); fd_pager_init(fd_reu_read,fd_reu_write);
    for(check_index=0;check_index<sizeof vectors/sizeof vectors[0];++check_index) {
        v=vectors+check_index;
        fd_mem_write(0x7c00,(v->op<<3)|(v->word?5:4));
        fd_mem_write(0x7c01,(uint8_t)v->b); fd_mem_write(0x7c02,(uint8_t)(v->b>>8));
        fd_cpu_init(&check_cpu); check_cpu.r[AX]=v->a; check_cpu.flags=v->flags;
        if(fd_cpu_run(&check_cpu,1,0)!=FD_X86_BUDGET || check_cpu.r[AX]!=v->result ||
            check_cpu.flags!=v->expected_flags) { check_fail=2; goto finished; }
    }
    /* Verify boundary fallback and 20-bit wrap on the actual cc65 pager. */
    if(fd_mem_write_word(0xfffff,0x1234) || fd_mem_read_word(0xfffff,&result) || result!=0x1234 ||
        fd_mem_write_word(0x3ff,0xabcd) || fd_mem_read_word(0x3ff,&result) || result!=0xabcd) check_fail=3;
    if(!check_fail) check_fail=fast_checks();
    if(!check_fail) check_fail=pager_checks();
finished:
    check_signature=0x43505521UL; for(;;) {}
    return 0;
}
