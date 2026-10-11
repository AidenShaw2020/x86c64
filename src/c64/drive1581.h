/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_DRIVE1581_H
#define FD_DRIVE1581_H
#include <stdint.h>
int fd_1581_open(void);
void fd_1581_close(void);
int fd_1581_read(uint32_t lba,uint8_t *data);
int fd_1581_write(uint32_t lba,const uint8_t *data);
extern uint8_t fd_1581_error;
extern uint32_t fd_1581_reads,fd_1581_writes;
#endif
