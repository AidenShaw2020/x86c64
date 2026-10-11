/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xt.h"
#include "pager.h"
#include <string.h>
static uint8_t pic_mask,pic_base,pic_init,pic_isr,ppi_b,keyboard_data,keyboard_pending;
static uint8_t crtc_index,crtc[32],status_phase;
static uint8_t dma_flip,dma_mask;
static uint16_t dma[8];
static uint16_t pit_count[3],pit_reload[3],pit_latch[3];
static uint8_t pit_rw[3],pit_read_phase[3],pit_write_phase[3],pit_latched[3];
static fd_keyboard get_key;
uint8_t fd_xt_booted,fd_xt_post_code;
static int port_read(uint16_t p,uint8_t *v) {
    uint8_t ch,rw; uint16_t count;
    *v=255; /* An absent ISA device leaves the bus pulled high. */
    if(p<8) { *v=(uint8_t)(dma[p]>>(dma_flip?8:0)); dma_flip^=1; }
    else if(p==8) *v=0;
    else if(p==0x20) *v=pic_isr;
    else if(p==0x21) *v=pic_mask;
    else if(p>=0x40 && p<=0x42) {
        ch=(uint8_t)(p-0x40); rw=pit_rw[ch];
        count=pit_latched[ch]?pit_latch[ch]:pit_count[ch];
        *v=(uint8_t)(count>>((rw==2 || (rw==3 && pit_read_phase[ch]))?8:0));
        if(rw==3) { pit_read_phase[ch]^=1; if(!pit_read_phase[ch]) pit_latched[ch]=0; }
        else pit_latched[ch]=0;
        if(!pit_latched[ch]) pit_count[ch]-=257;
    } else if(p==0x60) *v=(ppi_b&0x80)?0x2c:keyboard_data; /* CGA80, 64K motherboard, no FPU */
    else if(p==0x61) *v=ppi_b;
    else if(p==0x62) *v=(uint8_t)(((ppi_b&8)?2:12)|((++status_phase&1)?0x20:0));
    else if(p==0x3d4) *v=crtc_index;
    else if(p==0x3d5) *v=crtc[crtc_index&31];
    else if(p==0x3da) *v=(++status_phase&1)?9:0;
    return 0;
}
static int port_write(uint16_t p,uint8_t v) {
    uint8_t ch,rw;
    if(p<8) { if(dma_flip) dma[p]=(dma[p]&255)|((uint16_t)v<<8);
        else dma[p]=(dma[p]&0xff00)|v; dma_flip^=1; }
    else if(p==0x0c) dma_flip=0;
    else if(p==0x0d) { dma_flip=0; dma_mask=15; }
    else if(p==0x0a) { if(v&4) dma_mask|=1<<(v&3); else dma_mask&=~(1<<(v&3)); }
    else if(p==0x20) {
        if(v&0x10) { pic_init=1; pic_isr=0; }
        else if(v&0x20) pic_isr=0;
    } else if(p==0x21) {
        if(pic_init==1) { pic_base=v&0xf8; pic_init=2; }
        else if(pic_init==2) pic_init=0; else pic_mask=v;
    } else if(p==0x43) {
        ch=v>>6; if(ch<3) { rw=(v>>4)&3;
            if(!rw) { pit_latch[ch]=pit_count[ch]; pit_latched[ch]=1; pit_read_phase[ch]=0; }
            else { pit_rw[ch]=rw; pit_read_phase[ch]=pit_write_phase[ch]=0; pit_latched[ch]=0; }
        }
    } else if(p>=0x40 && p<=0x42) {
        ch=(uint8_t)(p-0x40); rw=pit_rw[ch];
        if(rw==2 || (rw==3 && pit_write_phase[ch])) pit_reload[ch]=(pit_reload[ch]&255)|((uint16_t)v<<8);
        else pit_reload[ch]=(pit_reload[ch]&0xff00)|v;
        if(rw==3) pit_write_phase[ch]^=1;
        pit_count[ch]=pit_reload[ch];
    } else if(p==0x61) {
        if((ppi_b&0x40) && !(v&0x40)) { keyboard_data=0xaa; keyboard_pending=1; }
        if(v&0x80) keyboard_pending=0;
        ppi_b=v;
    } else if(p==0x3d4) crtc_index=v;
    else if(p==0x3d5) crtc[crtc_index&31]=v;
    else if(p==0x80) fd_xt_post_code=v;
    else if(p==0xe9 && v==0x19) fd_xt_booted=1;
    return 0;
}
void fd_xt_init(fd_console_put console,fd_keyboard keyboard,fd_sector_read r,fd_sector_write w) {
    pic_mask=255; pic_base=8; pic_init=pic_isr=ppi_b=keyboard_pending=0;
    keyboard_data=0xaa; crtc_index=status_phase=dma_flip=0; dma_mask=15;
    memset(dma,0,sizeof dma); memset(crtc,0,sizeof crtc);
    memset(pit_count,0,sizeof pit_count); memset(pit_reload,0,sizeof pit_reload);
    memset(pit_latched,0,sizeof pit_latched); memset(pit_read_phase,0,sizeof pit_read_phase);
    memset(pit_write_phase,0,sizeof pit_write_phase); memset(pit_rw,3,sizeof pit_rw);
    get_key=keyboard; fd_xt_booted=fd_xt_post_code=0;
    fd_bios_init(console,r,w); fd_bios_geometry(80,2,9,0);
    fd_cpu_io(port_read,port_write);
}
int fd_xt_interrupt(FDCPU *c,uint8_t n) {
    int result; uint32_t flags; uint8_t low,high,function=(uint8_t)(c->r[AX]>>8);
    /* The option ROM is a virtual disk controller. All other interrupts execute GLaBIOS/DOS code. */
    if(n!=0xf0 || c->s[CS]!=0xd000) return 1;
    result=fd_bios_interrupt(c,0x13);
    if(function==8 && !(c->flags&FD_CF)) { c->s[ES]=0xd000; c->r[DI]=0x80; }
    flags=fd_physical(c->s[SS],c->r[SP]+4);
    if(fd_mem_read(flags,&low) || fd_mem_read(flags+1,&high)) return -1;
    low=(low&~1)|(c->flags&1);
    if(fd_mem_write(flags,low) || fd_mem_write(flags+1,high)) return -1;
    return result;
}
void fd_xt_tick(FDCPU *c) {
    uint16_t key;
    pit_count[0]-=1024; pit_count[1]-=1024; pit_count[2]-=1024;
    if(!(c->flags&FD_IF) || pic_isr) return;
    if(!(pic_mask&2) && get_key && get_key(&key,1)) {
        keyboard_data=(uint8_t)(key>>8); keyboard_pending=1;
    }
    if(keyboard_pending && !(pic_mask&2)) { pic_isr=2; keyboard_pending=0; fd_cpu_interrupt(c,pic_base+1); }
    else if(!(pic_mask&1)) { pic_isr=1; fd_cpu_interrupt(c,pic_base); }
}
int fd_xt_option_rom(void) {
    /* Genuine executable ISA option ROM: install INT13/19, then return to POST. */
    static const uint8_t entry[]={0x31,0xc0,0x8e,0xc0,
        0x26,0xc7,0x06,0x4c,0,0x40,0,0x26,0xc7,0x06,0x4e,0,0,0xd0,
        0x26,0xc7,0x06,0x64,0,0x50,0,0x26,0xc7,0x06,0x66,0,0,0xd0,
        0x26,0xc7,0x06,0x78,0,0x80,0,0x26,0xc7,0x06,0x7a,0,0,0xd0,0xcb};
    static const uint8_t boot[]={0xb0,0x19,0xe6,0xe9,0x31,0xc0,0x8e,0xc0,0xbb,0,0x7c,
        0xb8,1,2,0xb9,1,0,0x31,0xd2,0xcd,0x13,0x72,5,0xea,0,0x7c,0,0,0xfa,0xf4,0xeb,0xfd};
    static const uint8_t dpt[]={0xdf,2,0x25,2,9,0x1b,0xff,0x54,0xf6,0x0f,8};
    uint16_t i; uint8_t sum=0,value;
    for(i=0;i<2048;++i) {
        value=0;
        if(i==0) value=0x55; else if(i==1) value=0xaa; else if(i==2) value=4;
        else if(i>=3 && i<3+sizeof entry) value=entry[i-3];
        else if(i==0x40) value=0xcd; else if(i==0x41) value=0xf0; else if(i==0x42) value=0xcf;
        else if(i>=0x50 && i<0x50+sizeof boot) value=boot[i-0x50];
        else if(i>=0x80 && i<0x80+sizeof dpt) value=dpt[i-0x80];
        if(i==2047) value=(uint8_t)-sum;
        sum+=value; if(fd_mem_write(0xd0000UL+i,value)) return -1;
    }
    return 0;
}
