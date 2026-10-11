; SPDX-License-Identifier: GPL-2.0-or-later
.export _fd_cart_bios_blob
.segment "RODATA"
_fd_cart_bios_blob:
.incbin "build/cart-bios.bin"
