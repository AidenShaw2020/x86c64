"""Read live FD64 state through VICE's documented binary monitor, then resume."""
from pathlib import Path
import socket,struct,re,json,datetime,argparse
ROOT=Path(__file__).resolve().parents[1]
class Monitor:
    def __init__(self,port=6502):
        self.socket=socket.create_connection(('127.0.0.1',port),timeout=10)
        self.sequence=0
    def receive(self,size):
        data=bytearray()
        while len(data)<size:
            piece=self.socket.recv(size-len(data))
            if not piece: raise EOFError('VICE disconnected')
            data.extend(piece)
        return bytes(data)
    def command(self,kind,body=b''):
        self.sequence+=1
        self.socket.sendall(struct.pack('<BBIIB',2,2,len(body),self.sequence,kind)+body)
        while True:
            header=self.receive(12)
            if header[:2]!=b'\2\2': raise ValueError('Unexpected monitor header')
            length,reply,error,sequence=struct.unpack_from('<IBBI',header,2)
            content=self.receive(length)
            if sequence!=self.sequence: continue
            if error: raise ValueError(f'Monitor error {error:02x}, command {kind:02x}')
            return content
    def memory(self,start,end):
        return self.command(1,struct.pack('<BHHBH',0,start,end,0,0))[2:]
    def dump(self,path):
        filename=str(Path(path).resolve()).replace('\\','/').encode('utf-8')
        if len(filename)>255: raise ValueError('Snapshot filename exceeds monitor limit')
        return self.command(0x41,bytes([1,0,len(filename)])+filename)
    def clock(self):
        """6510 clock from one CPU History record (VICE 3.10 binary API)."""
        content=self.command(0x86,struct.pack('<BI',0,1))
        if struct.unpack_from('<I',content)[0]!=1: return None
        position=5  # count (4), then item-size (1)
        registers=struct.unpack_from('<H',content,position)[0]; position+=2
        for _ in range(registers): position+=1+content[position]
        return struct.unpack_from('<Q',content,position)[0]
    def display(self,path):
        from PIL import Image
        content=self.command(0x84,b'\1\0')
        fields,width,height,x,y,inner_w,inner_h,bits,length=struct.unpack_from('<I6HBI',content)
        assert bits==8 and length==width*height
        # FL excludes both its own four bytes and the four-byte buffer length.
        start=8+fields
        pixels=content[start:start+length]
        # VICE 3.10 may omit four bytes at the end of the debug framebuffer.
        # Use only the complete active area, which lies before the missing border.
        if (y+inner_h-1)*width+x+inner_w>len(pixels):
            raise ValueError('Truncated VICE active display')
        visible=b''.join(pixels[row*width+x:row*width+x+inner_w] for row in range(y,y+inner_h))
        img=Image.frombytes('P',(inner_w,inner_h),visible)
        # VICE's default Pepto PAL palette. Pixel values come from the emulator.
        colors=[(0,0,0),(255,255,255),(104,55,43),(112,164,178),(111,61,134),(88,141,67),
            (53,40,121),(184,199,111),(111,79,37),(67,57,0),(154,103,89),(68,68,68),
            (108,108,108),(154,210,132),(108,94,181),(149,149,149)]
        img.putpalette([c for rgb in colors for c in rgb]+[0]*(768-48))
        img=img.convert('RGB')
        img.save(path)
    def close(self):
        try: self.command(0xaa)
        finally: self.socket.close()

def status(stem='fd64-dos-test',port=6502,screenshot=None):
    labels=(ROOT/'build'/(stem+'.lbl')).read_text()
    address=int(re.search(r'al ([0-9A-Fa-f]+) \._cpu\b',labels).group(1),16)
    monitor=Monitor(port)
    try:
        cpu=monitor.memory(address,address+34)
        if screenshot: monitor.display(screenshot)
        screen=monitor.memory(0x400,0x7e7)
        values=struct.unpack('<14H3BI',cpu)
        def character(ch):
            ch&=127
            if ch<32: return chr(ch+96)
            if 32<=ch<64 or 65<=ch<91: return chr(ch)
            return ' '
        lines=[''.join(character(ch) for ch in screen[i:i+40]).rstrip() for i in range(0,1000,40)]
        report={'time':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'pc':f'{values[9]:04X}:{values[12]:04X}','status':values[14],
            'opcode':f'{values[15]:02X}','instructions':values[-1],
            'flags':f'{values[13]:04X}',
            'registers':dict(zip(['AX','CX','DX','BX','SP','BP','SI','DI'],values[:8])),
            'screen':lines}
        markers={}
        for name in ('fd_xt_booted','fd_video_ready','fd_video_files','fd_video_keyboard_ok','fd_test_pass','fd_test_fail'):
            match=re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',labels)
            if match:
                at=int(match.group(1),16); markers[name]=monitor.memory(at,at)[0]
        if markers: report['markers']=markers
        (ROOT/'build'/'vice-live-status.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    finally: monitor.close()
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--stem',default='fd64-dos-test')
    parser.add_argument('--port',type=int,default=6502)
    parser.add_argument('--screenshot',type=Path); args=parser.parse_args()
    status(args.stem,args.port,args.screenshot)
