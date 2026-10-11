# Testování

Stejné C jádro se překládá pro PC i pro 6510. Nativní test nenahrazuje VICE.
C64 sestavení navíc používá assemblerové optimalizace. Samostatný
`python -B tools/dev.py test-c64-cpu` ověří tyto cesty proti 3 072
nezávislým aritmetickým vektorům a testům skoků, adres a invalidace cache.
Měření cyklů a jeho omezení popisuje performance.md.

## Rychlé regrese

    python -B tools/dev.py build
    python -B tools/dev.py test

Python ověřuje obrazy FAT16/MBR a zachování existujících souborů.
Nativní testy ověřují CPU, flags, pager, ochranu ROM, diskové chyby, CHS,
DMA hranici, resetový vstup původní GLaBIOS release ROM a syntetický boot.
ADC/SBB mají vyčerpávající test všech byte operandů a vstupního carry.
MOVSW testy porovnávají výsledek s nezávislým wordovým modelem při
překrývání, lichých adresách, přechodech stránek a wrapu SI přes FFFF.
Resetový probe release ROM končí u IN/OUT bez zařízení; nepočítá se jako POST.

VICE smoke testy ověřují pozitivní REU boot, chybějící REU, cyklový limit,
chybějící disk a špatný podpis. Úspěch vyžaduje DBGCART: exit(0) marker;
normální zavření VICE nestačí. Výsledek je v build/test-report.json.

## Skutečný GLaBIOS a FreeDOS

    python -B tools/build_glabios.py
    python -B tools/prepare_freedos.py
    python -B tools/dev.py build
    python -B tools/dev.py test-glabios

Nativní test spustí reset ROM a celý FreeDOS boot, zadá VER, DIR /B a ECHO.
C64 test nezávisle spustí viditelný VICE s 2MiB REU a od resetu vykoná stejný
boot. Automatické klávesy se posílají přes IRQ1 teprve po skutečném startu
shellu, s kontrolou předchozího vstupu v BIOSové klávesové frontě.

C64 guest marker se nastaví až po zobrazení souborů, AUTOEXEC markeru a
samostatné odpovědi ECHO. Obsah napsaného příkazu není odpověď. Monitor
čte marker, CPU PC a počet instrukcí; neslouží k naplnění paměti fake výstupem.
Report: build/glabios-test-report.json, status passed je jediný úspěch.
Status running znamená probíhající test, interrupted zavřené okno před
výsledkem, timeout vyčerpaný čas, failed chybu emulace. Po úspěchu se
přepne na ruční klávesnici a shell zůstane otevřený.

Běh může trvat dlouho. F1/F3/F5 přepínají pohled. Screenshot skutečného
VIC-II obrazu lze získat přes tools/vice_status.py (vyžaduje Pillow).
Příkaz run-glabios používá monitor 6503 a kopii vice-active.prg/lbl;
test-glabios samostatnou kopii vice-glabios-test.prg/lbl. Překlad dalších
souborů proto nezmění adresy právě spuštěného programu.

Varianta `python -B tools/dev.py test-glabios-fast` používá médium z
`python -B tools/unpack_kernel.py`, monitor 6504 a kopii vice-glabios-fast.
Report a logy mají prefix glabios-fast, takže nesmažou původní dlouhý test.
Kernel se před startem rozbalí na PC; ve VICE celý boot i DOSové příkazy
nadále vykonává 6510. Hash vstupu, výstupu, UPX a depackeru jsou zaznamenané.

Na skutečném C64/KFF nic ověřeno nebylo. Fyzický FDC/HDD ani perzistentní
zápis disku nejsou implementované. CPU a timer nejsou cyklově přesné.

## Dokončený C64 běh

2026-10-11: build/glabios-test-report.json má status passed,
post_complete a guest_pass_marker true. 3 120 934 x86 instrukcí, 1890,5 s
na místním PC ve warp režimu. Test zahrnoval GLaBIOS reset/POST, FreeDOS
kernel, FreeCom a odpověď na VER, DIR /B, ECHO přes IRQ1.
Skutečný screenshot: build/glabios-freedos-vice.png. Kopie spuštěného
PRG/symbolů zůstávají vice-glabios-test; hashe jsou v reportu a samostatném
glabios-run-artifacts.json. Nové překlady se mohou lišit například výchozím
pohledem; nepřepisují důkaz staršího běhu.

Snapshot build/glabios-ready.vsf vznikl z tohoto úspěšného VICE stroje,
se skutečnou REU, CPU a RAM. `python -B tools/vice_resume.py` jej obnoví
v novém viditelném okně přes dokumentované binary monitor Undump API.
Obnovení nenahrazuje ověření bootu od resetu. Testovací marker ve snapshotu
sám o sobě neprokazuje vykonání příkazů po obnovení.

