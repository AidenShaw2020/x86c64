"""Create a minimal bootable FreeDOS FAT12 image from the official release."""
from pathlib import Path
import struct,zipfile,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
SHA256="45b1fa7c52dd996c3bfa5e352ffcd410781b952a6ad629f15a4c9ec4bbaefc5a"

class FAT12:
    def __init__(self,data):
        self.data=data
        self.spc=data[13]; self.reserved=struct.unpack_from('<H',data,14)[0]
        self.fats=data[16]; self.entries=struct.unpack_from('<H',data,17)[0]
        self.spf=struct.unpack_from('<H',data,22)[0]
        self.root=(self.reserved+self.fats*self.spf)*512
        self.start=self.root+((self.entries*32+511)//512)*512
        self.fat=data[self.reserved*512:(self.reserved+self.spf)*512]
    def chain(self,cluster,size=None):
        content=bytearray(); seen=set()
        while 2<=cluster<0xff8 and (size is None or len(content)<size):
            if cluster in seen: raise ValueError('FAT loop')
            seen.add(cluster); address=self.start+(cluster-2)*self.spc*512
            content.extend(self.data[address:address+self.spc*512])
            entry=cluster*3//2; pair=self.fat[entry]|self.fat[entry+1]<<8
            cluster=(pair>>4 if cluster&1 else pair&0xfff)
        if size is not None and len(content)<size: raise ValueError('Truncated FAT chain')
        return bytes(content if size is None else content[:size])
    def files(self,rows=None,prefix=''):
        result={}
        if rows is None: rows=self.data[self.root:self.root+self.entries*32]
        for offset in range(0,len(rows),32):
            row=rows[offset:offset+32]
            if row[0]==0: break
            if row[0] in (229,46) or row[11]&8 or row[11]==15: continue
            name=(row[:8].decode('ascii').rstrip()+'.'+row[8:11].decode('ascii').rstrip()).rstrip('.')
            cluster=struct.unpack_from('<H',row,26)[0]; size=struct.unpack_from('<I',row,28)[0]
            if row[11]&16: result.update(self.files(self.chain(cluster),prefix+name+'/'))
            else: result[prefix+name]=self.chain(cluster,size)
        return result

def prepare(target=None,kernel=None):
    archive=ROOT/'build/downloads/FD14-FloppyEdition.zip'
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256: raise ValueError('Release digest mismatch')
    with zipfile.ZipFile(archive) as bundle: original=bundle.read('720k/x86BOOT.img')
    fs=FAT12(original); files=fs.files()
    print('Original files:',[(name,len(value)) for name,value in files.items()])
    print('CONFIG:',files.get('FDCONFIG.SYS',files.get('CONFIG.SYS',b'')))
    print('AUTOEXEC:',files.get('FDAUTO.BAT',files.get('AUTOEXEC.BAT',b''))[:800])
    selected={'KERNEL.SYS':files['KERNEL.SYS'],'COMMAND.COM':files['FREEDOS/BIN/COMMAND.COM']}
    packed_hash=hashlib.sha256(selected['KERNEL.SYS']).hexdigest()
    if kernel is not None:
        selected['KERNEL.SYS']=Path(kernel).read_bytes()
    selected['FDCONFIG.SYS']=b'SHELL=A:\\COMMAND.COM A:\\ /E:512 /P\r\n'
    selected['AUTOEXEC.BAT']=b'@ECHO OFF\r\nVER\r\nDIR A:\\ /B\r\nECHO FD64 FREEDOS READY\r\n'
    data=bytearray(len(original)); data[:512]=original[:512]
    fat=bytearray(fs.spf*512); fat[:3]=bytes((original[21],255,255))
    next_cluster=2
    for index,(name,content) in enumerate(selected.items()):
        first=next_cluster; count=(len(content)+fs.spc*512-1)//(fs.spc*512)
        for number in range(count):
            cluster=next_cluster; next_cluster+=1; value=0xfff if number==count-1 else next_cluster
            address=cluster*3//2
            if cluster&1:
                fat[address]=(fat[address]&15)|((value<<4)&255); fat[address+1]=value>>4
            else:
                fat[address]=value&255; fat[address+1]=(fat[address+1]&240)|(value>>8)
            offset=fs.start+(cluster-2)*fs.spc*512; piece=content[number*fs.spc*512:(number+1)*fs.spc*512]
            data[offset:offset+len(piece)]=piece
        base,extension=name.split('.'); row=bytearray(32)
        row[:11]=(base.ljust(8)+extension.ljust(3)).encode('ascii'); row[11]=0x20
        struct.pack_into('<H',row,26,first); struct.pack_into('<I',row,28,len(content))
        data[fs.root+index*32:fs.root+(index+1)*32]=row
    for copy in range(fs.fats):
        offset=(fs.reserved+copy*fs.spf)*512; data[offset:offset+len(fat)]=fat
    target=Path(target) if target is not None else ROOT/'build/freedos'
    target.mkdir(parents=True,exist_ok=True)
    (target/'FREEDOS.IMG').write_bytes(data)
    manifest={'release':'FreeDOS 1.4 Floppy Edition','archive_sha256':SHA256,
        'source_image':'720k/x86BOOT.img','image_sha256':hashlib.sha256(data).hexdigest(),
        'changes':'FAT rebuilt with original KERNEL.SYS/COMMAND.COM, minimal config and AUTOEXEC',
        'files':{name:hashlib.sha256(content).hexdigest() for name,content in selected.items()}}
    if kernel is not None:
        manifest['kernel_transform']='UPX unpacked before boot; original kernel code'
        manifest['original_packed_kernel_sha256']=packed_hash
    (target/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    assert FAT12(data).files()==selected
    print('Prepared',target/'FREEDOS.IMG')
if __name__=='__main__': prepare()
