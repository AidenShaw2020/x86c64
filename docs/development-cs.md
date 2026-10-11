# FD64 — FreeDOS na C64

Výzkumný PC-XT emulátor: x86 instrukce vykonává **6510 v C64**.
PAL C64 + 2MiB REU ve VICE poskytuje 1MiB PC paměťové mapy a 720KiB
disketový obraz. Kung Fu Flash zatím není implementovaný backend.

Nová varianta přesouvá **DOS a jeho zápisy na 1581**, zatímco **BIOS
a emulátor jsou v cartridge**. Soubory `build/1581/FD64.CRT` a
`build/1581/FD64.D81` používají Magic Desk + 1MiB REU + zařízení 8.
Sektory se průběžně čtou/zapisují přes IEC; disk se nekopíruje do REU.
Spuštění: `python -B tools/dev.py run-1581`. Příprava a ověření:
[docs/1581.md](1581.md).
Úplný test resetu cartridge, POSTu, FreeDOSu a příkazů prošel. Soubor
vytvořený DOSem zůstal ověřený v D81 po vypnutí VICE; report je
`build/glabios-1581-test-report.json`.

Skutečný GLaBIOS od resetu ve VICE dokončí POST a zavede FreeDOS kernel
i FreeCom. Celý C64 běh 2026-10-11 prošel příkazy VER, DIR /B a ECHO přes
IRQ klávesnice; report build/glabios-test-report.json má status passed.
Původní komprimované médium potřebovalo 3 120 934 x86 instrukcí a přibližně
31,5 minuty v místním warp režimu. Čas závisí na PC i souběžných emulátorech.
**Nativní test sám o sobě není důkaz dokončeného bootu na C64.**

Nové jádro používá kritické cesty v assembleru 6510. Ve stejných měřených
smyčkách s paměťovou mapou PC dosahuje 2,11× pro paměť se zásobníkem,
2,40× pro aritmetiku a 4,40× pro registry. Jde o cykly C64, nikoli obecný
násobek pro celý DOS. Metodika a samostatné regrese: [docs/performance.md](performance.md).
Další assemblerová optimalizace pageru zrychlila paměťovou smyčku ještě
1,36×: celkem 2,87× proti původní C verzi. Ověření zahrnuje také chyby
backendu, dirty stránky, hranice, ROM a CGA zápisy přímo na C64.
Úplný první optimalizovaný boot rozbaleného média ve VICE prošel POSTem,
FreeDOSem i příkazy za 720,4 s (přibližně 12 minut), oproti dřívějším
1290,3 s. Uložený stav rychlejšího DOSu obnoví
`python -B tools/vice_resume.py --accelerated`.

Následující optimalizace pageru prošla celým bootem a VER/DIR/ECHO za
600,4 s (přibližně 10 minut): další místní zrychlení asi 1,20× proti
předchozím 12 minutám. Report je `build/glabios-pager-test-report.json`.
Aktuální rychlejší DOS obnoví `python -B tools/vice_resume.py --name glabios-pager`.

## Sestavení a spuštění

Python 3.10+, cc65, VICE 3.10, Zig/Clang pro nativní testy, JWasm 2.20 pro ROM.
Stažené nástroje jsou v tools/, generované soubory v build/.

    python -B tools/build_glabios.py
    python -B tools/prepare_freedos.py
    python -B tools/dev.py build
    python -B tools/dev.py test
    python -B tools/dev.py run-glabios

Příprava FreeDOS vyžaduje oficiální FD14-FloppyEdition.zip v build/downloads/
a kontroluje SHA256. Obraz obsahuje původní bootovací kód, KERNEL.SYS,
COMMAND.COM, minimální konfiguraci a AUTOEXEC.BAT.

Rychlejší testovací médium odstraní UPX kompresi kernelu předem, na PC.
Vykonávaný kód FreeDOSu zůstává původní; rozbalení je reprodukovatelné pomocí
fddepack a UPX 4.2.4. Původní komprimované médium zůstane zachované.

    python -B tools/unpack_kernel.py
    python -B tools/dev.py test-glabios-fast

Pro ruční spuštění stejného média slouží `python -B tools/dev.py run-glabios-fast`.

