# SPDX-License-Identifier: GPL-2.0-or-later
"""Package validated CRT/D81 plus exact corresponding sources and notices."""
import hashlib, io, json, struct, subprocess, zipfile
from pathlib import Path
from fetch_release_sources import DEST, fetch
from d81 import extract_pc
from prepare_freedos import FAT12

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
TAG='v0.1.0-poc'

def digest(data): return hashlib.sha256(data).hexdigest()

def git(*args):
    return subprocess.check_output(['git',*args],cwd=ROOT).decode('utf-8').strip()

def package():
    inputs=fetch()
    if git('status','--porcelain'): raise RuntimeError('Commit all source changes before packaging')
    commit=git('rev-parse','HEAD')
    files=git('ls-files','-z').split('\0')
    files=[name for name in files if name]
    # Archive committed blobs, not worktree line-ending conversions.
    with zipfile.ZipFile(io.BytesIO(subprocess.check_output(['git','archive','--format=zip','HEAD'],cwd=ROOT))) as archive:
        committed={name:archive.read(name) for name in files}
    if not {'README.md','THIRD_PARTY_NOTICES.md','docs/screenshots/bios-post.png','docs/screenshots/freedos.png'}<=set(files):
        raise ValueError('Tracked publication files are missing')
    production=json.loads((BUILD/'release-verification-report.json').read_text())
    if production['status']!='passed' or production.get('response','').lower()!='x86c64 poc ok':
        raise ValueError('Production cold boot has not passed')
    evidence=json.loads((ROOT/'docs/evidence/production-release-report.json').read_text())
    if evidence['status']!='passed' or evidence['artifacts']!=production['artifacts']:
        raise ValueError('Committed production evidence differs')
    media={name:(BUILD/'1581'/name).read_bytes() for name in ('FD64.CRT','FD64.D81')}
    hashes={name:digest(data) for name,data in media.items()}
    if hashes!=production['artifacts']: raise ValueError('Production media changed after verification')
    crt=media['FD64.CRT']
    if crt[:16]!=b'C64 CARTRIDGE   ' or struct.unpack_from('>H',crt,22)[0]!=19:
        raise ValueError('Expected Magic Desk cartridge')
    banks={}; pos=64
    while pos<len(crt):
        if crt[pos:pos+4]!=b'CHIP': raise ValueError('Invalid CHIP packet')
        length,kind,bank,address,size=struct.unpack_from('>IHHHH',crt,pos+4)
        if length!=8208 or kind!=0 or address!=0x8000 or size!=8192 or bank in banks:
            raise ValueError('Unexpected cartridge geometry')
        banks[bank]=crt[pos+16:pos+length]; pos+=length
    if set(banks)!=set(range(16)): raise ValueError('Missing cartridge banks')
    prg=(BUILD/'fd64-1581.prg').read_bytes()
    if b''.join(banks[i] for i in range(1,8))[:len(prg)-2]!=prg[2:]:
        raise ValueError('Cartridge emulator differs from PRG')
    if digest(banks[8])!=inputs['glabios']['configured_rom_sha256']:
        raise ValueError('Cartridge BIOS mismatch')
    pc=extract_pc(media['FD64.D81']); dos=FAT12(pc).files()
    expected=FAT12((BUILD/'freedos-unpacked/FREEDOS.IMG').read_bytes()).files()
    if dos!=expected or set(dos)!={'KERNEL.SYS','COMMAND.COM','FDCONFIG.SYS','AUTOEXEC.BAT'}:
        raise ValueError('Production disk contains unexpected or modified files')
    if digest(dos['COMMAND.COM'])!=inputs['freecom_binary']['sha256']:
        raise ValueError('FreeCom binary mismatch')
    if digest(dos['KERNEL.SYS'])!='de002e10126a872430fe0745d8e1277ccf49b144eef1a6e497747b7fcda7998d':
        raise ValueError('Unpacked FreeDOS kernel mismatch')
    sources=['kernel-2043-source.zip','freecom-086-fd14-source.zip','cc65-b3bf30f-source.zip']
    manifest={'release':TAG,'commit':commit,'configuration':production['configuration'],
        'media_sha256':hashes,'dos_files_sha256':{name:digest(data) for name,data in dos.items()},
        'upstream_source_sha256':{name:digest((DEST/name).read_bytes()) for name in sources},
        'bios_sha256':digest(banks[8]),'production_test':production}
    out=BUILD/'release'; out.mkdir(exist_ok=True)
    prefix='x86c64-'+TAG
    binaries=out/(prefix+'-binaries.zip'); source=out/(prefix+'-corresponding-source.zip')
    manifest_bytes=(json.dumps(manifest,indent=2)+'\n').encode()
    instructions=f'''# x86c64 {TAG} — proof of concept

Install VICE 3.10 separately; no emulator or Commodore ROMs are included.
From this directory run (Windows: x64sc.exe or its full path):

    x64sc -default -pal -warp +sound -reu -reusize 1024 -drive8type 1581 -drive8truedrive +trapdevice8 -devicebackend8 0 -8 FD64.D81 -cartcrt FD64.CRT

BIOS and emulator: FD64.CRT (Magic Desk). FreeDOS: FD64.D81 (1581 device 8).
Boot is very slow; the earlier complete persistence test took 12 min 46 sec
in VICE warp. This release is validated in VICE, not on physical hardware.
F1/F3 select left/right 40 columns; F5 shows all 80 columns.
Keep D81 writable, use a copy for experiments and close VICE normally.
Do not use Commodore VALIDATE on this hybrid disk; it would free PC blocks.

GPL sources accompany this ZIP as {prefix}-corresponding-source.zip
at https://github.com/AidenShaw2020/x86c64/releases/tag/{TAG} .
Keep sources and notices available when redistributing. See
THIRD_PARTY_NOTICES.md and LICENSES/. No MS-DOS binaries are included.

Source commit: {commit}
'''
    with zipfile.ZipFile(binaries,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in media.items(): z.writestr(name,data)
        z.writestr('README.md',instructions); z.writestr('manifest.json',manifest_bytes)
        z.writestr('SHA256SUMS-media.txt',''.join(value+'  '+name+'\n' for name,value in hashes.items()))
        for name in files:
            if name in ('LICENSE','THIRD_PARTY_NOTICES.md','docs/release.md','docs/release-inputs.json') or name.startswith(('LICENSES/','docs/evidence/','docs/screenshots/')):
                z.writestr(name,committed[name])
    with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name in files:
            if name.startswith(('build/','.git/','tools/cc65/','tools/vice/','tools/zig/','tools/upx/','tools/jwasm/')):
                raise ValueError('Unexpected generated/tool binary in Git: '+name)
            z.writestr('project/'+name,committed[name])
        for name in sources: z.write(DEST/name,'upstream/'+name)
        for name in ('GLABIOS.ASM','configuration.json'):
            z.write(BUILD/'glabios'/name,'generated-glabios/'+name)
        for folder in ('freedos','kernel-unpacked','freedos-unpacked'):
            z.write(BUILD/folder/'manifest.json','preparation/'+folder+'-manifest.json')
        z.writestr('manifest.json',manifest_bytes)
        z.writestr('README.md','''# Corresponding source

Project source is in project/. Build and transformation instructions are
in project/docs/release.md. The source commit and binary hashes are in
manifest.json. Original notices/licenses are preserved.

Extract upstream/kernel-2043-source.zip for the FreeDOS kernel and boot
code. Extract upstream/freecom-086-fd14-source.zip for the exact FreeCom
source package (including SUPPL). Extract upstream/cc65-b3bf30f-source.zip
for the C64 compiler/runtime source. Their own build instructions are
included. generated-glabios/ records the configured assembly source and
options used to build the BIOS contained in the cartridge; unchanged
upstream BIOS source is in project/vendor/glabios/.

The FreeDOS kernel is unpacked and flattened by project/tools/unpack_kernel.py.
Only preparation configuration is changed; the DOS program logic is original.
External compiler/emulator executables are obtained separately.
''')
    for path in (binaries,source):
        with zipfile.ZipFile(path) as z:
            if z.testzip() is not None: raise ValueError('Invalid ZIP: '+path.name)
    sums=''.join(digest(path.read_bytes())+'  '+path.name+'\n' for path in (binaries,source))
    (out/'SHA256SUMS.txt').write_text(sums,encoding='utf-8')
    (out/'manifest.json').write_bytes(manifest_bytes)
    for path in (binaries,source): print(path.name,path.stat().st_size,'bytes')
    print('X86C64_RELEASE_PACKAGE_VERIFIED',commit)

if __name__=='__main__': package()
