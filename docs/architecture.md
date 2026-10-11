# Architektura prototypu

x86 instrukce vykonává 6510 v emulované C64. KFF ARM se jako CPU nepoužívá;
požadavek na testovatelnost v emulaci má přednost. Aktuální konfigurace je
PAL C64 + 2MiB REU ve VICE. Tím není deklarovaná podpora REU na fyzickém KFF.

## Paměť

První MiB REU je backing store 20bitové PC mapy: 640KiB konvenční RAM,
CGA VRAM B8000–BBFFF, disková ROM D0000–D07FF a GLaBIOS FE000–FFFFF.
Zápisy do ROM a nepřipojených oblastí se ignorují; prázdná ISA paměť vrací FF.

Pager má osm 512B rámců, 512B přenosový buffer a metadata. Dirty stránky
zapisuje při vytěsnění/flush; chyba backendu zachová původní dirty obsah.
Přenosy REU DMA 90h/91h přes DF00 pozastaví 6510. Čtení instrukcí používá
span v residentní stránce; generační čítač zneplatní pointer při vytěsnění.
Testy pokrývají chyby I/O, hranice, přetečení adres, ROM ochranu i REP.
Wordové přístupy používají jedno hledání rámce; na hranicích se rozdělí.
Zápis do textové CGA paměti nadále označí všechny dotčené řádky.
Přístupy k již načteným stránkám na C64 používají assemblerový pager:
nejprve ověří aktuální rámec, pak prohledá všech osm rámců. Výběr rámce
nemění generaci instrukčního okna; načtení nové stránky ji zneplatní.
Načítání, vytěsnění, chyby backendu, hraniční wordové přístupy a zápis
do CGA nadále obsluhuje C. Kontrola při překladu chrání velikost stránky,
počet rámců a rozložení jejich metadat používaných assemblerem.

## CPU a zařízení

Interpret implementuje běžné instrukce 8086 včetně segmentů, flags,
REP/string, nepřímých a vzdálených skoků, IVT/IRET, IN/OUT, MUL/DIV a HLT.
Nepodporovaný opcode zastaví CPU. Model není cyklově přesný: nepředstírá
8088 prefetch queue, přesné časování PIT ani chování všech nezdokumentovaných
opcode. IRQ se doručují mezi rozpočtovanými dávkami instrukcí.
Na C64 používají fetch, ALU, výpočet fyzické adresy a běžné registrové
instrukce assembler 6510. Nepodporované rychlou cestou přecházejí do C;
všechny x86 instrukce nadále podléhají stejnému rozpočtu. Nativní sestavení
používá přenosnou C implementaci. Podrobnosti a měření: performance.md.

xt.c modeluje základ 8259 PIC, 8253 PIT, PPI/XT klávesnici, CGA CRTC a DMA
registry. Emulátorová konfigurace GLaBIOS vypíná fyzické testy nedodaného FDC.
Systémové BIOSové služby vykonává ROM; blokové přenosy poskytuje option ROM.

## Disk

Druhý MiB REU obsahuje 720KiB FAT12 diskety: 80 cylindrů, dvě hlavy,
devět 512B sektorů. Obraz se přes IEC8 FSDevice načte jednou, potom se sektory
čtou/zapisují náhodně přes DMA. Zápisy nepersistují po ukončení emulátoru.
Toto je původní varianta. Nová varianta 1581 používá pouze 1MiB REU
pro PC RAM: fd_1581_read/write obsluhují PC sektory přímo přes IEC U1/U2.
DOS a jeho zápisy jsou na D81, zatímco BIOS a celý emulátor jsou v Magic
Desk CRT. Reset cartridge zkopíruje program do C64 RAM; BIOS se později
načte z cartridge banky 8 do FE000 v REU. Rozhraní IO1 cartridge a IO2
REU se nepřekrývají. Rozložení média a spouštění: [1581.md](1581.md).
Virtuální diskové služby kontrolují CHS, rozsah, 64KiB DMA hranici a CF/AH.

Příprava média kopíruje původní FreeDOS bootovací kód, kernel 2043 a
FreeCom 0.86 z oficiálního vydání 1.4. Přidá minimální FDCONFIG a AUTOEXEC.
Prázdný 32MiB HDD.IMG z tools/images.py zůstává samostatný a nebootovatelný.
Disková a operační paměť se nesmějí navzájem překrývat.

## Obrazovka

C64 VIC-II má šířku 320 bodů. Plný 80sloupcový text proto používá 4 body
na znak a sdílenou barvu dvou znaků v bitmapové buňce. Čitelný pohled F1/F3
ukáže polovinu obrazovky s původním 8×8 fontem; F5 celý 80sloupcový obraz.
CGA barvy se převádějí na nejbližší dostupnou barvu pevné palety C64.

## Kung Fu Flash

Stock KFF není 1MiB zapisovatelné RAM. Jeho firmware a flash nelze označit
za REU bez hardwarového důkazu. Budoucí backend potřebuje realizovatelné
blokové rozhraní a odpovídající model ve VICE. Fyzické C64/KFF zatím nebyly
ověřeny. Rychlost nativního testu neodpovídá rychlosti emulace na 6510.
