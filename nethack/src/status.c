/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>

struct nh_player_info player;

static void
draw_bar(int barlen, int val_cur, int val_max, nh_bool ishp)
{
    char str[COLNO];
    int fill_len = 0, bl, colorattr, color;

    bl = barlen - 2;
    if (val_max > 0 && val_cur > 0)
        fill_len = bl * val_cur / val_max;

    /* Rules: HP Pw max white white >2/3 green cyan >1/3 yellow blue >1/7 red
       magenta <=1/7 br.red br.magenta */
    if (val_cur == val_max)
        color = CLR_GRAY;
    else if (val_cur * 3 > val_max * 2)
        color = ishp ? CLR_GREEN : CLR_CYAN;
    else if (val_cur * 3 > val_max * 1)
        color = ishp ? CLR_YELLOW : CLR_BLUE;
    else
        color = ishp ? CLR_RED : CLR_MAGENTA;
    if (val_cur * 7 <= val_max)
        color |= 8;     /* request a bolded attribute */
    colorattr = curses_color_attr(color, 0);

    sprintf(str, "%*d / %-*d", (bl - 3) / 2, val_cur, (bl - 2) / 2, val_max);

    wattron(statuswin, colorattr);
    wprintw(statuswin, ishp ? "HP:" : "Pw:");
    wattroff(statuswin, colorattr);
    waddch(statuswin, '[');
    wattron(statuswin, colorattr);
    wattron(statuswin, A_REVERSE);
    wprintw(statuswin, "%.*s", fill_len, str);
    wattroff(statuswin, A_REVERSE);
    wprintw(statuswin, "%s", &str[fill_len]);
    wattroff(statuswin, colorattr);
    waddch(statuswin, ']');
}

/*

The status bar looks like this:

Two-line:
12345678901234567890123456789012345678901234567890123456789012345678901234567890
HP:[    15 / 16    ] Def:127 Xp:30 Astral Plane  Twelveletter, Student of Stones
Pw:[     5 / 5     ] $4294967295 S:480000 T:4294967295         Burdened Starving

Three-line:
12345678901234567890123456789012345678901234567890123456789012345678901234567890
Twelveletter the Chaotic Gnomish Student of Stones          Dx:18 Co:18 St:18/01
HP:[    15 / 16    ] Def:137 Xp:30(10000000) Astral Plane   In:18 Wi:18 Ch:18
Pw:[     5 / 5     ] $4294967295 S:480000 T:4294967295         Burdened Starving

Leaving the "gnomish" out for now because that information is awkward to get at
(it's given in a format the client doesn't understand).
*/

static const struct {
    const char *name;
    int color;
} statuscolors[] = {
    /* encumberance */
    { "Burdened", CLR_BROWN },
    { "Stressed", CLR_RED },
    { "Strained", CLR_ORANGE },
    { "Overtaxed", CLR_ORANGE },
    { "Overloaded", CLR_ORANGE },
    /* hunger */
    { "Satiated", CLR_RED },
    { "Hungry", CLR_RED },
    { "Weak", CLR_ORANGE },
    { "Fainting", CLR_BRIGHT_MAGENTA },
    { "Fainted", CLR_BRIGHT_MAGENTA },
    { "Starved", CLR_BRIGHT_MAGENTA },
    /* misc */
    { "Unarmed", CLR_MAGENTA },
    { "Lev", CLR_BROWN },
    { "Fly", CLR_GREEN },
    /* trapped */
    { "Held", CLR_RED },
    { "Pit", CLR_RED },
    { "Bear", CLR_RED },
    { "Web", CLR_RED },
    { "Infloor", CLR_RED },
    { "Lava", CLR_BRIGHT_MAGENTA },
    /* misc bad */
    { "Greasy", CLR_BRIGHT_BLUE },
    { "Blind", CLR_BRIGHT_BLUE },
    { "Conf", CLR_BRIGHT_BLUE },
    { "Lame", CLR_BRIGHT_BLUE },
    { "Stun", CLR_BRIGHT_BLUE },
    { "Hallu", CLR_BRIGHT_BLUE },
    /* misc fatal */
    { "FoodPois", CLR_BRIGHT_MAGENTA },
    { "Ill", CLR_BRIGHT_MAGENTA },
    { "Strangle", CLR_BRIGHT_MAGENTA },
    { "Slime", CLR_BRIGHT_MAGENTA },
    { "Petrify", CLR_BRIGHT_MAGENTA },
    { NULL, 0 }
};

