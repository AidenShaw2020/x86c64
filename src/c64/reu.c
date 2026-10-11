/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "reu.h"
#include <string.h>
#define REU ((volatile uint8_t *)0xdf00)
static uint8_t zeros[512];
static void transfer(uint32_t address, uint8_t *data, uint16_t count, uint8_t command) {
    uint16_t p = (uint16_t)data;
    REU[2] = (uint8_t)p; REU[3] = (uint8_t)(p >> 8);
    REU[4] = (uint8_t)address; REU[5] = (uint8_t)(address >> 8);
    REU[6] = (uint8_t)(address >> 16);
    REU[7] = (uint8_t)count; REU[8] = (uint8_t)(count >> 8);
    REU[9] = 0; REU[10] = 0; REU[1] = command;
}
void fd_reu_store(uint32_t address,uint8_t *data,uint16_t count) { transfer(address,data,count,0x90); }
void fd_reu_fetch(uint32_t address,uint8_t *data,uint16_t count) { transfer(address,data,count,0x91); }
int fd_reu_probe_2mb(void) {
    uint8_t saved0=0,saved16=0,a=0x35,b=0xca,x=0,y=0;
    transfer(0,&saved0,1,0x91); transfer(0x100000UL,&saved16,1,0x91);
    transfer(0,&a,1,0x90); transfer(0x100000UL,&b,1,0x90);
    transfer(0,&x,1,0x91); transfer(0x100000UL,&y,1,0x91);
    transfer(0,&saved0,1,0x90); transfer(0x100000UL,&saved16,1,0x90);
    return x==a && y==b;
}
int fd_reu_probe(void) {
    uint8_t saved0=0, saved8=0, a=0x35, b=0xca, x=0, y=0;
    transfer(0, &saved0, 1, 0x91); transfer(0x80000UL, &saved8, 1, 0x91);
    transfer(0, &a, 1, 0x90); transfer(0x80000UL, &b, 1, 0x90);
    transfer(0, &x, 1, 0x91); transfer(0x80000UL, &y, 1, 0x91);
    transfer(0, &saved0, 1, 0x90); transfer(0x80000UL, &saved8, 1, 0x90);
    return x == a && y == b;
}
void fd_reu_clear(void) {
    uint16_t page;
    memset(zeros, 0, sizeof zeros);
    for (page=0; page<2048; ++page) transfer((uint32_t)page << 9, zeros, 512, 0x90);
}
int fd_reu_read(uint16_t page, uint8_t *data) {
    if (page >= 2048) return -1;
    transfer((uint32_t)page << 9, data, 512, 0x91); return 0;
}
int fd_reu_write(uint16_t page, const uint8_t *data) {
    if (page >= 2048) return -1;
    transfer((uint32_t)page << 9, (uint8_t *)data, 512, 0x90); return 0;
}
