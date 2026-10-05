# Špecifikácia jazyka Prask Script
Prípona súboru: `.txt` *(akákoľvek prípona je v poriadku, pokiaľ ide o textový súbor, ale pre Windows sa odporúča `.txt`)*

Kľúčové slová: `program:` (direktíva), `nech`, `ak`, `koniec`, `zahraj`, `kamen`, `papier`, `noznice`, `minule kamen`, `minule papier`, `minule noznice`

Preddefinované premenné: `true`, `True`, `false`, `False`, `TOTAL_ROUNDS`

Operačné symboly: `(`, `)`, `!`, `not`, `nie`, `*`, `/`, `%`, `+`, `-`, `==`, `!=`, `>`, `>=`, `<`, `<=`, `&&`, `aj`, `and`, `xor`, `^`, `alebo`, `or`, `||`

## Sekcie zdrojového kódu

Obsah zdrojového kódu je rozdelený na dve sekcie: init a program. Tieto časti sú oddelené direktívou `program:`. V súbore musí byť práve jedna direktíva `program:`.
Sekcia Init sa vykoná raz, pred začiatkom hry.
Sekcia Program sa vykoná v každom ťahu.

### Sekcia Init

Sekcia Init sa vykoná raz pri inicializácii. Obsahuje iba definície premenných. Všetky premenné sú globálne. Syntax pre definíciu premennej je:
`nech MENO = VÝRAZ`
`MENO` je jedinečný identifikátor premennej. Môže obsahovať písmená, číslice a podčiarknik. Nesmie začínať číslicou a nesmie sa zhodovať s kľúčovým slovom alebo už existujúcim názvom premennej (vrátane preddefinovaných premenných).
Regulárny výraz pre platný názov premennej je: `/[a-zA-Z_]([a-zA-Z0-9_]*)/`
`VÝRAZ` je platný výraz, ktorý môže využívať už definované premenné.

Príklad:
```praskscript
nech X = 2
nech Y = 10 + 67*4
nech Z = (X + Y) / 2
```

### Sekcia Program

Sekcia Program sa vykoná v každom ťahu.
Podporované príkazy:
priradenie premennej (`MENO = VÝRAZ`), definovanie premennej NIE je možné v program sekcii,
podmienené príkazy (`ak PODMIENKA ... koniec`),
príkaz na ukončenie ťahu (`zahraj POLOŽKA`)

#### Priradenie premennej
Priradí hodnotu výrazu do premennej.

Syntax:
```praskscript
MENO = VÝRAZ
```
- `MENO` musí byť názov premennej definovanej v sekcii Init, inak dôjde k chybe.
- `VÝRAZ` musí byť platný výraz.

#### Podmienené príkazy
Vykoná obsiahnutý kód, ak je podmienka celé číslo nerovné 0 (teda po converzii na bool je 1). Vnorenia sú povolené.

Syntax:
```praskscript
ak PODMIENKA
...
koniec
```
- Podmienený kód je umiestnený medzi `ak` a `koniec` a vykoná sa, ak sa `PODMIENKA` vyhodnotí ako celé číslo väčšie ako 0.
- `PODMIENKA` musí byť platný výraz.

#### Príkaz na ukončenie ťahu
Zahrá ťah a ukončí program. Ak interpret dosiahne koniec súboru bez nájdenia príkazu `zahraj`, program spadne.

Syntax:
```praskscript
zahraj POLOŽKA
```
- `POLOŽKA` je jedno z kľúčových slov: `kamen`, `papier`, `noznice`

Tento príkaz okamžite zastaví program a zahrá zadaný ťah.

## Preddefinované premenné
Preddefinované premenné sú konštanty (true, false) alebo nesú údaje o aktuálnej hre. Všetky sú nastavené interpretom pred spustením sekcie Init alebo Program.

**Preddefinované premenné nie sú iba na čítanie, takže si ich nepokazte!**
*(Bol som príliš lenivý na implementáciu ochrany proti zápisu a vždy je väčšia zábava, keď si môžete kód urobiť neopraviteľným!)*

### Základné konštanty
*Nastavené pred Init*
```
true = 1
True = 1
false = 0
False = 0
```

### Informácie o hre
*Nastavené pred Init*
```
TOTAL_ROUNDS
```
Celkový počet kôl v hre

### Posledný ťah súpera
*Nastavené pred Init na -1, mení sa každým ťahom*

*(Sú to výrazy, ktoré sa správajú ako premenné iba na čítanie.)*

Ich hodnota sa mení pri každom vykonaní sekcie Program a odráža súperovu voľbu v poslednom kole. Ak ide o prvý ťah, všetky sú nastavené na `0`.

