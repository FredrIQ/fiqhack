/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-17 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <signal.h>
#include <locale.h>
#include <time.h>
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
    curses_request_command,
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
    curses_server_cancel,
};

/*----------------------------------------------------------------------------*/

static char *tileprefix;

void
set_font_file(const char *fontfilename)
{
    char namebuf[strlen(tileprefix) + strlen(fontfilename) + 1];
    strcpy(namebuf, tileprefix);
    strcat(namebuf, fontfilename);
    set_faketerm_font_file(namebuf);
}
void
set_tile_file(const char *tilefilename)
{
    if (!tilefilename) {
        set_tiles_tile_file(NULL, TILES_PER_ROW, TILES_PER_COL);
        return;
    }
    char namebuf[strlen(tileprefix) + strlen(tilefilename) + 1];
    strcpy(namebuf, tileprefix);
    strcat(namebuf, tilefilename);
    set_tiles_tile_file(namebuf, TILES_PER_ROW, TILES_PER_COL);
}

void
init_curses_ui(const char *dataprefix)
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

    if (LINES < ROWNO + 3 || COLS < COLNO + 1) {
        fprintf(stderr,
                "Sorry, your terminal is too small for NetHack 4. Current: "
                "(%x, %x)\n", COLS, LINES);
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


enum framechars {
    FC_HLINE, FC_VLINE,
    FC_ULCORNER, FC_URCORNER, FC_LLCORNER, FC_LRCORNER,
    FC_LTEE, FC_RTEE, FC_TTEE, FC_BTEE
};

static const char ascii_borders[] = {
    [FC_HLINE] = '-', [FC_VLINE] = '|',
    [FC_ULCORNER] = '-', [FC_URCORNER] = '-',
    [FC_LLCORNER] = '-', [FC_LRCORNER] = '-',
    [FC_LTEE] = '|', [FC_RTEE] = '|', [FC_TTEE] = '-', [FC_BTEE] = '-',
};
static const cchar_t *const *const unicode_borders[] = {
    [FC_HLINE] = &WACS_HLINE , [FC_VLINE] = &WACS_VLINE,
    [FC_ULCORNER] = &WACS_ULCORNER, [FC_URCORNER] = &WACS_URCORNER,
    [FC_LLCORNER] = &WACS_LLCORNER, [FC_LRCORNER] = &WACS_LRCORNER,
    [FC_LTEE] = &WACS_LTEE, [FC_RTEE] = &WACS_RTEE,
    [FC_TTEE] = &WACS_TTEE, [FC_BTEE] = &WACS_BTEE,
};

static void
set_frame_cchar(cchar_t *cchar, enum framechars which, nh_bool mainframe)
{
    if (settings.graphics == ASCII_GRAPHICS) {
        wchar_t w[2] = {ascii_borders[which], 0};
        setcchar(cchar, w, (attr_t)0, mainframe ? MAINFRAME_PAIR : FRAME_PAIR,
                 NULL);
    } else {
        int wchar_count = getcchar(*unicode_borders[which],
                                   NULL, NULL, NULL, NULL);
        wchar_t w[wchar_count + 1];
        attr_t attr;
        short pairnum;
        getcchar(*unicode_borders[which], w, &attr, &pairnum, NULL);
        attr = 0;
        pairnum = mainframe ? MAINFRAME_PAIR : FRAME_PAIR;
        setcchar(cchar, w, attr, pairnum, NULL);
    }
}

/* All these functions draw in MAINFRAME_PAIR on basewin or sidebar,
   FRAME_PAIR otherwise */
void
nh_mvwvline(WINDOW *win, int y, int x, int len)
{
    cchar_t c;
    set_frame_cchar(&c, FC_VLINE, win == basewin || win == sidebar);
    mvwvline_set(win, y, x, &c, len);
}
void
nh_mvwhline(WINDOW *win, int y, int x, int len)
{
    cchar_t c;
    set_frame_cchar(&c, FC_HLINE, win == basewin || win == sidebar);
    mvwhline_set(win, y, x, &c, len);
}
void
nh_box(WINDOW *win)
{
    cchar_t c[6];
    set_frame_cchar(c+0, FC_VLINE, win == basewin || win == sidebar);
    set_frame_cchar(c+1, FC_HLINE, win == basewin || win == sidebar);
    set_frame_cchar(c+2, FC_ULCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+3, FC_URCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+4, FC_LLCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+5, FC_LRCORNER, win == basewin || win == sidebar);
    wborder_set(win, c+0, c+0, c+1, c+1, c+2, c+3, c+4, c+5);
}
static void
nh_mvwaddch(WINDOW *win, int y, int x, enum framechars which)
{
    cchar_t c;
    set_frame_cchar(&c, which, win == basewin || win == sidebar);
    mvwadd_wch(win, y, x, &c);
}

static void
draw_frame(void)
{
    if (!ui_flags.draw_frame)
        return;

    /* vertical lines */
    nh_mvwvline(basewin, 1, 0, ui_flags.viewheight);
    nh_mvwvline(basewin, 1, COLNO + 1, ui_flags.viewheight);

    /* horizontal top line above the message win */
    nh_mvwhline(basewin, 0, 1, COLNO);
    nh_mvwaddch(basewin, 0, 0, FC_ULCORNER);
    nh_mvwaddch(basewin, 0, COLNO + 1, FC_URCORNER);

    /* horizontal line between message and map windows */
    nh_mvwhline(basewin, 1 + ui_flags.msgheight, 1, COLNO);
    nh_mvwaddch(basewin, 1 + ui_flags.msgheight, 0, FC_LTEE);
    nh_mvwaddch(basewin, 1 + ui_flags.msgheight, COLNO + 1, FC_RTEE);

    /* horizontal line between map and status */
    nh_mvwhline(basewin, 2 + ui_flags.msgheight + ROWNO, 1, COLNO);
    nh_mvwaddch(basewin, 2 + ui_flags.msgheight + ROWNO, 0, FC_LTEE);
    nh_mvwaddch(basewin, 2 + ui_flags.msgheight + ROWNO, COLNO + 1, FC_RTEE);

    /* horizontal bottom line */
    nh_mvwhline(basewin, ui_flags.viewheight + 1, 1, COLNO);
    nh_mvwaddch(basewin, ui_flags.viewheight + 1, 0, FC_LLCORNER);
    nh_mvwaddch(basewin, ui_flags.viewheight + 1, COLNO + 1, FC_LRCORNER);

    if (!ui_flags.draw_sidebar)
        return;

    nh_mvwhline(basewin, 0, COLNO + 2, COLS - COLNO - 3);
    nh_mvwaddch(basewin, 0, COLNO + 1, FC_TTEE);
    nh_mvwaddch(basewin, 0, COLS - 1, FC_URCORNER);

    nh_mvwhline(basewin, ui_flags.viewheight + 1, COLNO + 2, COLS - COLNO - 3);
    nh_mvwaddch(basewin, ui_flags.viewheight + 1, COLNO + 1, FC_BTEE);
    nh_mvwaddch(basewin, ui_flags.viewheight + 1, COLS - 1, FC_LRCORNER);

    nh_mvwvline(basewin, 1, COLS - 1, ui_flags.viewheight);
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
    case TILESET_DAWNHACK_16:
        set_tile_file("dawnhack-16.png");
        break;
    case TILESET_DAWNHACK_32:
        set_tile_file("dawnhack-32.png");
        break;
    case TILESET_SLASHEM_16:
        set_tile_file("slashem-16.png");
        break;
    case TILESET_SLASHEM_32:
        set_tile_file("slashem-32.png");
        break;
    case TILESET_SLASHEM_3D:
        set_tile_file("slashem-3d.png");
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

        /* In a particularly small window, we can get crashes due to statuswin
           not fitting inside basewin. */
        if (getmaxy(basewin) < ui_flags.msgheight + ROWNO + statusheight ||
            getmaxx(basewin) < COLNO)
            statuswin = NULL;
        else
            statuswin =
                derwin(basewin, statusheight, COLNO,
                       ui_flags.msgheight + ROWNO, 0);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }

    setup_tiles();
    mark_mapwin_for_full_refresh();

    keypad(mapwin, TRUE);
    keypad(msgwin, TRUE);
    leaveok(mapwin, FALSE);
    leaveok(msgwin, FALSE);

    if (statuswin)
        leaveok(statuswin, TRUE);
    if (sidebar)
        leaveok(sidebar, TRUE);

    ui_flags.ingame = TRUE;
    setup_showlines();
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
    if (statuswin) {
        delwin(statuswin);
        statuswin = NULL;
    }
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

        /* In a particularly small window, we can get crashes due to statuswin
           not fitting inside basewin. */
        if (getmaxy(basewin) < ui_flags.msgheight + ROWNO + statusheight ||
            getmaxx(basewin) < COLNO)
            statuswin = NULL;
        else
            statuswin =
                derwin(basewin, statusheight, COLNO,
                       ui_flags.msgheight + ROWNO, 0);

        if (ui_flags.draw_sidebar)
            sidebar =
                derwin(basewin, ui_flags.viewheight, COLS - COLNO, 0, COLNO);
    }

    if (mapwin) setup_tiles();
    mark_mapwin_for_full_refresh();

    if (statuswin)
        leaveok(statuswin, TRUE);
    if (sidebar)
        leaveok(sidebar, TRUE);

    redo_showlines();
    redraw_game_windows();
    doupdate();
}


