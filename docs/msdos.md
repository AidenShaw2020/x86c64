# Možnost MS-DOS

PC/XT emulátor není vázaný na FreeDOS. MS-DOS může být druhým testovacím
systémem, pokud je dostupná kompletní bootovací disketa pro IBM PC a její
geometrie odpovídá podporovanému řadiči. Současný backend používá 720KiB,
80 cylindrů, dvě hlavy a devět sektorů. Pro obvyklou starší 360KiB disketu
je třeba přidat geometrii 40×2×9 do BIOSu i loaderu.

Oficiální repozitář Microsoft obsahuje MS-DOS 1.25, 2.0 a zdroj 4.0 pod
MIT licencí. Balík 2.0 je OEM sada: obsahuje MSDOS.SYS, COMMAND.COM,
SYSINIT.OBJ a SYSIMES.OBJ, ale vyžaduje systémovou část pro konkrétní
počítač (obvykle IO.SYS nebo IBMBIO.COM) a bootovací sektor. Tyto chybějící
části nejsou stejné jako GLaBIOS: GLaBIOS je ROM počítače; IO.SYS poskytuje
DOSové ovladače a inicializaci. Kopie samotného MSDOS.SYS na FreeDOS médium
proto není funkční náhrada.

Pro první pokus by dávala smysl lehká verze 2.x. Předpokládaná úspora RAM
a času musí být změřena na skutečně bootujícím médiu. Starší systém také
omezí dostupné DOSové API a kompatibilitu novějších aplikací.

MS-DOS zatím není sestavený ani testovaný. Současný funkční cíl zůstává
FreeDOS + GLaBIOS ve VICE; změna systému nenahrazuje opravu případné chyby
x86 jádra nebo zařízení.

Prameny: [Microsoft MS-DOS](https://github.com/microsoft/MS-DOS),
[OEM sada 2.0 a její README](https://github.com/microsoft/MS-DOS/tree/main/v2.0/bin).
