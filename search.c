/*
 *	SEARCH.C
 *	Tom Kerrigan's Simple Chess Program (TSCP), modified
 *
 *	Copyright 1997 Tom Kerrigan
 *  Modifications: Copyright 2014 Vance Zuo
 */


#include <stdio.h>
#include <string.h>
#include <omp.h>
#include "defs.h"
#include "data.h"
#include "protos.h"


// private data structures for parallel search
#pragma omp threadprivate(color, piece)
#pragma omp threadprivate(side, xside, castle, ep, fifty, hash, ply, hply)
#pragma omp threadprivate(gen_dat, first_move)
#pragma omp threadprivate(hist_dat)
#pragma omp threadprivate(pv, pv_length, follow_pv)


/* booleans for when search should stop */
BOOL stop_search;
BOOL cutoff;


/* think() calls search() iteratively. Search statistics
   are printed depending on the value of output:
   0 = no output
   1 = normal output
   2 = xboard format output */

void think(int output)
{
	int i, j, x;

	/* try the opening book first */
	pv[0][0].u = book_move();
	if (pv[0][0].u != -1)
		return;

	start_time = get_ms();
	stop_time = start_time + max_time;

	ply = 0;
	nodes = 0;

	memset(pv, 0, sizeof(pv));
	memset(history, 0, sizeof(history));
	if (output == 1)
		printf("ply      nodes  score  pv\n");
		
	stop_search = FALSE;
	cutoff = FALSE;
	for (i = 1; i <= max_depth; ++i) {
		follow_pv = TRUE;
		x = (*search_func)(-10000, 10000, i);
		if (stop_search)
			break;
			
		if (output == 1)
			printf("%3d  %9d  %5d ", i, nodes, x);
		else if (output == 2)
			printf("%d %d %d %d", i, x, (get_ms() - start_time) / 10, nodes);
		if (output) {
			for (j = 0; j < pv_length[0]; ++j)
				printf(" %s", move_str(pv[0][j].b));
			printf("\n");
			fflush(stdout);
		}
		
		if (x > 9000 || x < -9000)
			break;
	}
	
	/* make sure to take back the line we were searching */
	while (ply)
		takeback();
}


/* search() does just that, in negamax fashion */

int search(int alpha, int beta, int depth)
{
	int i, j, x;
	BOOL c, f;

	/* we're as deep as we want to be; call quiesce() to get
	   a reasonable score and return it. */
	if (!depth)
		return (*quiesce_func)(alpha,beta);

	#pragma omp atomic
	++nodes;

	/* do some housekeeping every 1024 nodes */
	if ((nodes & 1023) == 0 && timeout())
		return alpha;

	pv_length[ply] = ply;

	/* if this isn't the root of the search tree (where we have
	   to pick a move and can't simply return 0) then check to
	   see if the position is a repeat. if so, we can assume that
	   this line is a draw and return 0. */
	if (ply && reps())
		return 0;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return (*eval_func)();
	if (hply >= HIST_STACK - 1)
		return (*eval_func)();

	/* are we in check? if so, we want to search deeper */
	c = in_check(side);
	if (c)
		++depth;
		
	gen();
	
	if (follow_pv)  /* are we following the PV? */
		sort_pv();
		
	f = FALSE;

	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].m.b))
			continue;
		f = TRUE;
		x = -search(-beta, -alpha, depth - 1);
		takeback();
		if (stop_search)
			return alpha;
		if (x > alpha) {

			/* this move caused a cutoff, so increase the history
			   value so it gets ordered high next time we can
			   search it */
			history[(int)gen_dat[i].m.b.from][(int)gen_dat[i].m.b.to] += depth;
			if (x >= beta)
				return beta;
			alpha = x;

			/* update the PV */
			pv[ply][ply] = gen_dat[i].m;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv[ply][j] = pv[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
		}
	}

	/* no legal moves? then we're in checkmate or stalemate */
	if (!f) {
		if (c)
			return -10000 + ply;
		else
			return 0;
	}

	/* fifty move draw rule */
	if (fifty >= 100)
		return 0;
	return alpha;
}


