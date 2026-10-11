/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <cbm.h>
#include "disk.h"
#include "../core/disk.h"
static uint8_t opened;
static uint32_t next_sector;
void fd_c64_disk_close(void) {
    if(opened) cbm_close(2);
    opened=0; next_sector=0;
}
int fd_c64_sector_read(uint32_t lba, uint8_t *data) {
    if(fd_disk_range(lba,1)) return -1;
    if(opened && lba<next_sector) fd_c64_disk_close();
    if(!opened) {
        /* Sequential raw file on IEC drive 8; VICE FSDevice provides it. */
        if(cbm_open(2,8,2,"xtboot.img,s,r")) { cbm_close(2); return -1; }
        opened=1; next_sector=0;
    }
    do {
        if(cbm_read(2,data,512)!=512) { fd_c64_disk_close(); return -1; }
    } while(next_sector++<lba);
    return 0;
}
