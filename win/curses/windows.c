/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include "nhcurses.h"

/*
 * _nc_unicode_locale(): return true, if the locale supports unicode
 * ncurses has _nc_unicode_locale(), but it is not part of the curses API.
 * For portability this function should probably be reimplemented.
 */
extern int _nc_unicode_locale(void);

WINDOW *mapwin, *msgwin, *statuswin, *sidebar;
struct gamewin *firstgw, *lastgw;
int orig_cursor;
const char quitchars[] = " \r\n\033";
static SCREEN *curses_scr;

struct nh_window_procs curses_windowprocs = {
    curses_player_selection,
    curses_clear_map,
    curses_pause,
    curses_display_buffer,
    curses_update_status,
    curses_print_message,
    curses_display_menu,
    curses_display_objects,
    curses_list_items,
    curses_update_screen,
    curses_raw_print,
    curses_query_key,
    curses_getpos,
    curses_getdir,
    curses_yn_function,
    curses_getline,
    curses_delay_output,
    curses_notify_level_changed,
    curses_outrip,
};

/*----------------------------------------------------------------------------*/

void init_curses_ui(void)
{
    /* set up the default system locale by reading the environment variables */
    setlocale(LC_ALL, "");
    
    curses_scr = newterm(NULL, stdout, stdin);
    set_term(curses_scr);
    
    noecho();
    raw();
    meta(stdscr, TRUE);
    orig_cursor = curs_set(0);
    keypad(stdscr, TRUE);
    set_escdelay(20);
    
    init_nhcolors();
    ui_flags.playmode = MODE_NORMAL;
    ui_flags.unicode = _nc_unicode_locale();
}


void exit_curses_ui(void)
{
    cleanup_sidebar(TRUE);
    curs_set(orig_cursor);
    endwin();
    delscreen(curses_scr);
}


void draw_frame(void)
{
    if (!ui_flags.draw_frame)
	return;
    
    /* vertical lines */
    mvwvline(stdscr, 1, 0, ACS_VLINE, 4 + ui_flags.msgheight + ROWNO);
    mvwvline(stdscr, 1, COLNO + 1, ACS_VLINE, 4 + ui_flags.msgheight + ROWNO);

    /* horizontal top line above the message win */
    mvwaddch(stdscr, 0, 0, ACS_ULCORNER);
    whline(stdscr, ACS_HLINE, COLNO);
    mvwaddch(stdscr, 0, COLNO + 1, ACS_URCORNER);
    
    /* horizontal line between message and map windows */
    mvwaddch(stdscr, 1 + ui_flags.msgheight, 0, ACS_LTEE);
    whline(stdscr, ACS_HLINE, COLNO);
    mvwaddch(stdscr, 1 + ui_flags.msgheight, COLNO + 1, ACS_RTEE);
    
    /* horizontal line between map and status */
    mvwaddch(stdscr, 2 + ui_flags.msgheight + ROWNO, 0, ACS_LTEE);
    whline(stdscr, ACS_HLINE, COLNO);
    mvwaddch(stdscr, 2 + ui_flags.msgheight + ROWNO, COLNO + 1, ACS_RTEE);
    
    /* horizontal bottom line */
    mvwaddch(stdscr, ui_flags.viewheight + 1, 0, ACS_LLCORNER);
    whline(stdscr, ACS_HLINE, COLNO);
    mvwaddch(stdscr, ui_flags.viewheight + 1, COLNO + 1, ACS_LRCORNER);
    
    if (!ui_flags.draw_sidebar)
	return;
    
    mvwaddch(stdscr, 0, COLNO + 1, ACS_TTEE);
    whline(stdscr, ACS_HLINE, COLS - COLNO - 3);
    mvwaddch(stdscr, 0, COLS - 1, ACS_URCORNER);
    
    mvwaddch(stdscr, ui_flags.viewheight + 1, COLNO + 1, ACS_BTEE);
    whline(stdscr, ACS_HLINE, COLS - COLNO - 3);
    mvwaddch(stdscr, ui_flags.viewheight + 1, COLS - 1, ACS_LRCORNER);
    
    mvwvline(stdscr, 1, COLS - 1, ACS_VLINE, ui_flags.viewheight);
}


