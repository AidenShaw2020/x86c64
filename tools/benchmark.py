"""Compare deterministic 6510 interpreter costs in visible VICE, using CIA2 cycles."""
import argparse,hashlib,json,re,struct,subprocess,time
from dev import BUILD,ROOT,run,tool
from vice_status import Monitor

def benchmark(name,flags=('-O',),pc_map=False,reuse_core=None):
    destination=BUILD/'benchmarks'/name; destination.mkdir(parents=True,exist_ok=True)
    cc=tool('cl65.exe','cc65'); objects=[]
    for source in ('tests/c64_benchmark.c','src/core/x86.c','src/core/pager.c','src/c64/reu.c'):
        if reuse_core and source!='tests/c64_benchmark.c': continue
        obj=destination/(source.replace('/','_')+'.o')
        source_flags=flags if source in ('src/core/x86.c','src/core/pager.c') else tuple(f for f in flags if f!='-Cl')
        defines=['-D','FD_BENCH_PC_MAP'] if pc_map else []
        run([cc,'-t','c64',*source_flags,*defines,'-c','-o',obj,source]); objects.append(obj)
    if reuse_core:
        old=BUILD/'benchmarks'/reuse_core
        objects.extend(sorted(old.glob('src_*.o')))
        objects.extend(old/module for module in ('fetch.o','alu.o','fast.o','address.o','pager-fast.o') if (old/module).exists())
    else:
        for module in ('fetch','alu','fast','address','pager-fast'):
            obj=destination/(module+'.o')
            source='pager' if module=='pager-fast' else module
            run([cc,'-t','c64','-c','-o',obj,'src/c64/'+source+'.s']); objects.append(obj)
    prg=destination/'benchmark.prg'; labels=destination/'benchmark.lbl'
    run([cc,'-t','c64','-o',prg,'-m',destination/'benchmark.map','-Ln',labels,*objects])
    symbols=labels.read_text()
    def address(name): return int(re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',symbols).group(1),16)
    report={'status':'running','compiler_flags':list(flags),'static_locals_scope':['x86','pager'],
        'configuration':'VICE PAL C64 + 1MiB REU; CIA2 cycles',
        'artifacts':{s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in
            ('src/core/x86.c','src/core/pager.c','src/c64/fetch.s','src/c64/alu.s','src/c64/fast.s','src/c64/address.s','src/c64/pager.s','tests/c64_benchmark.c')},
        'prg_sha256':hashlib.sha256(prg.read_bytes()).hexdigest()}
    report['pc_map']=pc_map
    if reuse_core:
        report['reused_core']=reuse_core
        report['artifacts']=json.loads((old/'report.json').read_text())['artifacts'].copy()
        report['artifacts']['tests/c64_benchmark.c']=hashlib.sha256((ROOT/'tests/c64_benchmark.c').read_bytes()).hexdigest()
    port=6506
    with (destination/'vice.log').open('w') as log:
        process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams','-warp','+sound','-pal',
            '-reu','-reusize','1024','-binarymonitor','-binarymonitoraddress',f'127.0.0.1:{port}',
            '-windowxpos','80','-windowypos','80','-logfile',str(destination/'internal.log'),
            '-autostartprgmode','1','-autostart',str(prg)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
            creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        print(f'Visible benchmark {name}, PID {process.pid}',flush=True)
        started=time.monotonic()
        try:
            while time.monotonic()-started<180:
                time.sleep(1)
                if process.poll() is not None: raise RuntimeError('Benchmark VICE exited')
                try: monitor=Monitor(port)
                except OSError: continue
                try:
                    if monitor.memory(address('bench_signature'),address('bench_signature')+3)!=b'886d': continue
                    fail=monitor.memory(address('bench_fail'),address('bench_fail'))[0]
                    if fail: raise RuntimeError(f'Guest benchmark failure {fail}')
                    cycles=struct.unpack('<3I',monitor.memory(address('bench_cycles'),address('bench_cycles')+11))
                    ins=struct.unpack('<3I',monitor.memory(address('bench_instructions'),address('bench_instructions')+11))
                    if ins!=(16385,24577,16385): raise RuntimeError(f'Unexpected instruction counts {ins}')
                    report['workloads']=[{'name':n,'c64_cycles':c,'x86_instructions':i,'cycles_per_instruction':round(c/i,2)}
                        for n,c,i in zip(('register_loop','memory_stack','arithmetic'),cycles,ins)]
                    report['status']='passed'; break
                finally: monitor.close()
            else: raise RuntimeError('Benchmark timeout')
        except Exception as error:
            report.update(status='failed',error=str(error)); raise
        finally:
            if process.poll() is None:
                try:
                    monitor=Monitor(port); monitor.command(0xbb); monitor.socket.close(); process.wait(timeout=10)
                except Exception: process.terminate(); process.wait(timeout=10)
            (destination/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report.get('workloads',report),indent=2),flush=True)
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('name'); parser.add_argument('--static-locals',action='store_true')
    parser.add_argument('--pc-map',action='store_true'); parser.add_argument('--reuse-core')
    args=parser.parse_args(); benchmark(args.name,('-O','-Cl') if args.static_locals else ('-O',),args.pc_map,args.reuse_core)
