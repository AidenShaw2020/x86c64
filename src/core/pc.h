/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_PC_H
#define FD_PC_H
#include "bios.h"
typedef int (*fd_keyboard)(uint16_t *key, uint8_t consume);
int fd_pc_init(fd_console_put console, fd_keyboard keyboard,
    fd_sector_read read_sector, fd_sector_write write_sector);
int fd_pc_interrupt(FDCPU *cpu, uint8_t interrupt);
void fd_pc_tick(FDCPU *cpu);
extern uint8_t fd_pc_last_interrupt, fd_pc_last_function;
#endif
