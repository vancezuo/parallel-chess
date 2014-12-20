/*
 *	PEVAL.C
 *	Tom Kerrigan's Simple Chess Program (TSCP), modified
 *
 *  Copy of eval.c, except with no private data structures. This allows
 *  peval() to work on evaluation in parallel.
 *
 *	Copyright 1997 Tom Kerrigan
 *  Modifications: Copyright 2014 Vance Zuo
 */


#include <string.h>
#include "defs.h"
#include "data.h"
#include "protos.h"


/* shared_pawn_rank[x][y] is the rank of the least advanced pawn of color x on 
   file y - 1. There are "buffer files" on the left and right to avoid 
   special-case logic later. If there's no pawn on a rank, we pretend the pawn 
   is impossibly far advanced (0 for LIGHT and 7 for DARK). This makes it easy 
   to test for pawns on a rank and it simplifies some pawn evaluation code. */
int shared_pawn_rank[2][10];

int shared_piece_mat[2];  /* the value of a side's pieces */
int shared_pawn_mat[2];  /* the value of a side's pawns */

int peval()
{
	int i;
	int f;  /* file */
	int score[2];  /* each side's score */
	
	/* set up shared_pawn_rank, shared_piece_mat, and shared_pawn_mat. */
	for (i = 0; i < 10; ++i) {
		shared_pawn_rank[LIGHT][i] = 0;
		shared_pawn_rank[DARK][i] = 7;
	}
	shared_piece_mat[LIGHT] = 0;
	shared_piece_mat[DARK] = 0;
	shared_pawn_mat[LIGHT] = 0;
	shared_pawn_mat[DARK] = 0;
	for (i = 0; i < 64; ++i) {
		if (color[i] == EMPTY)
			continue;
		if (piece[i] == PAWN) {
			shared_pawn_mat[color[i]] += piece_value[PAWN];
			f = COL(i) + 1;  /* add 1 because of the extra file in the array */
			if (color[i] == LIGHT) {
				if (shared_pawn_rank[LIGHT][f] < ROW(i))
					shared_pawn_rank[LIGHT][f] = ROW(i);
			} else {
				if (shared_pawn_rank[DARK][f] > ROW(i))
					shared_pawn_rank[DARK][f] = ROW(i);
			}
		} else {
			shared_piece_mat[color[i]] += piece_value[piece[i]];
		}
	}

	/* this is the second pass: evaluate each piece */
	score[LIGHT] = shared_piece_mat[LIGHT] + shared_pawn_mat[LIGHT];
	score[DARK] = shared_piece_mat[DARK] + shared_pawn_mat[DARK];
	#pragma omp parallel 
	{
	int own_score[2] = {0, 0};
	#pragma omp for private(i) nowait
	for (i = 0; i < 64; ++i) {
		if (color[i] == EMPTY)
			continue;
		if (color[i] == LIGHT) {
			switch (piece[i]) {
				case PAWN:
					own_score[LIGHT] += peval_light_pawn(i);
					break;
				case KNIGHT:
					own_score[LIGHT] += knight_pcsq[i];
					break;
				case BISHOP:
					own_score[LIGHT] += bishop_pcsq[i];
					break;
				case ROOK:
					if (shared_pawn_rank[LIGHT][COL(i) + 1] == 0) {
						if (shared_pawn_rank[DARK][COL(i) + 1] == 7) {
							own_score[LIGHT] += ROOK_OPEN_FILE_BONUS;
						} else {
							own_score[LIGHT] += ROOK_SEMI_OPEN_FILE_BONUS;
						}
					}
					if (ROW(i) == 1) {
						own_score[LIGHT] += ROOK_ON_SEVENTH_BONUS;
					}
					break;
				case KING:
					if (shared_piece_mat[DARK] <= 1200) {
						own_score[LIGHT] += king_endgame_pcsq[i];
					} else {
						own_score[LIGHT] += peval_light_king(i);
					}
					break;
			}
		} else {
			switch (piece[i]) {
				case PAWN:
					own_score[DARK] += peval_dark_pawn(i);
					break;
				case KNIGHT:
					own_score[DARK] += knight_pcsq[flip[i]];
					break;
				case BISHOP:
					own_score[DARK] += bishop_pcsq[flip[i]];
					break;
				case ROOK:
					if (shared_pawn_rank[DARK][COL(i) + 1] == 7) {
						if (shared_pawn_rank[LIGHT][COL(i) + 1] == 0) {
							own_score[DARK] += ROOK_OPEN_FILE_BONUS;
						} else {
							own_score[DARK] += ROOK_SEMI_OPEN_FILE_BONUS;
						}
					}
					if (ROW(i) == 6) {
						own_score[DARK] += ROOK_ON_SEVENTH_BONUS;
					}
					break;
				case KING:
					if (shared_piece_mat[LIGHT] <= 1200) {
						own_score[DARK] += king_endgame_pcsq[flip[i]];
					} else {
						own_score[DARK] += peval_dark_king(i);
					}
					break;
			}
		}
	}
	#pragma omp atomic
	score[LIGHT] += own_score[LIGHT];
	#pragma omp atomic
	score[DARK] += own_score[DARK];
	}

	/* the score[] array is set, now return the score relative
	   to the side to move */
	if (side == LIGHT)
		return score[LIGHT] - score[DARK];
	return score[DARK] - score[LIGHT];
}

