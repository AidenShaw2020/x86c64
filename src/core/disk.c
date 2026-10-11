/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "disk.h"
int fd_chs_lba(uint16_t cylinder, uint8_t head, uint8_t sector, uint32_t *lba) {
    if (cylinder >= 1024 || head >= 4 || !sector || sector > 16) return -1;
    *lba = ((uint32_t)cylinder * 4 + head) * 16 + sector - 1; return 0;
}
int fd_disk_range(uint32_t lba, uint16_t count) {
    return !count || lba >= FD_DISK_SECTORS || count > FD_DISK_SECTORS-lba ? -1 : 0;
}

int fd_disk_read(fd_sector_read read_sector, uint32_t lba, uint16_t count, uint8_t *data) {
    if(fd_disk_range(lba,count)) return -1;
    while(count--) { if(read_sector(lba++,data)) return -1; data+=512; } return 0;
}
int fd_disk_write(fd_sector_write write_sector, uint32_t lba, uint16_t count, const uint8_t *data) {
    if(fd_disk_range(lba,count)) return -1;
    while(count--) { if(write_sector(lba++,data)) return -1; data+=512; } return 0;
}
