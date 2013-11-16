/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-16 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <signal.h>
#include <locale.h>
#include "tile.h"

#if !defined(PDCURSES)
/*
 * _nc_unicode_locale(): return true, if the locale supports unicode
 * ncurses has _nc_unicode_locale(), but it is not part of the curses API.
 * For portability this function should probably be reimplemented.
 */
extern int _nc_unicode_locale(void);
#else
# define _nc_unicode_locale() (1)       /* ... as a macro, for example ... */
#endif

WINDOW *basewin, *mapwin, *msgwin, *statuswin, *sidebar;
struct gamewin *firstgw, *lastgw;
int orig_cursor;
const char quit_chars[] = " \r\n\033";

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
    curses_print_message_nonblocking,
};

/*----------------------------------------------------------------------------*/

static char *tileprefix;

void
set_font_file(char *fontfilename)
{
    char *namebuf = malloc(strlen(tileprefix) + strlen(fontfilename) + 1);
    strcpy(namebuf, tileprefix);
    strcat(namebuf, fontfilename);
    set_faketerm_font_file(namebuf);
    free(namebuf);
}
void
set_tile_file(char *tilefilename)
{
    if (!tilefilename) {
        set_tiles_tile_file(NULL, TILES_PER_ROW, TILES_PER_COL);
        return;
    }
    char *namebuf = malloc(strlen(tileprefix) + strlen(tilefilename) + 1);
    strcpy(namebuf, tileprefix);
    strcat(namebuf, tilefilename);
    set_tiles_tile_file(namebuf, TILES_PER_ROW, TILES_PER_COL);
    free(namebuf);
}

void
init_curses_ui(char *dataprefix)
{
    /* set up the default system locale by reading the environment variables */
    setlocale(LC_ALL, "");

    uncursed_set_title("NetHack 4");

    if (!initscr()) {
        fprintf(stderr, "Could not initialise the UI, exiting...\n");
        endwin();
        exit(1);
    }

    tileprefix = strdup(dataprefix);
    set_tile_file(NULL);
    set_font_file("font14.png");

    if (LINES < 24 || COLS < COLNO) {
        fprintf(stderr,
                "Sorry, your terminal is too small for NetHack 4. Current: (%x, %x)\n",
                COLS, LINES);
        endwin();
        exit(1);
    }

    noecho();
    raw();
    nonl();
    meta(basewin, TRUE);
    leaveok(basewin, TRUE);
    orig_cursor = curs_set(1);
    keypad(basewin, TRUE);

    init_nhcolors();
    ui_flags.playmode = MODE_NORMAL;
    ui_flags.unicode = 1;       /* uncursed will back-translate if needed */
    basewin = stdscr;
}


void
exit_curses_ui(void)
{
    cleanup_sidebar(TRUE);
    curs_set(orig_cursor);
    endwin();
    basewin = NULL;
}


static void
draw_frame(void)
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


static void
layout_game_windows(void)
{
    int statusheight;

    ui_flags.draw_frame = ui_flags.draw_sidebar = FALSE;
    statusheight = settings.status3 ? 3 : 2;

    /* 3 variable elements contribute to height: - message area (most
       important) - better status - horizontal frame lines (least important) */

    /* space for the frame? *//* horiz lines */
    if (settings.frame && COLS >= COLNO + 2 &&
        LINES >= ROWNO + 4 + settings.msgheight + statusheight)
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


static void
setup_tiles(void)
{
    switch (settings.graphics) {
    case TILESET_16:
        set_tile_file("tile-16.png");
        break;
    case TILESET_32:
        set_tile_file("tile-32.png");
        break;
    case TILESET_3D:
        set_tile_file("tile-3d.png");
        break;
    default: /* text */
        set_tile_file(NULL);
        wdelete_tiles_region(mapwin);
        return;
    }
    /* only reached if some tileset is selected */
    wset_tiles_region(mapwin, ROWNO, COLNO, 0, 0, ROWNO, COLNO, 0, 0);
}

void
create_game_windows(void)
{
    int statusheight;

    layout_game_windows();
    statusheight = ui_flags.status3 ? 3 : 2;

    werase(basewin);
    if (ui_flags.draw_frame) {
        msgwin = newwin(ui_flags.msgheight, COLNO, 1, 1);
        mapwin = newwin(ROWNO, COLNO, ui_flags.msgheight + 2, 1);
        statuswin =
            derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO + 3,
                   1);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO - 3, 1,
                       COLNO + 2);

        draw_frame();
    } else {
        msgwin = newwin(ui_flags.msgheight, COLNO, 0, 0);
        mapwin = newwin(ROWNO, COLNO, ui_flags.msgheight, 0);
        statuswin =
            derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO, 0);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }

    setup_tiles();

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


