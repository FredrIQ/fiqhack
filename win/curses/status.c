/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <string.h>
#include "nhcurses.h"

struct nh_player_info player;

/* MAXCO must hold longest uncompressed status line, and must be larger
 * than COLNO
 *
 * longest practical second status line at the moment is
 *	Astral Plane $:12345 HP:700(700) Pw:111(111) AC:-127 Xp:30/123456789
 *	T:123456 Satiated Conf FoodPois Ill Blind Stun Hallu Overloaded
 * -- or somewhat over 130 characters
 */
#if COLNO <= 140
#define MAXCO 160
#else
#define MAXCO (COLNO+20)
#endif


static void bot1(struct nh_player_info *pi)
{
	char newbot1[MAXCO];
	char *nb = newbot1;
	int i,j;

	strcpy(newbot1, pi->plname);
	if ('a' <= newbot1[0] && newbot1[0] <= 'z')
	    newbot1[0] += 'A'-'a';
	newbot1[10] = '\0';
	nb += strlen(newbot1);
	sprintf(nb, " the %s", pi->rank);

	strncat(nb, "  ", MAXCO);
	i = pi->max_rank_sz + 15;
	j = strlen(newbot1);
	if ((i - j) > 0)
		sprintf(nb += strlen(nb), "%*s", i-j, " ");	/* pad with spaces */
	
	if (pi->st == 18 && pi->st_extra) {
		if (pi->st_extra < 100)
		    sprintf(nb += strlen(nb), "St:18/%02d ", pi->st_extra);
		else
		    sprintf(nb += strlen(nb),"St:18/** ");
	} else
		sprintf(nb += strlen(nb), "St:%-1d ", pi->st);
	
	sprintf(nb += strlen(nb), "Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
		pi->dx, pi->co, pi->in, pi->wi, pi->ch);
	sprintf(nb += strlen(nb), (pi->align == A_CHAOTIC) ? "  Chaotic" :
			(pi->align == A_NEUTRAL) ? "  Neutral" : "  Lawful");
	
	if (settings.showscore)
	    sprintf(nb += strlen(nb), " S:%ld", pi->score);
	
	wmove(statuswin, 0, 0);
	waddstr(statuswin, newbot1);
	wclrtoeol(statuswin);
}


static void bot2(struct nh_player_info *pi)
{
	char newbot2[MAXCO];
	char *nb = newbot2;
	int i;

	strncpy(newbot2, pi->level_desc, MAXCO);
	sprintf(nb += strlen(newbot2),
		"%c:%-2ld HP:%d(%d) Pw:%d(%d) AC:%-2d", pi->coinsym,
		pi->gold, pi->hp, pi->hpmax, pi->en, pi->enmax, pi->ac);

	if (pi->monnum != pi->cur_monnum)
		sprintf(nb += strlen(nb), " HD:%d", pi->level);
	else if (settings.showexp)
		sprintf(nb += strlen(nb), " Xp:%u/%-1ld", pi->level, pi->xp);
	else
		sprintf(nb += strlen(nb), " Exp:%u", pi->level);

	if (settings.time)
	    sprintf(nb += strlen(nb), " T:%ld", pi->moves);
	
	for (i = 0; i < pi->nr_items; i++)
	    sprintf(nb += strlen(nb), " %s", pi->statusitems[i]);
	
	wmove(statuswin, 1, 0);
	waddstr(statuswin, newbot2);
	wclrtoeol(statuswin);
}


void curses_update_status(struct nh_player_info *pi)
{
    if (pi)
	player = *pi;
    
    if (player.x == 0)
	return; /* called from erase_menu_or_text, but the game isn't running */
    
    bot1(&player);
    bot2(&player);
    
    wrefresh(statuswin);
}
