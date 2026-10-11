"""Cold boot cartridge emulator/GLaBIOS with real 1581 IEC read/write."""
import hashlib,json,re,shutil,struct,subprocess,time
from dev import BUILD,ROOT,run,tool
from d81 import extract_pc,prepare
from build_cartridge import build as cartridge
from prepare_freedos import FAT12
from test_1581 import drive_options
from vice_status import Monitor
def test():
    name='glabios-1581'; stem='vice-'+name; folder=BUILD/'1581-tests'/str(time.time_ns())
    folder.mkdir(parents=True,exist_ok=True)
    image=folder/'FD64-TEST.D81'; crt=folder/'FD64-TEST.CRT'
    prepare(image,autotest=True); cartridge(autotest=True,target=crt)
    pc=extract_pc(image.read_bytes()); native=folder/'native-test.img'; native.write_bytes(pc)
    output=run([BUILD/'glabios-boot.exe',BUILD/'glabios/GLABIOS.ROM',native,'full'],log=BUILD/(name+'-native.log'))
    if 'FD64_GLABIOS_FREEDOS_PASS' not in output: raise RuntimeError('Native DOS boot failed')
    for suffix in ('.prg','.lbl'): shutil.copyfile(BUILD/('fd64-1581-test'+suffix),BUILD/(stem+suffix))
    symbols=(BUILD/(stem+'.lbl')).read_text()
    def at(symbol): return int(re.search(r'al ([0-9A-Fa-f]+) \._'+symbol+r'\b',symbols).group(1),16)
    log=(BUILD/(name+'-vice.log')).open('w'); port=6512
    process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams','+sound',
        '-warp','-pal','-reu','-reusize','1024',*drive_options(image),'-cartcrt',str(crt),
        '-binarymonitor','-binarymonitoraddress',f'127.0.0.1:{port}',
        '-windowxpos','80','-windowypos','80','-windowwidth','1100','-windowheight','850'],
        cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    report={'status':'running','configuration':'PAL C64, 1MiB REU, Magic Desk emulator/BIOS, true 1581 IEC',
        'native_pass':True,'pid':process.pid,'monitor':port,'disk_image':str(image.relative_to(ROOT)),
        'cartridge':str(crt.relative_to(ROOT)),'unpacked_kernel':True,'started':time.time(),
        'artifacts':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (image,crt,BUILD/(stem+'.prg'),BUILD/(stem+'.lbl'))}}
    path=BUILD/(name+'-test-report.json'); path.write_text(json.dumps(report,indent=2)); previous=0
    print(f'Visible cartridge + true 1581 boot PID {process.pid}, monitor {port}',flush=True)
    try:
        while time.time()-report['started']<7200:
            time.sleep(15)
            if process.poll() is not None: raise RuntimeError('VICE exited before DOS pass')
            monitor=Monitor(port)
            try:
                cpu=monitor.memory(at('cpu'),at('cpu')+34)
                ins=struct.unpack_from('<I',cpu,31)[0]
                report.update(instructions=ins,pc=f'{struct.unpack_from("<H",cpu,18)[0]:04X}:{struct.unpack_from("<H",cpu,24)[0]:04X}',
                    post_complete=bool(monitor.memory(at('fd_xt_booted'),at('fd_xt_booted'))[0]),
                    reads=struct.unpack('<I',monitor.memory(at('fd_1581_reads'),at('fd_1581_reads')+3))[0],
                    writes=struct.unpack('<I',monitor.memory(at('fd_1581_writes'),at('fd_1581_writes')+3))[0],
                    drive_error=monitor.memory(at('fd_1581_error'),at('fd_1581_error'))[0])
                failed=monitor.memory(at('fd_test_fail'),at('fd_test_fail'))[0]
                if failed: raise RuntimeError(f'Guest CPU failure {failed}: {report}')
                if monitor.memory(at('fd_test_pass'),at('fd_test_pass'))[0]:
                    if not report['reads'] or not report['writes']: raise RuntimeError('No actual 1581 disk transfers')
                    report['c64_cycles_since_reset']=monitor.clock()
                    monitor.display(BUILD/(name+'-freedos-vice.png'))
                    snapshot=BUILD/(name+'-ready.vsf'); monitor.dump(snapshot)
                    report['snapshot']=str(snapshot.relative_to(ROOT)); report['guest_pass_marker']=True
                    break
            finally: monitor.close()
            path.write_text(json.dumps(report,indent=2))
            if ins//250000!=previous//250000:
                print(f'Guest instructions {ins}, PC {report["pc"]}, disk reads {report["reads"]}, writes {report["writes"]}',flush=True)
            previous=ins
        else: raise TimeoutError('Cartridge/1581 cold boot timed out')
        # Exit our test instance to flush drive track buffers before host verification.
        monitor=Monitor(port)
        try: monitor.command(0xbb)
        finally: monitor.socket.close()
        process.wait(timeout=20)
        final=image.read_bytes(); pc=extract_pc(final); files=FAT12(pc).files()
        if files.get('WRITE81.TXT')!=b'FD64 1581 WRITE OK\r\n':
            raise RuntimeError('DOS-created file was not persisted in the D81')
        report.update(status='passed',persistent_file='WRITE81.TXT',persistent_content=files['WRITE81.TXT'].decode('ascii'),
            final_disk_sha256=hashlib.sha256(final).hexdigest())
        print('FD64_CARTRIDGE_1581_DOS_PERSISTENCE_PASS',flush=True)
    except Exception as error: report.update(status='failed',error=str(error)); raise
    finally:
        report['elapsed_seconds']=round(time.time()-report['started'],1)
        path.write_text(json.dumps(report,indent=2)+'\n'); log.close()
if __name__=='__main__': test()
