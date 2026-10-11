"""Verify and preserve the pinned GLaBIOS reference ROM and its source."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import zipfile

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/"vendor"/"glabios"
REVISION="b98b0fe396466fa41466a9521f40987c989ae077"
ROM_SHA256="c2bc9ec1893c0ea4098433c862b97e546c4877c6a21c5bc23792ea2b6639c7d4"

def prepare():
    rom=DEST/"GLABIOS_0.4.2_8X.ROM"
    data=rom.read_bytes()
    if len(data)!=8192 or hashlib.sha256(data).hexdigest()!=ROM_SHA256:
        raise RuntimeError("GLaBIOS ROM does not match the upstream release digest")
    archive=ROOT/"build"/"downloads"/"glabios-source.zip"
    prefix="GLaBIOS-"+REVISION
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            parts=PurePosixPath(item.filename).parts
            if not parts or parts[0]!=prefix or ".." in parts:
                raise RuntimeError("Unexpected source archive path")
            if len(parts)<2 or item.is_dir(): continue
            if parts[1] not in ("src","LICENSE","README.md"): continue
            target=DEST.joinpath(*parts[1:])
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(bundle.read(item))
    manifest={"project":"GLaBIOS","tag":"v0.4.2","revision":REVISION,
        "rom":rom.name,"rom_sha256":ROM_SHA256,"bytes":len(data),
        "mapped_base":"0xFE000","reset_address":"0xFFFF0",
        "reset_bytes":data[-16:].hex(),"license":"GPL-3.0-or-later; font CC-BY-SA-4.0",
        "upstream":"https://github.com/640-KB/GLaBIOS",
        "status":"Unmodified upstream reference ROM for native regression tests. The C64 production cartridge contains the separately generated ARCH_EMU/CPU_8088 ROM; POST and FreeDOS boot passed in VICE."}
    (DEST/"UPSTREAM.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print("GLaBIOS XT 8088 ROM verified; matching source and license preserved.")
    print("Reset bytes:",manifest["reset_bytes"])

if __name__=="__main__": prepare()
