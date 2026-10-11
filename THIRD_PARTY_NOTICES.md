# Third-party notices and redistribution

x86c64 is an experimental project, not an official FreeDOS, GLaBIOS,
Commodore, Kung Fu Flash or VICE release. Names identify compatibility
targets and upstream projects; they do not imply endorsement.

## Code and assets in this repository

| Component | Author / upstream | License | Location |
| --- | --- | --- | --- |
| C64 x86 interpreter, virtual XT, REU pager, video, cartridge and IEC backend | x86c64 contributors | GPL-2.0-or-later | `src/`, `tests/`, `tools/`; [LICENSE](LICENSE) |
| GLaBIOS v0.4.2 | 640KB and contributors, copyright 2022–2025 in pinned source | GPL-3.0-or-later | `vendor/glabios/`, including upstream notices and license |
| GLaBIOS font bitmaps | VileR, [Oldschool PC Fonts](https://int10h.org/oldschool-pc-fonts/readme/#legal_stuff) | CC-BY-SA-4.0 | `GFX_CHARSET` in `vendor/glabios/src/GLABIOS.ASM`; [license](LICENSES/CC-BY-SA-4.0.txt) |
| Fake86 reference files | Mike Chambers and upstream contributors | GPL-2.0-or-later | `vendor/fake86/`; original notices retained; **not linked** into this emulator |
| fddepack | ecm-pushbx | Permission notice at the beginning of `depack.c` | `vendor/fddepack/`; host preparation tool, **not linked** into C64 code |

The video renderer reads the GLaBIOS 8×8 font. For the full 80-column view,
it combines pairs of horizontal pixels into a 4×8 glyph at runtime.
This adaptation is identified here; the font retains its CC-BY-SA-4.0
attribution and share-alike terms. Screenshots show this rendered font.

New project code is available under GPL-2.0-or-later. Distribution of the
cartridge containing the emulator and GPLv3 GLaBIOS uses GPL-3.0-or-later
for those program components, selecting the GPLv3 option for our code.
The font and permissive runtime retain their own notices. This does not
relicense independently distributed FreeDOS programs.

## Additional components in the ready-to-run release

| Component | Author / upstream | License / notices | Corresponding source provided |
| --- | --- | --- | --- |
| FreeDOS kernel 2043 and FAT12 boot code | Pasquale J. Villani, Svante Frey and FreeDOS contributors | GPL-2.0-or-later; [COPYING](LICENSES/FreeDOS-kernel-COPYING.txt) | Official `ke2043s.zip`, including `boot/`, build files and original notices |
| FreeCom 0.86, `COMMAND.COM` | FreeCom / FreeDOS developers | GPL v2, with upstream per-file notices; [license](LICENSES/FreeCom-license.txt) | Exact `SOURCE/FREECOM/SOURCES.ZIP` from the official FreeDOS 1.4 `freecom.zip` |
| SUPPL library used by FreeCom | Steffen Kaiser and contributors | GNU Library GPL v2 / upstream per-file notices | Full `suppl/` source in the above FreeCom source archive; [license](LICENSES/SUPPL-LGPL-2.0.txt) |
| cc65 runtime linked into C64 code | Ullrich von Bassewitz and cc65 contributors | zlib-style license and per-file notices; [license](LICENSES/cc65.txt) | Full cc65 source at compiler revision `b3bf30f` |

The release supplies a **corresponding-source ZIP alongside the binary ZIP**,
not just links to upstream repositories. It contains the exact x86c64 source
revision, original GLaBIOS source, generated BIOS configuration/source, the
above FreeDOS/FreeCom/cc65 archives, licenses, and the transformation scripts.
Keep these sources and notices available if you redistribute the binaries.

The kernel executable is unpacked with fddepack and UPX 4.2.4, then flattened
with the relocation factor `0x60`; its source logic is unchanged. The scripts
and hashes record this transformation. The FAT12 image is rebuilt with minimal
configuration and AUTOEXEC; the D81 wraps it in a hybrid 1581 layout.
The release's `COMMAND.COM` is byte-identical to `BIN/COMMAND.COM` from
the official FreeDOS 1.4 `freecom.zip` package.

External tools (VICE, JWasm, Zig, UPX and compiler executables) are installed
separately and are not shipped in the release. Commodore BASIC/KERNAL/drive
ROMs, VICE snapshots containing them, and Microsoft/IBM DOS binaries are
not distributed. The source-only Microsoft MS-DOS repository is a research
reference; MS-DOS is not implemented or included here.

See [release provenance](docs/release.md), [pinned source inputs](docs/release-inputs.json)
and the original per-file notices. Those notices take precedence over this
inventory if a component has additional terms.
