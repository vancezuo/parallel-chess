parallel-chess
==============

This repository contains source code for Vance Zuo's a final project in Yale's CPSC 424 - Parallel Programming Techniques course. It modifies [Tom Kerrigan's Simple Chess Program](http://www.tckerrigan.com/chess/tscp) (TSCP) to test parallel static evaluation, quiescence search, and alpha-beta search. Of these, the first two fail laughably, due to overhead and Amdahl's Law. Parallel alpha-beta search methods (root splitting, and principal variation splitting) however yield reasonable speedups.

This derivative work has the permission of Tom Kerrigan, whose reserves all rights to TSCP. Further information on his website (http://www.tckerrigan.com):

> TSCP is copyrighted. You have my permission to download it, look at the code, and run it. If you want to do anything else with TSCP you must get my explicit permission. This includes redistributing TSCP, creating a derivative work, or using any of its code for any reason.
>
>To get permission, just e-mail me at tom.kerrigan@gmail.com. As long as you aren't trying to proft off my work or take credit for it, I will almost certainly give you permission.

Dependencies / Requirements
---------------------------
* C compiler with OpenMP support

Installation
------------
The included Makefile was designed to compile with ICC compilers that support OpenMP. However, with a few tweaks other compilers should be usable, e.g., for GCC, replace the first two lines with

```Makefile
CC = gcc
CFLAGS = -g -O3 -Wall -std=c99 -fopenmp
```

I have not tested this myself, though.

By default the required OpenMP libraries are dynamically linked. For static linking, i.e. including the libraries inside the executable itself, add the `-openmp-link static` flag for ICC, `-static` flag for GCC.

Usage
-----
Run `make` with included Makefile, or compile the code yourself. Then run the resulting executable `chess`. Type `help` into the command-line interface for a list of commands:

```
on - computer plays for the side to move
off - computer stops playing
auto - computer plays automatically, until game ends
st n - set search time to n seconds per move
sd n - set search depth to n ply per move
undo - takes back a move
new - starts a new game
d - display the board
bench [fen] - benchmark built-in, or fen, position
p [e|q|r|v] - set parallel function (rest use serial)
    e = parallel static evaluation
    q = parallel quiescence search
    r = parallel (root-splitting) alpha-beta search
    v = parallel (PV-splitting) alpha-beta search
t n - set number of threads to n
bye - exit the program
xboard - switch to XBoard mode
Enter moves in coordinate notation, e.g., e2e4, e7e8Q
```

The default time and depth limits are infinity and 5 but can be edited via `st` and `sd`, respectively. The `bench` command follows these settings.

Only one parallel method can be used at a time, since they would interfere with each other. Executing `p` without arguments resets to using only serial functions. Because TSCP's fundamental algorithm is unchanged, each method yields the same results for a given depth and position, just at different speeds. Setting PV splitting on (`p v`) will get the fastest/strongest engine.
