cs424-chess
===========

This repository contains source code for Vance Zuo's modifications of Tom Kerrigan's Simple Chess Program (TSCP) for a final project in Yale's CPSC 424 - Parallel Programming Techniques course.

Dependencies / Requirements
---------------------------
* C, with OpenMP support

Installation
------------
The included Makefile is designed to compile with ICC compilers that support OpenMP. However, with a few tweaks other compilers should be usable, e.g., for GCC, replace the first two lines with

  CC = gcc
  CFLAGS = -g -O3 -Wall -std=c99 -fopenmp
  
I have not tested this myself, though.

Usage
-----
Simply run the executable 'chess'. Type 'help' into the command-line interface for a list of commands.
