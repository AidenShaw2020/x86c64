/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pc.h"
#include "pager.h"
static fd_console_put console_put;
static fd_keyboard keyboard_get;
static uint8_t row,column,mode=3;
static uint32_t ticks;
uint8_t fd_pc_last_interrupt, fd_pc_last_function;
static uint8_t read8(uint32_t p) { uint8_t v=0; fd_mem_read(p,&v); return v; }
static uint16_t read16(uint32_t p) { uint16_t v=read8(p); return v|((uint16_t)read8(p+1)<<8); }
static void write16(uint32_t p,uint16_t v) { fd_mem_write(p,(uint8_t)v); fd_mem_write(p+1,(uint8_t)(v>>8)); }
static void cursor(void) { write16(0x450,((uint16_t)row<<8)|column); }
static void scroll_line(void) {
    uint32_t destination=0xb8000UL,source=0xb80a0UL;
    uint16_t remaining=3840,span,other;
    while(remaining) {
        span=(uint16_t)(512-(destination&511)); other=(uint16_t)(512-(source&511));
        if(span>other) span=other; if(span>remaining) span=remaining;
        fd_mem_copy(destination,source,span);
        destination+=span; source+=span; remaining-=span;
    }
    remaining=160;
    while(remaining) {
        span=(uint16_t)(512-(destination&511)); if(span>remaining) span=remaining;
        fd_mem_fill(destination,0x0720,span,1); destination+=span; remaining-=span;
    }
}
static int character(uint8_t ch) {
    if(console_put && console_put(ch)) return -1;
    if(ch==13) column=0;
    else if(ch==10) ++row;
    else if(ch==8) { if(column) --column; }
    else if(ch!=7) {
        fd_mem_write(0xb8000UL+((uint16_t)row*80+column)*2,ch);
        if(++column==80) { column=0; ++row; }
    }
    if(row>=25) {
        scroll_line();
        row=24;
    }
    cursor(); return 0;
}
int fd_pc_init(fd_console_put console, fd_keyboard keyboard,
    fd_sector_read read_sector, fd_sector_write write_sector) {
    unsigned n,j;
    static const uint8_t dpt[]={0xdf,2,0x25,2,9,0x1b,0xff,0x54,0xf6,15,8};
    console_put=console; keyboard_get=keyboard; row=column=0; mode=3; ticks=0;
    fd_bios_init(console,read_sector,write_sector); fd_bios_geometry(80,2,9,0);
    for(n=0;n<256;++n) {
        write16(n*4,0x100+n*4); write16(n*4+2,0xf000);
        if(fd_mem_write(0xf0100UL+n*4,0xcd) || fd_mem_write(0xf0101UL+n*4,(uint8_t)n) ||
            fd_mem_write(0xf0102UL+n*4,0xcf)) return -1;
    }
    /* IRQ and user hooks default to IRET, not recursive INT. */
    for(n=8;n<=15;++n) fd_mem_write(0xf0100UL+n*4,0xcf);
    fd_mem_write(0xf0170,0xcf); /* INT1C */
    write16(0x410,0x21); write16(0x413,640); write16(0x44a,80);
    fd_mem_write(0x449,3); fd_mem_write(0x484,24); fd_mem_write(0x475,0);
    write16(0x41a,0x1e); write16(0x41c,0x1e); write16(0x480,0x1e); write16(0x482,0x3e);
    write16(0x463,0x3d4); write16(0x78,0xff00); write16(0x7a,0xf000);
    for(j=0;j<sizeof dpt;++j) fd_mem_write(0xfff00UL+j,dpt[j]);
    for(j=0;j<2000;++j) write16(0xb8000UL+(uint32_t)j*2,0x0720);
    return fd_pager_flush();
}
static int video(FDCPU *c,uint8_t f) {
    uint16_t count,i,p; uint8_t ch,attr,lines,top,left,bottom,right,r,col;
    switch(f) {
    case 0: mode=(uint8_t)c->r[AX]&0x7f; row=column=0; fd_mem_write(0x449,mode); cursor(); return 0;
    case 1: return 0;
    case 2: row=(uint8_t)(c->r[DX]>>8); column=(uint8_t)c->r[DX]; cursor(); return 0;
    case 3: c->r[DX]=read16(0x450+((c->r[BX]>>8)&7)*2); c->r[CX]=0x0607; return 0;
    case 5: fd_mem_write(0x462,(uint8_t)c->r[AX]); return 0;
    case 6: case 7:
        lines=(uint8_t)c->r[AX]; attr=(uint8_t)(c->r[BX]>>8);
        top=(uint8_t)(c->r[CX]>>8); left=(uint8_t)c->r[CX];
        bottom=(uint8_t)(c->r[DX]>>8); right=(uint8_t)c->r[DX];
        if(top>bottom || bottom>=25 || left>right || right>=80) return 0;
        for(i=0;i<=(uint16_t)(bottom-top);++i) {
            r=f==6 ? top+(uint8_t)i:bottom-(uint8_t)i;
            for(col=left;col<=right;++col) {
                p=((uint16_t)r*80+col)*2;
                if(!lines || (f==6 ? r+lines>bottom:r<top+lines)) count=((uint16_t)attr<<8)|32;
                else count=read16(0xb8000UL+(f==6 ? p+lines*160:p-lines*160));
                write16(0xb8000UL+p,count);
            }
        }
        return 0;
    case 8: c->r[AX]=read16(0xb8000UL+((uint16_t)row*80+column)*2); return 0;
    case 9: case 10:
        ch=(uint8_t)c->r[AX]; attr=f==9 ? (uint8_t)c->r[BX]:7;
        p=((uint16_t)row*80+column)*2; count=c->r[CX];
        for(i=0;i<count && p<4000;++i,p+=2) write16(0xb8000UL+p,((uint16_t)attr<<8)|ch);
        return 0;
    case 0x0e: return character((uint8_t)c->r[AX]);
    case 0x0f: c->r[AX]=0x5000|mode; c->r[BX]=(c->r[BX]&255)|((uint16_t)read8(0x462)<<8); return 0;
    case 0x1a: c->r[AX]&=0xff00; return 0; /* not VGA */
    default: return -1;
    }
}
int fd_pc_interrupt(FDCPU *c,uint8_t n) {
    uint16_t key,vector=read16((uint16_t)n*4),segment=read16((uint16_t)n*4+2);
    uint8_t f=(uint8_t)(c->r[AX]>>8); int result=0;
    fd_pc_last_interrupt=n; fd_pc_last_function=f;
    /* Honor hooks installed by DOS; only our own BIOS vectors are trapped. */
    if(c->s[CS]!=0xf000 && (segment!=0xf000 || vector!=0x100+(uint16_t)n*4)) return 1;
    if(n==0x10) result=video(c,f);
    else if(n==0x12 || n==0x13) result=fd_bios_interrupt(c,n);
    else if(n==0x11) c->r[AX]=read16(0x410);
    else if(n==0x16) {
        if(f==0 || f==0x10 || f==1 || f==0x11) {
            if(keyboard_get && keyboard_get(&key,f==0 || f==0x10)) { c->r[AX]=key; c->flags&=~FD_ZF; }
            else if(f==1 || f==0x11) c->flags|=FD_ZF;
            else { c->ip-=2; c->status=FD_X86_WAIT; }
        } else if(f==2) c->r[AX]&=0xff00;
        else if(f==0x12) c->r[AX]=0;
        else result=-1;
    } else if(n==0x1a) {
        if(f==0) { c->r[CX]=(uint16_t)(ticks>>16); c->r[DX]=(uint16_t)ticks; c->r[AX]&=0xff00; }
        else if(f==1) { ticks=((uint32_t)c->r[CX]<<16)|c->r[DX]; c->flags&=~FD_CF; }
        else c->flags|=FD_CF; /* Original XT has no battery backed RTC. */
    }
    else if(n==0x15) { c->flags|=FD_CF; c->r[AX]=0x8600|(c->r[AX]&255); }
    else if(n==0x14) c->r[AX]=0x8000;
    else if(n==0x17) c->r[AX]=0x9000;
    else if(n==0x29) result=character((uint8_t)c->r[AX]);
    else if(n==0x1c || n==0x28) result=0;
    else result=-1;
    if(!result && c->s[CS]==0xf000 && c->status!=FD_X86_WAIT) {
        /* A far call to the ROM thunk returns BIOS status through saved FLAGS. */
        write16(fd_physical(c->s[SS],c->r[SP]+4),c->flags|2);
    }
    return result;
}
void fd_pc_tick(FDCPU *c) {
    ++ticks; write16(0x46c,(uint16_t)ticks); write16(0x46e,(uint16_t)(ticks>>16));
    if(c->flags&FD_IF) fd_cpu_interrupt(c,8);
}