/* prs_search() does search in parallel by splitting the root node */

int prs_search(int alpha, int beta, int depth)
{
	int i, j, x;
	BOOL c, f;

	/* we're as deep as we want to be; call quiesce() to get
	   a reasonable score and return it. */
	if (!depth)
		return (*quiesce_func)(alpha,beta);

	++nodes;

	/* do some housekeeping every 1024 nodes */
	if ((nodes & 1023) == 0 && timeout())
		return alpha;

	pv_length[ply] = ply;

	/* if this isn't the root of the search tree (where we have
	   to pick a move and can't simply return 0) then check to
	   see if the position is a repeat. if so, we can assume that
	   this line is a draw and return 0. */
	if (ply && reps())
		return 0;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return (*eval_func)();
	if (hply >= HIST_STACK - 1)
		return (*eval_func)();

	/* are we in check? if so, we want to search deeper */
	c = in_check(side);
	if (c)
		++depth;
		
	gen();
	
	if (follow_pv)  /* are we following the PV? */
		sort_pv();
		
	f = FALSE;
	cutoff = FALSE;
	best_pv_length = 0;
	
	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i)
		sort(i);
		
	// omp_synchronize_state();
	#pragma omp parallel for schedule(dynamic,1) copyin(color, piece, \
			side, xside, castle, ep, fifty, hash, ply, hply, \
			gen_dat, first_move,  hist_dat, pv, pv_length, follow_pv) \
			private(i, j, x)
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		if (stop_search || cutoff || !makemove(gen_dat[i].m.b))
			continue;
		f = TRUE;
		x = -search(-beta, -alpha, depth - 1);
		takeback();
		// printf("Move %d (%s)... %d [%d]\n", i, move_str(gen_dat[i].m.b), x, omp_get_thread_num());
		#pragma omp critical
		if (x > alpha && !cutoff) {
			/* this move caused a cutoff, so increase the history
			   value so it gets ordered high next time we can
			   search it */
			history[(int)gen_dat[i].m.b.from][(int)gen_dat[i].m.b.to] += depth;
			if (x >= beta) {
				cutoff = TRUE;
			} else {
				alpha = x;

				// update the (local) PV
				best_pv[ply] = pv[ply][ply] = gen_dat[i].m;
				for (j = ply + 1; j < pv_length[ply + 1]; ++j)
					best_pv[j] = pv[ply][j] = pv[ply + 1][j];
				best_pv_length = pv_length[ply] = pv_length[ply + 1];
				
				// printf("\tPV %d ", x);
				// for (j = 0; j < best_pv_length; ++j)
					// printf(" %s", move_str(best_pv[j].b));
				// printf("\n");
				// fflush(stdout);
			}
		}
	}
	
	// update the PV
	if (best_pv_length > 0) {
		pv[ply][ply] = best_pv[ply];
		for (j = ply + 1; j < best_pv_length; ++j)
			pv[ply][j] = best_pv[j];
		pv_length[ply] = best_pv_length;
	}

	if (cutoff)
		return beta;
	
	/* no legal moves? then we're in checkmate or stalemate */
	if (!f) {
		if (c)
			return -10000 + ply;
		else
			return 0;
	}

	/* fifty move draw rule */
	if (fifty >= 100)
		return 0;
	return alpha;
}


/* pvs_search() does search in parallel via principal variation splitting */

