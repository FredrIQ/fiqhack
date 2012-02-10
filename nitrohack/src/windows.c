/* Copyright (c) Daniel Thaler, 2011.                             */
/* NitroHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <signal.h>
#include <locale.h>

#if !defined(PDCURSES)
/*
 * _nc_unicode_locale(): return true, if the locale supports unicode
 * ncurses has _nc_unicode_locale(), but it is not part of the curses API.
 * For portability this function should probably be reimplemented.
 */
extern int _nc_unicode_locale(void);
#else
# define set_escdelay(x)
# define _nc_unicode_locale() (1) /* ... as a macro, for example ... */
#endif

WINDOW *basewin, *mapwin, *msgwin, *statuswin, *sidebar;
struct gamewin *firstgw, *lastgw;
int orig_cursor;
const char quit_chars[] = " \r\n\033";
static SCREEN *curses_scr;

struct nh_window_procs curses_windowprocs = {
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
    
    if (LINES < 24 || COLS < COLNO) {
	fprintf(stderr, "Sorry, your terminal is too small for NitroHack. Current: (%x, %x)\n", COLS, LINES);
	endwin();
	exit(0);
    }
    
    noecho();
    raw();
    nonl();
    meta(basewin, TRUE);
    leaveok(basewin, TRUE);
    orig_cursor = curs_set(1);
    keypad(basewin, TRUE);
    set_escdelay(20);
    
    init_nhcolors();
    ui_flags.playmode = MODE_NORMAL;
    ui_flags.unicode = _nc_unicode_locale();
    
    /* with PDCurses/Win32 stdscr is not NULL before newterm runs, which caused
     * crashes. So basewin is a copy of stdscr which is known to be NULL before
     * curses is inited. */
    basewin = stdscr;

#if defined(PDCURSES)
    PDC_set_title("NitroHack");
#if defined(WIN32)
    if (settings.win_height > 0 && settings.win_width > 0)
	resize_term(settings.win_height, settings.win_width);
#endif
#endif
}


void exit_curses_ui(void)
{
    cleanup_sidebar(TRUE);
    curs_set(orig_cursor);
    endwin();
    delscreen(curses_scr);
    basewin = NULL;
}


static void draw_frame(void)
{
    if (!ui_flags.draw_frame)
	return;
    
    /* vertical lines */
    mvwvline(basewin, 1, 0, ACS_VLINE, ui_flags.viewheight);
    mvwvline(basewin, 1, COLNO + 1, ACS_VLINE, ui_flags.viewheight);

    /* horizontal top line above the message win */
    mvwaddch(basewin, 0, 0, ACS_ULCORNER);
    whline(basewin, ACS_HLINE, COLNO);
    mvwaddch(basewin, 0, COLNO + 1, ACS_URCORNER);
    
    /* horizontal line between message and map windows */
    mvwaddch(basewin, 1 + ui_flags.msgheight, 0, ACS_LTEE);
    whline(basewin, ACS_HLINE, COLNO);
    mvwaddch(basewin, 1 + ui_flags.msgheight, COLNO + 1, ACS_RTEE);
    
    /* horizontal line between map and status */
    mvwaddch(basewin, 2 + ui_flags.msgheight + ROWNO, 0, ACS_LTEE);
    whline(basewin, ACS_HLINE, COLNO);
    mvwaddch(basewin, 2 + ui_flags.msgheight + ROWNO, COLNO + 1, ACS_RTEE);
    
    /* horizontal bottom line */
    mvwaddch(basewin, ui_flags.viewheight + 1, 0, ACS_LLCORNER);
    whline(basewin, ACS_HLINE, COLNO);
    mvwaddch(basewin, ui_flags.viewheight + 1, COLNO + 1, ACS_LRCORNER);
    
    if (!ui_flags.draw_sidebar)
	return;
    
    mvwaddch(basewin, 0, COLNO + 1, ACS_TTEE);
    whline(basewin, ACS_HLINE, COLS - COLNO - 3);
    mvwaddch(basewin, 0, COLS - 1, ACS_URCORNER);
    
    mvwaddch(basewin, ui_flags.viewheight + 1, COLNO + 1, ACS_BTEE);
    whline(basewin, ACS_HLINE, COLS - COLNO - 3);
    mvwaddch(basewin, ui_flags.viewheight + 1, COLS - 1, ACS_LRCORNER);
    
    mvwvline(basewin, 1, COLS - 1, ACS_VLINE, ui_flags.viewheight);
}