int peval_light_pawn(int sq)
{
	int r;  /* the value to return */
	int f;  /* the pawn's file */

	r = 0;
	f = COL(sq) + 1;

	r += pawn_pcsq[sq];

	/* if there's a pawn behind this one, it's doubled */
	if (shared_pawn_rank[LIGHT][f] > ROW(sq))
		r -= DOUBLED_PAWN_PENALTY;

	/* if there aren't any friendly pawns on either side of
	   this one, it's isolated */
	if ((shared_pawn_rank[LIGHT][f - 1] == 0) &&
			(shared_pawn_rank[LIGHT][f + 1] == 0))
		r -= ISOLATED_PAWN_PENALTY;

	/* if it's not isolated, it might be backwards */
	else if ((shared_pawn_rank[LIGHT][f - 1] < ROW(sq)) &&
			(shared_pawn_rank[LIGHT][f + 1] < ROW(sq)))
		r -= BACKWARDS_PAWN_PENALTY;

	/* add a bonus if the pawn is passed */
	if ((shared_pawn_rank[DARK][f - 1] >= ROW(sq)) &&
			(shared_pawn_rank[DARK][f] >= ROW(sq)) &&
			(shared_pawn_rank[DARK][f + 1] >= ROW(sq)))
		r += (7 - ROW(sq)) * PASSED_PAWN_BONUS;

	return r;
}

int peval_dark_pawn(int sq)
{
	int r;  /* the value to return */
	int f;  /* the pawn's file */

	r = 0;
	f = COL(sq) + 1;

	r += pawn_pcsq[flip[sq]];

	/* if there's a pawn behind this one, it's doubled */
	if (shared_pawn_rank[DARK][f] < ROW(sq))
		r -= DOUBLED_PAWN_PENALTY;

	/* if there aren't any friendly pawns on either side of
	   this one, it's isolated */
	if ((shared_pawn_rank[DARK][f - 1] == 7) &&
			(shared_pawn_rank[DARK][f + 1] == 7))
		r -= ISOLATED_PAWN_PENALTY;

	/* if it's not isolated, it might be backwards */
	else if ((shared_pawn_rank[DARK][f - 1] > ROW(sq)) &&
			(shared_pawn_rank[DARK][f + 1] > ROW(sq)))
		r -= BACKWARDS_PAWN_PENALTY;

	/* add a bonus if the pawn is passed */
	if ((shared_pawn_rank[LIGHT][f - 1] <= ROW(sq)) &&
			(shared_pawn_rank[LIGHT][f] <= ROW(sq)) &&
			(shared_pawn_rank[LIGHT][f + 1] <= ROW(sq)))
		r += ROW(sq) * PASSED_PAWN_BONUS;

	return r;
}

int peval_light_king(int sq)
{
	int r;  /* the value to return */
	int i;

	r = king_pcsq[sq];

	/* if the king is castled, use a special function to pevaluate the
	   pawns on the appropriate side */
	if (COL(sq) < 3) {
		r += peval_lkp(1);
		r += peval_lkp(2);
		r += peval_lkp(3) / 2;  /* problems with pawns on the c & f files
								  are not as severe */
	}
	else if (COL(sq) > 4) {
		r += peval_lkp(8);
		r += peval_lkp(7);
		r += peval_lkp(6) / 2;
	}

	/* otherwise, just assess a penalty if there are open files near
	   the king */
	else {
		for (i = COL(sq); i <= COL(sq) + 2; ++i)
			if ((shared_pawn_rank[LIGHT][i] == 0) &&
					(shared_pawn_rank[DARK][i] == 7))
				r -= 10;
	}

	/* scale the king safety value according to the opponent's material;
	   the premise is that your king safety can only be bad if the
	   opponent has enough pieces to attack you */
	r *= shared_piece_mat[DARK];
	r /= 3100;

	return r;
}

/* peval_lkp(f) pevaluates the Light King Pawn on file f */

int peval_lkp(int f)
{
	int r = 0;

	if (shared_pawn_rank[LIGHT][f] == 6);  /* pawn hasn't moved */
	else if (shared_pawn_rank[LIGHT][f] == 5)
		r -= 10;  /* pawn moved one square */
	else if (shared_pawn_rank[LIGHT][f] != 0)
		r -= 20;  /* pawn moved more than one square */
	else
		r -= 25;  /* no pawn on this file */

	if (shared_pawn_rank[DARK][f] == 7)
		r -= 15;  /* no enemy pawn */
	else if (shared_pawn_rank[DARK][f] == 5)
		r -= 10;  /* enemy pawn on the 3rd rank */
	else if (shared_pawn_rank[DARK][f] == 4)
		r -= 5;   /* enemy pawn on the 4th rank */

	return r;
}

int peval_dark_king(int sq)
{
	int r;
	int i;

	r = king_pcsq[flip[sq]];
	if (COL(sq) < 3) {
		r += peval_dkp(1);
		r += peval_dkp(2);
		r += peval_dkp(3) / 2;
	}
	else if (COL(sq) > 4) {
		r += peval_dkp(8);
		r += peval_dkp(7);
		r += peval_dkp(6) / 2;
	}
	else {
		for (i = COL(sq); i <= COL(sq) + 2; ++i)
			if ((shared_pawn_rank[LIGHT][i] == 0) &&
					(shared_pawn_rank[DARK][i] == 7))
				r -= 10;
	}
	r *= shared_piece_mat[LIGHT];
	r /= 3100;
	return r;
}

int peval_dkp(int f)
{
	int r = 0;

	if (shared_pawn_rank[DARK][f] == 1);
	else if (shared_pawn_rank[DARK][f] == 2)
		r -= 10;
	else if (shared_pawn_rank[DARK][f] != 7)
		r -= 20;
	else
		r -= 25;

	if (shared_pawn_rank[LIGHT][f] == 0)
		r -= 15;
	else if (shared_pawn_rank[LIGHT][f] == 2)
		r -= 10;
	else if (shared_pawn_rank[LIGHT][f] == 3)
		r -= 5;

	return r;
}
