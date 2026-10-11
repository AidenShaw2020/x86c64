"""Restore passed optimized VICE state and verify a NEW DOS ECHO in real pixels."""
import argparse,hashlib,json,time
from dev import BUILD
from vice_resume import resume
from vice_status import Monitor

def verify(name='glabios-accelerated'):
    from PIL import Image
    snapshot=BUILD/(name+'-ready.vsf')
    # This response is absent from the cold-boot test and its snapshot.
    suffix=name.removeprefix('glabios-')
    response='FD64 '+suffix.upper()+' OK'; command='ECHO '+response+'\r'
    process=resume(name=name)
    report={'status':'running','test':'New DOS response after '+name+' snapshot restore',
        'snapshot_sha256':hashlib.sha256(snapshot.read_bytes()).hexdigest(),'response':response,
        'pid':process.pid,'monitor':6505}
    path=BUILD/(suffix+'-resume-report.json'); path.write_text(json.dumps(report,indent=2))
    started=time.monotonic()
    try:
        monitor=Monitor(6505)
        try:
            # Font cache consists of the actual GLaBIOS font reduced to 4 pixels.
            font=monitor.memory(0xb400,0xb7ff)
            data=command.encode('ascii'); monitor.command(0x72,bytes([len(data)])+data)
        finally: monitor.close()
        while time.monotonic()-started<180:
            time.sleep(2)
            if process.poll() is not None: raise RuntimeError('Restored VICE exited')
            screenshot=BUILD/(name+'-resumed.png')
            monitor=Monitor(6505)
            try: monitor.display(screenshot)
            finally: monitor.close()
            with Image.open(screenshot) as picture:
                pixels=picture.convert('RGB'); found=None
                for row in range(25):
                    matches=True
                    for column,ch in enumerate(response):
                        for line in range(8):
                            bits=font[ord(ch)*8+line]
                            for bit in range(4):
                                visible=pixels.getpixel((column*4+bit,row*8+line))!=(0,0,0)
                                if visible!=bool(bits&(8>>bit)): matches=False; break
                            if not matches: break
                        if not matches: break
                    if matches: found=row; break
            if found is not None:
                report.update(status='passed',response_row=found,screenshot=str(screenshot.relative_to(BUILD.parent)),
                    elapsed_seconds=round(time.monotonic()-started,1))
                print('FD64_'+suffix.upper()+'_SNAPSHOT_RESPONSE_PASS',flush=True); return report
        raise RuntimeError('No new DOS response in the actual VIC-II pixels')
    except Exception as error:
        report.update(status='failed',error=str(error)); raise
    finally: path.write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--name',choices=('glabios-accelerated','glabios-pager','glabios-1581'),default='glabios-accelerated')
    verify(parser.parse_args().name)
