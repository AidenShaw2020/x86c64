"""Magic Desk CRT: C64 emulator in banks 1.., original GLaBIOS in bank 8.

Magic Desk uses IO1 only; EasyFlash's IO2 RAM would collide with the REU.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from dev import BUILD,ROOT,run,tool
def assemble(source,config,target):
    obj=target.with_suffix('.o')
    run([tool('ca65.exe','cc65'),'-I',ROOT,source,'-o',obj])
    run([tool('ld65.exe','cc65'),'-C',config,'-o',target,obj])
def bios_blob():
    assemble('src/c64/cart_bios.s','tools/cart_ram.cfg',BUILD/'cart-bios.bin')
def build(autotest=False,target=None):
    prg=BUILD/('fd64-1581-test.prg' if autotest else 'fd64-1581.prg')
    data=prg.read_bytes(); start=struct.unpack_from('<H',data)[0]; payload=data[2:]
    if start!=0x0801 or len(payload)>7*8192: raise ValueError('Payload exceeds cartridge banks 1..7')
    if len(payload)>65535 or start+len(payload)>0xb000: raise ValueError('Invalid C64 payload range')
    (BUILD/'cart-payload.inc').write_text(f'PAYLOAD_START=${start:04X}\nPAYLOAD_SIZE=${len(payload):04X}\nPAYLOAD_ENTRY=$080D\n')
    assemble('src/c64/cart_copy.s','tools/cart_ram.cfg',BUILD/'cart-copy.bin')
    assemble('src/c64/cart_boot.s','tools/cart_boot.cfg',BUILD/'cart-boot.bin')
    banks=[bytearray(b'\xff'*8192) for _ in range(16)]
    banks[0][:]=(BUILD/'cart-boot.bin').read_bytes()
    for p in range(0,len(payload),8192): banks[1+p//8192][:len(payload[p:p+8192])]=payload[p:p+8192]
    bios=(BUILD/'glabios/GLABIOS.ROM').read_bytes()
    if len(bios)!=8192: raise ValueError('Expected 8KiB GLaBIOS')
    banks[8][:]=bios
    header=bytearray(64); header[:16]=b'C64 CARTRIDGE   '
    struct.pack_into('>IHHBB',header,16,64,0x0100,19,0,1)
    header[32:64]=b'FD64 / GLABIOS / 1581'.ljust(32,b'\0')
    crt=bytes(header)+b''.join(b'CHIP'+struct.pack('>IHHHH',8208,0,n,0x8000,8192)+b for n,b in enumerate(banks))
    target=Path(target) if target else BUILD/'1581'/('FD64-TEST.CRT' if autotest else 'FD64.CRT')
    target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(crt)
    manifest={'format':'Magic Desk CRT type 19','bios_bank':8,'payload_banks':'1..7',
        'prg_sha256':hashlib.sha256(data).hexdigest(),'bios_sha256':hashlib.sha256(bios).hexdigest(),
        'crt_sha256':hashlib.sha256(crt).hexdigest(),'autotest':autotest}
    target.with_name(target.name+'.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Built',target); return target
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--autotest',action='store_true')
    parser.add_argument('--output',type=Path); args=parser.parse_args(); build(args.autotest,args.output)
