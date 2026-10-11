/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xt.h"
#include "pager.h"
static uint8_t ram[1048576],disk[737280];
static int read_page(uint16_t p,uint8_t *d) { memcpy(d,ram+(uint32_t)p*512,512); return 0; }
static int write_page(uint16_t p,const uint8_t *d) { memcpy(ram+(uint32_t)p*512,d,512); return 0; }
static int read_sector(uint32_t p,uint8_t *d) { if(p>=1440) return -1; memcpy(d,disk+p*512,512); return 0; }
static int write_sector(uint32_t p,const uint8_t *d) { if(p>=1440) return -1; memcpy(disk+p*512,d,512); return 0; }
static int output(uint8_t c) { (void)c; return 0; }
static unsigned ready,keypos,full_boot,keyboard_ok;
static const uint8_t keys[]={0x3a,0x2f,0x12,0x13,0x1c,0x20,0x17,0x13,0x39,0x35,0x30,0x1c,
    0x12,0x2e,0x23,0x18,0x39,0x21,0x20,0x07,0x05,0x39,0x25,0x12,0x15,0x30,0x18,0x1e,0x13,0x20,0x39,0x18,0x25,0x1c};
static int keyboard(uint16_t *k,uint8_t consume) {
    if(!ready || keypos>=sizeof keys) return 0;
    /* Wait for the previous make code to be consumed by DOS, not an arbitrary delay. */
    if(fd_mem_get(0x41a)!=fd_mem_get(0x41c) || fd_mem_get(0x41b)!=fd_mem_get(0x41d)) return 0;
    *k=(uint16_t)keys[keypos]<<8; if(consume) ++keypos; return 1;
}
static void check_screen(void) {
    unsigned i; char text[2001];
    for(i=0;i<2000;++i) text[i]=(char)fd_mem_get(0xb8000UL+i*2); text[2000]=0;
    if(strstr(text,"FD64 FREEDOS READY")) ready=1;
    /* Require the ECHO response on a new line, separately from the command. */
    for(i=0;i<25;++i) if(!memcmp(text+i*80,"FD64 KEYBOARD OK ",17)) keyboard_ok=1;
}
static void screen(void) {
    unsigned y,x; int ch;
    for(y=0;y<25;++y) { for(x=0;x<80;++x) { ch=fd_mem_get(0xb8000UL+y*160+x*2); putchar(ch>=32&&ch<127?ch:' '); } putchar('\n'); }
}
int main(int argc,char **argv) {
    FILE *f; FDCPU c; unsigned long round,last_trace=0; int s,trace=getenv("FD64_BOOT_TRACE")!=0; unsigned i;
    if(argc<3 || argc>4) return 2; full_boot=argc==4;
    f=fopen(argv[1],"rb"); if(!f || fread(ram+0xfe000,1,8192,f)!=8192) return 2; fclose(f);
    f=fopen(argv[2],"rb"); if(!f || fread(disk,1,sizeof disk,f)!=sizeof disk) return 2; fclose(f);
    fd_pager_init(read_page,write_page); fd_xt_option_rom(); fd_mem_pc_map(1);
    fd_xt_init(output,keyboard,read_sector,write_sector); fd_cpu_reset(&c); c.ivt_enabled=1;
    for(round=0;round<200000;++round) {
        s=fd_cpu_run(&c,1024,fd_xt_interrupt);
        if(trace && c.instructions/250000UL!=last_trace/250000UL) {
            printf("TRACE ins=%lu PC=%04X:%04X stage=%s\n",(unsigned long)c.instructions,c.s[CS],c.ip,
                ready?"shell":(fd_xt_booted?"DOS boot":"POST"));
            last_trace=c.instructions;
        }
        if(fd_xt_booted && !full_boot) { screen(); printf("FD64_GLABIOS_POST_PASS ins=%lu PC=%04X:%04X\n",(unsigned long)c.instructions,c.s[CS],c.ip); return 0; }
        if(full_boot && fd_video_dirty) { check_screen(); fd_video_dirty=0; }
        if(keyboard_ok) { screen(); printf("FD64_GLABIOS_FREEDOS_PASS ins=%lu\n",(unsigned long)c.instructions); return 0; }
        if(s!=FD_X86_BUDGET && s!=FD_X86_HALT) break;
        fd_xt_tick(&c);
    }
    screen(); fprintf(stderr,"STOP %u OP %02X %04X:%04X ins=%lu\n",c.status,c.last_opcode,c.s[CS],c.ip,(unsigned long)c.instructions);
    fprintf(stderr,"AX%04X BX%04X CX%04X DX%04X SI%04X DI%04X SS%04X SP%04X DS%04X ES%04X FLAGS%04X\n",c.r[AX],c.r[BX],c.r[CX],c.r[DX],c.r[SI],c.r[DI],c.s[SS],c.r[SP],c.s[DS],c.s[ES],c.flags);
    for(i=0;i<16;++i) fprintf(stderr,"%02X ",fd_mem_get(fd_physical(c.s[CS],c.ip+i))); return 1;
}