static void
draw_status(struct nh_player_info *pi, nh_bool threeline)
{
    char buf[COLNO];
    int i, j, k;

    /* penultimate line */
    wmove(statuswin, (threeline ? 1 : 0), 0);
    draw_bar(15, pi->hp, pi->hpmax, TRUE);
    wprintw(statuswin, " Def:%d %s:%d", 10 - pi->ac,
            pi->monnum == pi->cur_monnum ? "Xp" : "HD", pi->level);
    if (threeline && pi->monnum == pi->cur_monnum) {
        /* keep this synced with newuexp in exper.c */
        long newuexp = 10L * (1L << pi->level);

        if (pi->level >= 10)
            newuexp = 10000L * (1L << (pi->level - 10));
        if (pi->level >= 20)
            newuexp = 10000000L * ((long)(pi->level - 19));
        wprintw(statuswin, "(%ld)", newuexp - pi->xp);
    }
    wprintw(statuswin, " %s", pi->level_desc);
    wclrtoeol(statuswin);

    /* last line */
    wmove(statuswin, (threeline ? 2 : 1), 0);
    draw_bar(15, pi->en, pi->enmax, FALSE);
    wprintw(statuswin, " %c%ld S:%ld T:%ld", pi->coinsym, pi->gold, pi->score,
            pi->moves);
    wclrtoeol(statuswin);

    /* status */
    j = getmaxx(statuswin) + 1;
    for (i = 0; i < pi->nr_items; i++) {
        int color = CLR_WHITE, colorattr;
        j -= strlen(pi->statusitems[i]) + 1;
        for (k = 0; statuscolors[k].name; k++) {
            if (!strcmp(pi->statusitems[i], statuscolors[k].name)) {
                color = statuscolors[k].color;
                break;
            }
        }
        colorattr = curses_color_attr(color, 0);
        wmove(statuswin, (threeline ? 2 : 1), j);
        wattron(statuswin, colorattr);
        wprintw(statuswin, "%s", pi->statusitems[i]);
        wattroff(statuswin, colorattr);
    }

    /* name */
    if (threeline) {
        sprintf(buf, "%.12s the %s %s", pi->plname,
                (pi->align == A_CHAOTIC) ? "Chaotic" :
                (pi->align == A_NEUTRAL) ? "Neutral" :
                "Lawful", pi->rank);
        wmove(statuswin, 0, 0);
    } else {
        sprintf(buf, "%.12s, %s", pi->plname, pi->rank);
        wmove(statuswin, 0, getmaxx(statuswin) - strlen(buf));
    }
    wprintw(statuswin, "%s", buf);
    wclrtoeol(statuswin);

    /* abilities (in threeline mode) "In:18 Wi:18 Ch:18" = 17 chars */
    if (threeline) {
        wmove(statuswin, 0, getmaxx(statuswin) - (pi->st == 18 ? 20 : 17));
        wprintw(statuswin, "Dx:%-2d Co:%-2d St:%-2d", pi->dx, pi->co, pi->st);
        if (pi->st == 18)
            wprintw(statuswin, "/%02d", pi->st_extra);
        wmove(statuswin, 1, getmaxx(statuswin) - (pi->st == 18 ? 20 : 17));
        wprintw(statuswin, "In:%-2d Wi:%-2d Ch:%-2d", pi->in, pi->wi, pi->ch);
    }
}

void
curses_update_status(struct nh_player_info *pi)
{
    if (pi)
        player = *pi;

    if (player.x == 0)
        return; /* called before the game is running */

    draw_status(&player, ui_flags.status3);

    /* prevent the cursor from flickering in the status line */
    wmove(mapwin, player.y, player.x - 1);

    wnoutrefresh(statuswin);
}

void
curses_update_status_silent(struct nh_player_info *pi)
{
    if (pi)
        player = *pi;
}
