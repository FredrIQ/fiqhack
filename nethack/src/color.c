/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

struct color {
    short r, g, b;
};

static struct color oyellow, owhite, ohired, ohigreen, ohiyellow, ohiblue,
                    ohimagenta, ohicyan, ohiwhite;

short colorlist[] = {COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
                     COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, -1,
                     COLOR_WHITE, COLOR_RED+8, COLOR_GREEN+8, COLOR_YELLOW+8,
                     COLOR_BLUE+8, COLOR_MAGENTA+8, COLOR_CYAN+8, COLOR_WHITE+8};

/*
 * Initialize curses colors to colors used by NetHack
 * (from Karl Garrison's curses UI for Nethack 3.4.3)
 */
void init_nhcolors(void)
{
    if (!has_colors())
	return;
    
    ui_flags.color = TRUE;
    
    start_color();
    use_default_colors();
    set_darkgray(); /* this will init pair 1 (black), and other
                       color+darkgray combos */
    init_pair(2, COLOR_RED, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_BLUE, -1);
    init_pair(6, COLOR_MAGENTA, -1);
    init_pair(7, COLOR_CYAN, -1);
    init_pair(8, -1, -1);

    if (COLORS >= 16) {
	init_pair(9, COLOR_WHITE, -1);
	init_pair(10, COLOR_RED + 8, -1);
	init_pair(11, COLOR_GREEN + 8, -1);
	init_pair(12, COLOR_YELLOW + 8, -1);
	init_pair(13, COLOR_BLUE + 8, -1);
	init_pair(14, COLOR_MAGENTA + 8, -1);
	init_pair(15, COLOR_CYAN + 8, -1);
	init_pair(16, COLOR_WHITE + 8, -1);
    }

    /* Set up background colors too. We have 6 possible non-default
       background colors (don't use white, there are terminals that
       hate it), combined with 16 possible foregrounds for each,
       equals 96 more pairs. Or 48 more if we're using bold. */
    if (COLOR_PAIRS >= 113 || (COLORS < 16 && COLOR_PAIRS >= 57)) {
        int bg, fg;
        for (bg = 1; bg <= 6; bg++) {
            /* skip darkgray, use_darkgray does that */
            for (fg = 1; fg <= (COLORS >= 16 ? 16 : 8); fg++) {
                init_pair(bg * (COLORS >= 16 ? 16 : 8) + fg + 1,
                          fg == bg ? COLOR_BLACK : fg, bg);
            }
        }
    }

    if (!can_change_color())
	return;
    
    /* Preserve initial terminal colors */
    color_content(COLOR_YELLOW, &oyellow.r, &oyellow.g, &oyellow.b);
    color_content(COLOR_WHITE, &owhite.r, &owhite.g, &owhite.b);
    
    /* Set colors to appear as NetHack expects */
    init_color(COLOR_YELLOW, 500, 300, 0);
    init_color(COLOR_WHITE, 600, 600, 600);
    
    if (COLORS >= 16) {
	/* Preserve initial terminal colors */
	color_content(COLOR_RED + 8, &ohired.r, &ohired.g, &ohired.b);
	color_content(COLOR_GREEN + 8, &ohigreen.r, &ohigreen.g, &ohigreen.b);
	color_content(COLOR_YELLOW + 8, &ohiyellow.r, &ohiyellow.g, &ohiyellow.b);
	color_content(COLOR_BLUE + 8, &ohiblue.r, &ohiblue.g, &ohiblue.b);
	color_content(COLOR_MAGENTA + 8, &ohimagenta.r, &ohimagenta.g, &ohimagenta.b);
	color_content(COLOR_CYAN + 8, &ohicyan.r, &ohicyan.g, &ohicyan.b);
	color_content(COLOR_WHITE + 8, &ohiwhite.r, &ohiwhite.g, &ohiwhite.b);
    
	/* Set colors to appear as NetHack expects */
	init_color(COLOR_RED + 8, 1000, 500, 0);
	init_color(COLOR_GREEN + 8, 0, 1000, 0);
	init_color(COLOR_YELLOW + 8, 1000, 1000, 0);
	init_color(COLOR_BLUE + 8, 0, 0, 1000);
	init_color(COLOR_MAGENTA + 8, 1000, 0, 1000);
	init_color(COLOR_CYAN + 8, 0, 1000, 1000);
	init_color(COLOR_WHITE + 8, 1000, 1000, 1000);
    }
}


int curses_color_attr(int nh_color, int bg_color)
{
    int color = nh_color + 1;
    int cattr = A_NORMAL;
    
    if (COLORS < 16 && color > 8) {
	color -= 8;
	cattr = A_BOLD;
    }
    if (COLOR_PAIRS >= 113 || (COLORS < 16 && COLOR_PAIRS >= 57)) {
        color += bg_color * (COLORS >= 16 ? 16 : 8);
    }
    cattr |= COLOR_PAIR(color);
    
    if (color == 1 && settings.darkgray)
	cattr |= A_BOLD;
	
    return cattr;
}


void set_darkgray(void)
{
    if (settings.darkgray)
	init_pair(1, COLOR_BLACK, -1);
    else
	init_pair(1, COLOR_BLUE, -1);

    if (COLOR_PAIRS >= 113 || (COLORS < 16 && COLOR_PAIRS >= 57)) {
        int bg;
        for (bg = 1; bg <= 6; bg++) {
            init_pair(bg * (COLORS >= 16 ? 16 : 8) + 1,
                      settings.darkgray ? COLOR_BLACK : COLOR_BLUE, bg);
        }
    }
}

/* color.c */
