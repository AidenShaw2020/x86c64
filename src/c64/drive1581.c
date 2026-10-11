/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <cbm.h>
#include <stdio.h>
#include <string.h>
#include "drive1581.h"
static uint8_t opened;
static char command_text[32],error_text[48];
uint8_t fd_1581_error;
uint32_t fd_1581_reads,fd_1581_writes;
static int status(void) {
    int count=cbm_read(15,error_text,sizeof error_text);
    if(count<2 || error_text[0]<'0' || error_text[0]>'9' ||
        error_text[1]<'0' || error_text[1]>'9') { fd_1581_error=255; return -1; }
    fd_1581_error=(error_text[0]-'0')*10+error_text[1]-'0';
    return fd_1581_error ? -1 : 0;
}
static int command(const char *text) {
    unsigned length=strlen(text);
    if(cbm_write(15,text,length)!=length) { fd_1581_error=255; return -1; }
    return status();
}
void fd_1581_close(void) {
    if(opened) { cbm_close(3); cbm_close(15); }
    opened=0;
}
int fd_1581_open(void) {
    fd_1581_close(); fd_1581_reads=fd_1581_writes=0;
    if(cbm_open(15,8,15,"")) { cbm_close(15); fd_1581_error=255; return -1; }
    /* Drain the drive's power-on message before initializing the medium. */
    status();
    /* cc65 translates lower-case source letters to unshifted PETSCII. */
    if(command("i0\r") || cbm_open(3,8,3,"#")) {
        cbm_close(3); cbm_close(15); return -1;
    }
    opened=1; return status();
}
static int block(uint16_t lba,uint8_t half,uint8_t *data,uint8_t writing) {
    uint8_t track=(uint8_t)(lba/20)+1;
    uint8_t sector=(uint8_t)(lba%20)*2+half;
    /* Track 40 belongs to the CBM directory/BAM, never to the PC volume. */
    if(track>=40) ++track;
    if(writing) {
        if(command("b-p:3 0\r") || cbm_write(3,data,256)!=256) return -1;
    }
    sprintf(command_text,"u%u:3 0 %u %u\r",writing?2:1,track,sector);
    if(command(command_text)) return -1;
    if(!writing && cbm_read(3,data,256)!=256) { fd_1581_error=255; return -1; }
    return 0;
}
int fd_1581_read(uint32_t lba,uint8_t *data) {
    if(!opened || lba>=1440) return -1;
    if(block((uint16_t)lba,0,data,0) || block((uint16_t)lba,1,data+256,0)) return -1;
    ++fd_1581_reads; return 0;
}
int fd_1581_write(uint32_t lba,const uint8_t *data) {
    if(!opened || lba>=1440) return -1;
    if(block((uint16_t)lba,0,(uint8_t *)data,1) ||
        block((uint16_t)lba,1,(uint8_t *)data+256,1)) return -1;
    ++fd_1581_writes; return 0;
}
