"""Build and test only this workspace; never flashes hardware."""
from pathlib import Path
import os
import sys
import json
import subprocess
import tempfile
import argparse
import re
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build"
sys.dont_write_bytecode=True
def tool(name,folder):
    root=ROOT/"tools"/folder
    candidates=[root/name,*root.glob("*/"+name),root/"bin"/name,*root.glob("*/bin/"+name)]
    for p in candidates:
        if p.is_file(): return str(p)
    import shutil
    found=shutil.which(name)
    if found: return found
    raise RuntimeError(f"Missing {name}; install in tools/{folder} or PATH")
def run(args,timeout=180,log=None):
    result=subprocess.run([str(a) for a in args],cwd=ROOT,capture_output=True,
        text=True,errors="replace",timeout=timeout,
        creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    output=result.stdout+result.stderr
    if log: Path(log).write_text(output,encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {args[0]}\n{output[-8000:]}")
    return output
def build():
    BUILD.mkdir(exist_ok=True)
    cc=tool("cl65.exe","cc65") if os.name=="nt" else tool("cl65","cc65")
    shared=[]
    for name in ("x86","pager","demo","bios","disk"):
        obj=BUILD/(name+".o")
        flags=['-Cl'] if name in ('x86','pager') else []
        run([cc,"-t","c64","-O",*flags,"-I","src/core","-c","-o",obj,"src/core/"+name+".c"])
        shared.append(obj)
    for name in ('fetch','alu','fast','address','pager-fast'):
        obj=BUILD/(name+'.o')
        source='pager' if name=='pager-fast' else name
        run([cc,'-t','c64','-c','-o',obj,'src/c64/'+source+'.s']); shared.append(obj)
    reu=BUILD/"reu.o"
    run([cc,"-t","c64","-O","-c","-o",reu,"src/c64/reu.c"])
    disk=BUILD/"c64-disk.o"
    run([cc,"-t","c64","-O","-c","-o",disk,"src/c64/disk.c"])
    for autotest in (False,True):
        stem="fd64-test" if autotest else "fd64"
        obj=BUILD/(stem+".o")
        args=[cc,"-t","c64","-O","-c","-o",obj]
        if autotest: args+=["-D","FD_AUTOTEST"]
        run(args+["src/c64/main.c"])
        run([cc,"-t","c64","-o",BUILD/(stem+".prg"),"-m",BUILD/(stem+".map"),obj,reu,disk,*shared])
    pc=BUILD/"pc.o"
    run([cc,"-t","c64","-O","-c","-o",pc,"src/core/pc.c"])
    for autotest in (False,True):
        stem="fd64-dos-test" if autotest else "fd64-dos"
        obj=BUILD/(stem+".o"); args=[cc,"-t","c64","-O","-c","-o",obj]
        if autotest: args += ["-D","FD_AUTOTEST"]
        run(args+["src/c64/dos.c"])
        run([cc,"-t","c64","-o",BUILD/(stem+".prg"),"-m",BUILD/(stem+".map"),
            "-Ln",BUILD/(stem+".lbl"),obj,reu,pc,*shared])
    extras=[]
    for source in ("src/core/xt.c","src/c64/video.c"):
        obj=BUILD/(Path(source).stem+".o")
        run([cc,"-t","c64","-O","-c","-o",obj,source]); extras.append(obj)
    for autotest in (False,True):
        stem="fd64-glabios-test" if autotest else "fd64-glabios"
        obj=BUILD/(stem+".o"); args=[cc,"-t","c64","-O","-c","-o",obj]
        if autotest: args += ["-D","FD_AUTOTEST"]
        run(args+["src/c64/glabios.c"])
        run([cc,"-t","c64","-o",BUILD/(stem+".prg"),"-m",BUILD/(stem+".map"),
            "-Ln",BUILD/(stem+".lbl"),obj,*extras,reu,*shared])
        match=re.search(r'^BSS\s+[0-9A-F]+\s+([0-9A-F]+)',(BUILD/(stem+'.map')).read_text(),re.M)
        if not match or int(match.group(1),16)>=0xb000:
            raise RuntimeError('C64 BSS overlaps font area B000-B7FF')
    drive=BUILD/'drive1581.o'
    run([cc,'-t','c64','-O','-c','-o',drive,'src/c64/drive1581.c'])
    from build_cartridge import bios_blob
    bios_blob()
    cart=BUILD/'cart-bios.o'; blob=BUILD/'cart-blob.o'
    run([cc,'-t','c64','-O','-c','-o',cart,'src/c64/cart_bios.c'])
    run([cc,'-t','c64','-c','-o',blob,'src/c64/cart_blob.s'])
    for autotest in (False,True):
        stem='fd64-1581-test' if autotest else 'fd64-1581'
        obj=BUILD/(stem+'.o'); args=[cc,'-t','c64','-O','-D','FD_DISK_1581']
        if autotest: args+=['-D','FD_AUTOTEST']
        run([*args,'-c','-o',obj,'src/c64/glabios.c'])
        run([cc,'-t','c64','-o',BUILD/(stem+'.prg'),'-m',BUILD/(stem+'.map'),
            '-Ln',BUILD/(stem+'.lbl'),obj,drive,cart,blob,*extras,reu,*shared])
        match=re.search(r'^BSS\s+[0-9A-F]+\s+([0-9A-F]+)',(BUILD/(stem+'.map')).read_text(),re.M)
        if not match or int(match.group(1),16)>=0xb000:
            raise RuntimeError('1581 build BSS overlaps font area B000-B7FF')
    zig=tool("zig.exe","zig") if os.name=="nt" else tool("zig","zig")
    run([zig,"cc","-std=c99","-O2","-Wall","-Wextra","-I","src/core",
        "tests/native.c","src/core/x86.c","src/core/pager.c","src/core/demo.c",
        "src/core/disk.c","src/core/bios.c","-o",BUILD/("native-tests.exe" if os.name=="nt" else "native-tests")],
        log=BUILD/"native-build.log")
    run([zig,"cc","-std=c99","-O2","-Wall","-Wextra","-I","src/core",
        "tests/pc_boot.c","src/core/x86.c","src/core/pager.c","src/core/bios.c",
        "src/core/disk.c","src/core/pc.c","-o",BUILD/"pc-boot.exe"],log=BUILD/"pc-boot-build.log")
    run([zig,"cc","-std=c99","-O2","-Wall","-Wextra","-I","src/core",
        "tests/glabios_boot.c","src/core/x86.c","src/core/pager.c","src/core/bios.c",
        "src/core/xt.c","-o",BUILD/"glabios-boot.exe"],log=BUILD/"glabios-build.log")
    from boot_fixture import create
    create(BUILD/"fixtures"/"XTBOOT.IMG")
    create(BUILD/"fixtures-bad-signature"/"XTBOOT.IMG",False)
    (BUILD/"fixtures-missing").mkdir(exist_ok=True)
    print("Built C64 BIOS, disk boot fixture, VICE and native tests.")

def disk_options(folder):
    return ["+drive8truedrive","-trapdevice8","-busdevice8","-devicebackend8","1",
        "-fs8",str(folder),"+fs8convertp00","+fs8hidecbm"]

def vice_case(name,reu,cycles,expect_pass,headless=False,folder=None,expected_exit=None):
    vice=tool("x64sc.exe","vice") if os.name=="nt" else tool("x64sc","vice")
    args=[vice,"-default","-no-redirect-streams","-warp","+sound",
        "-logfile",str(BUILD/(name+"-internal.log")),
        "-pal","-debugcart","-limitcycles",str(cycles),"-autostartprgmode","1"]
    args+=(["-reu","-reusize","1024"] if reu else ["+reu"])
    if headless: args += ["-console"]
    args+=disk_options(folder or BUILD/"fixtures")
    args+=["-exitscreenshot",str(BUILD/(name+".png")),"-autostart",str(BUILD/"fd64-test.prg")]
    result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,errors="replace",
        timeout=180,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    output=result.stdout+result.stderr
    (BUILD/(name+".log")).write_text(output,encoding="utf-8")
    marker="DBGCART: exit(0)" in output
    passed=result.returncode==0 and marker
    if passed!=expect_pass:
        raise RuntimeError(f"VICE {name}: exit={result.returncode}, marker={marker}\n{output[-8000:]}")
    if name=="vice-no-reu" and result.returncode!=1:
        raise RuntimeError(f"Expected REU failure exit 1, got {result.returncode}")
    if expected_exit is not None and (result.returncode!=expected_exit or
            f"DBGCART: exit({expected_exit})" not in output):
        raise RuntimeError(f"Expected BIOS failure {expected_exit}, got {result.returncode}\n{output[-4000:]}")
    print(f"{name}: expected outcome confirmed (exit {result.returncode}, marker {marker})")
    cycles_match=re.search(r"DBGCART: exit\(\d+\) cycles elapsed: (\d+)",output)
    return {"name":name,"exit":result.returncode,"pass_marker":marker,"expected_pass":expect_pass,
        "c64_cycles":int(cycles_match.group(1)) if cycles_match else None}
def test(headless=False):
    report={"status":"running","configuration":"PAL C64, 1MiB REU, x86 runs on 6510","cases":[]}
    BUILD.mkdir(exist_ok=True)
    try:
        out=run([sys.executable,"-B","-m","unittest","discover","-s","tests","-p","test_*.py"],
            log=BUILD/"python-tests.log"); print(out.strip())
        report["cases"].append({"name":"python-images","output":out.strip()})
        exe=BUILD/("native-tests.exe" if os.name=="nt" else "native-tests")
        with tempfile.TemporaryDirectory(prefix="fd64-",dir=BUILD) as d:
            out=run([exe,d,BUILD/"fixtures"/"XTBOOT.IMG",
                ROOT/"vendor"/"glabios"/"GLABIOS_0.4.2_8X.ROM"],
                log=BUILD/"native-tests.log"); print(out.strip())
            if "FD64_NATIVE_PASS" not in out: raise RuntimeError("Native pass marker missing")
        report["cases"].append({"name":"native","output":out.strip()})
        report["cases"].append(vice_case("vice-reu",True,100000000,True,headless))
        report["cases"].append(vice_case("vice-no-reu",False,20000000,False,headless))
        report["cases"].append(vice_case("vice-cycle-limit",True,100000,False,headless))
        report["cases"].append(vice_case("vice-missing-disk",True,100000000,False,headless,
            BUILD/"fixtures-missing",5))
        report["cases"].append(vice_case("vice-bad-signature",True,100000000,False,headless,
            BUILD/"fixtures-bad-signature",6))
        report["status"]="passed"
    except Exception as e:
        report["status"]="failed"; report["error"]=str(e); raise
    finally:
        (BUILD/"test-report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
def images():
    import importlib.util
    spec=importlib.util.spec_from_file_location("images",ROOT/"tools/images.py")
    module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    path=BUILD/"sd"/"FD64"; path.mkdir(parents=True,exist_ok=True)
    for name,function in (("PC.RAM",module.create_ram),("HDD.IMG",module.create_disk)):
        target=path/name
        if target.exists(): print(f"Preserved existing {target}"); continue
        function(target); print(f"Created {target}")
def launch(glabios=False,fast=False):
    vice=tool("x64sc.exe","vice") if os.name=="nt" else tool("x64sc","vice")
    stem="fd64-glabios" if glabios else "fd64-dos"
    prg=BUILD/(stem+".prg")
    if not prg.is_file(): raise RuntimeError("Run python tools/dev.py build first")
    folder=BUILD/('freedos-unpacked' if fast else 'freedos')
    if not (folder/'FREEDOS.IMG').is_file():
        raise RuntimeError('Prepare the boot medium with tools/'+('unpack_kernel.py' if fast else 'prepare_freedos.py'))
    active='vice-active-fast' if fast else 'vice-active'
    port=6504 if fast else 6503
    if glabios:
        import shutil
        shutil.copyfile(BUILD/"glabios"/"GLABIOS.ROM",folder/"GLABIOS.ROM")
    import shutil
    shutil.copyfile(BUILD/(stem+".lbl"),BUILD/(active+'.lbl'))
    shutil.copyfile(prg,BUILD/(active+'.prg'))
    process=subprocess.Popen([vice,"-default","+sound","-warp","-pal","-reu","-reusize","2048",
        "-binarymonitor","-binarymonitoraddress",f"127.0.0.1:{port}",
        "-windowxpos","80","-windowypos","80","-windowwidth","1100","-windowheight","850",
        *disk_options(folder),"-logfile",str(BUILD/(active+'.log')),
        "-autostartprgmode","1","-autostart",str(BUILD/(active+'.prg'))],
        cwd=ROOT,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    (BUILD/(active+'.pid')).write_text(str(process.pid),encoding="ascii")
    print(f"Started visible VICE (PID {process.pid}): {stem}, C64 + 2MB REU, warp on, monitor {port}")

def prepare_1581():
    from build_cartridge import build as cartridge
    from d81 import prepare
    cartridge()
    image=BUILD/'1581/FD64.D81'
    if image.exists(): print(f'Preserved DOS disk and its writes: {image}')
    else: prepare(image)

def launch_1581():
    from test_1581 import drive_options
    folder=BUILD/'1581'; crt=folder/'FD64.CRT'; image=folder/'FD64.D81'
    if not crt.is_file() or not image.is_file(): raise RuntimeError('Run prepare-1581 first')
    import shutil
    shutil.copyfile(BUILD/'fd64-1581.lbl',BUILD/'vice-active-1581.lbl')
    log=(BUILD/'vice-active-1581.log').open('w')
    process=subprocess.Popen([tool('x64sc.exe','vice'),'-default','-no-redirect-streams',
        '+sound','-warp','-pal','-reu','-reusize','1024',*drive_options(image),'-cartcrt',str(crt),
        '-binarymonitor','-binarymonitoraddress','127.0.0.1:6513',
        '-windowxpos','80','-windowypos','80','-windowwidth','1100','-windowheight','850'],
        cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (BUILD/'vice-active-1581.pid').write_text(str(process.pid)); log.close()
    print(f'Started visible cartridge + true 1581, PID {process.pid}, 1MiB REU, monitor 6513')
def test_dos():
    output=run([BUILD/"pc-boot.exe",BUILD/"freedos"/"FREEDOS.IMG"],log=BUILD/"freedos-native.log")
    print(output,flush=True)
    if "FD64_FREEDOS_BOOT_PASS" not in output: raise RuntimeError("FreeDOS native marker missing")
    vice=tool("x64sc.exe","vice")
    args=[vice,"-default","-no-redirect-streams","-warp","+sound","-pal",
        "-binarymonitor","-binarymonitoraddress","127.0.0.1:6502",
        "-windowxpos","80","-windowypos","80","-reu","-reusize","2048",
        *disk_options(BUILD/"freedos"),"-logfile",str(BUILD/"freedos-vice-internal.log"),
        "-debugcart","-limitcycles","50000000000","-autostartprgmode","1",
        "-exitscreenshot",str(BUILD/"freedos-vice.png"),"-autostart",str(BUILD/"fd64-dos-test.prg")]
    print("Starting visible VICE FreeDOS integration test...",flush=True)
    with (BUILD/"freedos-vice.log").open("w",encoding="utf-8") as log:
        result=subprocess.run(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
            timeout=7200,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    output=(BUILD/"freedos-vice.log").read_text(encoding="utf-8",errors="replace")
    passed=result.returncode==0 and "DBGCART: exit(0)" in output
    (BUILD/"freedos-test-report.json").write_text(json.dumps({"passed":passed,
        "exit":result.returncode,"configuration":"C64 PAL, 2MiB REU, 8086 on 6510, 720KiB floppy",
        "native_output":(BUILD/"freedos-native.log").read_text(),
        "vice_cycles":re.findall(r"cycles elapsed: (\d+)",output)},indent=2))
    if not passed: raise RuntimeError("FreeDOS VICE boot failed: "+output[-2000:])
    print("FD64_FREEDOS_VICE_PASS",flush=True)
if __name__=="__main__":
    parser=argparse.ArgumentParser(); parser.add_argument("command",choices=("build","test","test-c64-cpu","benchmark","test-dos","test-glabios","test-glabios-fast","images","run","run-glabios","run-glabios-fast","prepare-1581","run-1581","test-1581","test-1581-sectors"))
    parser.add_argument("--headless",action="store_true",help="Run VICE tests without windows (CI only)")
    args=parser.parse_args()
    if args.command=="test": test(args.headless)
    elif args.command=='test-c64-cpu':
        from cpu_check import test as cpu_check
        cpu_check()
    elif args.command=='benchmark':
        from benchmark import benchmark
        benchmark('current',('-O','-Cl'),True)
    elif args.command in ("test-glabios","test-glabios-fast"):
        from vice_glabios_test import test as test_glabios
        test_glabios(args.command=='test-glabios-fast')
    elif args.command=='test-1581':
        from vice_1581_test import test as test_1581
        test_1581()
    elif args.command=='test-1581-sectors':
        from test_1581 import probe
        probe()
    else: {"build":build,"images":images,"run":launch,"run-glabios":lambda:launch(True),
        "run-glabios-fast":lambda:launch(True,True),"test-dos":test_dos,
        "prepare-1581":prepare_1581,"run-1581":launch_1581}[args.command]()
