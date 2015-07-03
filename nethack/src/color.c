/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-21 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

static short colorlist[] = {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE,
    COLOR_BLACK + 8, COLOR_RED + 8, COLOR_GREEN + 8, COLOR_YELLOW + 8,
    COLOR_BLUE + 8, COLOR_MAGENTA + 8, COLOR_CYAN + 8, COLOR_WHITE + 8
};

/* Initialize uncursed color pairs to the colors used by NetHack. */
void
init_nhcolors(void)
{
    int bg, fg;
    short bgColor, fgColor;

    start_color();

    /* Set up all color pairs. We now unconditionally use A_BOLD plus colors 0
       to 7 foreground and background, with a total requirement of 64 pairs;
       uncursed folds colors beyond 7 into bold anyway. Note that a white
       background doesn't render correctly on some terminals, thus we use it
       only for solid rock.

       We also set up another 7 pairs for light-color-on-dark color, a
       combination that's not otherwise possible (dark-color-on-dark-color has
       its foreground replaced with black, black-on-dark-color has its
       foreground replaced with darkgray or blue respectively). */
    for (bg = 0; bg <= 8; bg++) {

        /* Do not set up background colors if we're low on color pairs. */
        if (bg == 1 && COLOR_PAIRS < MAINFRAME_PAIR)
            break;

        /* For no background, use black; otherwise use the color from the color
           map. */
        bgColor = bg ? colorlist[bg] : COLOR_BLACK;

        for (fg = 0; fg <= 7; fg++) {

            /* Handle the darkgray setting. */
            fgColor = colorlist[fg];
            if (fgColor == COLOR_BLACK) {
                if (settings.darkgray)
                    fgColor = COLOR_BLACK | 8;
                else
                    fgColor = COLOR_BLUE;
            }

            /* Replace foreground=background with black foreground. */
            if (fgColor == bgColor && fgColor != -1)
                fgColor = COLOR_BLACK;

            init_pair(bg * 8 + fg + 1, fgColor, bgColor);
        }
    }

    if (COLOR_PAIRS >= MAINFRAME_PAIR)
        for (bg = 1; bg <= 7; bg++)
            init_pair(64 + bg, bg | 8, bg);

    /* If we have at least 72 colour pairs, then we use pair 71 for the main
       background frame; this allows us to change its color to warn about
       critical situations via palette changes (which saves having to do a
       bunch of complex redrawing). The default color of the frame is color 7
       from the color map (light gray, if no explicit color was specified). */
    if (COLOR_PAIRS > MAINFRAME_PAIR)
        init_pair(MAINFRAME_PAIR, colorlist[7], colorlist[0]);
}

int
curses_color_attr(int nh_color, int bg_color)
{
    int color = nh_color;
    int cattr = A_NORMAL;

    if (color >= 8) {
        color -= 8;
        if (color == bg_color && color && COLOR_PAIRS >= MAINFRAME_PAIR)
            bg_color = 8; /* "bold foreground on unbold background" */
        else if (color > 0)
            cattr |= A_BOLD;
    }

    if (COLOR_PAIRS >= MAINFRAME_PAIR)
        color += bg_color * 8;

    cattr |= COLOR_PAIR(color + (bg_color == 8 ? 0 : 1));
    return cattr;
}

void
set_darkgray(void)
{
    init_nhcolors();
}

/* color.c */
