/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "demo.h"
#include "pager.h"
/* xor ax,ax; mov ds,ax; mov ss,ax; mov sp,7c00; cld;
 * mov si,7c1b; mov ah,0e; lodsb; test al,al; jz hlt; int10; jmp lodsb; hlt */
static const uint8_t code[] = {
 0x31,0xc0,0x8e,0xd8,0x8e,0xd0,0xbc,0x00,0x7c,0xfc,
 0xbe,0x1b,0x7c,0xb4,0x0e,0xac,0x84,0xc0,0x74,0x04,0xcd,0x10,0xeb,0xf7,0xf4,
 0x90,0x90
};
const char fd_demo_expected[] = "FD64 X86 OK";
/* Numeric bytes preserve PC ASCII under cc65's PETSCII string mapping. */
const unsigned char fd_demo_ascii[] = {0x46,0x44,0x36,0x34,0x20,0x58,0x38,0x36,0x20,0x4f,0x4b,0};
int fd_demo_load(void) {
    unsigned i;
    for(i=0;i<sizeof code;++i) if(fd_mem_write(0x7c00UL+i,code[i])) return -1;
    for(i=0;i<sizeof fd_demo_ascii;++i)
        if(fd_mem_write(0x7c1bUL+i,fd_demo_ascii[i])) return -1;
    return 0;
}
