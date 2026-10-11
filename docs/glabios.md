# GLaBIOS a virtuální XT

Upstream v0.4.2 je připnutý na revizi
b98b0fe396466fa41466a9521f40987c989ae077. Původní 8X ROM, zdroj a GPL-3.0
licence jsou zachované ve vendor/glabios. Prototyp používá vlastní sestavení
tohoto upstream zdroje.

tools/build_glabios.py vytvoří upravenou kopii ASM v build/glabios, sestaví
ji JWasm 2.20 a upstream gla2rom doplní checksum. ROM má 8192 bajtů, platný
resetový skok a nulový 8bitový součet. Přesné volby a SHA256 jsou
v build/glabios/configuration.json.

Konfigurace vychází z ARCH_EMU, CPU_8088. RAM je 640KiB. Dlouhý test RAM,
fyzický FDC, BASIC ROM a POST testy fyzické klávesnice jsou vypnuté.
Základní test RAM v ROM stále probíhá. Základní PIC, PIT, PPI, CGA a IRQ
klávesnice jsou modelované v xt.c; není to emulace přesného časování PC-XT.

Reset začíná na FFFF:0000. Systémové vektory instalují a obsluhují instrukce
GLaBIOS. Naše 2KiB option ROM na D000 je při POST vyhledaná a vykonaná.
Instaluje INT13, INT19 a diskovou tabulku. INT13 thunk používá privátní
INT F0 pro přenos sektorů virtuální diskety. Bootovací kód ROM načte sektor
na 0000:7C00 a skočí do něj. Fyzický NEC765 FDC není implementovaný.

Výstup vzniká zápisy BIOSu a DOSu do B8000. C64 jej vykresluje jako hires
bitmapu s původním BIOSovým fontem a překladem CGA barev na VIC-II.
F1/F3 zobrazí levou/pravou polovinu v plném fontu; F5 všech 80 sloupců.

tests/glabios_boot.c ověřuje POST a celý FreeDOS boot nativně.
tools/dev.py test-glabios samostatně ověřuje tutéž cestu v C64 ve VICE.
Úspěch vyžaduje skutečnou odpověď na ECHO, nikoli jen banner nebo echo
napsaného příkazu. Výsledek: build/glabios-test-report.json.
