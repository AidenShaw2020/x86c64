# Zrychlení 6510 interpretu

Optimalizace z 2026-10-11 zachovává PAL C64 a REU. x86 nadále vykonává
6510; nejčastější cesty nyní používají assembler. Přenosy diskety a systémové
služby se nemění. Skutečný BIOS i FreeDOS zůstávají původními x86 programy.

- `src/c64/fetch.s`: načítání instrukcí z residentní stránky a skoky uvnitř
  jejího okna. Generace pageru, změna CS a wrap IP zneplatňují cestu.
- `src/c64/alu.s`: ADD/ADC/SUB/SBB/CMP/AND/OR/XOR a flags pro 8/16 bitů.
- `src/c64/fast.s`: dávky běžných registrových instrukcí, immediatů,
  INC/DEC, LOOP, krátkých skoků a JC/JNC/JZ/JNZ. Ostatní opcodes a všechny
  prefixy vrací před jejich spotřebováním úplnému C interpretu.
- `src/c64/address.s`: přímý výpočet 20bitové segmentované adresy.
- Pager čte a zapisuje slovo jedním hledáním rámce. Hraniční přístupy
  zachovávají přechod stránek, ochranu ROM a samostatný wrap offsetu segmentu.
- `src/c64/pager.s`: byte/word přístupy k aktuálnímu rámci a hledání mezi
  všemi osmi načtenými rámci v assembleru. Chybějící stránky a zápisy do CGA
  používají C cestu; dirty metadata, ochrana ROM a chyby backendu zůstávají
  zachované. Kontrola layoutu rámců chrání konstanty assembleru.
- Jen x86 a pager používají cc65 `-Cl`. Tyto cesty na C64 obsluhují jeden
  CPU a nejsou reentrantní. Kontrola layoutu FDCPU chrání assemblerové offsety.

## Srovnání podle cyklů C64

`tools/benchmark.py` počítá cykly pomocí dvou kaskádovaných časovačů CIA2.
Všechny varianty vykonají stejné instrukce, s aktivní PC paměťovou mapou,
stejným backendem REU a maskovaným KERNAL IRQ. Čas monitoru ani rychlost
hostitelského počítače nemění emulovaný počet cyklů. Nastavení video režimu
je v obou variantách shodné. Jde o syntetické smyčky, nikoli čas celého DOSu.

| Zátěž | x86 instrukcí | Původní cykly C64 | Nové cykly C64 | Zrychlení |
|---|---:|---:|---:|---:|
| Registry: INC/LOOP | 16 385 | 78 762 671 | 17 887 162 | 4,40× |
| Paměť, zásobník, LOOP | 24 577 | 203 218 993 | 96 150 058 | 2,11× |
| ADD/XOR/ROL/LOOP | 16 385 | 96 349 645 | 40 205 926 | 2,40× |

Reporty a skutečně spuštěné PRG jsou v `build/benchmarks/baseline-pc-map/`
a `build/benchmarks/accelerated-pc-map/`. Původní jádro bylo použito
z objektů archivovaného benchmarku `baseline`; sestavovat dnešní zdroj
pod názvem baseline by nebylo platné srovnání.

Nové měření aktuální verze:

    python -B tools/dev.py benchmark

Volby `--pc-map --static-locals` samostatného benchmarku umožní stejné
nastavení jako v tabulce. K reprodukci původního běhu jsou nutné uložené
objekty staré verze; `--reuse-core baseline` je používá bez překladu.

Další optimalizace pageru má samostatný report
`build/benchmarks/pager-scan-pc-map/report.json`. Paměťová a zásobníková
smyčka potřebuje 70 903 631 cyklů místo předchozích 96 150 058: další
zrychlení 1,36×, přibližně o 26 % méně cyklů. Proti původní C verzi
(203 218 993 cyklů) je celkový násobek 2,87×. Registrová a aritmetická
smyčka se touto změnou prakticky nemění.

    python -B tools/benchmark.py pager-scan-pc-map --pc-map --static-locals

## Ověření

Nativní regrese: 311 751 kontrol. Zachován vyčerpávající test byte ADC/SBB,
přidány wordové přístupy, chyby druhé stránky, ROM ochrana, CGA dirty řádky,
wrap segmentu a stack SP=1. Pět krátkých VICE testů rovněž prošlo.