Rozbalený kernel: build/glabios-fast-test-report.json má rovněž status
passed, 1 993 217 instrukcí a 1290,3 s. Jeho kopie PRG obsahuje optimalizaci
MOVSW; aktuální překlad navíc odstraňuje překryv horního řádku, nastavuje
80sloupcový pohled jako výchozí a zarovnává bitmapu v PAL obraze.
Zarovnání bylo ověřeno na živém zobrazovači přes registry VIC-II; žádná
PC paměť ani DOSový výstup se pro tuto vizuální kontrolu nenahrazovaly.

Obnovení bylo ověřeno novým ECHO FD64 RESUME OK přes skutečný C64
klávesnicový buffer. Samostatná odpověď se porovnala ve skutečných pixelech
VIC-II s vykreslením BIOSového fontu, bez spoléhání na uložený guest marker.
Report: build/vice-resumed-report.json; screenshot: build/glabios-resumed.png.

## Optimalizované jádro 6510

`build/glabios-accelerated-test-report.json`: status passed, skutečný POST,
FreeDOS/FreeCom, VER, DIR /B a ECHO; 2 030 020 instrukcí a 720,4 s.
Snapshot `build/glabios-accelerated-ready.vsf` a PRG/symboly
`vice-glabios-accelerated` patří přesně tomuto běhu. Hashe PRG, symbolů
a média jsou v reportu. Kontrola layoutu FDCPU nezměnila tyto binární soubory.

    python -B tools/vice_resume.py --accelerated

`tools/verify_snapshot.py` obnoví tento snapshot a pošle nový ECHO.
Úspěch vyžaduje jeho samostatnou odpověď v pixelech skutečného VIC-II obrazu;
uložený marker cold-boot testu nestačí. Report je accelerated-resume-report.json.

Další assemblerový pager má oddělený test:

    python -B tools/vice_glabios_test.py --fast --name glabios-pager --port 6510

Používá kopii `vice-glabios-pager.prg/lbl` a report
`build/glabios-pager-test-report.json`. Předchozí důkazy bootu nepřepisuje.
Samostatné C64 CPU testy kromě 3 072 aritmetických vektorů ověřují rychlé
byte/word přístupy, chyby backendu při načítání a vytěsnění, zachování dirty
obsahu po chybě, ROM ochranu, wrap busu a CGA dirty řádky.

Po úspěšném bootu lze nový snapshot obnovit a ověřit samostatným příkazem:

    python -B tools/verify_snapshot.py --name glabios-pager

Tento test píše nový `ECHO FD64 PAGER OK` a vyžaduje odpověď v pixelech
VIC-II. Report je `build/pager-resume-report.json`, obrázek
`build/glabios-pager-resumed.png`. Obnovený DOS nechá otevřený.

Studený start s tímto pagerem 2026-10-11 prošel: POST, FreeDOS/FreeCom,
VER/DIR/ECHO, 2 023 836 instrukcí, 9 860 314 458 cyklů C64 od resetu,
600,4 s na místním PC ve warp režimu. Report má status passed,
post_complete i guest_pass_marker true. Použité médium je stejné jako
v předchozím 720,4s běhu; přesné hashe artefaktů jsou v reportu.
`build/pager-resume-report.json` má rovněž status passed: nová odpověď
`FD64 PAGER OK` byla po obnovení ověřena ve skutečném obrazu VIC-II.

## Cartridge + 1581

`python -B tools/dev.py test-1581-sectors` používá skutečně emulovaný
procesor 1581 a IEC U1/U2. Čtení, zápisy přes přeskočenou adresářovou
stopu, konec disku, zavření/znovuotevření i hostitelské ověření přetrvávajícího
D81 prošly; report je `build/1581-check-report.json`.

`python -B tools/dev.py test-1581` začíná resetem CRT s emulátorem a BIOSem.
Používá 1MiB REU pouze pro PC RAM, skutečně emulovanou 1581 a vlastní
testovací D81. Kontroluje POST, FreeDOS/FreeCom, VER/DIR/ECHO a navíc
soubor vytvořený DOSem na disketě. Po zavření testovací instance kontroluje
FAT12 soubor v D81 na hostiteli. Report: `build/glabios-1581-test-report.json`.
Příprava nezapisuje do běžné uživatelské diskety `build/1581/FD64.D81`.
Původní testy s FSDevice a druhým MiB REU jsou zachované odděleně.
Cartridge/1581 běh 2026-10-11 prošel: 2 180 551 x86 instrukcí,
387 čtení, 5 zápisů, chyba mechaniky 0, 766,0 s v místním warp režimu.
WRITE81.TXT byl po vypnutí VICE ověřen v D81 s přesným očekávaným obsahem.
Obnovení prošlo novým ECHO ve skutečných pixelech VIC-II
(`build/1581-resume-report.json`). Po něm prošel ruční `TYPE WRITE81.TXT`,
včetně nových skutečných čtení z 1581 (`build/1581-file-read-report.json`).
