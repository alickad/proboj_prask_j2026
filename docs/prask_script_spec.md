# Prask Script language specification
File extension: `.txt` _(any extension is fine as long as the file is a text file, but `.txt` is recommended for Windows)_

Keywords: `program:` (directive), `nech`, `ak`, `koniec`, `zahraj`, `kamen`, `papier`, `noznice`, `minule kamen`, `minule papier`, `minule noznice`

Predefined variables: `true`, `True`, `false`, `False`, `TOTAL_ROUNDS`

Operation symbols: `(`, `)`, `!`, `not`, `nie`, `*`, `/`, `%`, `+`, `-`, `==`, `!=`, `>`, `>=`, `<`, `<=`, `&&`, `aj`, `and`, `xor`, `^`, `alebo`, `or`, `||`

## Source code sections

Source code content is broken into two sections: init and program. These parts are separated by 'program:' directive. There shall be exactly one 'program:' directive.
The Init section is executed once, before the game begins.
The Program section is executed on every turn.

### Init section

Init section will be executed once on init (surprise!). 
Init section only contains variable definitions. All variables are global. Syntax for variable definition is:
`nech NAME = EXPRESSION`
`NAME` is a unique identifier of the variable. It can contain letters, digits and underscore. It cannot start with a digit and it cannot match a keyword or existing variable name (including predefined variables).
Regex for valid variable name is: `/[a-zA-Z_]([a-zA-Z0-9_]*)/`
`EXPRESSION` is a valid expression, it can utilize already defined variables

Example:
```praskscirpt
nech X = 2
nech Y = 10 + 67*4
nech Z = (X + Y) / 2
```

### Program section

Program section will be executed on every turn.
Supported statements:
variable assignment (`NAME = EXPRESSION`),
conditional statements (`ak CONDITION ... koniec`),
return statement (`zahraj ITEM`)

#### Variable assignment
Assign the value of expression to a variable.

Syntax: 
```praskscript
NAME = EXPRESSION
```
- `NAME` must be a name of variable defined in Init section, otherwise error is thrown
- `EXPRESSION` must be a valid expression

#### Conditional statements
Execute contained code if condition is integer bigger than 0. Nesting is allowed.

Syntax:
```praskscript
ak CONDITION
...
koniec
```
- Conditional code is contained between `ak` and `koniec` and is executed if CONDITION is evaluates to integer bigger than 0.
- `CONDITION` must be a valid expression

#### Return statements
Play a turn and halt. If interpreter reaches end of the file without finding a zahraj statement, the program crashes.

Syntax:
```praskscript
zahraj ITEM
```
- `ITEM` is one of keywords: `kamen`, `papier`, `noznice`

This immidiately halts the program and plays the specified turn.

## Predefined variables
Prederined variables are constants (true, false) carry data about the current game. All of them are set by the interpreter before Init or Program section is executed.

