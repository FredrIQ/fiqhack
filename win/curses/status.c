/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>

struct nh_player_info player;

/*
 * longest practical second status line at the moment is
 *	Astral Plane $:12345 HP:700(700) Pw:111(111) AC:-127 Xp:30/123456789
 *	T:123456 Satiated Conf FoodPois Ill Blind Stun Hallu Overloaded
 * -- or somewhat over 130 characters
 */

static void classic_status(struct nh_player_info *pi)
{
    char buf[COLNO];
    int i;
    
    /* line 1 */
    sprintf(buf, "%.10s the %-*s  ", pi->plname,
	    pi->max_rank_sz + 8 - (int)strlen(pi->plname), pi->rank);
    buf[0] = toupper(buf[0]);
    mvwaddstr(statuswin, 0, 0, buf);
    
    if (pi->st == 18 && pi->st_extra) {
	if (pi->st_extra < 100)
	    wprintw(statuswin, "St:18/%02d ", pi->st_extra);
	else
	    wprintw(statuswin,"St:18/** ");
    } else
	wprintw(statuswin, "St:%-1d ", pi->st);
    
    wprintw(statuswin, "Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
	    pi->dx, pi->co, pi->in, pi->wi, pi->ch);
    wprintw(statuswin, (pi->align == A_CHAOTIC) ? "  Chaotic" :
		    (pi->align == A_NEUTRAL) ? "  Neutral" : "  Lawful");
    
    if (settings.showscore)
	wprintw(statuswin, " S:%ld", pi->score);
    
    wclrtoeol(statuswin);

    /* line 2 */
    mvwaddstr(statuswin, 1, 0, pi->level_desc);
    wprintw(statuswin, " %c:%-2ld HP:%d(%d) Pw:%d(%d) AC:%-2d", pi->coinsym,
	    pi->gold, pi->hp, pi->hpmax, pi->en, pi->enmax, pi->ac);

    if (pi->monnum != pi->cur_monnum)
	wprintw(statuswin, " HD:%d", pi->level);
    else if (settings.showexp)
	wprintw(statuswin, " Xp:%u/%-1ld", pi->level, pi->xp);
    else
	wprintw(statuswin, " Exp:%u", pi->level);

    if (settings.time)
	wprintw(statuswin, " T:%ld", pi->moves);
    
    for (i = 0; i < pi->nr_items; i++)
	wprintw(statuswin, " %s", pi->statusitems[i]);
    
    wclrtoeol(statuswin);
}


static void draw_bar(int barlen, int val_cur, int val_max, const char *prefix)
{
    char str[COLNO], bar[COLNO];
    int fill_len = 0, bl, percent, colorattr, color;
    
    bl = barlen-2;
    if (val_max <= 0 || val_cur <= 0)
	percent = 0;
    else {
	percent = 100 * val_cur / val_max;
	fill_len = bl * val_cur / val_max;
    }
    
    if (percent < 25)
	color = CLR_RED;
    else if (percent < 50)
	color = CLR_BROWN; /* inverted this looks orange */
    else if (percent < 95)
	color = CLR_GREEN;
    else
	color = CLR_GRAY; /* inverted this is white, with better text contrast */
    colorattr = curses_color_attr(color);
    
    sprintf(str, "%s%d(%d)", prefix, val_cur, val_max);
    sprintf(bar, "%-*s", bl, str);
    waddch(statuswin, '[');
    wattron(statuswin, colorattr);
    
    wattron(statuswin, A_REVERSE);
    wprintw(statuswin, "%.*s", fill_len, bar);
    wattroff(statuswin, A_REVERSE);
    wprintw(statuswin, "%s", &bar[fill_len]);
    
    wattroff(statuswin, colorattr);
    waddch(statuswin, ']');
}


static void status3(struct nh_player_info *pi)
{
    char buf[COLNO];
    int i, namelen;
    
    /* line 1 */
    namelen = strlen(pi->plname) < 13 ? strlen(pi->plname) : 13;
    sprintf(buf, "%.13s the %-*s  ", pi->plname,
	    pi->max_rank_sz + 13 - namelen, pi->rank);
    buf[0] = toupper(buf[0]);
    mvwaddstr(statuswin, 0, 0, buf);
    wprintw(statuswin, "Con:%2d Str:", pi->co);
    if (pi->st == 18 && pi->st_extra) {
	if (pi->st_extra < 100)
	    wprintw(statuswin, "18/%02d  ", pi->st_extra);
	else
	    wprintw(statuswin,"18/**  ");
    } else
	wprintw(statuswin, "%2d  ", pi->st);

    waddstr(statuswin, pi->level_desc);

    if (settings.time)
	wprintw(statuswin, "  T:%ld", pi->moves);
    
    wprintw(statuswin, (pi->align == A_CHAOTIC) ? "  Chaotic" :
		    (pi->align == A_NEUTRAL) ? "  Neutral" : "  Lawful");
    wclrtoeol(statuswin);
    
    
    /* line 2 */
    wmove(statuswin, 1, 0);
    draw_bar(18 + pi->max_rank_sz, pi->hp, pi->hpmax, "HP:");
    wprintw(statuswin, "  Int:%2d Wis:%2d  %c:%-2ld  AC:%-2d  ", pi->in, pi->wi,
	    pi->coinsym, pi->gold, pi->ac);
    
    if (pi->monnum != pi->cur_monnum)
	wprintw(statuswin, "HD:%d", pi->level);
    else if (settings.showexp) {
	if (pi->xp < 1000000)
	    wprintw(statuswin, "Xp:%u/%-1ld", pi->level, pi->xp);
	else
	    wprintw(statuswin, "Xp:%u/%-1ldk", pi->level, pi->xp / 1000);
    }
    else
	wprintw(statuswin, "Exp:%u", pi->level);
    
    if (settings.showscore)
	wprintw(statuswin, "  S:%ld", pi->score);
    wclrtoeol(statuswin);

    /* line 3 */
    wmove(statuswin, 2, 0);
    draw_bar(18 + pi->max_rank_sz, pi->en, pi->enmax, "Pw:");
    wprintw(statuswin, "  Dex:%2d Cha:%2d ", pi->dx, pi->ch);
    
    wattron(statuswin, curses_color_attr(CLR_YELLOW));
    for (i = 0; i < pi->nr_items; i++)
	wprintw(statuswin, " %s", pi->statusitems[i]);
    wattroff(statuswin, curses_color_attr(CLR_YELLOW));
    wclrtoeol(statuswin);
}


void curses_update_status(struct nh_player_info *pi)
{
    if (pi)
	player = *pi;
    
    if (player.x == 0)
	return; /* called before the game is running */
    
    if (ui_flags.status3)
	status3(&player);
    else
	classic_status(&player);
    
    wrefresh(statuswin);
}

void curses_update_status_silent(struct nh_player_info *pi)
{
    if (pi)
	player = *pi;
}
