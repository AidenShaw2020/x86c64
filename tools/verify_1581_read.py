"""Read the DOS-created persistent file after a real 1581 snapshot restore."""
import json,re,struct,time
from dev import BUILD
from vice_status import Monitor
def verify():
    from PIL import Image
    labels=(BUILD/'vice-resumed.lbl').read_text()
    address=int(re.search(r'al ([0-9A-Fa-f]+) \._fd_1581_reads\b',labels).group(1),16)
    m=Monitor(6505)
    try:
        font=m.memory(0xb400,0xb7ff); before=struct.unpack('<I',m.memory(address,address+3))[0]
        command=b'TYPE WRITE81.TXT\r'; m.command(0x72,bytes([len(command)])+command)
    finally: m.close()
    started=time.monotonic(); response='FD64 1581 WRITE OK'
    path=BUILD/'1581-file-read-report.json'; screenshot=BUILD/'glabios-1581-file-read.png'
    report={'status':'running','command':'TYPE WRITE81.TXT','reads_before':before}
    try:
        while time.monotonic()-started<180:
            time.sleep(2); m=Monitor(6505)
            try: m.display(screenshot); count=struct.unpack('<I',m.memory(address,address+3))[0]
            finally: m.close()
            with Image.open(screenshot) as picture:
                pixels=picture.convert('RGB')
                match=all((pixels.getpixel((col*4+bit,23*8+line))!=(0,0,0))==bool(font[ord(ch)*8+line]&(8>>bit))
                    for col,ch in enumerate(response) for line in range(8) for bit in range(4))
            if match and count>before:
                report.update(status='passed',reads_after=count,response_row=23,
                    elapsed_seconds=round(time.monotonic()-started,1)); print('FD64_1581_RESTORED_FILE_READ_PASS'); return
        raise TimeoutError('No fresh TYPE response from 1581 on row 23')
    except Exception as error: report.update(status='failed',error=str(error)); raise
    finally: path.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__': verify()
