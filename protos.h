/*
 *	PROTOS.H
 *	Tom Kerrigan's Simple Chess Program (TSCP), modified
 *
 *	Copyright 1997 Tom Kerrigan
 *  Modifications: Copyright 2014 Vance Zuo
 */

#ifndef PROTOS_H
#define PROTOS_H

/* prototypes */

/* board.c */
void init_board();
void init_hash();
int hash_rand();
void set_hash();
BOOL in_check(int s);
BOOL attack(int sq, int s);
void gen();
void gen_caps();
void gen_push(int from, int to, int bits);
void gen_promote(int from, int to, int bits);
BOOL makemove(move_bytes m);
void takeback();

/* book.c */
void open_book();
void close_book();
int book_move();
BOOL book_match(char *s1, char *s2);

/* search.c */
void think(int output);
int search(int alpha, int beta, int depth);
int prs_search(int alpha, int beta, int depth);
int pvs_search(int alpha, int beta, int depth);
int quiesce(int alpha, int beta);
int p_quiesce(int alpha, int beta);
int reps();
void sort_pv();
void sort(int from);
BOOL timeout();
void omp_synchronize_state();

/* eval.c */
int eval();
int p_eval();
int eval_light_pawn(int sq);
int eval_dark_pawn(int sq);
int eval_light_king(int sq);
int eval_lkp(int f);
int eval_dark_king(int sq);
int eval_dkp(int f);

/* main.c */
int get_ms();
int main();
int parse_move(char *s);
char *move_str(move_bytes m);
void print_board();
void print_raw(int[64]);
void xboard();
void print_result();
void bench(char *fen, int iterations);

#endif /* PROTOS_H */