int pvs_search(int alpha, int beta, int depth)
{
	int i, i0, j, x;
	BOOL c, f;

	/* we're as deep as we want to be; call quiesce() to get
	   a reasonable score and return it. */
	if (!depth)
		return (*quiesce_func)(alpha,beta);

	++nodes;

	/* do some housekeeping every 1024 nodes */
	if ((nodes & 1023) == 0 && timeout())
		return alpha;

	pv_length[ply] = ply;

	/* if this isn't the root of the search tree (where we have
	   to pick a move and can't simply return 0) then check to
	   see if the position is a repeat. if so, we can assume that
	   this line is a draw and return 0. */
	if (ply && reps())
		return 0;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return (*eval_func)();
	if (hply >= HIST_STACK - 1)
		return (*eval_func)();

	/* are we in check? if so, we want to search deeper */
	c = in_check(side);
	if (c)
		++depth;
		
	gen();
	
	if (follow_pv)  /* are we following the PV? */
		sort_pv();
		
	f = FALSE;
	cutoff = FALSE;
	best_pv_length = 0;

	/* search first/PV variation before doing rest in parallel */
	for (i0 = first_move[ply]; i0 < first_move[ply + 1]; ++i0) {
		sort(i0);
		if (!makemove(gen_dat[i0].m.b))
			continue;
		f = TRUE;
		x = -pvs_search(-beta, -alpha, depth - 1);
		takeback();
		if (stop_search)
			return alpha;
		if (x > alpha) {
			history[(int)gen_dat[i0].m.b.from][(int)gen_dat[i0].m.b.to] += depth;
			if (x >= beta)
				return beta;
			alpha = x;

			/* update the PV */
			pv[ply][ply] = gen_dat[i0].m;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv[ply][j] = pv[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
		}
		i0++;
		break;
	}
	
	/* loop through the moves */
	for (i = i0; i < first_move[ply + 1]; ++i)
		sort(i);
	
	#pragma omp parallel for schedule(dynamic,1) copyin(color, piece, \
			side, xside, castle, ep, fifty, hash, ply, hply, \
			gen_dat, first_move, hist_dat, pv, pv_length, follow_pv) \
			private(i, j, x)
	for (i = i0; i < first_move[ply + 1]; ++i) {
		if (stop_search || cutoff || !makemove(gen_dat[i].m.b))
			continue;
		f = TRUE;
		x = -search(-beta, -alpha, depth - 1);
		takeback();
		#pragma omp critical
		if (x > alpha && !cutoff) {
			/* this move caused a cutoff, so increase the history
			   value so it gets ordered high next time we can
			   search it */
			history[(int)gen_dat[i].m.b.from][(int)gen_dat[i].m.b.to] += depth;
			if (x >= beta) {
				cutoff = TRUE;
			} else {
				alpha = x;

				// update the (local) PV
				best_pv[ply] = pv[ply][ply] = gen_dat[i].m;
				for (j = ply + 1; j < pv_length[ply + 1]; ++j)
					best_pv[j] = pv[ply][j] = pv[ply + 1][j];
				best_pv_length = pv_length[ply] = pv_length[ply + 1];
			}
		}
	}
	
	// update the PV
	if (best_pv_length > 0) {
		pv[ply][ply] = best_pv[ply];
		for (j = ply + 1; j < best_pv_length; ++j)
			pv[ply][j] = best_pv[j];
		pv_length[ply] = best_pv_length;
	}

	if (cutoff)
		return beta;
	
	/* no legal moves? then we're in checkmate or stalemate */
	if (!f) {
		if (c)
			return -10000 + ply;
		else
			return 0;
	}

	/* fifty move draw rule */
	if (fifty >= 100)
		return 0;
	return alpha;
}


/* quiesce() is a recursive minimax search function with
   alpha-beta cutoffs. In other words, negamax. But it
   only searches capture sequences and allows the evaluation
   function to cut the search off (and set alpha). The idea
   is to find a position where there isn't a lot going on
   so the static evaluation function will work. */

