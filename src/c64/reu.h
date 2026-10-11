/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_REU_H
#define FD_REU_H
#include <stdint.h>
int fd_reu_probe_2mb(void);
void fd_reu_store(uint32_t address, uint8_t *data, uint16_t count);
void fd_reu_fetch(uint32_t address, uint8_t *data, uint16_t count);
int fd_reu_probe(void);
void fd_reu_clear(void);
int fd_reu_read(uint16_t page, uint8_t *data);
int fd_reu_write(uint16_t page, const uint8_t *data);
#endif
