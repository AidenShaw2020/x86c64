"""Independent Python arithmetic oracle -> actual 6510 assembly tests in VICE."""
import hashlib,itertools,json,random,re,struct,subprocess,time
from dev import BUILD,ROOT,run,tool
from vice_status import Monitor

def vectors():
    rng=random.Random(8086)
    for word in (0,1):
        mask=65535 if word else 255; sign=(mask+1)//2
        edge=(0,1,15,16,sign-1,sign,mask-1,mask)
        operands=[*itertools.product(edge,repeat=2),*((rng.randrange(mask+1),rng.randrange(mask+1)) for _ in range(32))]
        for op in range(8):
            for a,b in operands:
                for carry in (0,1):
                    initial=0x0fd6|carry
                    incoming=carry if op in (2,3) else 0
                    subtract=op in (3,5,7)
                    if op in (1,4,6):
                        raw=(a|b) if op==1 else ((a&b) if op==4 else (a^b)); flags=0
                    else:
                        raw=a-b-incoming if subtract else a+b+incoming
                        flags=int(raw<0 or raw>mask)
                        if (a^b^(raw&mask))&16: flags|=16
                        sa=a if a<sign else a-mask-1; sb=b if b<sign else b-mask-1
                        signed=sa-sb-incoming if subtract else sa+sb+incoming
                        if signed< -sign or signed>=sign: flags|=0x800
                    value=raw&mask
                    if value==0: flags|=0x40
                    if value&sign: flags|=0x80
                    if (value&255).bit_count()%2==0: flags|=4
                    flags|=initial&~0x8d5
                    # Byte operations preserve AH; CMP preserves the whole AX.
                    ax=a if word else 0x5a00|a
                    result=ax if op==7 else (value if word else 0x5a00|value)
                    yield (ax,b,initial,result,flags,op,word)

def run_test():
    destination=BUILD/'cpu-check'; destination.mkdir(exist_ok=True)
    data=list(vectors())
    # Split vectors into batches so a test does not exceed C64 RAM.
    report={'status':'running','vectors':len(data),'batches':[]}
    (destination/'report.json').write_text(json.dumps(report,indent=2))
    cc=tool('cl65.exe','cc65'); objects=[]
    for source in ('src/core/x86.c','src/core/pager.c','src/c64/reu.c','src/c64/fetch.s','src/c64/alu.s','src/c64/fast.s','src/c64/address.s','src/c64/pager.s'):
        obj=destination/(source.replace('/','_')+'.o')
        run([cc,'-t','c64','-O','-Cl','-c','-o',obj,source]); objects.append(obj)
    for batch,start in enumerate(range(0,len(data),512)):
        part=data[start:start+512]
        header='static const Vector vectors[]={\n'+''.join('{'+','.join(str(x) for x in v)+'},\n' for v in part)+'};\n'
        (destination/'cpu_vectors.h').write_text(header)
        obj=destination/'check.o'; prg=destination/f'check-{batch}.prg'; labels=destination/f'check-{batch}.lbl'
        run([cc,'-t','c64','-O','-Cl','-I',destination,'-c','-o',obj,'tests/c64_cpu_check.c'])
        run([cc,'-t','c64','-o',prg,'-Ln',labels,obj,*objects])
        symbols=labels.read_text()
        def address(name): return int(re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',symbols).group(1),16)
        with (destination/f'vice-{batch}.log').open('w') as log:
            process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams','-warp','+sound','-pal',
                '-reu','-reusize','1024','-binarymonitor','-binarymonitoraddress','127.0.0.1:6507',
                '-autostartprgmode','1','-autostart',prg],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            deadline=time.monotonic()+120
            try:
                while time.monotonic()<deadline:
                    time.sleep(1)
                    if process.poll() is not None: raise RuntimeError('VICE CPU test exited')
                    try: m=Monitor(6507)
                    except OSError: continue
                    try:
                        if m.memory(address('check_signature'),address('check_signature')+3)!=b'!UPC': continue
                        fail=m.memory(address('check_fail'),address('check_fail'))[0]
                        index=struct.unpack('<H',m.memory(address('check_index'),address('check_index')+1))[0]
                        if fail:
                            cpu=m.memory(address('check_cpu'),address('check_cpu')+34)
                            vector=data[start+index] if index<len(part) else 'fast-path boundary test'
                            raise RuntimeError(f'6510 CPU failure {fail}, index {start+index}: {vector}; CPU {cpu.hex()}')
                        report['batches'].append({'vectors':len(part),'prg_sha256':hashlib.sha256(prg.read_bytes()).hexdigest()})
                        (destination/'report.json').write_text(json.dumps(report,indent=2))
                        print(f'6510 CPU vectors {start+len(part)}/{len(data)} passed',flush=True); break
                    finally: m.close()
                else: raise RuntimeError('6510 CPU test timeout')
            finally:
                if process.poll() is None:
                    try: m=Monitor(6507); m.command(0xbb); m.socket.close(); process.wait(timeout=10)
                    except Exception: process.terminate(); process.wait(timeout=10)
    report['status']='passed'; (destination/'report.json').write_text(json.dumps(report,indent=2))
    return report

def test():
    try: return run_test()
    except Exception as error:
        path=BUILD/'cpu-check/report.json'
        report=json.loads(path.read_text()) if path.exists() else {}
        report.update(status='failed',error=str(error)); path.write_text(json.dumps(report,indent=2))
        raise

if __name__=='__main__': test()
