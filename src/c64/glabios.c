/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <conio.h>
#include <cbm.h>
#include "reu.h"
#include "video.h"
#ifdef FD_DISK_1581
#include "drive1581.h"
void fd_cart_load_bios(void);
#endif
#include "../core/pager.h"
#include "../core/xt.h"
FDCPU cpu;
uint8_t fd_test_pass,fd_test_fail;
static uint8_t sector[512];
static uint32_t rounds;
static uint8_t pending_key;
#ifdef FD_DISK_1581
static uint8_t shift_phase;
#endif
static int disk_read(uint32_t lba,uint8_t *data) {
#ifdef FD_DISK_1581
    return fd_1581_read(lba,data);
#else
    if(lba>=1440) return -1; fd_reu_fetch(0x100000UL+lba*512,data,512); return 0;
#endif
}
static int disk_write(uint32_t lba,const uint8_t *data) {
#ifdef FD_DISK_1581
    return fd_1581_write(lba,data);
#else
    if(lba>=1440) return -1; fd_reu_store(0x100000UL+lba*512,(uint8_t *)data,512); return 0;
#endif
}
static int console(uint8_t ch) { (void)ch; return 0; }
static uint8_t scan(uint8_t ch) {
    static const uint8_t letters[]={30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};
    if(ch>=0x61 && ch<=0x7a) return letters[ch-0x61];
    if(ch>=0x41 && ch<=0x5a) return letters[ch-0x41];
    if(ch==13) return 28; if(ch==8) return 14; if(ch==32) return 57;
    if(ch==0x2f) return 53; if(ch>=0x31 && ch<=0x39) return ch-0x31+2;
#ifdef FD_DISK_1581
    if(ch=='.' || ch=='>') return 52;
    if(ch=='\\') return 43;
    if(ch==':') return 39;
#endif
    if(ch==0x30) return 11; return 0;
}
static int keyboard(uint16_t *key,uint8_t consume) {
    uint8_t ch;
#ifdef FD_AUTOTEST
    static const uint8_t keys[]={0x3a,0x2f,0x12,0x13,0x1c,0x20,0x17,0x13,0x39,0x35,0x30,0x1c,
        0x12,0x2e,0x23,0x18,0x39,0x21,0x20,0x07,0x05,0x39,0x25,0x12,0x15,0x30,0x18,0x1e,0x13,0x20,0x39,0x18,0x25,0x1c};
    static uint8_t position;
    if(!fd_test_pass) {
        if(!fd_video_ready || position>=sizeof keys) return 0;
        if(fd_mem_get(0x41a)!=fd_mem_get(0x41c) || fd_mem_get(0x41b)!=fd_mem_get(0x41d)) return 0;
        *key=(uint16_t)keys[position]<<8; if(consume) ++position; return 1;
    }
#endif
    if(!pending_key) return 0; ch=pending_key;
#ifdef FD_DISK_1581
    if(ch=='>' || ch==':') {
        /* Deliver genuine Shift make/key/break codes through IRQ1. */
        *key=(uint16_t)(shift_phase==0?42:(shift_phase==1?scan(ch):170))<<8;
        if(consume) { if(++shift_phase==3) { shift_phase=0; pending_key=0; } }
        return 1;
    }
#endif
    if(consume) pending_key=0;
    *key=((uint16_t)scan(ch)<<8)|ch; return scan(ch)!=0;
}
static void poll_keyboard(void) {
    uint8_t ch;
    if(pending_key || !kbhit()) return;
    ch=cgetc();
    if(ch==CH_F1) { fd_video_mode(0); return; }
    if(ch==CH_F3) { fd_video_mode(1); return; }
    if(ch==CH_F5) { fd_video_mode(2); return; }
    if(ch>=0xc1 && ch<=0xda) ch-=0x80;
    if(ch==20) ch=8; pending_key=ch;
}
static int load(const char *name,uint16_t pages,uint32_t address) {
    uint16_t page;
    if(cbm_open(2,8,2,name)) { cbm_close(2); return -1; }
    for(page=0;page<pages;++page) {
        if(cbm_read(2,sector,512)!=512) { cbm_close(2); return -1; }
        fd_reu_store(address+(uint32_t)page*512,sector,512);
        if(!(page&127)) cputc('.');
    }
    cbm_close(2); return 0;
}
int main(void) {
    int status;
    clrscr(); puts("FD64 / GLABIOS PC-XT");
#ifdef FD_DISK_1581
    if(!fd_reu_probe()) { puts("FAIL: 1MB REU REQUIRED"); return 1; }
#else
    if(!fd_reu_probe() || !fd_reu_probe_2mb()) { puts("FAIL: 2MB REU REQUIRED"); return 1; }
#endif
    puts("CLEARING PC RAM..."); fd_reu_clear();
    puts("LOADING GLABIOS.ROM...");
#ifdef FD_DISK_1581
    puts("BIOS FROM CARTRIDGE ROM..."); fd_cart_load_bios();
#else
    if(load("glabios.rom,s,r",16,0xfe000UL)) { puts("FAIL: BIOS ROM READ"); return 1; }
#endif
#ifdef FD_DISK_1581
    puts("\nOPENING DOS ON 1581 / DEVICE 8...");
    if(fd_1581_open()) { printf("FAIL: 1581 ERROR %u\n",fd_1581_error); return 1; }
#else
    puts("\nLOADING FREEDOS.IMG...");
    if(load("freedos.img,s,r",1440,0x100000UL)) { puts("FAIL: FLOPPY READ"); return 1; }
#endif
    fd_pager_init(fd_reu_read,fd_reu_write);
    if(fd_xt_option_rom()) { puts("FAIL: OPTION ROM"); return 1; }
    fd_mem_pc_map(1); fd_xt_init(console,keyboard,disk_read,disk_write);
    fd_cpu_reset(&cpu); cpu.ivt_enabled=1;
    puts("\nRESET FFFF:0000 / ACTUAL BIOS POST");
    puts("F1 LEFT / F3 RIGHT / F5 ALL 80 COLS");
    fd_video_init();
    for(;;) {
        status=fd_cpu_run(&cpu,1024,fd_xt_interrupt);
        poll_keyboard();
        if(!(rounds++&15)) fd_video_refresh();
#ifdef FD_AUTOTEST
        if(fd_video_ready && fd_video_keyboard_ok && fd_video_files) {
            fd_test_pass=1; /* Keep the completed shell visible and interactive. */
        }
#endif
        if(status!=FD_X86_BUDGET && status!=FD_X86_HALT) {
            fd_test_fail=cpu.status;
            fd_video_refresh(); fd_video_text();
            printf("\nFAIL %u OP %02X PC %04X:%04X\n",cpu.status,cpu.last_opcode,cpu.s[CS],cpu.ip);
            printf("INS %lu POST %02X\n",cpu.instructions,fd_xt_post_code); return 1;
        }
        fd_xt_tick(&cpu);
    }
}
