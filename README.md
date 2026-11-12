# ydice
Dice rolling simulator via terminal

# COMMANDS
(x)d(y): Rolls (x) dice of (y) faces

(x)d(y)k(z): Rolls (x) dice of (y) faces and choose the (z) highest ones

(x)d(y)dk(z): Rolls (x) dice of (y) faces and choose the (z) lowest ones

---

loop(x, y): Loops "y", "x" times

if(x, y): If "x" is true (not 0), then do "y"

if(x, y, z): If "x" is true (not 0), then do "y", otherwise do "z"

---

$x: Variable "x"

@x or @x(): Macro "x"

@x(a, b, c): Macro "x" with parameters "a", "b" and "c"

# OPERATORS
Arithmetic: + - * / % ^

Logical: || && == != > < >= <=

Unary: - !

# GLOBAL CONFIGS (0 off | 1 on)

-DEBUG_MESSAGES: Shows what the program is currently doing, like adding tokens, calculating dice or in a loop function. Can cause big slowdowns if there is too much stuff happening.

-SHOW_LOADED: Shows all Vars and Macros loaded.

-PRINT_COLORS: Prints dice results as colored (Green for Highest value, Red for Lowest value, Grey for "ignored" results). Some terminals might not have support for this, if that's the case (weird codes appearing and no colors) toggle it off.

-FULL_DICES: Shows all dice results of each individual dice roll if on.

-FULL_LISTS: Shows all the individual results of a list if on.

-FULL_LOOPS: Shows a list of every iteration of a loop if on.

# CONFIG ORDER & SYNTAX

~

GLOBAL CONFIG LIST

Current globals are listed above

Syntax is "NAME n", where 'n' is 0 or 1

Every config is in the file by default

~

VARIABLES

Variables implemented by the user

Syntax is "$NAME value", where 'NAME' is the name of the value you want to call, and 'value' is a numerical value

To call a variable you simply put "$NAME" in the text to substitute it with the value you put

An example is in the file by default

~

MACROS

Macros implemented by the user

Syntax is "@NAME(params) "INPUT"", where 'NAME' is the name of the macro you want to call, 'params' are all the inputs you want to pass to the macro (multiple inputs must have a ',' between them), and 'INPUT' is the macro itself in the form of a text string like the program (quotes around the string are OBLIGATORY)

TO call a macro you simply put "@NAME" in the text to substitute the it with the text, changing all instances of the params with your own (inputing a param doesn't require a '$' before, but putting them inside the macro string DOES, kinda like using a local variable of sorts)

An example is in the file by default

~

NOTE: All the '~' are important before, between and after each "section" of the config file for it to work

# DEVELOPER'S NOTE
Hey, this is basically a random silly project I made :)

It takes inspiration mostly from Roll20 (good site, recomend), but it did lack some features so I decided to make my own out of boredom, so yeah, there u go

Credits go to me for making the program, no AI involved :D (AI CANNOT make code this messy ;-;), and my friends who made my play RPGs I guess :/

Anyways that's it, good gambling!