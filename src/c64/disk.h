/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_C64_DISK_H
#define FD_C64_DISK_H
#include <stdint.h>
int fd_c64_sector_read(uint32_t lba, uint8_t *data);
void fd_c64_disk_close(void);
#endif
