# Prask proboj jeseň 2026 - Kameň, papier, nožnice
proboj_prask_j2026
KAMEŇ PAPIER NOŽNICE

## Interpreter

### Usage (for linux)

#### Compile the interpreter

```
gcc interpreter.c -o interpreter
```

#### Run Praskscript

```
./interpreter examples/example.psc
```

#### Usage

```
Usage: ./interpreter [flags] file(s)
Flags:
 --help / -h: Print this message and exit

 --no-log: (has no effect in manual mode or with DEBUG enabled) Disable all log messages except for final score and error messages --output / -o: Output file name (.txt extension is reccomended), default: `game_replay/game.txt`
                Note: Output file will never overwrite another, it will always be made unique by adding #[number] to it (e.g.: game.txt -> game#2.txt)
                Warning: Try to not do weird things with paths (e.g.: ~/../home/Documents/../Pictures/g.txt), this wasn't tested properly (yet)

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
$ ./interpreter -m -r 10 -n human bot examples/example_program.txt
$ ./interpreter -r 50 --names botA botB --output game_replay/epic_game.txt examples/example_program.txt examples/example_program2.txt
```

#### Debug
For debugging, define the DEBUG macro trough gcc flag

Note: You can enable even more messages by defining DEEP_DEBUG macro (some logs are too messy to be shown in DEBUG)

```
gcc interpreter.c -o interpreter -D DEBUG
```

## Praskscript syntax