Tato varianta zapisuje samostatný build/glabios-fast-test-report.json.
Nativní boot s příkazy potřebuje přibližně 1,95 milionu instrukcí oproti
3,09 milionu u komprimovaného kernelu. Oba obrazy již prošly celým testem
ve VICE: rozbalený kernel 1 993 217 instrukcí / přibližně 21,5 minuty,
komprimovaný kernel 3 120 934 instrukcí / přibližně 31,5 minuty.
Jde o místní naměřené běhy; souběh a výkon PC ovlivňují čas.

run-glabios otevře viditelný VICE s warp režimem a nechá jej otevřený.
Start je velmi pomalý: jde o softwarovou emulaci x86 na 1MHz 6510.
F1 zobrazí levou polovinu PC obrazovky, F3 pravou, F5 všech 80 sloupců.
Výchozí pohled je všech 80 sloupců.
40sloupcový pohled zachovává původní 8×8 BIOSový font. CGA atributy převádí
na paletu C64. Plný 80sloupcový pohled musí font zúžit na 4 body a dvojice
znaků sdílí barvu jedné bitmapové buňky VIC-II.
Všech 25 PC řádků zůstává viditelných; ovládací lišta je nezakrývá.
Položky POST Video/FPU/COM/Size leží v pravé polovině: zobrazí je F3.

    python -B tools/dev.py test-glabios

Dlouhý test zavede FreeDOS, počká na shell a přes IRQ1 napíše VER, DIR /B
a ECHO. Kontroluje skutečnou odpověď v PC video paměti. Dokončený shell
nechá interaktivně otevřený. Ukončení okna před markerem znamená přerušení
testu. Pro screenshoty přes VICE monitor je potřeba Pillow.

Obnovení skutečně nabootovaného stroje bez čekání na nový POST:

    python -B tools/vice_resume.py

Použije build/glabios-ready.vsf, který obsahuje C64 RAM, CPU i 2MiB REU.
Vyžaduje úspěšný předchozí VICE test a otevře viditelné okno. Monitor má
port 6505; jeho mapování symbolů je v build/vice-resumed.lbl. Jde o obnovení
skutečného běhu, nikoli nový boot. Studený start run-glabios stále vykoná POST.

## Paměť a disk

- 00000–9FFFF: 640KiB konvenční RAM.
- B8000–BBFFF: 16KiB CGA video RAM.
- D0000–D07FF: naše disková option ROM.
- FE000–FFFFF: 8KiB GLaBIOS; zápisy do ROM se ignorují.
- Nepřipojené oblasti vracejí FF; adresování má 20 bitů.
- Druhý MiB REU obsahuje 720KiB diskety. Přenosy používají náhodný DMA
  přístup. Zápisy zatím žijí pouze v REU a nepersistují do souboru.

GLaBIOS se sestavuje z nezměněného upstream zdroje pomocí emulátorové
konfigurace. Dlouhý test RAM a testy neexistujícího fyzického FDC jsou vypnuté.
Disk poskytuje vykonaná ISA option ROM a virtuální blokový řadič.
Systémové služby obrazovky a klávesnice vykonává skutečná x86 ROM.

Starší fd64.prg / fd64-test.prg jsou rychlé testy CPU, REU a syntetického
bootu s 1MiB REU. fd64-dos.prg používá hostitelské BIOSové služby a nemá
GLaBIOS POST. Prázdný 32MiB HDD.IMG z dev.py images ještě není bootovatelný.

MS-DOS může být další systém pro stejný emulátor PC/XT. Zveřejněný
balík Microsoft MS-DOS 2.0 obsahuje DOS a nástroje, ale ne kompletní
bootovací obraz s IBM PC IO.SYS. Jeho boot zatím nebyl implementován ani
otestován; podrobnosti v docs/msdos.md.

Podrobnosti: [architektura](architecture.md), [testování](testing.md),
[GLaBIOS](glabios.md), [další práce](roadmap.md), [zdroje](sources.md).

## Licence

Nový kód: GPL-2.0-or-later, viz LICENSE. GLaBIOS: GPL-3.0, zdroj a licence
ve vendor/glabios. FreeDOS kernel a FreeCom mají vlastní GPL licence.
Generované obrazy se neukládají do zdrojového repozitáře. Hotový POC a odpovídající zdroje jsou publikované jako release; přehled licencí je v ../THIRD_PARTY_NOTICES.md. vendor/fake86
je zachovaná reference a není součástí sestaveného programu.
