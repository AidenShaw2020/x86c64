/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <cbm.h>
#include "reu.h"
#include "../core/pager.h"
#include "../core/pc.h"
FDCPU cpu;
static uint8_t sector[512];
static uint32_t rounds;
static uint8_t ready,keyboard_ok,input_pos,recent_pos;
static uint8_t recent[32];
static const uint8_t ready_text[]={0x46,0x44,0x36,0x34,0x20,0x46,0x52,0x45,0x45,0x44,0x4f,0x53,0x20,0x52,0x45,0x41,0x44,0x59,0};
static const uint8_t key_text[]={10,0x46,0x44,0x36,0x34,0x20,0x4b,0x45,0x59,0x42,0x4f,0x41,0x52,0x44,0x20,0x4f,0x4b,13,0};
static const uint8_t keys[]={0x56,0x45,0x52,13,0x44,0x49,0x52,0x20,0x2f,0x42,13,
    0x45,0x43,0x48,0x4f,0x20,0x46,0x44,0x36,0x34,0x20,0x4b,0x45,0x59,0x42,0x4f,0x41,0x52,0x44,0x20,0x4f,0x4b,13};
static void debug_exit(uint8_t code) {
#ifdef FD_AUTOTEST
    unsigned frame;
    for(frame=0;frame<2;++frame) {
        while(*(volatile uint8_t *)0xd011&0x80) {}
        while(!(*(volatile uint8_t *)0xd011&0x80)) {}
        while(*(volatile uint8_t *)0xd011&0x80) {}
    }
    *(volatile uint8_t *)0xd7ff=code;
#else
    (void)code;
#endif
}
static int disk_read(uint32_t lba,uint8_t *data) {
    if(lba>=1440) return -1;
    fd_reu_fetch(0x100000UL+lba*512,data,512); return 0;
}
static int disk_write(uint32_t lba,const uint8_t *data) {
    if(lba>=1440) return -1;
    fd_reu_store(0x100000UL+lba*512,(uint8_t *)data,512); return 0;
}
static int contains(const uint8_t *text) {
    unsigned n=strlen((const char *)text);
    return recent_pos>=n && !memcmp(recent+recent_pos-n,text,n);
}
static int console(uint8_t ch) {
    if(recent_pos==sizeof recent) { memmove(recent,recent+1,--recent_pos); }
    recent[recent_pos++]=ch;
    if(contains(ready_text)) ready=1;
    if(contains(key_text)) keyboard_ok=1;
    /* PC CRLF: use one C64 newline; numeric values are ASCII, not PETSCII. */
    if(ch==10) cbm_k_bsout(13);
    else if(ch==8) cbm_k_bsout(20);
    else if(ch!=13 && ch!=7) {
        if(ch>=0x41 && ch<=0x5a) ch|=0x80;
        else if(ch>=0x61 && ch<=0x7a) ch-=0x20;
        cbm_k_bsout(ch);
    }
    return 0;
}
static int keyboard(uint16_t *key,uint8_t consume) {
    uint8_t ch;
#ifdef FD_AUTOTEST
    if(!ready || input_pos>=sizeof keys) return 0;
    ch=keys[input_pos]; if(consume) ++input_pos;
#else
    static uint8_t pending;
    if(!pending && kbhit()) {
        pending=cgetc();
        if(pending>=0xc1 && pending<=0xda) pending-=0x80;
        else if(pending>=0x41 && pending<=0x5a) pending+=0x20;
        if(pending==20) pending=8;
    }
    if(!pending) return 0;
    ch=pending; if(consume) pending=0;
#endif
    *key=((uint16_t)(ch==13 ? 0x1c:ch==8 ? 0x0e:0x20)<<8)|ch; return 1;
}
int main(void) {
    unsigned page; int status;
    clrscr(); puts("FD64 / FREEDOS BOOT PROTOTYPE");
    if(!fd_reu_probe() || !fd_reu_probe_2mb()) { puts("FAIL: 2MB REU REQUIRED"); debug_exit(1); return 1; }
    puts("1MB PC RAM + REU FLOPPY CACHE");
    puts("CLEARING PC RAM..."); fd_reu_clear();
    puts("LOADING FREEDOS.IMG FROM IEC8...");
    if(cbm_open(2,8,2,"freedos.img,s,r")) { cbm_close(2); puts("FAIL: DISK OPEN"); debug_exit(2); return 1; }
    for(page=0;page<1440;++page) {
        if(cbm_read(2,sector,512)!=512) { cbm_close(2); puts("FAIL: DISK READ"); debug_exit(2); return 1; }
        fd_reu_store(0x100000UL+(uint32_t)page*512,sector,512);
        if(!(page&127)) cputc('.');
    }
    cbm_close(2); puts("\nDISK READY: 720KB FAT12");
    fd_pager_init(fd_reu_read,fd_reu_write);
    if(fd_pc_init(console,keyboard,disk_read,disk_write) || fd_bios_boot(&cpu)) {
        puts("FAIL: BIOS BOOT"); debug_exit(3); return 1;
    }
    cpu.ivt_enabled=1;
    puts("BOOTING ACTUAL X86 FREEDOS...");
    for(;;) {
        status=fd_cpu_run(&cpu,1024,fd_pc_interrupt);
#ifdef FD_AUTOTEST
        if(ready && keyboard_ok) {
            puts("\nPASS: FREEDOS + KEYBOARD"); debug_exit(0); return 0;
        }
#endif
        if(status!=FD_X86_BUDGET && status!=FD_X86_WAIT && status!=FD_X86_HALT) {
            printf("\nFAIL %u OP %02X PC %04X:%04X\n",cpu.status,cpu.last_opcode,cpu.s[CS],cpu.ip);
            printf("INT %02X AH %02X INS %lu\n",fd_pc_last_interrupt,fd_pc_last_function,cpu.instructions);
            debug_exit(4); return 1;
        }
        ++rounds; fd_pc_tick(&cpu);
        if(status==FD_X86_HALT && !(cpu.flags&FD_IF)) { puts("FAIL: CPU HALTED"); debug_exit(5); return 1; }
    }
}
