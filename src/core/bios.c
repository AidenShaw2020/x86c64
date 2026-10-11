/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "bios.h"
#include "pager.h"
static fd_console_put console_put;
static fd_sector_read disk_read;
static fd_sector_write disk_write;
static uint8_t sector_buffer[512], disk_status;
static uint16_t disk_cylinders=1024;
static uint8_t disk_heads=4, disk_sectors=16, disk_drive=0x80;
void fd_bios_geometry(uint16_t cylinders, uint8_t heads, uint8_t sectors, uint8_t drive) {
    disk_cylinders=cylinders; disk_heads=heads; disk_sectors=sectors; disk_drive=drive;
}

void fd_bios_init(fd_console_put console, fd_sector_read read_sector,
                  fd_sector_write write_sector) {
    console_put=console; disk_read=read_sector; disk_write=write_sector;
    disk_status=0;
    fd_bios_geometry(1024,4,16,0x80);
}
static void disk_result(FDCPU *c, uint8_t status, uint8_t completed) {
    disk_status=status;
    c->r[AX]=((uint16_t)status<<8)|completed;
    if(status) c->flags|=FD_CF; else c->flags&=~FD_CF;
}
static int transfer(FDCPU *c, uint8_t write) {
    uint32_t lba, address, bytes;
    uint16_t cylinder, i;
    uint8_t count, head, sector, completed=0;
    count=(uint8_t)c->r[AX];
    cylinder=(c->r[CX]>>8)|((c->r[CX]&0xc0)<<2);
    head=(uint8_t)(c->r[DX]>>8); sector=(uint8_t)(c->r[CX]&0x3f);
    lba=((uint32_t)cylinder*disk_heads+head)*disk_sectors+(sector ? sector-1:0);
    if(!count || count>128 || cylinder>=disk_cylinders || head>=disk_heads ||
        !sector || sector>disk_sectors || lba+count>(uint32_t)disk_cylinders*disk_heads*disk_sectors) {
        disk_result(c,4,0); return 0;
    }
    address=fd_physical(c->s[ES],c->r[BX]); bytes=(uint32_t)count*512;
    /* PC-XT DMA cannot cross a physical 64 KiB boundary. */
    if((address&0xffffUL)+bytes>65536UL) {
        disk_result(c,9,0); return 0;
    }
    if(write && !disk_write) { disk_result(c,3,0); return 0; }
    if(!write && !disk_read) { disk_result(c,0x20,0); return 0; }
    while(completed<count) {
        if(write) {
            for(i=0;i<512;++i) if(fd_mem_read(address+i,&sector_buffer[i])) {
                c->status=FD_X86_MEMORY; return 0;
            }
            if(disk_write(lba,sector_buffer)) { disk_result(c,0x20,completed); return 0; }
        } else {
            if(disk_read(lba,sector_buffer)) { disk_result(c,0x20,completed); return 0; }
            for(i=0;i<512;++i) if(fd_mem_write(address+i,sector_buffer[i])) {
                c->status=FD_X86_MEMORY; return 0;
            }
        }
        ++completed; ++lba; address+=512;
    }
    disk_result(c,0,completed); return 0;
}
int fd_bios_interrupt(FDCPU *c, uint8_t interrupt) {
    uint8_t function=(uint8_t)(c->r[AX]>>8);
    if(interrupt==0x10 && function==0x0e)
        return console_put ? console_put((uint8_t)c->r[AX]) : -1;
    if(interrupt==0x12) { c->r[AX]=640; return 0; }
    if(interrupt!=0x13) return -1;
    if((uint8_t)c->r[DX]!=disk_drive) { disk_result(c,1,0); return 0; }
    switch(function) {
        case 0: disk_result(c,0,0); return 0;
        case 1: disk_result(c,disk_status,(uint8_t)c->r[AX]); return 0;
        case 2: return transfer(c,0);
        case 3: return transfer(c,1);
        case 8:
            c->r[CX]=((disk_cylinders-1)<<8)|(((disk_cylinders-1)>>2)&0xc0)|disk_sectors;
            c->r[DX]=((uint16_t)(disk_heads-1)<<8)|1;
            disk_result(c,0,0);
            if(!disk_drive) { c->r[BX]=(c->r[BX]&0xff00)|3; c->s[ES]=0xf000; c->r[DI]=0xff00; }
            return 0;
        case 0x15:
            disk_result(c,0,0); c->r[AX]=disk_drive ? 0x0300:0x0100;
            if(disk_drive) {
                uint32_t sectors=(uint32_t)disk_cylinders*disk_heads*disk_sectors;
                c->r[CX]=(uint16_t)(sectors>>16); c->r[DX]=(uint16_t)sectors;
            }
            return 0;
        case 0x16: if(!disk_drive) { disk_result(c,0,0); return 0; } break;
        default: disk_result(c,1,0); return 0;
    }
    disk_result(c,1,0); return 0;
}
int fd_bios_boot(FDCPU *c) {
    unsigned i;
    fd_cpu_init(c);
    if(!disk_read || disk_read(0,sector_buffer)) return FD_BOOT_IO;
    if(sector_buffer[510]!=0x55 || sector_buffer[511]!=0xaa) return FD_BOOT_SIGNATURE;
    for(i=0;i<512;++i) if(fd_mem_write(0x7c00UL+i,sector_buffer[i])) {
        c->status=FD_X86_MEMORY; return FD_BOOT_MEMORY;
    }
    c->r[DX]=disk_drive;
    return FD_BOOT_OK;
}
