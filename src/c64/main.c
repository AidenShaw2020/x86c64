/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <cbm.h>
#include "reu.h"
#include "../core/pager.h"
#include "../core/x86.h"
#include "../core/demo.h"
#include "../core/bios.h"
#include "disk.h"
static FDCPU cpu;
static unsigned output_pos;
static int output_error;
static const uint8_t *expected;
static unsigned expected_length;
static const uint8_t disk_expected[]={
    0x46,0x44,0x36,0x34,0x20,0x44,0x49,0x53,0x4b,0x20,
    0x42,0x4f,0x4f,0x54,0x20,0x4f,0x4b
};
static void debug_exit(uint8_t result) {
#ifdef FD_AUTOTEST
    /* debugcart D7FF: emulator exit status. */
    *(volatile uint8_t *)0xd7ff = result;
#else
    (void)result;
#endif
}
static int console(uint8_t ch) {
    if(output_pos >= expected_length || ch != expected[output_pos]) output_error=1;
    output_pos++;
    cbm_k_bsout(ch >= 0x41 && ch <= 0x5a ? ch | 0x80 : ch);
    return 0;
}
static int memory_test(void) {
    unsigned i;
    uint8_t v;
    for(i=0;i<24;++i) if(fd_mem_write((uint32_t)i*512+511,(uint8_t)(i+1))) return -1;
    for(i=0;i<24;++i)
        if(fd_mem_read((uint32_t)i*512+511,&v) || v!=i+1) return -1;
    if(fd_mem_write(0xfffffUL,0xab) || fd_mem_write(0x100000UL,0xcd)) return -1;
    if(fd_mem_read(0xfffffUL,&v) || v!=0xab) return -1;
    if(fd_mem_read(0,&v) || v!=0xcd) return -1;
    return fd_pager_flush();
}
int main(void) {
    int result;
    clrscr(); puts("FD64 HOST BIOS / PC-XT PROTOTYPE");
    if(!fd_reu_probe()) { puts("FAIL: 1MB REU REQUIRED"); debug_exit(1); return 1; }
    puts("POST: 1MB REU FOUND, CLEARING...");
    fd_reu_clear(); fd_pager_init(fd_reu_read,fd_reu_write);
    if(memory_test() || fd_demo_load()) { puts("FAIL: MEMORY"); debug_exit(2); return 2; }
    puts("POST: RAM SAMPLE + PAGER OK");
    puts("POST: X86 CPU CHECK:");
    expected=fd_demo_ascii; expected_length=strlen(fd_demo_expected);
    fd_bios_init(console,fd_c64_sector_read,0);
    fd_cpu_init(&cpu);
    result=fd_cpu_run(&cpu,1024,fd_bios_interrupt);
    if(result!=FD_X86_HALT || output_error ||
       output_pos!=strlen(fd_demo_expected)) {
        printf("\nFAIL: STATUS %u OP %02X\n",cpu.status,cpu.last_opcode);
        printf("PC %04X CS %04X AX %04X\n",cpu.ip,cpu.s[CS],cpu.r[AX]);
        debug_exit(3); return 3;
    }
    if(fd_pager_flush()) { puts("FAIL: FLUSH"); debug_exit(4); return 4; }
    puts("\nPOST: CPU CHECK OK");
    puts("BOOT: IEC8 / XTBOOT.IMG");
    result=fd_bios_boot(&cpu);
    if(result!=FD_BOOT_OK) {
        printf("FAIL: BOOT %s\n",result==FD_BOOT_IO ? "READ" :
            result==FD_BOOT_SIGNATURE ? "SIGNATURE" : "MEMORY");
        fd_c64_disk_close(); debug_exit((uint8_t)(4+result)); return 1;
    }
    puts("POST: DISK SIGNATURE OK");
    puts("BOOTING X86 AT 0000:7C00:");
    output_pos=0; output_error=0;
    expected=disk_expected; expected_length=sizeof disk_expected;
    result=fd_cpu_run(&cpu,1024,fd_bios_interrupt);
    fd_c64_disk_close();
    if(result!=FD_X86_HALT || output_error || output_pos!=expected_length) {
        printf("\nFAIL: DISK BOOT %u OP %02X\n",cpu.status,cpu.last_opcode);
        printf("PC %04X CS %04X AX %04X\n",cpu.ip,cpu.s[CS],cpu.r[AX]);
        debug_exit(8); return 1;
    }
    if(fd_pager_flush()) { puts("FAIL: FLUSH"); debug_exit(4); return 4; }
    puts("\nPASS: CPU + RAM + DISK BOOT");
    puts("FREEDOS BOOT NOT IMPLEMENTED");
#ifdef FD_AUTOTEST
    /* Let two complete VIC frames render before the exit screenshot. */
    {
        unsigned frame;
        for(frame=0;frame<2;++frame) {
            while(*(volatile uint8_t *)0xd011 & 0x80) {}
            while(!(*(volatile uint8_t *)0xd011 & 0x80)) {}
            while(*(volatile uint8_t *)0xd011 & 0x80) {}
        }
    }
#endif
    debug_exit(0);
    return 0;
}
