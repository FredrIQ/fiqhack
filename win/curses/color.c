/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

struct color {
    short r, g, b;
};

static struct color oyellow, owhite, ohired, ohigreen, ohiyellow, ohiblue,
                    ohimagenta, ohicyan, ohiwhite;

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
    set_darkgray(); /* this will init pair 1 (black) */
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


int curses_color_attr(int nh_color)
{
    int color = nh_color + 1;
    int cattr = A_NORMAL;
    
    if (COLORS < 16 && color > 8) {
	color -= 8;
	cattr = A_BOLD;
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
}

/* color.c */
