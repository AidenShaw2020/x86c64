# Prameny (ověřeno 2026-10-10 až 2026-10-11)

POC release doplňuje [přesné zdrojové balíky a jejich hashe](release-inputs.json),
[licence a autory](../THIRD_PARTY_NOTICES.md) a [postup reprodukce](release.md).
Font GLaBIOSu pochází od VileR / [Oldschool PC Fonts](https://int10h.org/oldschool-pc-fonts/readme/#legal_stuff),
CC-BY-SA-4.0; převod z 8×8 na 4×8 provádí renderer za běhu.

- KFF: https://codeberg.org/KimJorgensen/KungFuFlash
  GitHub mirror https://github.com/KimJorgensen/KungFuFlash
  analyzovaná revize 7121a018c8608feafc2180d910ce58acddbec5e3.
  firmware/memory.h, cartridges/easyflash.c, cartridges/kff.c,
  stm32f4xx/STM32F405RGTx_FLASH.ld.
- MCU: https://www.st.com/en/microcontrollers-microprocessors/stm32f405rg.html
- EasyFlash: https://skoe.de/easyflash/develdocs/
- FreeDOS: https://github.com/FDOS/kernel — 8086+/80386+ IBM PC kernel.
- GLaBIOS: https://github.com/640-KB/GLaBIOS/releases/tag/v0.4.2
  revize b98b0fe396466fa41466a9521f40987c989ae077, GPL-3.0.
  XT/8088 ROM GLABIOS_0.4.2_8X.ROM, SHA256 ověřen proti upstream release:
  c2bc9ec1893c0ea4098433c862b97e546c4877c6a21c5bc23792ea2b6639c7d4.
  Nezměněný zdroj a licence jsou ve vendor/glabios. Běží ARCH_EMU konfigurace;
  přesné volby jsou v tools/build_glabios.py.
- FreeDOS 1.4 Floppy Edition: https://download.freedos.org/1.4/FD14-FloppyEdition.zip
  https://www.freedos.org/download/verify.txt
  SHA256 45b1fa7c52dd996c3bfa5e352ffcd410781b952a6ad629f15a4c9ec4bbaefc5a.
  Původní 720k/x86BOOT.img, kernel 2043 a FreeCom 0.86; manifest v build/freedos.
  Zdroj kernelu: https://github.com/FDOS/kernel
  Zdroj FreeCom: https://github.com/FDOS/freecom
- JWasm 2.20: https://github.com/Baron-von-Riedesel/JWasm/releases/tag/v2.20
- fddepack: https://hg.pushbx.org/ecm/fddepack/rev/85a255f5048a
  Nezměněný depack.c a původní licenční text jsou ve vendor/fddepack.
  Používá se jen na PC při přípravě média, ne jako hostitelská x86 emulace.
- UPX 4.2.4: https://github.com/upx/upx/releases/tag/v4.2.4
  Formát původního FreeDOS kernelu a flattening:
  https://github.com/FDOS/kernel/issues/75
  https://github.com/FDOS/kernel/blob/master/utils/exeflat.c
- MS-DOS: https://github.com/microsoft/MS-DOS
  https://github.com/microsoft/MS-DOS/tree/main/v2.0/bin
  Balík 2.0 obsahuje MSDOS.SYS/COMMAND.COM a SYSINIT/SYSIMES, avšak
  potřebuje odpovídající OEM BIOS/IO.SYS a bootovací kód; není to hotová
  IBM PC systémová disketa. Dosud žádný MS-DOS boot test v projektu.
- SeaBIOS: https://github.com/coreboot/seabios/blob/master/Makefile
  (i386 a 32bitové části, není ROM pro 8086/PC-XT).
  https://github.com/coreboot/seabios/blob/master/src/disk.c
  pro ověření formátu BIOS CHS a stavů. Kód SeaBIOS není začleněn.
- VICE: https://vice-emu.sourceforge.io/vice_6.html
  https://vice-emu.sourceforge.io/vice_7.html
  https://github.com/VICE-Team/svn-mirror/releases/tag/3.10.0
  GTK3VICE-3.10-win64.zip SHA256
  beb3791d90e98f7e012c0028f26fbaa6545c8fce676f852a67f8ad893dee9d08.
- cc65: https://cc65.github.io/
  cc65-snapshot-win64.zip lokální SHA256
  00b7c3a353be58f97ca277a2fb1911a1436a7f8c6280d16b4f9e33c92cee3b80
  (rolling snapshot, archiv je potřeba uložit pro přesnou reprodukci).
- Zig 0.14.1: https://ziglang.org/download/index.json
  SHA256 554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c.
  Používá se pouze pro nativní překlad C a testy, nikoli ARM emulaci.
- Kandidátní referenční Fake86:
  https://github.com/doug65536/fake86
  revize 0bbdaaa457b477157f3c2da95f96fea90234fa93, GPL-2.0-or-later.
  Originální copyrighty a licence zachovány ve vendor/fake86.
