# SPDX-License-Identifier: GPL-2.0-or-later
"""Fetch exact corresponding sources; fail on a changed upstream digest."""
import hashlib, json, urllib.request, zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'build/release-inputs'

def digest(data): return hashlib.sha256(data).hexdigest()

def fetch():
    manifest=json.loads((ROOT/'docs/release-inputs.json').read_text())
    DEST.mkdir(parents=True,exist_ok=True)
    for item in manifest['files']:
        path=DEST/item['name']
        if not path.exists():
            request=urllib.request.Request(item['url'],headers={'User-Agent':'x86c64-release'})
            with urllib.request.urlopen(request,timeout=60) as response:
                data=response.read()
            if digest(data)!=item['sha256']:
                raise ValueError('Upstream hash mismatch: '+item['name'])
            path.write_bytes(data)
        if digest(path.read_bytes())!=item['sha256']:
            raise ValueError('Cached source hash mismatch: '+item['name'])
        print('Verified',item['name'])
    derived=manifest['derived_freecom_source']
    with zipfile.ZipFile(DEST/'freecom-086-package.zip') as archive:
        source=archive.read(derived['member'])
        if digest(archive.read(manifest['freecom_binary']['member']))!=manifest['freecom_binary']['sha256']:
            raise ValueError('FreeCom binary provenance mismatch')
    if digest(source)!=derived['sha256']: raise ValueError('FreeCom source mismatch')
    (DEST/derived['name']).write_bytes(source)
    print('Verified exact FreeCom source and COMMAND.COM provenance')
    return manifest

if __name__=='__main__': fetch()
