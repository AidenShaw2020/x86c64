/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_DISK_H
#define FD_DISK_H
#include <stdint.h>
#define FD_DISK_SECTORS 65536UL
typedef int (*fd_sector_read)(uint32_t lba, uint8_t *data);
typedef int (*fd_sector_write)(uint32_t lba, const uint8_t *data);
int fd_disk_read(fd_sector_read read_sector, uint32_t lba, uint16_t count, uint8_t *data);
int fd_disk_write(fd_sector_write write_sector, uint32_t lba, uint16_t count, const uint8_t *data);
int fd_chs_lba(uint16_t cylinder, uint8_t head, uint8_t sector, uint32_t *lba);
int fd_disk_range(uint32_t lba, uint16_t count);
#endif
