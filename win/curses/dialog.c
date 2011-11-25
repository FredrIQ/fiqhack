/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "nhcurses.h"


WINDOW *newdialog(int height, int width)
{
    if (height > LINES) height = LINES;
    if (width > COLS) width = COLS;
    
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    
    WINDOW *win = newwin(height, width, starty, startx);
    keypad(win, TRUE);
    meta(win, TRUE);
    wattron(win, FRAME_ATTRS);
    box(win, 0 , 0);
    wattroff(win, FRAME_ATTRS);
    
    return win;
}


enum nh_direction curses_getdir(const char *query, boolean restricted)
{
    int key;
    enum nh_direction dir;
    
    key = curses_msgwin(query);
    if (key == '.' || key == 's')
	    return DIR_SELF;
    
    dir = key_to_dir(key);
    if (dir == DIR_NONE)
	curses_msgwin("What a strange direction!");
    
    return dir;
}


char curses_yn_function(const char *query, const char *resp, char def)
{
    int width, height, key;
    WINDOW *win;
    char prompt[QBUFSZ];
    char *rb, respbuf[QBUFSZ];

    strcpy(respbuf, resp);
    /* any acceptable responses that follow <esc> aren't displayed */
    if ((rb = strchr(respbuf, '\033')) != 0)
	*rb = '\0';
    sprintf(prompt, "%s [%s] ", query, respbuf);
    if (def)
	sprintf(prompt + strlen(prompt), "(%c) ", def);
    
    height = 3;
    width = strlen(prompt) + 5;
    win = newdialog(height, width);
    mvwprintw(win, 1, 2, prompt);
    wrefresh(win);
    curs_set(1);
    
    do {
	key = tolower(nh_wgetch(win));
	
	if (key == '\033') {
	    if (strchr(resp, 'q'))
		key = 'q';
	    else if (strchr(resp, 'n'))
		key = 'n';
	    else
		key = def;
	    break;
	} else if (strchr(quitchars, key)) {
	    key = def;
	    break;
	}
	
	if (!strchr(resp, key))
	    key = 0;
	
    } while (!key);
    
    curs_set(0);
    delwin(win);
    redraw_game_windows();
    return key;
}


char curses_query_key(const char *query, int *count)
{
    int width, height, key;
    WINDOW *win;
    int cnt = 0;
    boolean hascount = FALSE;
    
    height = 3;
    width = strlen(query) + 4;
    win = newdialog(height, width);
    mvwprintw(win, 1, 2, query);
    wrefresh(win);
    
    curs_set(1);
    key = nh_wgetch(win);
    while ((isdigit(key) || key == KEY_BACKSPACE) && count != NULL) {
	cnt = 10*cnt + (key - '0');
	key = nh_wgetch(win);
	hascount = TRUE;
    }
    
    if (count != NULL) {
	if (!hascount && !cnt)
	    cnt = -1; /* signal to caller that no count was given */
	*count = cnt;
    }
    
    curs_set(0);
    delwin(win);
    redraw_game_windows();
    return key;
}


int curses_msgwin(const char *msg)
{
    int key, len;
    int width = strlen(msg) + 4;
    int prevcurs = curs_set(0); /* cursor might be on if called from getpos() */
    WINDOW *win = newdialog(3, width);
    
    len = strlen(msg);
    while (isspace(msg[len-1]))
	len--;
    
    mvwaddnstr(win, 1, 2, msg, len);
    wrefresh(win);
    key = nh_wgetch(win); /* wait for any key */
    
    delwin(win);
    curs_set(prevcurs);
    redraw_game_windows();
    
    return key;
}