```
minule kamen
```
`1`, ak súper v minulom kole zahral kamen, inak `0`

```
minule papier
```
`1`, ak súper v minulom kole zahral papier, inak `0`

```
minule noznice
```
`1`, ak súper v minulom kole zahral noznice, inak `0`

## Výrazy
Výraz môže vykonávať matematické operácie s premennými a konštantami. Musia byť napísané na jednom riadku.

Operácie sa vyhodnocujú v poradí priorít (zostupne), operácie s rovnakou prioritou sa vyhodnocujú zľava doprava.
Priorita operácií:
1. Zátvorky: `(`, `)`
2. Logické NIE (`!`, `not`, `nie`)
3. Multiplikatívne: Násobenie (`*`), Celočíselné delenie (`/`), Zvyšok po delení (`%`), Unárny mínus (`-{výraz}`)
4. Aditívne: Sčítanie (`+`), Odčítanie (`-`)
5. Porovnanie: Rovnosť (`==`), Nerovnosť (`!=`), Väčší (`>`), Väčší alebo rovný (`>=`), Menší (`<`), Menší alebo rovný (`<=`)
6. Logické:
   6.1 Logické A (`&&`, `aj`, `and`)
   6.2 Logické XOR (`xor`, `^`)
   6.3 Logické ALEBO (`alebo`, `or`, `||`)

Podporované matematické operácie:
### Základné operácie:
- Sčítanie `A + B`
- Odčítanie `A - B`
- Negácia `-A`
- Násobenie `A * B` *(Poznámka: Implicitné násobenie, napr. `A(B + C)`, nie je podporované.)*
- Celočíselné delenie `A / B` - vydelí A číslom B a zaokrúhli smerom k nule (odstráni desatinnú časť); delenie nulou spôsobí pád programu (vráti chybu).
- Zvyšok po delení `A % B` - vráti zvyšok po delení A / B (vždy vráti celé číslo >= 0).

### Porovnanie:
Všetky porovnávacie operácie vrátia `1`, ak je podmienka pravdivá, a `0`, ak nie je.
- Rovnosť `A == B`
- Nerovnosť `A != B` *(Poznámka: Táto operácia sa pri kompilácii prevádza na !(A == B))*
- Väčší než `A > B`
- Väčší alebo rovný `A >= B`
- Menší než `A < B`
- Menší alebo rovný `A <= B`

### Logické operácie
Pred operáciou sa všetky hodnoty normalizujú na `1` (pravda) a `0` (nepravda): hodnoty rovné `0` sa považujú za nepravdu, všetky ostatné hodnoty za pravdu.
- A - `A aj B`, `A and B` alebo `A && B` - vráti `1`, ak sú obe hodnoty pravdivé, inak `0`.
- ALEBO - `A alebo B`, `A or B` alebo `A || B` - vráti `1`, ak je aspoň jedna z hodnôt pravdivá, inak `0`.
- NIE - `nie A`, `not A` alebo `!A` - neguje hodnotu A, vráti `1`, ak je A nepravda, inak `0`.
- XOR - `A xor B` alebo `A ^ B` - vráti `1`, ak je práve jedna z hodnôt pravdivá, inak `0`.

### Zátvorky
Zátvorky sa používajú na určenie priority operácií.
Príklad: `(A + B) * C`

### Komentáre
Prask Script podporuje jednoriadkové komentáre. Všetko po znaku `#` až po koniec riadka sa ignoruje.

## Ladenie (Debugging)
*Na aktiváciu režimu DEBUG definujte makro DEBUG pri kompilácii: `gcc interpreter.c -o interpreter -D DEBUG`*

**V režime DEBUG môžete použiť dodatočné príkazy:**
- `!debug assert VÝRAZ`
- `!debug print VÝRAZ`

### `!debug assert`
Asercia (tvrdenie); ak je hodnota výrazu 0, vráti chybu (v sekcii Program) alebo zastaví vykonávanie (v sekcii Init).

### `!debug print`
Vypíše hodnotu výrazu.

## Príklad programu
```
nech kamene = 0
nech papiere = 0
nech noznice = 0

program:
ak minule kamen
	kamene = kamene + 1
koniec
ak minule papier
	papiere = papiere + 1
koniec
ak minule noznice
	noznice = noznice + 1
koniec

ak kamene >= papiere aj kamene >= noznice
	zahraj papier
koniec
ak papiere >= kamene aj papiere >= noznice
	zahraj noznice
koniec

zahraj kamen

```
