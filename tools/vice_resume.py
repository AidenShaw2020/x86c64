"""Resume an actually booted C64/FreeDOS VICE snapshot, without a new POST."""
import argparse,json,shutil,subprocess,time
from dev import BUILD,ROOT,tool,disk_options
from vice_status import Monitor

def resume(fast=False,accelerated=False,name=None):
    name=name or ('glabios-accelerated' if accelerated else ('glabios-fast' if fast else 'glabios'))
    if name not in ('glabios','glabios-fast','glabios-accelerated','glabios-pager','glabios-1581'):
        raise ValueError('Unknown boot test name')
    snapshot=BUILD/(name+'-ready.vsf')
    report=json.loads((BUILD/(name+'-test-report.json')).read_text())
    if report['status']!='passed' or not snapshot.is_file():
        raise RuntimeError('A passed VICE boot test and saved snapshot are required')
    source='vice-glabios-test' if name=='glabios' else 'vice-'+name
    shutil.copyfile(BUILD/(source+'.lbl'),BUILD/'vice-resumed.lbl')
    folder=BUILD/('freedos-unpacked' if report.get('unpacked_kernel') else 'freedos')
    if name=='glabios-1581':
        from test_1581 import drive_options
        image=ROOT/report['disk_image']; crt=ROOT/report['cartridge']
        if not image.is_file() or not crt.is_file(): raise RuntimeError('1581 snapshot medium/cartridge missing')
        options=[*drive_options(image),'-cartcrt',str(crt)]; reu_size='1024'
    else: options=disk_options(folder); reu_size='2048'
    log=(BUILD/'vice-resumed.log').open('w',encoding='utf-8')
    process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams',
        '+sound','-warp','-pal','-reu','-reusize',reu_size,
        '-binarymonitor','-binarymonitoraddress','127.0.0.1:6505',
        '-windowxpos','80','-windowypos','80','-windowwidth','1100','-windowheight','850',
        *options],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
        creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (BUILD/'vice-resumed.pid').write_text(str(process.pid))
    log.close()
    deadline=time.monotonic()+20
    while True:
        if process.poll() is not None: raise RuntimeError('VICE startup failed; see build/vice-resumed.log')
        try: monitor=Monitor(6505); break
        except ConnectionRefusedError:
            if time.monotonic()>=deadline: raise
            time.sleep(0.2)
    try:
        filename=str(snapshot).replace('\\','/').encode('utf-8')
        if len(filename)>255: raise ValueError('Snapshot filename exceeds monitor limit')
        monitor.command(0x42,bytes([len(filename)])+filename)
    finally: monitor.close()
    print(f'Resumed real saved C64/FreeDOS state in visible VICE, PID {process.pid}, monitor 6505')
    return process

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--fast',action='store_true')
    parser.add_argument('--accelerated',action='store_true')
    parser.add_argument('--name',choices=('glabios','glabios-fast','glabios-accelerated','glabios-pager','glabios-1581'))
    args=parser.parse_args(); resume(args.fast,args.accelerated,args.name)