static void layout_game_windows(void)
{
    int statusheight;
    ui_flags.draw_frame = ui_flags.draw_sidebar = FALSE;
    statusheight = settings.status3 ? 3 : 2;
    
    /* 3 variable elements contribute to height: 
     *  - message area (most important)
     *  - better status
     *  - horizontal frame lines (least important)
     */
    
    /* space for the frame? */
    if (settings.frame && COLS >= COLNO + 2 &&
	LINES >= ROWNO + 4 /* horiz lines */ + settings.msgheight + statusheight)
	ui_flags.draw_frame = TRUE;
    
    if (settings.sidebar && COLS >= COLNO + 20)
	ui_flags.draw_sidebar = TRUE;
    
    /* create subwindows */
    if (ui_flags.draw_frame) {
	ui_flags.msgheight = settings.msgheight;
	ui_flags.status3 = settings.status3;
	ui_flags.viewheight = ui_flags.msgheight + ROWNO + statusheight + 2;
    } else {
	int spare_lines = LINES - ROWNO - 2;
	spare_lines = spare_lines >= 1 ? spare_lines : 1;
	ui_flags.msgheight = min(settings.msgheight, spare_lines);
	if (ui_flags.msgheight < spare_lines)
	    ui_flags.status3 = settings.status3;
	else {
	    ui_flags.status3 = FALSE;
	    statusheight = 2;
	}
	    
	ui_flags.viewheight = ui_flags.msgheight + ROWNO + statusheight;
    }
}


void create_game_windows(void)
{
    int statusheight;

    layout_game_windows();
    statusheight = ui_flags.status3 ? 3 : 2;
    
    werase(basewin);
    if (ui_flags.draw_frame) {
	msgwin = newwin(ui_flags.msgheight, COLNO, 1, 1);
	mapwin = newwin(ROWNO, COLNO, ui_flags.msgheight + 2, 1);
	statuswin = derwin(basewin, statusheight, COLNO,
			   ui_flags.msgheight + ROWNO + 3, 1);
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(basewin, ui_flags.viewheight, COLS - COLNO - 3, 1, COLNO+2);
	
	draw_frame();
    } else {
	msgwin = newwin(ui_flags.msgheight, COLNO, 0, 0);
	mapwin = newwin(ROWNO, COLNO, ui_flags.msgheight, 0);
	statuswin = derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO, 0);
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }
    
    keypad(mapwin, TRUE);
    keypad(msgwin, TRUE);
    leaveok(mapwin, FALSE);
    leaveok(msgwin, FALSE);
    leaveok(statuswin, TRUE);
    if (sidebar)
	leaveok(sidebar, TRUE);
    
    ui_flags.ingame = TRUE;
    redraw_game_windows();
}


