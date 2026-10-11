/* SPDX-License-Identifier: GPL-2.0-or-later
 * Deterministic interpreter workloads. CIA2 counts actual C64 cycles.
 * No PC timing, ROM or instruction replacement is used.
 */
#include <stdio.h>
#include <conio.h>
#include "../src/core/x86.h"
#include "../src/core/pager.h"
#include "../src/c64/reu.h"
#define CIA ((volatile uint8_t *)0xdd00)
FDCPU bench_cpu;
uint32_t bench_cycles[3],bench_instructions[3];
uint32_t bench_signature;
uint8_t bench_done,bench_fail;
static const uint8_t registers[]={0x40,0xe2,0xfd,0xf4};
static const uint8_t memory[]={0xa1,0x00,0x04,0x40,0xa3,0x00,0x04,0x50,0x58,0xe2,0xf5,0xf4};
static const uint8_t arithmetic[]={0x05,0x13,0x00,0x35,0x55,0xaa,0xd1,0xc0,0xe2,0xf6,0xf4};
static void timer_start(void) {
    CIA[14]=0; CIA[15]=0; CIA[13]=0x7f; (void)CIA[13];
    CIA[4]=255; CIA[5]=255; CIA[6]=255; CIA[7]=255;
    CIA[15]=0x51; CIA[14]=0x11;
}
static uint32_t timer_stop(void) {
    uint32_t remaining;
    CIA[14]=0; CIA[15]=0;
    remaining=(uint32_t)CIA[4]|((uint32_t)CIA[5]<<8)|
        ((uint32_t)CIA[6]<<16)|((uint32_t)CIA[7]<<24);
    return 0xffffffffUL-remaining;
}
static void workload(uint8_t index,const uint8_t *code,uint8_t size,uint16_t loops) {
    uint8_t i; int status;
    fd_pager_init(fd_reu_read,fd_reu_write);
#ifdef FD_BENCH_PC_MAP
    fd_mem_pc_map(1);
#endif
    for(i=0;i<size;++i) if(fd_mem_write(0x7c00UL+i,code[i])) bench_fail=1;
    fd_mem_write(0x400,0); fd_mem_write(0x401,0);
    fd_cpu_init(&bench_cpu); bench_cpu.r[CX]=loops;
    timer_start();
    do { status=fd_cpu_run(&bench_cpu,1024,0); } while(status==FD_X86_BUDGET);
    bench_cycles[index]=timer_stop(); bench_instructions[index]=bench_cpu.instructions;
    if(status!=FD_X86_HALT || bench_cpu.r[CX]!=0 ||
        (index<2 && bench_cpu.r[AX]!=loops) || bench_cpu.r[SP]!=0x7c00) bench_fail=2;
}
int main(void) {
    clrscr(); puts("FD64 / 6510 X86 BENCHMARK");
    if(!fd_reu_probe()) { bench_fail=3; bench_done=1; for(;;) {} }
    fd_reu_clear();
    /* The harness owns CIA2 during measurement; KERNAL IRQ remains masked. */
    __asm__("sei");
    workload(0,registers,sizeof registers,8192);
    workload(1,memory,sizeof memory,4096);
    workload(2,arithmetic,sizeof arithmetic,4096);
    bench_done=1; bench_signature=0x64363838UL; for(;;) {}
    return 0;
}
