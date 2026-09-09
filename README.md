# Prask proboj jeseň 2026 - Kameň, papier, nožnice
proboj_prask_j2026
KAMEŇ PAPIER NOŽNICE

## Interpreter

### Usage (for linux)

#### Compile the interpreter

```
gcc kamenpapier.c -o kamenpapier
```

#### Interpret Praskscript

```
./kamenpapier example.psc
```

#### Usage

```
Usage: ./kamenpapier [flags] file(s)
Flags:
 --help / -h: Print this message and exit

 --output / -o: Output file name (.txt extension is reccomended), default: `kamenpapier_game_replay.txt`

 -r <ROUNDS>: (default: 50) Number of rounds played (if one of players errors out or doesn't play a turn, the game will be ended early)
 --names / -n: Provide names of players separated by space (default: player1 and player2).
               Order: In manual mode, the first name is of human and the second of program
                      In program mode, names belong to programs in same order as provided argument files

 -m: Manual mode: You will combat program, you need to provide one program file as an argument
 -p: Program mode (default): Two programs combat each other, you need to provide two program files as arguments

Files:
  In manual mode,  provide one file
  In program mode, provide two files

Example usage:
$ ./kamenpapier -m -r 10 -n human bot -o example_output.txt example.psc
$ ./kamenpapier -r 50 --names botA botB --output example_output.txt example.psc example2.psc
```

#### Debug
For debugging, define the DEBUG macro trough gcc flag

```
gcc kamenpapier.c -o kamenpapier -D DEBUG
```

