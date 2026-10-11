"""Blank test images; deliberately no FreeDOS boot files."""
from pathlib import Path
import struct
SECTOR=512
TOTAL=65536
START=64
VOLUME=TOTAL-START
SPC=4
ROOT_ENTRIES=512
ROOT_SECTORS=32
def fat_sectors():
    n=1
    while True:
        clusters=(VOLUME-1-2*n-ROOT_SECTORS)//SPC
        needed=((clusters+2)*2+511)//512
        if needed<=n: return n
        n=needed
def create_ram(path):
    with Path(path).open("xb") as f: f.truncate(1024*1024)
def create_disk(path):
    fat=fat_sectors()
    mbr=bytearray(SECTOR)
    mbr[:4]=b"\xfa\xf4\xeb\xfd"
    # Active FAT16 partition: CHS 1/0/1 .. 1023/3/16.
    mbr[446:462]=struct.pack("<B3sB3sII",0x80,b"\x00\x01\x01",0x06,b"\x03\xd0\xff",START,VOLUME)
    mbr[510:512]=b"\x55\xaa"
    vbr=bytearray(SECTOR)
    vbr[:3]=b"\xeb\x3c\x90"; vbr[3:11]=b"FD64LAB "
    struct.pack_into("<HBHBHHBHHHII",vbr,11,512,SPC,1,2,ROOT_ENTRIES,VOLUME,0xf8,fat,16,4,START,0)
    vbr[36:39]=bytes([0x80,0,0x29])
    struct.pack_into("<I",vbr,39,0x46443634)
    vbr[43:54]=b"FD64 EMPTY "; vbr[54:62]=b"FAT16   "
    vbr[62:66]=b"\xfa\xf4\xeb\xfd"; vbr[510:512]=b"\x55\xaa"
    with Path(path).open("xb") as f:
        f.truncate(TOTAL*SECTOR)
        f.seek(0); f.write(mbr); f.seek(START*SECTOR); f.write(vbr)
        for offset in (START+1,START+1+fat):
            f.seek(offset*SECTOR); f.write(b"\xf8\xff\xff\xff")
