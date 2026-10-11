# Další práce

- Hotovo: sestavení C64 PRG, x86 interpret, REU pager, chyby úložiště,
  syntetický diskový boot a negativní VICE testy.
- Hotovo nativně: GLaBIOS POST, skutečný FreeDOS kernel + FreeCom,
  VER/DIR/ECHO přes IRQ klávesnici.
- Hotovo ve VICE: skutečný GLaBIOS POST, FreeDOS + FreeCom, VER/DIR/ECHO
  přes IRQ1. Běh 2026-10-11 má status passed, 3 120 934 instrukcí,
  přibližně 31,5 minuty ve warp režimu. Uloženo i skutečné nabootované VSF.
- Hotovo ve VICE: rozbalený kernel, 1 993 217 instrukcí, přibližně
  21,5 minuty. Obnovený VSF ověřen novým klávesnicovým ECHO a jeho odpovědí.
  Výchozí obraz ukazuje všech 80×25 znaků, F1/F3 zvětšení.
- Hotovo: assemblerové fetch/ALU/adresování a dávky běžných instrukcí;
  smyčky 2,11–4,40× rychlejší podle cyklů C64. Úplný boot rozbaleného média
  s VER/DIR/ECHO prošel za 12 minut, oproti dřívějším 21,5 minutám.
- Hotovo: assemblerové byte/word přístupy a hledání mezi osmi residentními
  rámci pageru. Paměťová smyčka dalších 1,36× rychlejší, celkem 2,87×
  proti původní verzi. Úplný POST/FreeDOS/VER/DIR/ECHO prošel za 10 minut
  místo předchozích 12 minut, ve stejném místním warp režimu.
- Dále: zrychlovat zbývající ModR/M a paměťové instrukce, rozšířit klávesnici.
- Hotovo ve VICE: emulator/GLaBIOS v Magic Desk CRT, FreeDOS na 1581,
  1MiB REU pouze pro PC RAM. Úplný POST/DOS/VER/DIR/ECHO prošel;
  soubor vytvořený DOSem byl ověřen v D81 po vypnutí VICE. Přímé IEC
  U1/U2 čtení/zápis, hranice tracků a persistenci ověřují samostatné testy.
- Dále: COM/EXE regrese, bootovatelný FAT16 HDD,
  přesnější časovač/PIC, audit 8086 krajních případů.
- KFF: navrhnout emulovatelné úložiště realizovatelné hardwarem bez ARM x86
  emulace. Fyzická validace až po dokončení testovatelného prototypu.

Jde o výzkumný prototyp, nikoli dokončenou hardwarovou náhradu PC-XT.