static void
resize_game_windows(void)
{
    int statusheight;

    layout_game_windows();

    if (!ui_flags.ingame)
        return;

    /* statuswin and sidebar never accept input, so simply recreating those is
       easiest. */
    delwin(statuswin);
    if (sidebar) {
        delwin(sidebar);
        sidebar = NULL;
    }

    if (mapwin) wdelete_tiles_region(mapwin);

    statusheight = ui_flags.status3 ? 3 : 2;
    if (ui_flags.draw_frame) {
        mvwin(msgwin, 1, 1);
        wresize(msgwin, ui_flags.msgheight, COLNO);
        mvwin(mapwin, ui_flags.msgheight + 2, 1);
        wresize(mapwin, ROWNO, COLNO);
        statuswin =
            derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO + 3,
                   1);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO - 3, 1,
                       COLNO + 2);
        draw_frame();
    } else {
        mvwin(msgwin, 0, 0);
        wresize(msgwin, ui_flags.msgheight, COLNO);
        mvwin(mapwin, ui_flags.msgheight, 0);
        wresize(mapwin, ROWNO, COLNO);
        statuswin =
            derwin(basewin, statusheight, COLNO, ui_flags.msgheight + ROWNO, 0);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }

    if (mapwin) setup_tiles();

    leaveok(statuswin, TRUE);
    if (sidebar)
        leaveok(sidebar, TRUE);

    redraw_game_windows();
    doupdate();
}


void
destroy_game_windows(void)
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


void
redraw_game_windows(void)
{
    struct gamewin *gw;

    wnoutrefresh(basewin);

    if (ui_flags.ingame) {

        wnoutrefresh(mapwin);
        wnoutrefresh(msgwin);

        /* statuswin can become NULL if the terminal is resized to microscopic
           dimensions */
        if (statuswin) {
            wnoutrefresh(statuswin);
        }

        if (sidebar) {
            wnoutrefresh(sidebar);
        }

        draw_frame();
    }

    for (gw = firstgw; gw; gw = gw->next) {
        gw->draw(gw);
        wnoutrefresh(gw->win);
    }
}


void
rebuild_ui(void)
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
        wrefresh(basewin);
    }
}


void
handle_resize(void)
{
    struct gamewin *gw;

    for (gw = firstgw; gw; gw = gw->next) {
        if (!gw->resize)
            continue;

        gw->resize(gw);
        wnoutrefresh(gw->win);
    }

    rebuild_ui();
    doupdate();
}


int
nh_wgetch(WINDOW * win)
{
    int key = 0;

    doupdate(); /* required by pdcurses, noop for ncurses */
    do {
        key = wgetch(win);
        if (key == KEY_HANGUP) {
            nh_exit_game(EXIT_FORCE_SAVE);
            nh_lib_exit();
            exit(0);
        }

        if (key == 0x3 && ui_flags.playmode == MODE_WIZARD) {
            /* we're running in raw mode, so ctrl+c doesn't work. for wizard we 
               emulate this to allow breaking into gdb. */
            raise(SIGINT);
            key = 0;
        }

        if (key == KEY_RESIZE) {
            key = 0;
            handle_resize();
        }

    } while (!key);

    return key;
}


struct gamewin *
alloc_gamewin(int extra)
{
    struct gamewin *gw = malloc(sizeof (struct gamewin) + extra);

    memset(gw, 0, sizeof (struct gamewin) + extra);

    if (firstgw == NULL && lastgw == NULL)
        firstgw = lastgw = gw;
    else {
        lastgw->next = gw;
        gw->prev = lastgw;
        lastgw = gw;
    }

    return gw;
}


void
delete_gamewin(struct gamewin *gw)
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

void
curses_pause(enum nh_pause_reason reason)
{
    doupdate();
    if (reason == P_MESSAGE && msgwin != NULL)
        pause_messages();
    else if (mapwin != NULL)
        /* P_MAP: pause to show the result of detection or similar */
        get_map_key(FALSE);
}


/* expand tabs into proper number of spaces */
static char *
tabexpand(char *sbuf)
{
    char buf[BUFSZ];
    char *bp, *s = sbuf;
    int idx;

    if (!*s)
        return sbuf;

    /* warning: no bounds checking performed */
    for (bp = buf, idx = 0; *s; s++)
        if (*s == '\t') {
            do
                *bp++ = ' ';
            while (++idx % 8);
        } else {
            *bp++ = *s;
            idx++;
        }
    *bp = 0;
    return strcpy(sbuf, buf);
}


void
curses_display_buffer(const char *inbuf, nh_bool trymove)
{
    char *line, *buf;
    char linebuf[BUFSZ];
    int icount, size;
    struct nh_menuitem *items;

    buf = strdup(inbuf);
    icount = 0;
    size = 10;
    items = malloc(size * sizeof (struct nh_menuitem));

    line = strtok(buf, "\n");
    do {
        strncpy(linebuf, line, BUFSZ);
        if (strchr(linebuf, '\t') != 0)
            tabexpand(linebuf);
        add_menu_txt(items, size, icount, linebuf, MI_TEXT);

        line = strtok(NULL, "\n");
    } while (line);

    curses_display_menu(items, icount, NULL, PICK_NONE, PLHINT_ANYWHERE, NULL);
    free(items);
    free(buf);
}


void
curses_raw_print(const char *str)
{
    endwin();
    fprintf(stderr, "%s\n", str);
    refresh();

    curses_msgwin(str);
}


/* sleep for 50 ms */
void
curses_delay_output(void)
{
    doupdate();
#if defined(WIN32)
    Sleep(45);
#else
    usleep(50 * 1000);
#endif
}