int quiesce(int alpha,int beta)
{
	int i, j, x;
	
	#pragma omp atomic
	++nodes;

	/* do some housekeeping every 1024 nodes */
	if ((nodes & 1023) == 0 && timeout())
		return alpha;

	pv_length[ply] = ply;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return (*eval_func)();
	if (hply >= HIST_STACK - 1)
		return (*eval_func)();

	/* check with the evaluation function */
	x = (*eval_func)();
	if (x >= beta)
		return beta;
	if (x > alpha)
		alpha = x;
		
	gen_caps();
	if (follow_pv)  /* are we following the PV? */
		sort_pv();
	
	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		sort(i);
		if (!makemove(gen_dat[i].m.b))
			continue;
		x = -quiesce(-beta, -alpha);
		takeback();
		// printf("\tMove %d (%s)... %d [%d]\n", i, move_str(gen_dat[i].m.b), x, omp_get_thread_num());
		if (stop_search || cutoff)
			return alpha;
		if (x > alpha) {
			if (x >= beta)
				return beta;
			alpha = x;

			/* update the PV */
			pv[ply][ply] = gen_dat[i].m;
			for (j = ply + 1; j < pv_length[ply + 1]; ++j)
				pv[ply][j] = pv[ply + 1][j];
			pv_length[ply] = pv_length[ply + 1];
				
			// printf("\tPV %d ", x);
			// for (j = 0; j < best_pv_length; ++j)
				// printf(" %s", move_str(best_pv[j].b));
			// printf("\n");
			// fflush(stdout);
		}
	}
	return alpha;
}


/* p_quiesce() is a parallel version of quiesce(). */

int p_quiesce(int alpha,int beta)
{
	int i, j, x;
	move best;
	
	++nodes;

	/* do some housekeeping every 1024 nodes */
	if ((nodes & 1023) == 0 && timeout())
		return alpha;

	pv_length[ply] = ply;

	/* are we too deep? */
	if (ply >= MAX_PLY - 1)
		return (*eval_func)();
	if (hply >= HIST_STACK - 1)
		return (*eval_func)();

	/* check with the evaluation function */
	x = (*eval_func)();
	if (x >= beta)
		return beta;
	if (x > alpha)
		alpha = x;

	gen_caps();
	if (follow_pv)  /* are we following the PV? */
		sort_pv();

	cutoff = FALSE;
	best_pv_length = 0;
			
	/* loop through the moves */
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i)
		sort(i);

	// omp_synchronize_state();
	// #pragma omp parallel for ordered schedule(dynamic,1) private(i, j, x)
	#pragma omp parallel for schedule(dynamic,1) copyin(color, piece, \
			side, xside, castle, ep, fifty, hash, ply, hply, \
			gen_dat, first_move,  hist_dat, pv, pv_length, follow_pv) \
			private(i, j, x)
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		if (stop_search || cutoff || !makemove(gen_dat[i].m.b))
			continue;
		x = -quiesce(-beta, -alpha);
		takeback();
		#pragma omp critical
		if (x > alpha && !cutoff) {
			if (x >= beta) {
				cutoff = TRUE;
			} else {
				alpha = x;

				// update the (local) PV
				best_pv[ply] = pv[ply][ply] = gen_dat[i].m;
				for (j = ply + 1; j < pv_length[ply + 1]; ++j)
					best_pv[j] = pv[ply][j] = pv[ply + 1][j];
				best_pv_length = pv_length[ply] = pv_length[ply + 1];
			}
		}
	} // */
	
	/* #pragma omp parallel private(i, x) copyin(color, piece, side, xside, \
			castle, ep, fifty, hash, ply, hply, gen_dat, first_move, \
			pv, pv_length, follow_pv) num_threads(1)
	{
	#pragma omp master
	for (i = first_move[ply]; i < first_move[ply + 1]; ++i) {
		#pragma omp task 
		{
		if (!stop_search && !cutoff && makemove(gen_dat[i].m.b)) {
			x = -quiesce(-beta, -alpha);
			takeback();
			#pragma omp critical (quiesce_check)
			if (x > alpha) {
				if (x >= beta) {
					cutoff = TRUE;
				} else {
					alpha = x;

					// update the (local) PV
					best_pv[ply] = pv[ply][ply] = gen_dat[i].m;
					for (j = ply + 1; j < pv_length[ply + 1]; ++j)
						best_pv[j] = pv[ply][j] = pv[ply + 1][j];
					best_pv_length = pv_length[ply] = pv_length[ply + 1];
				}
			}
		}
		}
	}
	} // */	
	
	// update the PV
	if (best_pv_length > 0) {
		pv[ply][ply] = best_pv[ply];
		for (j = ply + 1; j < best_pv_length; ++j)
			pv[ply][j] = best_pv[j];
		pv_length[ply] = best_pv_length;
	}
	
	if (cutoff)
		return beta;	
	return alpha;
}


