# POC release and source provenance

The pre-release `v0.1.0-poc` targets **VICE 3.10, PAL C64, 1MiB REU,
Magic Desk cartridge and true-drive emulation of a 1581 on device 8**.
This is a proof of concept. No physical C64, cartridge/expander, REU or
1581 has been validated by these tests.

## Downloads

- `x86c64-v0.1.0-poc-binaries.zip`: `FD64.CRT`, `FD64.D81`, instructions,
  component licenses, manifests and test evidence.
- `x86c64-v0.1.0-poc-corresponding-source.zip`: project source at the release
  commit, GLaBIOS source/configuration, pinned upstream source archives for
  FreeDOS, FreeCom and cc65, and all preparation scripts.
- `SHA256SUMS.txt`: SHA256 digests of both release archives.

The binary package deliberately excludes VICE, Commodore ROMs and VSF
snapshots. Install your own emulator and legally available ROMs separately.

## Upstream inputs

The upstream FreeDOS 1.4 Floppy Edition SHA256 is
`45b1fa7c52dd996c3bfa5e352ffcd410781b952a6ad629f15a4c9ec4bbaefc5a`.
Only its 720KiB boot sector, kernel and FreeCom are used, with our minimal
configuration. The full distribution and its unrelated packages are not
redistributed in this POC.

The original packed kernel hash
`f34a7483c575fcf2709d9a7d0bc3db81c6211c279530f9e1bf78576b9233924d`
matches `bin/KERNEL.SYS` in the official
[kernel 2043 binary package](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/kernel/2043/ke2043_86f32.zip).
Its matching [source package](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/kernel/2043/ke2043s.zip)
is supplied with this release, including boot-sector source and build instructions.
`tools/unpack_kernel.py` records the unpacking/flattening procedure; the
resulting kernel hash is
`de002e10126a872430fe0745d8e1277ccf49b144eef1a6e497747b7fcda7998d`.

The FreeCom hash
`077808379e896476f7f69d62e6c8989d8fc23e8ef58d1c8492db1ac106784107`
matches `BIN/COMMAND.COM` in the official
[FreeDOS 1.4 freecom.zip](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.4/base/freecom.zip).
We supply its own `SOURCE/FREECOM/SOURCES.ZIP` with SUPPL sources and build
files. This is more precise than using the current development branch.

The compiler reports `cl65 V2.19 - Git b3bf30f`. Its
[full source archive](https://github.com/cc65/cc65/tree/b3bf30f), including
runtime sources and original notices, accompanies the release.

GLaBIOS source is pinned to upstream v0.4.2 revision
`b98b0fe396466fa41466a9521f40987c989ae077`. `tools/build_glabios.py` adds
configuration overrides to a **generated copy**, leaving vendor source
unchanged. The cartridge contains this configured ARCH_EMU/CPU_8088 ROM,
SHA256 `2eb0b19b5092f89da40c3a733057dee77730a96cb36463485f921f945331f719`.
The upstream reference ROM in `vendor/` is a separate, unmodified test input.

All archive URLs and byte digests are pinned in [release-inputs.json](release-inputs.json).
License texts and attribution are in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## Rebuilding on Windows

Install Python 3.10+, cc65 at the above revision, VICE 3.10, JWasm 2.20,
Zig 0.14.1 and UPX 4.2.4 in the locations described in
[development notes](development-cs.md). The current build scripts are tested
on Windows. Linux/macOS users can run the prebuilt CRT/D81 in VICE;
their source-build workflow has not yet been validated.

The tool lookup accepts these layouts (or PATH where supported):

| Tool | Download / source | Expected local executable |
| --- | --- | --- |
| cc65 | [pinned source](https://github.com/cc65/cc65/tree/b3bf30f); Windows binaries from [cc65](https://cc65.github.io/) | `tools/cc65/bin/cl65.exe`, `ca65.exe`, `ld65.exe` |
| VICE 3.10 | [official release](https://github.com/VICE-Team/svn-mirror/releases/tag/3.10.0) | `tools/vice/GTK3VICE-3.10-win64/bin/x64sc.exe` |
| JWasm 2.20 | [official release](https://github.com/Baron-von-Riedesel/JWasm/releases/tag/v2.20) | `tools/jwasm/JWasm.exe` |
| Zig 0.14.1 | [official download index](https://ziglang.org/download/index.json) | `tools/zig/zig-x86_64-windows-0.14.1/zig.exe` |
| UPX 4.2.4 | [official release](https://github.com/upx/upx/releases/tag/v4.2.4) | `tools/upx/upx-4.2.4-win64/upx.exe` |

The directory name around VICE may differ between release archives; the
lookup also searches one subdirectory with `bin/`. JWasm and UPX are searched
recursively in their respective tool directories. Check that your cc65
compiler reports `cl65 V2.19 - Git b3bf30f` to reproduce this release's
runtime/compiler input. A newer rolling snapshot may produce different
bytes; the source archive at the pinned revision is supplied.

Download the official `FD14-FloppyEdition.zip` to `build/downloads/` and
verify the digest above. GLaBIOS source and the fddepack source are already
in this repository. Then run:

```powershell
python -B tools/build_glabios.py
python -B tools/prepare_freedos.py
python -B tools/unpack_kernel.py
python -B tools/dev.py build
python -B tools/dev.py prepare-1581
python -B tools/dev.py test
python -B tools/dev.py test-1581-sectors
python -B tools/dev.py test-1581
```

`prepare-1581` preserves an existing D81 including its user files; use a
new build directory for a clean image. The long test uses a disposable
test disk and a build with automatic keyboard input. Production artifacts
are `build/1581/FD64.CRT` and `build/1581/FD64.D81`.

To verify the actual production pair and capture fresh BIOS/DOS screenshots:

```powershell
python -m pip install Pillow
python -B tools/verify_release.py
```

This starts a visible VICE with a disposable copy of the production disk,
waits for the real BIOS/DOS, and slowly enters a new ECHO. It checks
the ECHO response in actual VIC-II pixels. The visible DOS remains open.

To prepare the two redistributable archives, first fetch the hash-checked
upstream sources, then package an otherwise clean Git commit:

```powershell
python -B tools/fetch_release_sources.py
python -B tools/package_release.py
```

The packaging tool verifies the CRT payload/BIOS, D81's actual FAT12 files,
upstream source hashes and production test result before creating archives.
Generated files stay under ignored `build/release/`.
