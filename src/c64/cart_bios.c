/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdint.h>
#include <string.h>
extern const uint8_t fd_cart_bios_blob[192];
void fd_cart_load_bios(void) {
    memcpy((void *)0x033c,fd_cart_bios_blob,192);
    ((void (*)(void))0x033c)();
}
