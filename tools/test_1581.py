"""Check the real IEC U1/U2 backend with VICE's 1581 drive CPU enabled."""
import hashlib,json,re,shutil,subprocess,time
from dev import BUILD,ROOT,tool,run
from d81 import prepare,extract_pc
from vice_status import Monitor
def drive_options(image,true_drive=True):
    return ['-drive8type','1581','-drive8truedrive' if true_drive else '+drive8truedrive',
        '+trapdevice8' if true_drive else '-trapdevice8','-devicebackend8','0','-8',str(image)]
def probe():
    folder=BUILD/'1581-check'/str(time.time_ns()); folder.mkdir(parents=True,exist_ok=True)
    image=folder/'probe.d81'
    # Only the disposable probe medium is recreated, never a user's DOS disk.
    if image.exists(): raise FileExistsError('Preserved existing probe; use a fresh directory')
    prepare(image)
    original=image.read_bytes(); cc=tool('cl65.exe','cc65')
    prg=folder/'check.prg'; labels=folder/'check.lbl'
    run([cc,'-t','c64','-O','-c','-o',folder/'check.o','tests/c64_1581_check.c'])
    run([cc,'-t','c64','-o',prg,'-Ln',labels,folder/'check.o',BUILD/'drive1581.o'])
    symbols=labels.read_text()
    def at(name): return int(re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',symbols).group(1),16)
    log=(folder/'vice.log').open('w'); port=6511
    process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams',
        '+sound','-warp','-pal',*drive_options(image),'-binarymonitor',
        '-binarymonitoraddress',f'127.0.0.1:{port}','-autostartprgmode','1','-autostart',str(prg)],
        cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    report={'status':'running','configuration':'PAL C64, true 1581 drive CPU, IEC U1/U2',
        'pid':process.pid,'prg_sha256':hashlib.sha256(prg.read_bytes()).hexdigest()}
    path=folder/'report.json'; path.write_text(json.dumps(report,indent=2)); started=time.monotonic()
    print(f'Visible true-drive 1581 probe PID {process.pid}, port {port}',flush=True)
    try:
        while time.monotonic()-started<240:
            time.sleep(2)
            if process.poll() is not None: raise RuntimeError('Probe VICE exited')
            try: monitor=Monitor(port)
            except ConnectionRefusedError: continue
            try:
                signature=monitor.memory(at('check_signature'),at('check_signature')+3)
                if signature!=b'1851': continue
                failed=monitor.memory(at('check_fail'),at('check_fail'))[0]
                passed=monitor.memory(at('check_pass'),at('check_pass'))[0]
                report.update(guest_error=failed,drive_error=monitor.memory(at('fd_1581_error'),at('fd_1581_error'))[0])
                monitor.display(folder/'screen.png')
                if failed or not passed: raise RuntimeError(f'1581 probe failed: {report}')
            finally: monitor.close()
            monitor=Monitor(port)
            try: monitor.command(0xbb)
            finally: monitor.socket.close()
            process.wait(timeout=20)
            actual=image.read_bytes(); pc=bytearray(extract_pc(original))
            for lba in (779,780,1439): pc[lba*512:(lba+1)*512]=bytes((i^lba^0xa5)&255 for i in range(512))
            if extract_pc(actual)!=pc: raise RuntimeError('Persistent D81 sector contents differ')
            # CBM header/BAM/files must remain unchanged by raw PC writes.
            from d81 import offset,pc_location
            changed={offset(*pc_location(lba,half))+i for lba in (779,780,1439) for half in (0,1) for i in range(256)}
            if any(a!=b and i not in changed for i,(a,b) in enumerate(zip(original,actual))):
                raise RuntimeError('1581 write changed blocks outside requested PC sectors')
            report.update(status='passed',persistent_image_sha256=hashlib.sha256(actual).hexdigest(),
                elapsed_seconds=round(time.monotonic()-started,1))
            print('FD64_1581_PERSISTENT_SECTOR_PASS',flush=True); return
        raise TimeoutError('1581 sector probe timed out')
    except Exception as error: report.update(status='failed',error=str(error)); raise
    finally:
        report['directory']=str(folder.relative_to(ROOT))
        path.write_text(json.dumps(report,indent=2)+'\n')
        (BUILD/'1581-check-report.json').write_text(json.dumps(report,indent=2)+'\n'); log.close()
if __name__=='__main__': probe()
