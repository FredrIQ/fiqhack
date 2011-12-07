/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

/* A normal tombstone for end of game display. */
static const char *rip_txt[] = {
    "                       ----------",
    "                      /          \\",
    "                     /    REST    \\",
    "                    /      IN      \\",
    "                   /     PEACE      \\",
    "                  /                  \\",
    "                  |                  |", /* Name of player */
    "                  |                  |", /* Amount of $ */
    "                  |                  |", /* Type of death */
    "                  |                  |", /* . */
    "                  |                  |", /* . */
    "                  |                  |", /* . */
    "                  |       1001       |", /* Real year of death */
    "                 *|     *  *  *      | *",
    "        _________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______",
    NULL
};
#define STONE_LINE_CENT 28	/* char[] element of center of stone face */
#define STONE_LINE_LEN 16	/* # chars that fit on one line
				 * (note 1 ' ' border)
				 */
#define NAME_LINE 6		/* *char[] line # for player name */
#define GOLD_LINE 7		/* *char[] line # for amount of gold */
#define DEATH_LINE 8		/* *char[] line # for death description */
#define YEAR_LINE 12		/* *char[] line # for year */

static void center(char *line, char *text)
{
	char *ip,*op;
	ip = text;
	op = &line[STONE_LINE_CENT - ((strlen(text)+1)>>1)];
	while (*ip) *op++ = *ip++;
}


void curses_outrip(struct nh_menuitem *items, int icount, boolean tombstone,
		          char *plname, long gold, char *killbuf, int year)
{
    char **dp, **rip;
    char *dpx;
    char buf[BUFSZ];
    int x, i;
    int line, txtpos = 0;
    
    /* clear all game windows and print to the screen directly.
     * Those windows will be destroyed later. */
    clear();
    
    if (tombstone) {
	rip = dp = malloc(sizeof(rip_txt));
	for (x = 0; rip_txt[x]; x++)
	    dp[x] = strdup(rip_txt[x]);
	dp[x] = NULL;

	/* Put name on stone */
	sprintf(buf, "%s", plname);
	buf[STONE_LINE_LEN] = 0;
	center(rip[NAME_LINE], buf);

	/* Put $ on stone */
	sprintf(buf, "%ld Au", gold);
	buf[STONE_LINE_LEN] = 0; /* It could be a *lot* of gold :-) */
	center(rip[GOLD_LINE], buf);

	strcpy(buf, killbuf);
	/* Put death type on stone */
	for (line=DEATH_LINE, dpx = buf; line<YEAR_LINE; line++) {
	    int i,i0;
	    char tmpchar;
	    
	    /* break up long text */
	    if ( (i0=strlen(dpx)) > STONE_LINE_LEN) {
		for (i = STONE_LINE_LEN;
		    ((i0 > STONE_LINE_LEN) && i); i--)
			if (dpx[i] == ' ') i0 = i;
		if (!i) i0 = STONE_LINE_LEN;
	    }
	    tmpchar = dpx[i0];
	    dpx[i0] = 0;
	    center(rip[line], dpx);
	    if (tmpchar != ' ') {
		dpx[i0] = tmpchar;
		dpx= &dpx[i0];
	    } else
		dpx= &dpx[i0+1];
	}

	/* Put year on stone */
	sprintf(buf, "%4d", year);
	center(rip[YEAR_LINE], buf);

	for (i = 0; rip[i]; i++) {
	    mvaddstr(i+1, 0, rip[i]);
	    free(rip[x]);
	}
	free(rip);
	rip = NULL;
	txtpos = sizeof(rip_txt)/sizeof(rip_txt[0])+ 2;
    }
    
    for (i = 0; i < icount; i++)
	mvaddstr(txtpos + i, 0, items[i].caption);
    mvaddstr(LINES-1, 0, "--More--");
    
    refresh();
    getch();
}