Samostatný C64 test vykonává 3 072 aritmetických vektorů vytvořených
nezávislým Python modelem, včetně carry, overflow, parity, AH a CMP.
Dále kontroluje MOV/INC/DEC, rozpočty instrukcí, 32bitový čítač, prefixy,
skoky, wrap IP/busu, self modification a vytěsnění instrukční stránky.
Na C64 používá skutečné assemblerové cesty, které nativní test neobsahuje.
Nový pager navíc prochází na C64 kontrolami byte/word hranic, wrapu busu,
dirty vytěsnění i chyb načtení/zápisu backendu. Testy ověřují zachování
původního obsahu po chybě, ochranu ROM a označení obou CGA řádků při
wordovém zápisu přes jejich hranici.

    python -B tools/dev.py test-c64-cpu

Report: `build/cpu-check/report.json`; úspěchem je pouze `status: passed`.
Úplný závěrečný boot má oddělený `build/glabios-accelerated-test-report.json`.
Dokončil se s `status: passed`, `post_complete` a `guest_pass_marker` true:
2 030 020 x86 instrukcí, 12 006 179 637 cyklů C64 od resetu a 720,4 s
na místním PC ve warp režimu. Test vykonal GLaBIOS POST, FreeDOS kernel,
FreeCom a odpovědi na VER, DIR /B a ECHO přes IRQ1.

Dřívější běh stejného rozbaleného média trval 1290,3 s; nyní přibližně
720,4 s, tedy místní zrychlení celého startu asi 1,79×. Oba testy kontrolují
marker každých 30 s. Výkon PC a souběžné emulátory ovlivňují reálný čas;
tento údaj není stejně přesné srovnání jako cykly syntetických smyček.

Příkaz pro jeho opakování, s viditelným oknem a samostatným monitorem:

    python -B tools/vice_glabios_test.py --fast --name glabios-accelerated --port 6509

Pro screenshot tohoto testu je potřeba Python s Pillow. Až úspěšný boot
vytvoří snapshot, lze jej obnovit bez dalšího studeného startu:

    python -B tools/vice_resume.py --accelerated

Obnovení prošlo i nezávislou kontrolou nového `ECHO FD64 ACCELERATED OK`:
`build/accelerated-resume-report.json` má status passed. Odpověď je
ověřená přímo v pixelech VIC-II; starý testovací marker se nepoužívá.
Screenshot: `build/glabios-accelerated-resumed.png`. Obnovený DOS zůstává
otevřený pro ruční zadávání příkazů.

Další assemblerový pager má úspěšný samostatný cold-boot report
`build/glabios-pager-test-report.json`: `status: passed`, `post_complete`
a `guest_pass_marker` true. GLaBIOS, FreeDOS, FreeCom a VER/DIR/ECHO
potřebovaly 2 023 836 x86 instrukcí, 9 860 314 458 cyklů C64 od resetu
a 600,4 s na místním PC ve warp režimu. Proti předchozím 720,4 s je
místní zrychlení přibližně 1,20×, proti dřívějším 1290,3 s asi 2,15×.
Oba optimalizované běhy používaly stejné rozbalené médium a kontrolu
markeru každých 30 s. Vliv hostitelského PC a okamžiku kontroly zůstává.

    python -B tools/vice_glabios_test.py --fast --name glabios-pager --port 6510
    python -B tools/vice_resume.py --name glabios-pager

PRG a symboly `build/vice-glabios-pager.prg/lbl` patří právě tomuto běhu;
jejich SHA256 i hashe média/ROM jsou v reportu. Snapshot je
`build/glabios-pager-ready.vsf`, skutečný obraz
`build/glabios-pager-freedos-vice.png`. Doplnění kontroly layoutu rámců
při překladu nezměnilo výsledné PRG ani symboly; shoda byla ověřena.
Také obnovení nového snapshotu prošlo: `build/pager-resume-report.json`
má status passed, po novém `ECHO FD64 PAGER OK` se samostatná odpověď
ověřila v pixelech VIC-II. Obnovený aktuální DOS zůstává viditelně otevřený.

Zrychlení nezavádí cyklově přesný 8088 ani PIT. IRQ zůstávají mezi dávkami
instrukcí, stejně jako před optimalizací. Celý boot se měří zvlášť; násobky
z tabulky nejsou zárukou stejného zrychlení každého DOSového programu.
