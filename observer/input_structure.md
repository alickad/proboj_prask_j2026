## Input file structure
1. name of player 1
2. name of player 2
3. scores, separated by space, -1 means error
rest of lines - turns separated with space, valid turns: `KAMEN`, `PAPIER`, `NOZNICE`, `error`

Examples:
```txt
Krtko
Zein
5 3
KAMEN PAPIER
NOZNICE PAPIER
KAMEN PAPIER
NOZNICE KAMEN
NOZNICE PAPIER
NOZNICE PAPIER
NOZNICE PAPIER
NOZNICE PAPIER
```


```txt
Teto
Miku
-1 3
KAMEN PAPIER
NOZNICE PAPIER
KAMEN PAPIER
NOZNICE KAMEN
error KAMEN
```
In this case player 1 errored out and was disqualified
