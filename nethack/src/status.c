/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>

struct nh_player_info player;
static void draw_statuses(struct nh_player_info *, nh_bool);
static long xp(int);

static long
xp(int lev)
{
    /* keep this synced with newuexp() in the engine */
    switch (lev) {
    case  1: return     20; /* n^2 */
    case  2: return     40;
    case  3: return     80;
    case  4: return    160;
    case  5: return    320;
    case  6: return    640;
    case  7: return   1280;
    case  8: return   2560;
    case  9: return   5120;
    case 10: return  10000; /* triangle numbers */
    case 11: return  15000;
    case 12: return  21000;
    case 13: return  28000;
    case 14: return  36000;
    case 15: return  45000;
    case 16: return  55000;
    case 17: return  66000;
    case 18: return  81000; /* n*n series */
    case 19: return 100000;
    case 20: return 142000;
    case 21: return 188000;
    case 22: return 238000;
    case 23: return 292000;
    case 24: return 350000;
    case 25: return 412000;
    case 26: return 478000;
    case 27: return 548000;
    case 28: return 622000;
    case 29: return 700000;
    default: return -1;
    }
    return 1000000; /* shouldn't be reached */
}

static int
hpen_color(int val_cur, int val_max, nh_bool ishp)
{
    /*
     * Rules:  HP     Pw
     *    max  white  white
     *   >2/3  green  cyan
     *   >1/3  yellow blue
     *   >1/7  red    magenta
     *  <=1/7  br.red br.magenta
     *
     * Note for the HP/PW bars: we can't draw a brightly colored background,
     * but the dim color gets substituted
     */
    if (val_cur == val_max)
        return CLR_GRAY;
    if (val_cur * 3 > val_max * 2)
        return ishp ? CLR_GREEN : CLR_CYAN;
    if (val_cur * 3 > val_max * 1)
        return ishp ? CLR_YELLOW : CLR_BLUE;
    if (val_cur * 7 > val_max * 1)
        return ishp ? CLR_RED : CLR_MAGENTA;
    return ishp ? CLR_ORANGE : CLR_BRIGHT_MAGENTA;
}