**Predefined variables are not read-only so don't mess them up!.**
_(I was too lazy to implement read-only, and it's always more fun when you can make your code undebuggable!)_

### Basic constants
*Set before Init*
```
true = 1
True = 1
false = 0
False = 0
```

### Game info
*Set before Init*
```
TOTAL_ROUNDS
```
Total number of rounds in a game

### Opponent's last turn
*Set before Init to -1, changes every turn*

_(These are expressions that behave like read-only variables.)_

Their value changes every time program section is executed and reflect opponent's choice in last round. If this is the first turn, all of them are set to `0`.

```
minule kamen
```
`1` if opponent played kamen last round, `0` otherwise

```
minule papier
```
`1` if opponent played papier last round, `0` otherwise

```
minule noznice
```
`1` if opponent played noznice last round, `0` otherwise


## Expressions
Expression can perform mathematic operations on variables and constants. They must be written on one line. 

Operations are evaluated in descending priority order, same-priority operations are evaluated left-to-right
Operation priority:
1. Parentheses: `(`, `)`
2. Logical NOT (`!`, `not`, `nie`)
3. Multiplicative: Multiplication (`*`), Whole number division (`/`), Modulo (`%`), Unary minus (`-{expr}`)
4. Additive: Addition (`+`), Subtraction (`-`)
5. Comparison: Equal (`==`), Not equal (`!=`), Greater than (`>`), Greater than or equal (`>=`), Less than (`<`), Less than or equal (`<=`)
6. Logical:
   
   6.1 Logical AND (`&&`, `aj`, `and`)
   
   6.2 Logical XOR (`xor`, `^`)
   
   6.3 Logical OR (`alebo`, `or`, `||`)

Supported mathematic operations:
### Basic operations:
- Addition `A + B` - add two values
- Subtraction `A - B` - subtract B from A
- Negation `-A` - negate A
- Multiplication `A * B` - multiply A and B _(Note: Implicit multiplication (e.g. `A(B + C)`) is not supported.)_
- Whole number division `A / B` - devide A with B and truncate toward zero (discarding any fractional part), division by zero results in program crash
- Modulo operation `A % B` - gives a remained of division A / B (always returns integer >=0)

### Comparison:
All comparison operation return `1` if condition is true and `0` if it's not.
- Equal `A == B` - returns `1` if A is equal to B, `0` otherwise
- Not equal `A != B` - returns `1` if A is different to B, `0` otherwise (_Note: This operation is converted to !(A == B) in compile time_)
- More than `A > B` - returns `1` if A is bigger than B, `0` otherwise
- More than or equal `A >= B` - returns `1` if A is bigger or equal to B, `0` otherwise
- Less than `A < B` - returns `1` if A is smaller than B, `0` otherwise
- Less than or equal `A <= B` - returns `1` if A is smaller or equal to B, `0` otherwise

### Logical operations
Before operation, all values are normalised to `1`_(true)_ and `0`_(false)_: values equal to `0` are considered `0`_(false)_, all other values are `1`_(true)_
- AND - `A a B`, `A and B` or `A && B` - returns `1` if both A and B are true, `0` otherwise
- OR - `A alebo B`, `A or B` or `A || B` - returns `1` if at least one of A and B is true, `0` otherwise
- NOT - `nie A`, `not A` or `!A` - negate the value of A, returns `1` if A is false, `0` otherwise
- XOR - `A xor B` or `A ^ B` - returns `1` if at exactly one of A and B is true, `0` otherwise

### Parenthesis
Parenthesis are used to give priority to certain operation.
Parenthesis shall follow standard syntax.
Example: `(A + B) * C`
In the example parenthesis give priority to addition over multiplication.

### Comments
Prask scirpt supports full-line comments. Everyting after `#` charecter up to newline character is ignored

Syntax:
```praskscript
# This is a comment
nech X = 2 # This also a valid comment

program: #Comment here

# This is a comment too!
# zahraj kamen # this will not be executed, it's a comment

zahraj papier
```

### Example program
```praskscript
# Keep track of what turn are we playing
nech tah = -1

program:
tah = tah + 1

# We will play papier every even turn
ak !(tah % 2)
zahraj papier
koniec

# If the opponent plays the same thing twice in a row, we will win this round
ak minule kamen
zahraj papier
koniec

ak minule papier
zahraj noznice
koniec

ak minule noznice
zahraj kamen
koniec


# Always add fallback, so your program doesn't crash
zahraj kamen
```

## Debugging
_To enable DEBUG mode, define macro DEBUG when compiling: `gcc interpreter.c -o interpreter -D DEBUG`_

**In DEBUG mode, you can use additional statements:**
- `!debug assert EXPR`
- `!debug print EXPR`


### `!debug assert`
Assert; if value of expression is 0, return error (when in Program section) or halt execution (when in Init section)

**Syntax:**
```
!debug assert EXPR
```

Where `EXPR` is a valid expression

**Example:**
```
!debug assert (1 + 2) / 3 + 2 == 3
```

**Example `assertion failed` message:**
```
Assertion on line 12 (instruction 57) FAILED. Returning error.
```

### `!debug print`
Print value of expression

**Syntax:**
```
!debug print EXPR
```

Where `EXPR` is a valid expression

**Example:**
```
!debug print x + y
```

---

_Written by human, reviewed by Gemini AI_
