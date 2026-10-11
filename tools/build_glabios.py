"""Reproducible GLaBIOS emulator configuration; never modifies vendor sources."""
from pathlib import Path
import re,subprocess,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
def build():
    out=ROOT/'build'/'glabios'; out.mkdir(exist_ok=True)
    source=(ROOT/'vendor/glabios/src/GLABIOS.ASM').read_text()
    options={'POST_TEST_DMA_TC0':0,'POST_TEST_KB':0,'POST_TEST_KEY':0,
        'FDC_ENABLE':0,'RAM_TEST_NONE':1,'MAX_RAM_AUTO':0,'MAX_RAM':640,
        'BASIC_ROM':0,'RANDOM_TAGLINE':0}
    # The upstream ARCH_EMU profile supplies the remaining emulator settings.
    block='\n; FD64 virtual XT: block device option ROM, no physical FDC.\n'
    block+='\n'.join(f'{key} = {value}' for key,value in options.items())+'\n'
    # Insert after all upstream architecture overrides, before code/data definitions.
    marker='\n\tIF FDC_ENABLE EQ 0'
    if marker not in source: raise ValueError('Configuration boundary changed')
    source=source.replace(marker,block+marker,1)
    asm=out/'GLABIOS.ASM'; asm.write_text(source)
    exe=next((ROOT/'tools/jwasm').rglob('JWasm.exe'),None)
    if exe is None: exe=next((ROOT/'tools/jwasm').rglob('jwasm.exe'))
    subprocess.run([str(exe),'-Fl','-Zm','-bin','-DARCH_TYPE=ARCH_EMU',
        '-DCPU_TYPE=CPU_8088','-FoGLABIOS.BIN',str(asm)],cwd=out,check=True)
    subprocess.run(['python','-B',str(ROOT/'vendor/glabios/src/gla2rom/gla2rom.py'),
        str(out/'GLABIOS.BIN'),str(out/'GLABIOS.ROM')],cwd=ROOT,check=True)
    rom=(out/'GLABIOS.ROM').read_bytes()
    assert len(rom)==8192 and sum(rom)&255==0 and rom[0x1ff0]==0xea
    (out/'configuration.json').write_text(json.dumps({'upstream':'v0.4.2',
        'architecture':'ARCH_EMU','cpu':'CPU_8088','overrides':options,
        'sha256':hashlib.sha256(rom).hexdigest()},indent=2)+'\n')
if __name__=='__main__': build()
