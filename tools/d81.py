"""1581 medium: reserved raw 720KiB FAT12 and a separate CBM filesystem.

Tracks 1..39 and 41..73 hold PC sectors; track 40 is the CBM directory/BAM.
Tracks 74..80 are available for CBM files; emulator/BIOS come from cartridge.
Never VALIDATE this hybrid disk:
reserved raw PC blocks have no CBM file chain to reconstruct their allocation.
"""
import argparse,hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SIZE=819200
def offset(track,sector):
    if not 1<=track<=80 or not 0<=sector<40: raise ValueError('Invalid 1581 track/sector')
    return ((track-1)*40+sector)*256
def pc_location(lba,half=0):
    if not 0<=lba<1440 or half not in (0,1): raise ValueError('Invalid PC sector')
    track=lba//20+1
    return track+(track>=40),lba%20*2+half
def extract_pc(data):
    if len(data)!=SIZE: raise ValueError('Expected 819200-byte D81')
    return b''.join(data[offset(*pc_location(lba,half)):offset(*pc_location(lba,half))+256]
        for lba in range(1440) for half in range(2))
def create(pc,files):
    if len(pc)!=737280: raise ValueError('Expected 720KiB FAT12 volume')
    if len(files)>8: raise ValueError('Single directory sector supports eight files')
    data=bytearray(SIZE); free={t:set(range(40)) for t in range(1,81)}
    def used(t,s): free[t].remove(s)
    for lba in range(1440):
        for half in range(2):
            t,s=pc_location(lba,half); p=offset(t,s)
            data[p:p+256]=pc[lba*512+half*256:lba*512+(half+1)*256]; used(t,s)
    for s in range(4): used(40,s)
    header=offset(40,0); data[header:header+4]=bytes((40,3,68,0))
    data[header+4:header+20]=b'FD64 DOS 1581'.ljust(16,b'\xa0')
    data[header+20:header+29]=b'\xa0\xa0FD\xa03D\xa0\xa0'
    directory=offset(40,3); data[directory:directory+2]=b'\0\xff'
    available=[(t,s) for t in range(74,81) for s in range(40)]
    for index,(name,content,kind) in enumerate(files):
        if not content or len(name)>16 or kind not in (1,2): raise ValueError('Invalid CBM file')
        count=(len(content)+253)//254
        if count>len(available): raise ValueError('Loader and BIOS exceed the 1581 system area')
        chain=available[:count]; del available[:count]
        for number,(t,s) in enumerate(chain):
            used(t,s); p=offset(t,s); chunk=content[number*254:(number+1)*254]
            data[p:p+2]=bytes(chain[number+1]) if number+1<count else bytes((0,len(chunk)+1))
            data[p+2:p+2+len(chunk)]=chunk
        entry=directory+index*32
        data[entry+2:entry+5]=bytes((0x80|kind,*chain[0]))
        data[entry+5:entry+21]=name.upper().encode('ascii').ljust(16,b'\xa0')
        struct.pack_into('<H',data,entry+30,count)
    for part in range(2):
        bam=offset(40,part+1)
        data[bam:bam+8]=bytes((40 if not part else 0,2 if not part else 255,68,187,70,68,192,0))
        for n in range(40):
            t=part*40+n+1; entry=bam+16+n*6; data[entry]=len(free[t])
            for s in free[t]: data[entry+1+s//8]|=1<<(s%8)
    assert extract_pc(data)==pc
    return bytes(data)
def prepare(target=None,autotest=False):
    target=Path(target) if target else ROOT/'build/1581/FD64.D81'
    if target.exists(): raise FileExistsError(f'Preserved existing disk: {target}')
    folder=ROOT/'build/freedos-unpacked'
    pc=(folder/'FREEDOS.IMG').read_bytes()
    if autotest:
        from prepare_freedos import FAT12
        fs=FAT12(pc); rows=bytearray(pc)
        content=b'@ECHO OFF\r\nVER\r\nDIR A:\\ /B\r\nECHO FD64 1581 WRITE OK>WRITE81.TXT\r\nTYPE WRITE81.TXT\r\nECHO FD64 FREEDOS READY\r\n'
        for p in range(fs.root,fs.root+fs.entries*32,32):
            if pc[p:p+11]==b'AUTOEXECBAT':
                cluster=struct.unpack_from('<H',pc,p+26)[0]
                if len(content)>fs.spc*512: raise ValueError('AUTOEXEC exceeds its existing cluster')
                position=fs.start+(cluster-2)*fs.spc*512
                rows[position:position+fs.spc*512]=content.ljust(fs.spc*512,b'\0')
                struct.pack_into('<I',rows,p+28,len(content)); break
        else: raise ValueError('AUTOEXEC not found')
        pc=bytes(rows)
    # Emulator and BIOS are supplied by CRT, not by the DOS disk.
    files=[]
    data=create(pc,files); target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(data)
    manifest={'format':'1581 D81 hybrid CBM + raw FAT12','image':str(target),
        'image_sha256':hashlib.sha256(data).hexdigest(),'pc_sha256':hashlib.sha256(pc).hexdigest(),
        'pc_tracks':'1-39,41-73','cbm_directory_track':40,'system_tracks':'74-80',
        'autotest':autotest,'files':{n:hashlib.sha256(c).hexdigest() for n,c,k in files},
        'warning':'Do not use CBM VALIDATE: raw FAT12 sectors are reserved in BAM'}
    target.with_name(target.name+'.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared',target); return target
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--output',type=Path)
    parser.add_argument('--autotest',action='store_true'); args=parser.parse_args()
    prepare(args.output,args.autotest)
