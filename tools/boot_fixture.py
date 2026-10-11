"""A synthetic two-sector PC boot test, deliberately not a FreeDOS image."""
from pathlib import Path

EXPECTED = b"FD64 DISK BOOT OK"

def boot_sector():
    code = bytearray()
    labels, fixups = {}, []
    def emit(*values): code.extend(values)
    def label(name): labels[name] = len(code)
    def branch(op, target):
        emit(op, 0); fixups.append((len(code)-1, target))
    emit(0xea,5,0x7c,0,0)                          # normalize CS:IP by far jump
    emit(0x31,0xc0,0x8e,0xd8,0x8e,0xd0,0x8e,0xc0) # DS, SS, ES = 0
    emit(0xbc,0,0x7c,0xfc,0xbb,0,6)                 # SP=7C00, BX=0600
    emit(0xb8,1,2,0xb9,2,0,0xba,0x80,0,0xcd,0x13)  # read CHS 0/0/2
    branch(0x72,"error")
    emit(0xbe,0,6,0xb4,0x0e)
    label("print"); emit(0xac,0x84,0xc0); branch(0x74,"done")
    emit(0xcd,0x10); branch(0xeb,"print")
    label("done"); emit(0xf4)
    label("error"); emit(0xb0,0x45,0xb4,0x0e,0xcd,0x10,0xf4)
    for offset,target in fixups:
        distance=labels[target]-offset-1
        assert -128 <= distance <= 127
        code[offset]=distance & 255
    assert len(code) <= 510
    code.extend(bytes(510-len(code))); code.extend(b"\x55\xaa")
    return bytes(code)

def create(path, valid_signature=True):
    """Regenerate only disposable build fixtures, never the user's HDD.IMG."""
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    sector=bytearray(boot_sector())
    if not valid_signature: sector[510:]=b"\0\0"
    with path.open("wb") as stream:
        stream.write(sector); stream.write(EXPECTED+b"\0")
        stream.truncate(32*1024*1024)
