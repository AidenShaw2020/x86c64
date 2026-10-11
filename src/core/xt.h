/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_XT_H
#define FD_XT_H
#include "pc.h"
void fd_xt_init(fd_console_put console,fd_keyboard keyboard,
    fd_sector_read read_sector,fd_sector_write write_sector);
int fd_xt_interrupt(FDCPU *cpu,uint8_t interrupt);
void fd_xt_tick(FDCPU *cpu);
extern uint8_t fd_xt_booted,fd_xt_post_code;
int fd_xt_option_rom(void);
#endif