/* reps() returns the number of times the current position
   has been repeated. It compares the current value of hash
   to previous values. */

int reps()
{
	int i;
	int r = 0;

	for (i = hply - fifty; i < hply; ++i)
		if (hist_dat[i].hash == hash)
			++r;
	return r;
}


/* sort_pv() is called when the search function is following
   the PV (Principal Variation). It looks through the current
   ply's move list to see if the PV move is there. If so,
   it adds 10,000,000 to the move's score so it's played first
   by the search function. If not, follow_pv remains FALSE and
   search() stops calling sort_pv(). */

void sort_pv()
{
	int i;

	follow_pv = FALSE;
	for(i = first_move[ply]; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].m.u == pv[0][ply].u) {
			follow_pv = TRUE;
			gen_dat[i].score += 10000000;
			return;
		}
}


/* sort() searches the current ply's move list from 'from'
   to the end to find the move with the highest score. Then it
   swaps that move and the 'from' move so the move with the
   highest score gets searched next, and hopefully produces
   a cutoff. */

void sort(int from)
{
	int i;
	int bs;  /* best score */
	int bi;  /* best i */
	gen_t g;

	bs = -1;
	bi = from;
	for (i = from; i < first_move[ply + 1]; ++i)
		if (gen_dat[i].score > bs) {
			bs = gen_dat[i].score;
			bi = i;
		}
	g = gen_dat[from];
	gen_dat[from] = gen_dat[bi];
	gen_dat[bi] = g;
}


/* timeout() checks if the engine's time limit is up. */

BOOL timeout()
{
	if (get_ms() >= stop_time) {
		stop_search = TRUE;
		return TRUE;
	}
	return FALSE;
}


/* omp_synchronize_state() copies the master's board state to the board state 
   of other OpenMP threads. */
   
void omp_synchronize_state() {
	int master_color[64];
	int master_piece[64];
	int master_side;
	int master_xside;
	int master_castle;
	int master_ep;
	int master_fifty;
	int master_hash;
	int master_ply;
	int master_hply;
	gen_t master_gen_dat[GEN_STACK];
	int master_first_move[MAX_PLY];
	hist_t master_hist_dat[HIST_STACK];
	move master_pv[MAX_PLY][MAX_PLY];
	int master_pv_length[MAX_PLY];
	BOOL master_follow_pv;
	
	memcpy(master_color, color, sizeof(color));
	memcpy(master_piece, piece, sizeof(piece));
	master_side = side;
	master_xside = xside;
	master_castle = castle;
	master_ep = ep;
	master_fifty = fifty;
	master_hash = hash;
	master_ply = ply;
	master_hply = hply;
	memcpy(master_gen_dat, gen_dat, sizeof(gen_dat));
	memcpy(master_first_move, first_move, sizeof(first_move));
	memcpy(master_hist_dat, hist_dat, sizeof(hist_dat));
	memcpy(master_pv, pv, sizeof(pv));
	memcpy(master_pv_length, pv_length, sizeof(pv_length));
	master_follow_pv = follow_pv;
	
	#pragma omp parallel
	{
	memcpy(color, master_color, sizeof(color));
	memcpy(piece, master_piece, sizeof(piece));
	side = master_side;
	xside = master_xside;
	castle = master_castle;
	ep = master_ep;
	fifty = master_fifty;
	hash = master_hash;
	ply = master_ply;
	hply = master_hply;
	memcpy(gen_dat, master_gen_dat, sizeof(gen_dat));
	memcpy(first_move, master_first_move, sizeof(first_move));
	memcpy(hist_dat, master_hist_dat, sizeof(hist_dat));
	memcpy(pv, master_pv, sizeof(pv));
	memcpy(pv_length, master_pv_length, sizeof(pv_length));
	follow_pv = master_follow_pv;
	}
}