static void
draw_bar(int barlen, int val_cur, int val_max, nh_bool ishp, const char *buf)
{
    char str[ui_flags.mapwidth];
    int fill_len = 0, bl, colorattr, invcolorattr, color;

    bl = barlen - 2;
    if (val_max > 0 && val_cur > 0)
        fill_len = bl * val_cur / val_max;

    color = hpen_color(val_cur, val_max, ishp);
    colorattr = curses_color_attr(color, 0);        /* color on black */
    /* For the 'bar' portion, we use a 'color & 7'-colored background, and
       the foreground is either black or bolded background (depending on
       whether the foreground equals the background or not). */
    invcolorattr = curses_color_attr(color, color & 7);

    if (!buf)
        sprintf(str, "%*d / %-*d", (bl - 3) / 2, val_cur, (bl - 2) / 2, val_max);
    else {
        /* limit title to ensure it fits */
        sprintf(str, "%s", buf);
        if (strlen(str) > barlen - 2) /* doesn't include the [] */
            str[barlen - 2] = '\0';
    }

    if (statuswin) {
        wattron(statuswin, colorattr);
        if (!buf)
            wprintw(statuswin, ishp ? "HP:" : "Pw:");
        wattroff(statuswin, colorattr);
        waddch(statuswin, '[');
        wattron(statuswin, invcolorattr);
        wprintw(statuswin, "%.*s", fill_len, str);
        wattroff(statuswin, invcolorattr);
        wattron(statuswin, colorattr);
        wprintw(statuswin, "%s", &str[fill_len]);
        wattroff(statuswin, colorattr);
        waddch(statuswin, ']');
    }
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

TODO: If we have more than 80 columns, use them for ability scores on a two-line
status display.

The statuses on the status lines have associated colors, and can also affect the
main frame color. The rules are as follows:

- The main frame is only affected by things that are likely to lead to imminent
  death. Most of these are statuses; delayed instadeaths are bright magenta,
  hunger has its own color. Low HP also affects frame color, because that can
  also cause imminent death; and the frame is black when watching or replaying
  a game, to indicate that commands cannot be input.

- Statuses are colored depending on whether they are good/bad/neutral, how
  serious they are, and how they time out. There's no 100% set pattern, but
  looking at the existing statuses should give an idea of how the color scheme
  normally works.

*/

static const struct {
    const char *name;
    int color;
    int framecolor; /* -1 if this doesn't affect the frame */
} statuscolors[] = {
      /* encumberance */
      { "Burdened", CLR_RED, -1 },
      { "Stressed", CLR_RED, -1 },
      { "Strained", CLR_ORANGE, -1 },
      { "Overtaxed", CLR_ORANGE, -1 },
      { "Overloaded", CLR_ORANGE, -1 },
      /* hunger */
      { "Satiated", CLR_RED, -1 },
      { "Hungry", CLR_RED, -1 },
      { "Weak", CLR_ORANGE, CLR_BROWN },
      { "Fainting", CLR_BRIGHT_MAGENTA, CLR_MAGENTA },
      { "Fainted", CLR_BRIGHT_MAGENTA, CLR_MAGENTA },
      { "Starved", CLR_BRIGHT_MAGENTA, CLR_MAGENTA },
      /* misc */
      { "Melee", CLR_GRAY, -1 },
      { "Dig", CLR_BROWN, -1 },
      { "Ranged", CLR_GREEN, -1 },
      { "Unarmed", CLR_MAGENTA, -1 },
      { "NonWeap", CLR_ORANGE, -1 },
      { "cWielded", CLR_YELLOW, -1 },
      { "Lev", CLR_BROWN, -1 },
      { "Fly", CLR_GREEN, -1 },
      /* trapped */
      { "Held", CLR_RED, -1 },
      { "Pit", CLR_RED, -1 },
      { "Bear", CLR_RED, -1 },
      { "Web", CLR_RED, -1 },
      { "Infloor", CLR_RED, -1 },
      { "Lava", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      /* misc bad */
      { "Greasy", CLR_BRIGHT_BLUE, -1 },
      { "Blind", CLR_BRIGHT_BLUE, -1 },
      { "Conf", CLR_BRIGHT_BLUE, -1 },
      { "Lame", CLR_BRIGHT_BLUE, -1 },
      { "Stun", CLR_BRIGHT_BLUE, -1 },
      { "Hallu", CLR_BRIGHT_BLUE, -1 },
      { "Cancelled", CLR_BRIGHT_BLUE, -1 },
      { "Slow", CLR_BRIGHT_BLUE, -1 },
      { "Zombie", CLR_ORANGE, CLR_GREEN },
      /* misc fatal */
      { "FoodPois", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { "Ill", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { "Strangle", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { "Slime", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { "Petrify", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { "Zombie!", CLR_BRIGHT_MAGENTA, CLR_BRIGHT_MAGENTA },
      { NULL, 0 }
};

static void
draw_classic_status(struct nh_player_info *pi, nh_bool threeline)
{
    char buf[ui_flags.mapwidth];
    char title[ui_flags.mapwidth];

    /* line 1 */
    wmove(statuswin, 0, 0);
    sprintf(buf, " St:%-1d Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
            pi->st, pi->dx, pi->co, pi->in, pi->wi, pi->ch);
    sprintf(buf, "%s %s", buf,
            pi->align == A_CHAOTIC ? "Chaotic" :
            pi->align == A_NEUTRAL ? "Neutral" :
            pi->align == A_LAWFUL ? "Lawful" :
            "Unaligned");
    sprintf(title, "%.10s the %s", pi->plname, pi->rank);
    wmove(statuswin, 0, 0);
    int bar_len = ui_flags.mapwidth - strlen(buf) - 2;
    if (bar_len > strlen(title))
        bar_len = strlen(title);
    draw_bar(bar_len + 2, pi->hp, pi->hpmax, TRUE, title);
    wprintw(statuswin, "%s", buf);
    wclrtoeol(statuswin);

    /* line 2 (possibly not last line if status3 is enabled) */
    int colorattr; /* for HP/Pw colors */
    wmove(statuswin, 1, 0);
    wprintw(statuswin, "%s", pi->level_desc);
    wprintw(statuswin, "  %c:%-2ld ", pi->coinsym, pi->gold);
    wprintw(statuswin, " HP:");
    colorattr = curses_color_attr(hpen_color(pi->hp, pi->hpmax, TRUE), 0);
    wattron(statuswin, colorattr);
    wprintw(statuswin, "%d(%d)", pi->hp, pi->hpmax);
    wattroff(statuswin, colorattr);
    wprintw(statuswin, " Pw:");
    colorattr = curses_color_attr(hpen_color(pi->en, pi->enmax, FALSE), 0);
    wattron(statuswin, colorattr);
    wprintw(statuswin, "%d(%d)", pi->en, pi->enmax);
    wattroff(statuswin, colorattr);
    wprintw(statuswin, " %s:%d ", settings.show_ac ? "AC" : "Def",
            settings.show_ac ? pi->ac : 10 - pi->ac);
    wprintw(statuswin, " %s:%d",
            threeline ? "Xp" : "Exp",
            pi->level);
    if (threeline)
        wprintw(statuswin, "/%d", pi->xp);
    wprintw(statuswin, " T:%d", pi->moves);
    wclrtoeol(statuswin);

    /* Clear the 3rd line if we have one */
    if (threeline) {
        wmove(statuswin, 2, 0);
        wclrtoeol(statuswin);
    }

    draw_statuses(pi, threeline);
}

static void
draw_status(struct nh_player_info *pi, nh_bool threeline)
{
    if (!statuswin)
        return;

    if (ui_flags.statusheight < 2) {
        werase(statuswin);
        return;
    }

    if (settings.classic_status) {
        draw_classic_status(pi, threeline);
        return;
    }

    char buf[ui_flags.mapwidth];

    /* penultimate line */
    wmove(statuswin, (threeline ? 1 : 0), 0);
    draw_bar(15, pi->hp, pi->hpmax, TRUE, NULL);
    wprintw(statuswin, " %s:%d %s:%d", settings.show_ac ? "AC" : "Def",
            settings.show_ac ? pi->ac : 10 - pi->ac,
            pi->monnum == pi->cur_monnum ? "Xp" : "HD", pi->level);
    if (threeline && pi->monnum == pi->cur_monnum) {
        /* keep this synced with newuexp in exper.c */
        long newuexp = xp(pi->level);
        if (newuexp < 0)
            wprintw(statuswin, "(0)");
        else
            wprintw(statuswin, "(%ld)", newuexp - pi->xp);
    }
    wprintw(statuswin, " %s", pi->level_desc);
    wclrtoeol(statuswin);

    /* last line */
    wmove(statuswin, (threeline ? 2 : 1), 0);
    draw_bar(15, pi->en, pi->enmax, FALSE, NULL);
    wprintw(statuswin, " %c%ld S:%ld T:%ld", pi->coinsym, pi->gold, pi->score,
            pi->moves);
    if (getcurx(statuswin) > 0)
        wclrtoeol(statuswin);

    draw_statuses(pi, threeline);

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
    if (getcurx(statuswin) > 0)
        wclrtoeol(statuswin);

    /* abilities (in threeline mode) "In:18 Wi:18 Ch:18" = 17 chars */
    if (threeline) {
        wmove(statuswin, 0, getmaxx(statuswin) - 17);
        wprintw(statuswin, "St:%-2d Dx:%-2d Co:%-2d", pi->st, pi->dx, pi->co);
        wmove(statuswin, 1, getmaxx(statuswin) - 17);
        wprintw(statuswin, "In:%-2d Wi:%-2d Ch:%-2d", pi->in, pi->wi, pi->ch);
    }
}

/* Draws statuses (Ill, FoodPois, etc) and figures out frame color */
static void
draw_statuses(struct nh_player_info *pi, nh_bool threeline)
{
    int i, j, k;
    int mainframe_color = CLR_GRAY;
    j = getmaxx(statuswin) + 1;
    for (i = 0; i < pi->nr_items; i++) {
        int color = CLR_WHITE, colorattr;

        j -= strlen(pi->statusitems[i]) + 1;
        for (k = 0; statuscolors[k].name; k++) {
            if (!strcmp(pi->statusitems[i], statuscolors[k].name)) {
                color = statuscolors[k].color;
                if (statuscolors[k].framecolor != -1)
                    mainframe_color = statuscolors[k].framecolor;
                break;
            }
        }
        colorattr = curses_color_attr(color, 0);
        wmove(statuswin, (threeline ? 2 : 1), j);
        wattron(statuswin, colorattr);
        wprintw(statuswin, "%s", pi->statusitems[i]);
        wattroff(statuswin, colorattr);
    }

    /* frame color */
    if (pi->hp * 7 <= pi->hpmax)
        mainframe_color = CLR_RED;
    else if (pi->hp * 3 <= pi->hpmax &&
             mainframe_color != CLR_BRIGHT_MAGENTA) /* delayed instadeath overrides */
        mainframe_color = CLR_ORANGE;
    else if (pi->hp * 3 <= pi->hpmax * 2 &&
             mainframe_color == CLR_GRAY) /* food trouble+delayed instadeath overrides */
        mainframe_color = CLR_YELLOW;

    if (ui_flags.current_followmode != FM_PLAY)
        mainframe_color = CLR_BLACK;     /* a hint that we can't write */

    /* We change the frame color via palette manipulation, because it's awkward
       to correctly redraw otherwise. However, we don't want to do color
       mapping logic here. So we copy an existing palette entry. */
    uncursed_color fgcode, bgcode;
    pair_content(PAIR_NUMBER(curses_color_attr(mainframe_color, 0)),
                 &fgcode, &bgcode);
    init_pair(MAINFRAME_PAIR, fgcode + (mainframe_color > 7 ? 8 : 0), bgcode);
}

void
curses_update_status(struct nh_player_info *pi)
{
    if (pi)
        player = *pi;

    if (!game_is_running)
        return; /* called before the game is running */

    draw_status(&player, ui_flags.statusheight >= 3);

    /* prevent the cursor from flickering in the status line */
    wmove(mapwin, player.y, player.x);

    if (statuswin)
        wnoutrefresh(statuswin);
}

void
curses_update_status_silent(struct nh_player_info *pi)
{
    if (pi)
        player = *pi;
}
