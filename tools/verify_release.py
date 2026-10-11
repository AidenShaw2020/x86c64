# SPDX-License-Identifier: GPL-2.0-or-later
"""Cold boot the production CRT/D81 in visible VICE; capture real VIC output."""
import argparse, hashlib, json, re, shutil, struct, subprocess, time
from dev import BUILD, ROOT, tool
from test_1581 import drive_options
from vice_status import Monitor

def verify(attach=False):
    from PIL import Image
    path=BUILD/'release-verification-report.json'
    if attach:
        report=json.loads(path.read_text())
        if report['status']!='running' or not report.get('dos_ready'):
            raise ValueError('Only reconnect to an already booted running production check')
        folder=ROOT/report['folder']
    else:
        folder=BUILD/'release-verification'/str(time.time_ns())
        folder.mkdir(parents=True)
        for name in ('FD64.CRT','FD64.D81'):
            shutil.copyfile(BUILD/'1581'/name,folder/name)
    symbols=(BUILD/'fd64-1581.lbl').read_text()
    def at(name):
        return int(re.search(r'al ([0-9A-Fa-f]+) \._'+name+r'\b',symbols).group(1),16)
    port=6514
    started=time.monotonic()
    process=None
    if not attach:
        log=(folder/'vice.log').open('w')
        process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams',
            '+sound','-warp','-pal','-reu','-reusize','1024',*drive_options(folder/'FD64.D81'),
            '-cartcrt',str(folder/'FD64.CRT'),'-binarymonitor','-binarymonitoraddress',f'127.0.0.1:{port}',
            '-windowxpos','80','-windowypos','80','-windowwidth','1100','-windowheight','850'],
            cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
            creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        log.close()
        report={'status':'running','configuration':'PAL C64, 1MiB REU, production Magic Desk CRT, true 1581, VICE warp',
            'artifacts':{name:hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in ('FD64.CRT','FD64.D81')},
            'pid':process.pid,'monitor':port,'folder':str(folder.relative_to(ROOT))}
    pictures=ROOT/'docs/screenshots'; pictures.mkdir(exist_ok=True)
    # PC Caps Lock defaults off in production. C64 letters currently send
    # unshifted XT scan codes, so the genuine BIOS produces lower-case text.
    responses=('x86c64 poc ok','X86C64 POC OK')
    if attach:
        monitor=Monitor(port)
        try: font=monitor.memory(0xb400,0xb7ff)
        finally: monitor.close()
        report['verification_reconnected']=True
    feed=b'ECHO X86C64 POC OK\r'
    feed_position=0
    try:
        while time.monotonic()-started<7200:
            time.sleep(2)
            if process is not None and process.poll() is not None: raise RuntimeError('Production VICE exited')
            # Labels refer to C variables, initially arbitrary power-on RAM.
            # Allow the cartridge loader and C runtime to initialize them first.
            if not attach and time.monotonic()-started<10: continue
            try: monitor=Monitor(port)
            except ConnectionRefusedError: continue
            try:
                failed=monitor.memory(at('fd_test_fail'),at('fd_test_fail'))[0]
                if failed: raise RuntimeError(f'Production CPU failure {failed}')
                if not report.get('post_complete') and monitor.memory(at('fd_xt_booted'),at('fd_xt_booted'))[0]:
                    monitor.display(pictures/'bios-post.png')
                    report.update(post_complete=True,post_seconds=round(time.monotonic()-started,1))
                    print('Actual production BIOS POST captured',flush=True)
                if not report.get('dos_ready') and monitor.memory(at('fd_video_ready'),at('fd_video_ready'))[0]:
                    monitor.display(pictures/'freedos-ready.png')
                    report.update(dos_ready=True,ready_marker_seconds=round(time.monotonic()-started,1))
                    font=monitor.memory(0xb400,0xb7ff)
                    # Pace C64 input so the slower XT BIOS buffer cannot overflow.
                    report['command']='ECHO X86C64 POC OK'
                    print('Production DOS ready; entering a fresh ECHO',flush=True)
                if report.get('dos_ready'):
                    if not attach and feed_position<len(feed):
                        monitor.command(0x72,b'\1'+feed[feed_position:feed_position+1])
                        feed_position+=1
                    monitor.display(pictures/'freedos.png')
                    with Image.open(pictures/'freedos.png') as picture:
                        pixels=picture.convert('RGB')
                        found=next(((row,response) for response in responses for row in range(25) if all(
                            (pixels.getpixel((col*4+bit,row*8+line))!=(0,0,0))==bool(font[ord(ch)*8+line]&(8>>bit))
                            for col,ch in enumerate(response) for line in range(8) for bit in range(4))),None)
                    if found is not None:
                        report.update(status='passed',response=found[1],response_row=found[0])
                        report['reconnected_verification_seconds' if attach else 'elapsed_seconds']=round(time.monotonic()-started,1)
                        monitor.dump(folder/'ready.vsf')
                        print('X86C64_PRODUCTION_COLD_BOOT_PASS',flush=True)
                        break
            finally: monitor.close()
            path.write_text(json.dumps(report,indent=2)+'\n')
        else: raise TimeoutError('Production boot timed out')
    except Exception as error:
        report.update(status='failed',error=str(error)); raise
    finally: path.write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--attach',action='store_true',help='Reconnect to the running, already booted production check')
    verify(parser.parse_args().attach)
