/*
 *	DATA.H
 *	Tom Kerrigan's Simple Chess Program (TSCP), modified
 *
 *	Copyright 1997 Tom Kerrigan
 *  Modifications: Copyright 2014 Vance Zuo
 */

#ifndef DATA_H
#define DATA_H

/* this is basically a copy of data.c that's included by most
   of the source files so they can use the data.c variables */

extern int (*eval_func)();
extern int (*quiesce_func)(int, int);
extern int (*search_func)(int, int, int);

extern int threads;
   
extern int color[64];
extern int piece[64];

extern int side;
extern int xside;
extern int castle;
extern int ep;
extern int fifty;
extern int hash;
extern int ply;
extern int hply;

extern gen_t gen_dat[GEN_STACK];
extern int first_move[MAX_PLY];

extern int history[64][64];
extern hist_t hist_dat[HIST_STACK];

extern int max_time;
extern int max_depth;
extern int start_time;
extern int stop_time;
extern int nodes;

extern move pv[MAX_PLY][MAX_PLY];
extern int pv_length[MAX_PLY];
extern BOOL follow_pv;

extern move best_pv[MAX_PLY];
extern int best_pv_length;

extern int hash_piece[2][6][64];
extern int hash_side;
extern int hash_ep[64];

extern int mailbox[120];
extern int mailbox64[64];

extern BOOL slide[6];
extern int offsets[6];
extern int offset[6][8];
extern int castle_mask[64];
extern char piece_char[6];
extern int init_color[64];
extern int init_piece[64];

// eval.c data (for parallel search, and peval.c)

extern int piece_value[6];
extern int pawn_pcsq[64];
extern int knight_pcsq[64];
extern int bishop_pcsq[64];
extern int king_pcsq[64];
extern int king_endgame_pcsq[64];
extern int flip[64];

extern int pawn_rank[2][10];
extern int piece_mat[2];
extern int pawn_mat[2];

#endif /* DATA_H */
