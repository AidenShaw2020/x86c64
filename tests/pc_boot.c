/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pager.h"
#include "pc.h"
static uint8_t ram[1048576],disk[737280];
static unsigned key_pos,ready,keyboard_ok,commands_ok;
static char recent[128]; static unsigned recent_pos;
static const char keys[]="VER\rDIR /B\rECHO FD64 KEYBOARD OK\r";
static int read_page(uint16_t p,uint8_t *data) { memcpy(data,ram+(uint32_t)p*512,512); return 0; }
static int write_page(uint16_t p,const uint8_t *data) { memcpy(ram+(uint32_t)p*512,data,512); return 0; }
static int read_sector(uint32_t p,uint8_t *data) { if(p>=1440) return -1; memcpy(data,disk+p*512,512); return 0; }
static int write_sector(uint32_t p,const uint8_t *data) { if(p>=1440) return -1; memcpy(disk+p*512,data,512); return 0; }
static int console_put(uint8_t ch) {
    putchar(ch); fflush(stdout);
    if(recent_pos==sizeof recent-1) { memmove(recent,recent+1,--recent_pos); }
    recent[recent_pos++]=(char)ch; recent[recent_pos]=0;
    if(strstr(recent,"FD64 FREEDOS READY")) ready=1;
    if(strstr(recent,"\nFD64 KEYBOARD OK\r")) keyboard_ok=1;
    if(strstr(recent,"KERNEL.SYS")) commands_ok=1;
    return 0;
}
static int keyboard(uint16_t *key,uint8_t consume) {
    unsigned char ch;
    if(!ready || key_pos>=sizeof(keys)-1) return 0;
    ch=(unsigned char)keys[key_pos]; *key=ch|((uint16_t)(ch==13 ? 0x1c:0x20)<<8);
    if(consume) ++key_pos; return 1;
}
int main(int argc,char **argv) {
    FILE *f; FDCPU c; unsigned long rounds; int status; uint8_t byte;
    if(argc!=2) return 2;
    f=fopen(argv[1],"rb"); if(!f || fread(disk,1,sizeof disk,f)!=sizeof disk) return 2; fclose(f);
    fd_pager_init(read_page,write_page); fd_cpu_io(0,0);
    if(fd_pc_init(console_put,keyboard,read_sector,write_sector) || fd_bios_boot(&c)) return 2;
    c.ivt_enabled=1;
    for(rounds=0;rounds<500000;++rounds) {
        status=fd_cpu_run(&c,1024,fd_pc_interrupt);
        if(ready && keyboard_ok && commands_ok) { printf("\nFD64_FREEDOS_BOOT_PASS instructions=%lu\n",(unsigned long)c.instructions); return 0; }
        if(status!=FD_X86_BUDGET && status!=FD_X86_WAIT && status!=FD_X86_HALT) break;
        if(status==FD_X86_WAIT && key_pos>=sizeof(keys)-1) break;
        fd_pc_tick(&c);
        if(status==FD_X86_HALT && !(c.flags&FD_IF)) break;
    }
    fd_mem_read(fd_physical(c.s[CS],c.ip),&byte);
    fprintf(stderr,"\nBOOT STOP status=%u opcode=%02X PC=%04X:%04X next=%02X INT=%02X AH=%02X ins=%lu\n",
        c.status,c.last_opcode,c.s[CS],c.ip,byte,fd_pc_last_interrupt,fd_pc_last_function,(unsigned long)c.instructions);
    fprintf(stderr,"AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X ES=%04X SS=%04X SP=%04X\n",
        c.r[AX],c.r[BX],c.r[CX],c.r[DX],c.s[DS],c.s[ES],c.s[SS],c.r[SP]);
    return 1;
}