void
destroy_game_windows(void)
{
    if (ui_flags.ingame) {
        delwin(msgwin);
        delwin(mapwin);
        if (statuswin)
            delwin(statuswin);
        if (sidebar || ui_flags.draw_sidebar) {
            cleanup_sidebar(FALSE);
            if (sidebar)
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
        if (statuswin)
            wnoutrefresh(statuswin);

        if (sidebar)
            wnoutrefresh(sidebar);

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
            /* If we're in a game, EXIT_FORCE_SAVE will longjmp out to the
               normal game saved/over sequence, and eventually the control will
               get back here outside a game (if KEY_HANGUP is returned from any
               wgetch call, it will be returned from all future wgetch calls).

               If we're not in a game, EXIT_FORCE_SAVE will return normally, and
               from there, we spam ESC until the program is closed. (You can't
               ESC out of the main menu, so we use a special flag for that.) */
            ui_flags.done_hup = TRUE;
            return KEY_ESCAPE;
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

        if (key == KEY_OTHERFD) {
            key = 0;
            if (ui_flags.connected_to_server)
                nhnet_check_socket_fd();
        }

    } while (!key);

    return key;
}


/* It's possible for us to longjmp out of a menu handler, etc., while its game
   window is still allocated; and we also need to be able to keep track of the
   stack of windows to redraw/resize it.  Thus, we keep track of the windows
   allocated for this purpose in the firstgw chain.

   Note that alloc_gamewin and delete_gamewin aren't quite opposites (although
   delete_ cancels alloc_'s effect); delete_gamewin deletes the windows itself,
   whereas alloc_gamewin does not create the windows itself. */

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
    /* We must free win2 first, because it may be a child of win1. */
    if (gw->win2)
        delwin(gw->win2);
    if (gw->win)
        delwin(gw->win);

    /* Some windows have extra associated dynamic data (e.g. text being
       entered at a getlin prompt). If that's present, free that too.
       dyndata is a pointer to the data that needs freeing. */
    if (gw->dyndata && *(gw->dyndata))
        free(*(gw->dyndata));

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


void
delete_all_gamewins(void)
{
    while (firstgw)
        delete_gamewin(firstgw);
}


/*----------------------------------------------------------------------------*/
/* misc api functions */

void
curses_pause(enum nh_pause_reason reason)
{
    doupdate();
    if (reason == P_MESSAGE && msgwin != NULL)
        pause_messages();
    else if (mapwin != NULL) {
        /* P_MAP: pause to show the result of detection or similar */
        if (get_map_key(FALSE) == KEY_SIGNAL)
            uncursed_signal_getch();
    }
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
    char *line, **lines;
    char linebuf[BUFSZ * ROWNO];
    int lcount, i;
    struct nh_menulist menu;

    char buf[strlen(inbuf) + 1];
    strcpy(buf, inbuf);

    init_menulist(&menu);

    line = strtok(buf, "\n");
    do {
        strncpy(linebuf, line, BUFSZ * ROWNO);
        if (strchr(linebuf, '\t') != 0)
            tabexpand(linebuf);

        wrap_text(COLNO - 4, linebuf, &lcount, &lines);
        for (i = 0; i < lcount; ++i)
            add_menu_txt(&menu, lines[i], MI_TEXT);

        free_wrap(lines);

        line = strtok(NULL, "\n");
    } while (line);

    curses_display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                        NULL, null_menu_callback);
}


void
curses_raw_print(const char *str)
{
    endwin();
    fprintf(stderr, strchr(str, '\n') ? "%s" : "%s\n", str);
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
    nanosleep(&(struct timespec){ .tv_nsec = 50 * 1000 * 1000}, NULL);
#endif
}


void
curses_server_cancel(void)
{
    uncursed_signal_getch();
}
