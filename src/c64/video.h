/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_VIDEO_H
#define FD_VIDEO_H
#include <stdint.h>
void fd_video_init(void);
void fd_video_refresh(void);
void fd_video_text(void);
void fd_video_mode(uint8_t mode);
extern uint8_t fd_video_ready,fd_video_keyboard_ok,fd_video_files;
#endif
