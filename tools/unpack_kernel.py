"""Prepare an uncompressed FreeDOS kernel, retaining the upstream DOS code."""
from pathlib import Path
import hashlib,json,os,struct,subprocess
from dev import ROOT,BUILD,run,tool
from prepare_freedos import FAT12,prepare
def unpack():
    folder=BUILD/'kernel-unpacked'; folder.mkdir(exist_ok=True)
    packed=FAT12((BUILD/'freedos/FREEDOS.IMG').read_bytes()).files()['KERNEL.SYS']
    if b'UPX!' not in packed: raise ValueError('Expected the original packed release kernel')
    source=ROOT/'vendor/fddepack/depack.c'
    exe=folder/'fddepack.exe'
    run([tool('zig.exe','zig'),'cc','-O2',source,'-o',exe])
    upx=next((ROOT/'tools/upx').rglob('upx.exe'))
    (folder/'kernel.packed').write_bytes(packed)
    environment=os.environ.copy(); environment['PATH']=str(upx.parent)+os.pathsep+environment['PATH']
    result=subprocess.run([str(exe),'kernel.packed','kernel.exe'],cwd=folder,env=environment,
        capture_output=True,text=True,check=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (folder/'unpack.log').write_text(result.stdout+result.stderr)
    # fddepack does not propagate UPX's exit status; validate its output ourselves.
    image=(folder/'kernel.exe').read_bytes(); header=struct.unpack_from('<14H',image)
    if header[0]!=0x5a4d or header[4]<2: raise ValueError('Invalid unpacked MZ header')
    size=(header[2]-1)*512+(header[1] or 512)
    if size!=len(image) or size<=len(packed): raise ValueError('UPX did not unpack the kernel')
    body=bytearray(image[header[4]*16:])
    for off,seg in struct.iter_unpack('<HH',image[header[12]:header[12]+header[3]*4]):
        address=seg*16+off
        struct.pack_into('<H',body,address,(struct.unpack_from('<H',body,address)[0]+0x60)&65535)
    if body[2:8]!=b'CONFIG' or b'UPX!' in body: raise ValueError('Unexpected flattened kernel')
    flat=folder/'KERNEL.SYS'; flat.write_bytes(body)
    manifest={'source':'FreeDOS 1.4 kernel 2043','packed_sha256':hashlib.sha256(packed).hexdigest(),
        'unpacked_sha256':hashlib.sha256(body).hexdigest(),'upx_version':'4.2.4',
        'upx_sha256':hashlib.sha256(upx.read_bytes()).hexdigest(),
        'depacker_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'flattening':'MZ load module with relocation factor 0x60, no repacking'}
    (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    prepare(BUILD/'freedos-unpacked',flat)
if __name__=='__main__': unpack()
