/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_PAGER_H
#define FD_PAGER_H
#include <stdint.h>
#define FD_PAGE_SIZE 512
#define FD_CACHE_PAGES 8
#define FD_RAM_PAGES 2048
typedef int (*fd_page_read)(uint16_t page, uint8_t *data);
typedef int (*fd_page_write)(uint16_t page, const uint8_t *data);
void fd_pager_init(fd_page_read read_page, fd_page_write write_page);
void fd_mem_pc_map(uint8_t enable);
extern uint32_t fd_video_dirty;
extern uint16_t fd_pager_generation;
/* Read-only span; a cache replacement invalidates the returned pointer. */
const uint8_t *fd_mem_span(uint32_t address,uint16_t *bytes);
int fd_mem_read(uint32_t address, uint8_t *value);
int fd_mem_get(uint32_t address);
int fd_mem_write(uint32_t address, uint8_t value);
/* Little-endian words; page edges and the 20-bit bus wrap are preserved. */
int fd_mem_read_word(uint32_t address, uint16_t *value);
int fd_mem_write_word(uint32_t address, uint16_t value);
int fd_mem_copy(uint32_t destination, uint32_t source, uint16_t bytes);
int fd_mem_fill(uint32_t destination, uint16_t value, uint16_t bytes, uint8_t word);
int fd_pager_flush(void);
#endif