void create_game_windows(void)
{
    ui_flags.draw_frame = ui_flags.draw_sidebar = FALSE;
    
    if (settings.frame && COLS >= COLNO + 2 &&
	LINES >= ROWNO + 4 /* horiz frame lines */ + 1 /* messages */ + 2 /* status */)
	ui_flags.draw_frame = TRUE;
    
    if (settings.sidebar && COLS >= COLNO + 20)
	ui_flags.draw_sidebar = TRUE;
    
    /* create subwindows */
    if (ui_flags.draw_frame) {
	int spare_lines = LINES - ROWNO - 4 - 2;
	ui_flags.msgheight = min(settings.msgheight, spare_lines);
	
	msgwin = derwin(stdscr, ui_flags.msgheight, COLNO, 1, 1);
	mapwin = derwin(stdscr, ROWNO, COLNO, ui_flags.msgheight + 2, 1);
	statuswin = derwin(stdscr, 2, COLNO, ui_flags.msgheight + ROWNO + 3, 1);
	ui_flags.viewheight = ui_flags.msgheight + ROWNO + 2 + 2;
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(stdscr, ui_flags.viewheight, COLS - COLNO - 3, 1, COLNO+2);
	
	draw_frame();
    } else {
	int spare_lines = LINES - ROWNO - 2;
	ui_flags.msgheight = min(settings.msgheight, spare_lines);
	
	msgwin = derwin(stdscr, ui_flags.msgheight, COLNO, 0, 0);
	mapwin = derwin(stdscr, ROWNO, COLNO, ui_flags.msgheight, 0);
	statuswin = derwin(stdscr, 2, COLNO, ui_flags.msgheight + ROWNO, 0);
	ui_flags.viewheight = ui_flags.msgheight + ROWNO + 2;
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(stdscr, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }
    
    keypad(mapwin, TRUE);
    keypad(msgwin, TRUE);
    
    ui_flags.ingame = TRUE;
    redraw_game_windows();
}


void destroy_game_windows(void)
{
    if (ui_flags.ingame) {
	delwin(msgwin);
	delwin(mapwin);
	delwin(statuswin);
	if (sidebar || ui_flags.draw_sidebar) {
	    cleanup_sidebar(FALSE);
	    delwin(sidebar);
	}
	msgwin = mapwin = statuswin = sidebar = NULL;
    }
    
    ui_flags.ingame = FALSE;
}


void redraw_game_windows(void)
{
    struct gamewin *gw;
    
    redrawwin(stdscr);
    wnoutrefresh(stdscr);
    
    if (ui_flags.ingame) {
	redrawwin(mapwin);
	redrawwin(msgwin);
	redrawwin(statuswin);
	
	wnoutrefresh(mapwin);
	wnoutrefresh(msgwin);
	wnoutrefresh(statuswin);
	
	if (ui_flags.draw_sidebar) {
	    redrawwin(sidebar);
	    wnoutrefresh(sidebar);
	}
    }
    
    for (gw = firstgw; gw; gw = gw->next) {
	gw->draw(gw);
	redrawwin(gw->win);
	wnoutrefresh(gw->win);
    }
    
    doupdate();
}


void rebuild_ui(void)
{
    if (ui_flags.ingame) {
	destroy_game_windows();
	clear();
	create_game_windows();
	
	/* map, status and messagewin are now empty, because they were re-created */
	draw_msgwin();
	draw_map(0);
	curses_update_status(&player);
	draw_sidebar();
	
	/* draw all dialogs on top of the basic windows */
	redraw_game_windows();
    }
}


int nh_wgetch(WINDOW *win)
{
    int key = 0;
    
    do {
	key = wgetch(win);
#ifdef UNIX
	if (key == 0x3 && ui_flags.playmode == MODE_WIZARD) {
	    /* we're running in raw mode, so ctrl+c doesn't work.
	     * for wizard we emulate this to allow breaking into gdb. */
	    curs_set(orig_cursor);
	    kill(0, SIGINT);
	    curs_set(0);
	    key = 0;
#endif
	}
#ifdef ALLOW_RESIZE
	if (key == KEY_RESIZE) {
	    struct gamewin *gw;
	    key = 0;
	    rebuild_ui();
	    
	    for (gw = firstgw; gw; gw = gw->next) {
		if (!gw->resize)
		    continue;
		
		gw->resize(gw);
		redrawwin(gw->win);
		wnoutrefresh(gw->win);
	    }
	}
#endif
    } while (!key);
    
    return key;
}


struct gamewin *alloc_gamewin(int extra)
{
    struct gamewin *gw = malloc(sizeof(struct gamewin) + extra);
    memset(gw, 0, sizeof(struct gamewin) + extra);
    
    if (firstgw == NULL && lastgw == NULL)
	firstgw = lastgw = gw;
    else {
	lastgw->next = gw;
	gw->prev = lastgw;
	lastgw = gw;
    }
    
    return gw;
}


void delete_gamewin(struct gamewin *gw)
{
    if (firstgw == gw)
	firstgw = gw->next;
    if (lastgw == gw)
	lastgw = gw->prev;
    
    if (gw->prev)
	gw->prev->next = gw->next;
    if (gw->next)
	gw->next->prev = gw->prev;
    
    free(gw);
}


/*----------------------------------------------------------------------------*/
/* misc api functions */

void curses_pause(enum nh_pause_reason reason)
{
    if (reason == P_MESSAGE)
	pause_messages();
    else
	/* P_MAP: pause to show the result of detection or similar */
	nh_wgetch(stdscr);
}


/* expand tabs into proper number of spaces */
static char *tabexpand(char *sbuf)
{
    char buf[BUFSZ];
    char *bp, *s = sbuf;
    int idx;

    if (!*s) return sbuf;

    /* warning: no bounds checking performed */
    for (bp = buf, idx = 0; *s; s++)
	if (*s == '\t') {
	    do *bp++ = ' '; while (++idx % 8);
	} else {
	    *bp++ = *s;
	    idx++;
	}
    *bp = 0;
    return strcpy(sbuf, buf);
}


void curses_display_buffer(char *buf, boolean trymove)
{
    char *line;
    char linebuf[BUFSZ];
    int icount, size;
    struct nh_menuitem *items;
    
    icount = 0;
    size = 10;
    items = malloc(size * sizeof(struct nh_menuitem));


    line = strtok(buf, "\n");
    do {
	strncpy(linebuf, line, BUFSZ);
	if (strchr(linebuf, '\t') != 0)
	    tabexpand(linebuf);
	add_menu_txt(items, size, icount, linebuf, MI_TEXT);
	
	line = strtok(NULL, "\n");
    } while (line);
    
    curses_display_menu(items, icount, NULL, PICK_NONE, NULL);
    free(items);
}


void curses_raw_print(const char *str)
{
    endwin();
    fprintf(stderr, "%s\n", str);
    refresh();
    
    curses_msgwin(str);
}


/* sleep for 50 ms */
void curses_delay_output(void)
{
    usleep(50 * 1000);
}

