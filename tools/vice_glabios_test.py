"""End-to-end test: real GLaBIOS, FreeDOS, commands, then leave the GUI open."""
import hashlib,json,re,shutil,struct,subprocess,time
from dev import BUILD,ROOT,run,tool,disk_options
from vice_status import Monitor
def test(fast=False,name=None,port=None):
    folder=BUILD/('freedos-unpacked' if fast else 'freedos')
    stem='vice-'+name if name else ('vice-glabios-fast' if fast else 'vice-glabios-test')
    port=port or (6504 if fast else 6503)
    name=name or ('glabios-fast' if fast else 'glabios')
    out=run([BUILD/'glabios-boot.exe',BUILD/'glabios/GLABIOS.ROM',
        folder/'FREEDOS.IMG','full'],log=BUILD/(name+'-freedos-native.log'))
    print(out,flush=True)
    if 'FD64_GLABIOS_FREEDOS_PASS' not in out: raise RuntimeError('Native GLaBIOS/FreeDOS failed')
    for suffix in ('.prg','.lbl'):
        shutil.copyfile(BUILD/('fd64-glabios-test'+suffix),BUILD/(stem+suffix))
    shutil.copyfile(BUILD/'glabios/GLABIOS.ROM',folder/'GLABIOS.ROM')
    labels=(BUILD/(stem+'.lbl')).read_text()
    def address(name): return int(re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',labels).group(1),16)
    report={'status':'running','configuration':'PAL C64, 2MiB REU, real GLaBIOS on 6510',
        'native_pass':True,'started':time.time()}
    report['artifacts']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (BUILD/(stem+'.prg'),BUILD/(stem+'.lbl'),folder/'FREEDOS.IMG',folder/'GLABIOS.ROM')}
    report['unpacked_kernel']=fast
    path=BUILD/(name+'-test-report.json'); path.write_text(json.dumps(report,indent=2))
    log=(BUILD/(name+'-vice.log')).open('w',encoding='utf-8')
    process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams',
        '-warp','+sound','-pal','-reu','-reusize','2048','-binarymonitor',
        '-binarymonitoraddress',f'127.0.0.1:{port}','-windowxpos','80','-windowypos','80',
        '-windowwidth','1100','-windowheight','850',*disk_options(folder),
        '-logfile',str(BUILD/(name+'-vice-internal.log')),'-autostartprgmode','1',
        '-autostart',str(BUILD/(stem+'.prg'))],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
        creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (BUILD/(stem+'.pid')).write_text(str(process.pid))
    print(f'Visible GLaBIOS/FreeDOS integration test PID {process.pid}, monitor {port}',flush=True)
    last_ins=0
    try:
        while time.time()-report['started']<7200:
            time.sleep(30)
            if process.poll() is not None:
                report['status']='interrupted'; report['exit']=process.returncode
                raise RuntimeError('VICE exited before the guest pass marker')
            m=Monitor(port)
            try:
                passed=m.memory(address('fd_test_pass'),address('fd_test_pass'))[0]
                failed=m.memory(address('fd_test_fail'),address('fd_test_fail'))[0]
                cpu=m.memory(address('cpu'),address('cpu')+34)
                ins=struct.unpack_from('<I',cpu,31)[0]
                try: report['c64_cycles_since_reset']=m.clock()
                except ValueError: pass  # Older VICE has no CPU History API.
                report.update(instructions=ins,pc=f'{struct.unpack_from("<H",cpu,18)[0]:04X}:{struct.unpack_from("<H",cpu,24)[0]:04X}',
                    post_complete=bool(m.memory(address('fd_xt_booted'),address('fd_xt_booted'))[0]))
                if passed:
                    report['status']='passed'; report['guest_pass_marker']=True
                    try: m.display(BUILD/(name+'-freedos-vice.png'))
                    except ModuleNotFoundError: report['screenshot']='Pillow unavailable'
                    try:
                        snapshot=BUILD/(name+'-ready.vsf'); m.dump(snapshot)
                        report['snapshot']=str(snapshot.relative_to(ROOT))
                    except Exception as error: report['snapshot_error']=str(error)
                    print('FD64_GLABIOS_FREEDOS_VICE_PASS',flush=True); return
                if failed:
                    report['status']='failed'; report['guest_error']=failed
                    raise RuntimeError(f'C64 x86 execution failed, status {failed}, PC {report["pc"]}')
            finally: m.close()
            path.write_text(json.dumps(report,indent=2))
            if ins//250000!=last_ins//250000:
                print(f'Guest instructions {ins}, PC {report["pc"]}, POST complete {report["post_complete"]}',flush=True)
            last_ins=ins
        report['status']='timeout'; raise RuntimeError('No C64 pass marker within 2 hours')
    finally:
        report['elapsed_seconds']=round(time.time()-report['started'],1)
        path.write_text(json.dumps(report,indent=2)); log.close()
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(); parser.add_argument('--fast',action='store_true')
    parser.add_argument('--name'); parser.add_argument('--port',type=int)
    args=parser.parse_args(); test(args.fast,args.name,args.port)