static void resize_game_windows(void)
{
    int statusheight = ui_flags.status3 ? 3 : 2;
    
    layout_game_windows();
    
    if (!ui_flags.ingame)
	return;
    
    /* statuswin and sidebar never accept input, so simply recreating those is
     * easiest. */
    delwin(statuswin);
    if (sidebar) {
	delwin(sidebar);
	sidebar = NULL;
    }
    
    /* ncurses might have automatically changed the window sizes in resizeterm
     * while trying to do the right thing. Of course no size other than
     * COLNO x ROWNO is ever right for the map... */
    wresize(msgwin, ui_flags.msgheight, COLNO);
    wresize(mapwin, ROWNO, COLNO);
    
    if (ui_flags.draw_frame) {
	mvwin(msgwin, 1, 1);
	mvwin(mapwin, ui_flags.msgheight + 2, 1);
	statuswin = derwin(basewin, statusheight, COLNO,
			   ui_flags.msgheight + ROWNO + 3, 1);
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(basewin, ui_flags.viewheight, COLS - COLNO - 3, 1, COLNO+2);
	draw_frame();
    } else {
	mvwin(msgwin, 0, 0);
	mvwin(mapwin, ui_flags.msgheight, 0);
	statuswin = derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO, 0);
	
	if (ui_flags.draw_sidebar)
	    sidebar = derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }
    
    leaveok(statuswin, TRUE);
    if (sidebar)
	leaveok(sidebar, TRUE);
    
    redraw_game_windows();
    doupdate();
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
    
    redrawwin(basewin);
    wnoutrefresh(basewin);
    
    if (ui_flags.ingame) {
	redrawwin(mapwin);
	redrawwin(msgwin);
	
	wnoutrefresh(mapwin);
	wnoutrefresh(msgwin);
	
	/* statuswin can become NULL if the terminal is resized to microscopic dimensions */
	if (statuswin) {
	    redrawwin(statuswin);
	    wnoutrefresh(statuswin);
	}
	
	if (sidebar) {
	    redrawwin(sidebar);
	    wnoutrefresh(sidebar);
	}
	
	draw_frame();
    }
    
    for (gw = firstgw; gw; gw = gw->next) {
	gw->draw(gw);
	redrawwin(gw->win);
	wnoutrefresh(gw->win);
    }
}


void rebuild_ui(void)
{
    if (ui_flags.ingame) {
	wclear(basewin);
	resize_game_windows();
	
	/* some windows are now empty because they were re-created */
	draw_msgwin();
	draw_map(player.x, player.y);
	curses_update_status(&player);
	draw_sidebar();
	
	redraw_game_windows();
    } else if (basewin) {
	redrawwin(basewin);
	wrefresh(basewin);
    }
}


void handle_resize(void)
{
    struct gamewin *gw;

    for (gw = firstgw; gw; gw = gw->next) {
	if (!gw->resize)
	    continue;

	gw->resize(gw);
	redrawwin(gw->win);
	wnoutrefresh(gw->win);
    }

    rebuild_ui();
    doupdate();
}


#define META(c)  ((c)|0x80) /* bit 8 */
int nh_wgetch(WINDOW *win)
{
    int key = 0;
    
    doupdate(); /* required by pdcurses, noop for ncurses */
    do {
	key = wgetch(win);
#ifdef UNIX
	if (key == 0x3 && ui_flags.playmode == MODE_WIZARD) {
	    /* we're running in raw mode, so ctrl+c doesn't work.
	     * for wizard we emulate this to allow breaking into gdb. */
	    kill(0, SIGINT);
	    key = 0;
	}
#endif

	if (key == KEY_RESIZE) {
	    key = 0;
#ifdef PDCURSES
	    resize_term(0, 0);
#endif
	    handle_resize();
	}

#if !defined(WIN32)
	/* "hackaround": some terminals / shells / whatever don't directly pass
	 * on any combinations with the alt key. Instead these become ESC,<key>
	 * Try to reverse that here...
	 */
	if (key == KEY_ESC) {
	    int key2;
	    
	    nodelay(win, TRUE);
	    key2 = wgetch(win); /* check for a following letter */
	    nodelay(win, FALSE);
	    
	    if ('a' <= key2 && key2 <= 'z')
		key = META(key2);
	}
#endif
	    
    } while (!key);
    
#if defined(PDCURSES)
    /* PDCurses provides exciting new names for the enter key.
     * Translate these here, instead of checking for them all over the place. */
    if (key == PADENTER)
	key = '\r';
#endif
    
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
    doupdate();
    if (reason == P_MESSAGE && msgwin != NULL)
	pause_messages();
    else if (mapwin != NULL)
	/* P_MAP: pause to show the result of detection or similar */
	get_map_key(FALSE);
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


void curses_display_buffer(const char *inbuf, nh_bool trymove)
{
    char *line, *buf;
    char linebuf[BUFSZ];
    int icount, size;
    struct nh_menuitem *items;
    
    buf = strdup(inbuf);
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
    free(buf);
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
    doupdate();
#if defined(WIN32)
    Sleep(45);
#else
    usleep(50 * 1000);
#endif
}

