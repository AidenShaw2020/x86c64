/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "video.h"
#include "../core/pager.h"
#include <stdint.h>
#include <string.h>
#define VIC ((volatile uint8_t *)0xd000)
static uint8_t old_bank,old_mode,old_pointer,old_width;
static uint8_t view;
uint8_t fd_video_ready,fd_video_keyboard_ok,fd_video_files;
static const uint8_t ready_text[]={0x46,0x44,0x36,0x34,32,0x46,0x52,0x45,0x45,0x44,0x4f,0x53,32,0x52,0x45,0x41,0x44,0x59};
static const uint8_t key_text[]={0x46,0x44,0x36,0x34,32,0x4b,0x45,0x59,0x42,0x4f,0x41,0x52,0x44,32,0x4f,0x4b,32};
static const uint8_t file_text[]={0x4b,0x45,0x52,0x4e,0x45,0x4c,0x2e,0x53,0x59,0x53};
static void observe_row(uint8_t row) {
    uint8_t text[18],i;
    for(i=0;i<sizeof text;++i) text[i]=(uint8_t)fd_mem_get(0xb8000UL+(unsigned)row*160+i*2);
    if(!memcmp(text,ready_text,sizeof ready_text)) fd_video_ready=1;
    if(!memcmp(text,key_text,sizeof key_text)) fd_video_keyboard_ok=1;
    if(!memcmp(text,file_text,sizeof file_text)) fd_video_files=1;
}
static const uint8_t palette[]={0,6,5,3,2,4,9,15,11,14,13,3,10,4,7,1};
void fd_video_mode(uint8_t mode) { view=mode; fd_video_dirty=0x1ffffffUL; }
void fd_video_init(void) {
    unsigned i; uint8_t b;
    /* Actual GLaBIOS CP437 font, reduced from 8 to 4 pixels per character. */
    for(i=0;i<1024;++i) {
        b=(uint8_t)fd_mem_get(0xffa6eUL+i);
        ((uint8_t *)0xb000)[i]=b;
        ((uint8_t *)0xb400)[i]=((b&0xc0)?8:0)|((b&0x30)?4:0)|((b&12)?2:0)|((b&3)?1:0);
    }
    memset((void *)0xe000,0,8000); memset((void *)0xc000,0x16,1000);
    old_bank=*(volatile uint8_t *)0xdd00; old_mode=VIC[0x11];
    old_pointer=VIC[0x18]; old_width=VIC[0x16];
    *(volatile uint8_t *)0xdd00=old_bank&0xfc;
    /* Align all 320x200 bitmap pixels with the PAL active display. */
    VIC[0x18]=8; VIC[0x11]=(old_mode&0xf8)|0x24; VIC[0x16]=old_width&0xe8;
    fd_video_dirty=0x1ffffffUL;
    view=2; VIC[0x20]=0; /* Show the complete PC display by default. */
    fd_video_ready=fd_video_keyboard_ok=fd_video_files=0;
}
void fd_video_refresh(void) {
    uint8_t row,col,a,b,line,attr,attr_b; uint8_t *bitmap,*fa,*fb,*colors;
    uint32_t address;
    uint32_t dirty=fd_video_dirty;
    fd_video_dirty=0;
    for(row=0;row<25;++row) if(dirty&(1UL<<row)) {
        observe_row(row);
        bitmap=(uint8_t *)(0xe000+(unsigned)row*320);
        colors=(uint8_t *)(0xc000+(unsigned)row*40);
        for(col=0;col<40;++col) {
            address=0xb8000UL+(unsigned)row*160+(unsigned)col*(view==2?4:2)+(view==1?80:0);
            a=(uint8_t)fd_mem_get(address); attr=(uint8_t)fd_mem_get(address+1);
            if(view==2) {
                b=(uint8_t)fd_mem_get(address+2); attr_b=(uint8_t)fd_mem_get(address+3);
                if(a==32 && b!=32) attr=attr_b;
                fa=(uint8_t *)(0xb400+(unsigned)(a&127)*8);
                fb=(uint8_t *)(0xb400+(unsigned)(b&127)*8);
                for(line=0;line<8;++line) *bitmap++=(*fa++<<4)|*fb++;
            } else {
                fa=(uint8_t *)(0xb000+(unsigned)(a&127)*8);
                for(line=0;line<8;++line) *bitmap++=*fa++;
            }
            *colors++=(palette[attr&15]<<4)|palette[(attr>>4)&7];
        }
    }
}
void fd_video_text(void) {
    *(volatile uint8_t *)0xdd00=old_bank; VIC[0x11]=old_mode;
    VIC[0x18]=old_pointer; VIC[0x16]=old_width;
}
