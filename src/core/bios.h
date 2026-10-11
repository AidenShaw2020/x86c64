/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_BIOS_H
#define FD_BIOS_H
#include "x86.h"
#include "disk.h"
typedef int (*fd_console_put)(uint8_t ch);
enum { FD_BOOT_OK, FD_BOOT_IO, FD_BOOT_SIGNATURE, FD_BOOT_MEMORY };
void fd_bios_init(fd_console_put console, fd_sector_read read_sector,
                  fd_sector_write write_sector);
void fd_bios_geometry(uint16_t cylinders, uint8_t heads, uint8_t sectors, uint8_t drive);
int fd_bios_interrupt(FDCPU *cpu, uint8_t interrupt);
int fd_bios_boot(FDCPU *cpu);
#endif
