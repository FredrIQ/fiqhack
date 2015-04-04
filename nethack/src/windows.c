/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-01 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <signal.h>
#include <locale.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#ifndef WIN32
# include <dirent.h>
#endif

WINDOW *basewin, *mapwin, *msgwin, *statuswin, *sidebar, *extrawin, *loadwin;
struct gamewin *firstgw, *lastgw;
static int orig_cursor;
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
    curses_query_key_game,
    curses_getpos,
    curses_getdir,
    curses_yn_function_game,
    curses_getline,
    curses_delay_output,
    curses_load_progress,
    curses_notify_level_changed,
    curses_outrip,
    curses_print_message_nonblocking,
    curses_server_cancel,
};

/*----------------------------------------------------------------------------*/

static char *tileprefix = NULL;

static void setup_tiles(void);

void
set_font_file(const char *fontfilename)
{
    char namebuf[strlen(tileprefix) + strlen(fontfilename) + 1];
    strcpy(namebuf, tileprefix);
    strcat(namebuf, fontfilename);
    set_faketerm_font_file(namebuf);
}

/* Finds the start of the tile table in a tile file. (Specifically, it finds the
   tile table name, which is the first thing that the client might be interested
   in.) Returns 0 and prints an error if it can't find it, or the length of the
   tile table (including the name and size) if it can. */
static size_t
seek_tile_file(FILE *in) {

    size_t filelen;
    unsigned char header[12];

    fseek(in, 0, SEEK_END);
    filelen = ftell(in);
    rewind(in);

    if (fread(header, 1, 8, in) < 8) {
        curses_raw_print("Warning: tileset file is corrupted.\n");
        return 0;
    }

    /* There are two reasonable headers to see here. One is a PNG header, in
       which case we have an image-based tileset, embedded in an image. The
       other is a binary tileset header, in which case we have a cchar-based
       tileset, by itself. */
    if (memcmp(header, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {
        /* That's a PNG header. Follow PNG headers until we find the
           nhTb chunk. */
        if (fread(header, 1, 8, in) < 8) {
            curses_raw_print("Warning: tileset file is corrupted.\n");
            return 0;
        }
        while (header[4] != 'n' || header[5] != 'h' ||
               header[6] != 'T' || header[7] != 'b') {
            /* PNG is big-endian. */
            size_t len = header[0];
            len *= 256; len += header[1];
            len *= 256; len += header[2];
            len *= 256; len += header[3];
            /* Skip past the data and CRC. */
            fseek(in, len + 4, SEEK_CUR);
            if (fread(header, 1, 8, in) < 8) {
                curses_raw_print("Warning: 'tileset file' is just an image, "
                                 "not a tileset.\n");
                return 0;
            }
        }
        /* We found the PNG chunk we're looking for. Treat it as the whole
           file. */
        filelen = header[0];
        filelen *= 256; filelen += header[1];
        filelen *= 256; filelen += header[2];
        filelen *= 256; filelen += header[3];
        if (fread(header, 1, 8, in) < 8) {
            curses_raw_print("Warning: tileset file is corrupted.\n");
            return 0;
        }
    }

    /* A binary tileset header is twelve bytes. Read the other four. */
    if (fread(header + 8, 1, 4, in) < 4) {
        curses_raw_print("Warning: tileset file is corrupted.\n");
        return 0;
    }

    if (memcmp(header, "NH4TILESET\0\0", 12) != 0) {
        curses_raw_print("Warning: tileset file is in the wrong format.\n");
        return 0;
    }

    filelen -= 12;

    if (filelen < 84) {
        curses_raw_print("Warning: tileset file is too short.\n");
        return 0;
    }

    return filelen;
}

/* This function parses the given tile file. If "store_tilename_in" is NULL,
   then loads the tileset into the appropriate global variables (dimensions,
   image/cchar, tile table), and if the tile file is image-based, also sends it
   to libuncursed to start rendering the images. Otherwise, the tileset metadata
   is converted into a string and returned; the global variables remain
   untouched. */
void
load_tile_file(const char *tilebase, char *store_tilename_in)
{
    if (!store_tilename_in) {
        free(tiletable);
        tiletable = NULL;
        tiletable_len = 0;
    } else
        *store_tilename_in = '\0';

    if (!tileprefix)
        return;        /* UI not initialized yet */

    char namebuf1[strlen(tileprefix) + strlen(tilebase) + sizeof ".nh4ct"];
    strcpy(namebuf1, tileprefix);
    strcat(namebuf1, tilebase);
    strcat(namebuf1, ".nh4ct");

    char user_tilespath[
#ifdef AIMAKE_BUILDOS_MSWin32
        MAX_PATH
#else
        BUFSZ
#endif
        ];

    *user_tilespath = '\0';

    if (!ui_flags.connection_only)
        get_gamedirA(TILESET_DIR, user_tilespath);

    char namebuf2[strlen(user_tilespath) + strlen(tilebase) + sizeof ".nh4ct"];
    strcpy(namebuf2, user_tilespath);
    strcat(namebuf2, tilebase);
    strcat(namebuf2, ".nh4ct");

    /* On Windows, fopen has somehow been redefined to take a wchar_t * here.
       Make sure we call the function, not the macro. We look in two places: the
       tilesets we installed, and the user tiles path. The user tiles path
       takes precedence if there's a tileset in both locations. */
    FILE *in = *user_tilespath ? (fopen)(namebuf2, "rb") : NULL;
    char *whichnamebuf = namebuf2;

    if (!in) {
        in = (fopen)(namebuf1, "rb");
        whichnamebuf = namebuf1;
    }

    if (!in) {
        int e = errno;
        char errmsgbuf[sizeof namebuf1 + sizeof namebuf2 +
                       sizeof "Warning: could not open tileset file '' or ''\n"
                       + 1];
        sprintf(errmsgbuf, "Warning: could not open tileset file '%s' or '%s'\n",
                namebuf2, namebuf1);
        curses_raw_print(errmsgbuf);
        curses_raw_print(strerror(e));
        return;
    }

    int ttlen = seek_tile_file(in) - 84;
    if (ttlen == -84)
        return; /* error message has already been printed */

    if (store_tilename_in) {
        if (fread(store_tilename_in, 1, 80, in) != 80)
            curses_impossible("File length is longer than the file");
    } else
        fseek(in, 80, SEEK_CUR); /* skip the name */

    /* Unlike PNG, tile tables are little-endian. */
    int w = getc(in);
    w += getc(in) << 8;
    int h = getc(in);
    h += getc(in) << 8;

    if (store_tilename_in) {
        store_tilename_in[80] = '\0';

        char *p = store_tilename_in;
        int remlen = QBUFSZ;
        while (*p) {
            p++;
            remlen--;
        }

        if (w || h)
            snprintf(p, remlen - 1, " (graphical, %d x %d)", w, h);
        else
            snprintf(p, remlen - 1, " (text-based)");

        return;
    }

    if (w == 0 || h == 0) {
        if (ui_flags.initialized)
            set_tiles_tile_file(NULL, 0, 0);
        tiletable_is_cchar = 1;
    } else {
        if (ui_flags.initialized)
            set_tiles_tile_file(whichnamebuf, h, w);
        tiletable_is_cchar = 0;
    }

    /* Load up the entire table. */
    tiletable = malloc(ttlen);
    if (fread(tiletable, 1, ttlen, in) < ttlen) {
        curses_raw_print("Warning: tileset shrunk while in use.\n");
        free(tiletable);
        tiletable = NULL;
        tiletable_len = 0;
        return;
    }
    tiletable_len = ttlen;
}

static int
compare_tileset_descriptions(const void *d1, const void *d2)
{
    const struct tileset_description *td1 = d1;
    const struct tileset_description *td2 = d2;

    return strcmp(td1->basename, td2->basename);
}

struct tileset_description *
get_tileset_descriptions(int *count)
{
    char user_tilespath[
#ifdef AIMAKE_BUILDOS_MSWin32
        MAX_PATH
#else
        BUFSZ
#endif
        ];

    *user_tilespath = '\0';

    if (!ui_flags.connection_only)
        get_gamedirA(TILESET_DIR, user_tilespath);

    *count = 0;
    struct tileset_description *rv = malloc(1);

    int dirnum;
    for (dirnum = 0; dirnum < 2; dirnum++) {
        char *dir;
        if (dirnum == 0)
            dir = user_tilespath;
        else
            dir = tileprefix;

#ifdef WIN32
        HANDLE dirp;
        WIN32_FIND_DATAA dp;

        char pattern[strlen(dir) + sizeof "*.nh4ct"];
        strcpy(pattern, dir);
        strcat(pattern, "*.nh4ct");
        dirp = FindFirstFileA(pattern, &dp);

        while (dirp != INVALID_HANDLE_VALUE)
        {
            char d_name[strlen(dp.cFileName) + 1];
            strcpy(d_name, dp.cFileName);
            char *ep;
            for (ep = d_name; *ep; ep++)
                *ep = tolower(*ep);       /* Windows is case-insensitive */
#else
        DIR *dirp = opendir(dir);
        if (!dirp) /* If opendir fails, don't try to readdir */
            return rv;
        struct dirent *dp;
        while ((dp = readdir(dirp)))
        {
            char d_name[strlen(dp->d_name) + 1];
            strcpy(d_name, dp->d_name);
            char *ep;
#endif

            ep = strstr(d_name, ".nh4ct");
            if (!ep || ep[6] != '\0')
                continue;

            *ep = '\0';
            rv = realloc(rv, (1 + *count) * sizeof *rv);

            strncpy(rv[*count].basename, d_name, sizeof rv->basename);
            rv[*count].basename[sizeof rv->basename - 1] = '\0';
            load_tile_file(rv[*count].basename, rv[*count].desc);
            ++*count;

#ifdef WIN32
            if (!FindNextFileA(dirp, &dp))
                break;
        }

        if (dirp != INVALID_HANDLE_VALUE)
            FindClose(dirp);
#else
        }
        closedir(dirp);
#endif
    }

    /* Sort the tiles by basename. If we find duplicates, we eliminate them. */
    qsort(rv, *count, sizeof *rv, compare_tileset_descriptions);
    struct tileset_description *tdp1 = rv, *tdp2 = rv;
    /* tdp1 points to the position of a tile in the old array; tdp2 points to
       the position it should be placed in in the new array. */
    while (tdp2 - rv < *count - 1) {
        if (strcmp(tdp1[0].basename, tdp1[1].basename) == 0) {
            tdp1++;
            --*count;
        } else {
            if (tdp1 != tdp2)
                memcpy(tdp2, tdp1, sizeof *rv);
            tdp1++;
            tdp2++;
        }
    }
    if (tdp2 - rv < *count) /* i.e. *count, but this is clearer */
        memcpy(tdp2, tdp1, sizeof *rv);

    return rv;
}

/* A delayed-action curs_set; we don't show the cursor until just before we
   request a key. This prevents it flickering, and prevents the tiles view
   jumping around as we hide a message window. */
int
nh_curs_set(int visible)
{
    int old_want_cursor = ui_flags.want_cursor;

    ui_flags.want_cursor = visible;

    if (!visible && old_want_cursor)
        curs_set(0);

    return old_want_cursor;
}

void
init_curses_ui(const char *dataprefix)
{
    /* set up the default system locale by reading the environment variables */
    setlocale(LC_ALL, "");

    uncursed_set_title("NetHack 4");

    if (!initscr()) {
        fprintf(stderr, "Could not initialise the UI.\n");
        fprintf(stderr, "Press <Return> to exit.\n");
        getchar();
        exit(1);
    }

    tileprefix = strdup(dataprefix);
    set_font_file("font14.png");
    setup_tiles();

    setup_palette();

    noecho();
    raw();
    nonl();
    meta(stdscr, TRUE);
    leaveok(stdscr, TRUE);
    orig_cursor = curs_set(1);          /* not nh_curs_set */
    ui_flags.want_cursor = 1;
    keypad(stdscr, TRUE);

    while (LINES < ROWNO + 3 || COLS < COLNO + 1) {
        werase(stdscr);
        mvprintw(0, 0, "Your terminal is too small for NetHack 4.\n");
        printw("Current size: (%d, %d)\n", COLS, LINES);
        printw("Minimum size: (%d, %d)\n", COLNO + 1, ROWNO + 3);
        printw("Size your terminal larger, or press 'q' to quit.\n");
        int k = getch();
        if (k == 'q' || k == KEY_HANGUP) {
            endwin();
            exit(1);
        }
    }

    init_nhcolors();
    ui_flags.playmode = MODE_NORMAL;
    ui_flags.initialized = 1;
    basewin = stdscr;
}


void
exit_curses_ui(void)
{
    cleanup_sidebar(TRUE);
    curs_set(orig_cursor); /* not nh_curs_set */
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

/* We can't use an array for this; on Windows, you can't initialize a variable
   with the address of a variable from a DLL. */
static const cchar_t *
unicode_border(enum framechars which)
{
    switch(which)
    {
    case FC_HLINE: return WACS_HLINE;
    case FC_VLINE: return WACS_VLINE;
    case FC_ULCORNER: return WACS_ULCORNER;
    case FC_URCORNER: return WACS_URCORNER;
    case FC_LLCORNER: return WACS_LLCORNER;
    case FC_LRCORNER: return WACS_LRCORNER;
    case FC_LTEE: return WACS_LTEE;
    case FC_RTEE: return WACS_RTEE;
    case FC_TTEE: return WACS_TTEE;
    case FC_BTEE: return WACS_BTEE;
    }
    return WACS_CKBOARD; /* should be unreachable */
}

static void
set_frame_cchar(cchar_t *cchar, enum framechars which, nh_bool mainframe)
{
    if (ui_flags.asciiborders) {
        wchar_t w[2] = {ascii_borders[which], 0};
        setcchar(cchar, w, (attr_t)0, mainframe ? MAINFRAME_PAIR : FRAME_PAIR,
                 NULL);
    } else {
        int wchar_count = getcchar(unicode_border(which),
                                   NULL, NULL, NULL, NULL);
        wchar_t w[wchar_count + 1];
        attr_t attr;
        short pairnum;
        getcchar(unicode_border(which), w, &attr, &pairnum, NULL);
        attr = 0;
        pairnum = mainframe ? MAINFRAME_PAIR :
            ui_flags.ingame && ui_flags.current_followmode != FM_PLAY &&
            !ui_flags.in_zero_time_command ? NOEDIT_FRAME_PAIR : FRAME_PAIR;
        setcchar(cchar, w, attr, pairnum, NULL);
    }
}

/* All these functions draw in MAINFRAME_PAIR on basewin or sidebar,
   FRAME_PAIR otherwise. */
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
nh_window_border(WINDOW *win, int dismissable)
{
    cchar_t c[6];
    set_frame_cchar(c+0, FC_VLINE, win == basewin || win == sidebar);
    set_frame_cchar(c+1, FC_HLINE, win == basewin || win == sidebar);
    set_frame_cchar(c+2, FC_ULCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+3, FC_URCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+4, FC_LLCORNER, win == basewin || win == sidebar);
    set_frame_cchar(c+5, FC_LRCORNER, win == basewin || win == sidebar);
    wborder_set(win, c+0, c+0, c+1, c+1, c+2, c+3, c+4, c+5);

    /* Don't allow clicks "behind" the window. */
    uncursed_clear_mouse_regions();

    /* If we're popping up a modal window (that blocks mouse actions), we're not
       hovering over anything behind it. */
    ui_flags.maphoverx = ui_flags.maphovery = -1;

    if (settings.mouse && dismissable && getmaxx(win) > 6) {
        cchar_t x;
        const wchar_t *w = L"x";
        const wchar_t *ok = L"[OK]";

        wset_mouse_event(win, uncursed_mbutton_left,
                         KEY_ESCAPE, KEY_CODE_YES);
        setcchar(&x, w, (attr_t)0, PAIR_NUMBER(
                     curses_color_attr(CLR_ORANGE, CLR_BLACK)), NULL);
        mvwadd_wch(win, 0, getmaxx(win)-2, &x);

        if (dismissable == 2) {
            /* As well as the close box, we want an OK button that sends
               Return (13). */
            wset_mouse_event(win, uncursed_mbutton_left, 13, OK);
            mvwaddwstr(win, getmaxy(win)-1, getmaxx(win)-5, ok);
            mvwchgat(win, getmaxy(win)-1, getmaxx(win)-5, 4,
                     (attr_t)0, PAIR_NUMBER(curses_color_attr(
                                                CLR_GREEN, CLR_BLACK)), NULL);
        }

        wset_mouse_event(win, uncursed_mbutton_left, 0, ERR);
    }
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
    int framewidth = !!ui_flags.draw_outer_frame_lines;
    int y = framewidth;
    int x = framewidth;

    if (framewidth) {
        nh_mvwvline(basewin, 0, 0, LINES);
        nh_mvwvline(basewin, 0, COLS-1, LINES);
        nh_mvwhline(basewin, 0, 0, COLS);
        nh_mvwhline(basewin, LINES-1, 0, COLS);
        nh_mvwaddch(basewin, 0, 0, FC_ULCORNER);
        nh_mvwaddch(basewin, 0, COLS-1, FC_URCORNER);
        nh_mvwaddch(basewin, LINES-1, 0, FC_LLCORNER);
        nh_mvwaddch(basewin, LINES-1, COLS-1, FC_LRCORNER);
    }

    /* We draw vertical lines if a) the layout engine told us to; and b) there's
       actually somewhere to draw them (the only place where windows could be
       vertically separated is between the message/map/status/extra windows and
       the sidebar). */
    if (ui_flags.draw_vertical_frame_lines && ui_flags.sidebarwidth) {
        /* The lines themselves */
        nh_mvwvline(basewin, y, x + ui_flags.mapwidth, LINES - framewidth * 2);
        /* Connection to the outer border, if there is one */
        if (framewidth) {
            nh_mvwaddch(basewin, 0, ui_flags.mapwidth + x, FC_TTEE);
            nh_mvwaddch(basewin, LINES-1, ui_flags.mapwidth + x, FC_BTEE);
        }
    }

    if (ui_flags.draw_horizontal_frame_lines) {
        /* If we have vertical lines but not an outer frame, or vice versa,
           connecting just one end of the lines looks ugly, so we leave the
           disconnected. Exception: if both ends touch the frame, we don't
           care about vertical lines. */
        nh_bool connectends = framewidth &&
            (ui_flags.draw_vertical_frame_lines || !ui_flags.sidebarwidth);

        y += ui_flags.msgheight;
        nh_mvwhline(basewin, y, x, ui_flags.mapwidth);
        if (connectends) {
            nh_mvwaddch(basewin, y, 0, FC_LTEE);
            nh_mvwaddch(basewin, y, ui_flags.mapwidth + 1, FC_RTEE);
        }

        y += 1 + ui_flags.mapheight;
        nh_mvwhline(basewin, y, x, ui_flags.mapwidth);
        if (connectends) {
            nh_mvwaddch(basewin, y, 0, FC_LTEE);
            nh_mvwaddch(basewin, y, ui_flags.mapwidth + 1, FC_RTEE);
        }

        if (ui_flags.extraheight) {
            y += 1 + ui_flags.statusheight;
            nh_mvwhline(basewin, y, x, ui_flags.mapwidth);
            if (connectends) {
                nh_mvwaddch(basewin, y, 0, FC_LTEE);
                nh_mvwaddch(basewin, y, ui_flags.mapwidth + 1, FC_RTEE);
            }
        }
    }
}

static inline void
allocate_layout_space(int *from, int *to, int amount, int tomax)
{
    if (tomax > -1 && *to + amount > tomax)
        amount = tomax - *to;
    if (*from < amount)
        amount = *from;
    *from -= amount;
    *to += amount;
}

static void
layout_game_windows(void)
{
    /*
     * We lay out the windows in either one or two columns. The left column
     * contains messages, the map, and a status area; if we have a lot of
     * vertical space, a customizable fourth window (extrawin) is added, but it
     * doesn't appear on any but the largest windows. The right column might not
     * exist due to lack of horizontal space; if it does, it's used for the
     * inventory and the "things that are here" display, and is called the
     * "sidebar" in the source code.  This display always uses all the vertical
     * space that's available to it (the extrawin was originally designed to
     * balance out the columns).
     *
     * Priorities for height (from most to least important):
     * - ROWNO lines of map, 1 line of messages, 2 lines of status (mandatory)
     *   - Note: if we don't have ROWNO+3 lines, rendering will be broken; the
     *     status area is hidden by create_game_windows in this situation to
     *     avoid crashes, and beyond that, lines are taken from the map; if
     *     we get down to just 1 line, we have a 1-line map and 1-line message
     *     area that overlap
     * - Extra height on the map (as might be required by a tiles interface)
     * - 4 more lines of messages
     * - The third line of the status area (if settings.status3)
     * - More lines of messages, up to settings.msgheight
     * - Horizontal frame lines between windows
     * - Outermost frame lines (if there's also horizontal space for them)
     * - Extra filler window.
     *
     * Priorities for width (from most to least important):
     * - 80 columns of map
     * - Vertical frame lines between windows (otherwise the sidebar runs into
     *   the status area and possibly the map)
     * - (if sidebar == AB_TRUE) up to 40 columns of inventory sidebar
     * - Extra width on the map (as might be required by a tiles interface
     * - Outermost frame lines (if there's also vertical space for them)
     * - (if sidebar != AB_FALSE and there are at least 20 spare columns)
     *   fill the remaining width with sidebar
     * - (otherwise) center the map onscreen
     */
    int desired_map_y, desired_map_x;
    int y_remaining = LINES;
    int x_remaining = COLS;

    ui_flags.draw_horizontal_frame_lines = FALSE;
    ui_flags.draw_vertical_frame_lines = FALSE;
    ui_flags.draw_outer_frame_lines = FALSE;
    ui_flags.statusheight = 0;
    ui_flags.mapheight = 0;
    ui_flags.msgheight = 0;
    ui_flags.extraheight = 0;
    ui_flags.mapwidth = 0;
    ui_flags.sidebarwidth = 0;
    ui_flags.map_padding = 0;

    /* Work out how large libuncursed would like to draw the map. (The answer
       will normally be "80 by 21", but could be different if using a tiles
       interface with tiles that are not the same dimension as characters. */
    get_tile_dimensions(ROWNO, COLNO, &desired_map_y, &desired_map_x);

    /* If using particularly small tiles, we need to make the tiles region
       larger so that it covers the character region. (This codepath is
       unreachable with any of the default tilesets, when using the default
       font, but is included for futureproofing.) */
    if (desired_map_x < COLNO)
        desired_map_x = COLNO;
    if (desired_map_y < ROWNO)
        desired_map_y = ROWNO;

    /* Vertical layout. */
    allocate_layout_space(&y_remaining, &ui_flags.msgheight,
                          1, settings.msgheight);     /* minimal message area */
    allocate_layout_space(&y_remaining, &ui_flags.mapheight,
                          ROWNO, desired_map_y);   /* character region of map */
    allocate_layout_space(&y_remaining, &ui_flags.statusheight,
                          2, settings.status3 ? 3 : 2);        /* status area */
    /* The tiles region of the map is important, but unlike the character
       region, it's not mandatory, because it can scroll if necessary. (When not
       using tiles, the tiles and character regions are the same, so this is a
       no-op. */
    allocate_layout_space(&y_remaining, &ui_flags.mapheight,
                          y_remaining, desired_map_y); /* tiles region of map */
    allocate_layout_space(&y_remaining, &ui_flags.msgheight,
                          2, settings.msgheight);          /* 4 more messages */
    allocate_layout_space(&y_remaining, &ui_flags.statusheight,
                          1, settings.status3 ? 3 : 2); /* status area line 3 */
    allocate_layout_space(&y_remaining, &ui_flags.msgheight,
                          y_remaining, settings.msgheight); /* other messages */

    /* We need at least two horizontal separator lines: between map and
       messages, and between map and status. (We have a third if we have space
       for extrawin, but that's checked later. */
    if (y_remaining >= 2) {
        y_remaining -= 2;
        ui_flags.draw_horizontal_frame_lines = TRUE;
    }

    /* The rest of the vertical layout depends on the horizontal layout, so we
       stop laying out now and come back later. */

    /* Horizontal layout. */
    allocate_layout_space(&x_remaining, &ui_flags.mapwidth,
                          COLNO, desired_map_x);   /* character region of map */
    if (x_remaining) {
        /* We want a vertical frame line if there's anything to the right of the
           map. We handle this by allocating space for it, then deallocating the
           space again if it turns out there's nothing to separate. */
        x_remaining--;
        ui_flags.draw_vertical_frame_lines = TRUE;
    }
    if (settings.sidebar == AB_TRUE)
        allocate_layout_space(&x_remaining, &ui_flags.sidebarwidth,
                              40, -1);             /* sidebar */
    allocate_layout_space(&x_remaining, &ui_flags.mapwidth,
                          x_remaining, desired_map_x); /* tiles region of map */

    /* Do we have space for an outer frame? We need two spare rows; and two
       spare columns if we already allocated a sidebar (one spare column
       otherwise, because we can repurpose the one reserved for a vertical frame
       line). */
    if (y_remaining >= 2 && x_remaining >= (ui_flags.sidebarwidth ? 2 : 1) &&
        settings.whichframes == FRAME_ALL) {
        y_remaining -= 2;
        x_remaining -= 2;
        ui_flags.draw_outer_frame_lines = TRUE;
    }

    if (settings.sidebar != AB_FALSE &&
        x_remaining > (settings.sidebar == AB_AUTO ? 19 : 0))
        allocate_layout_space(&x_remaining, &ui_flags.sidebarwidth,
                              x_remaining, -1);

    /* Refund the separator between map and inventory, if necessary. */
    if (!ui_flags.sidebarwidth && ui_flags.draw_vertical_frame_lines) {
        x_remaining++;
        ui_flags.draw_vertical_frame_lines = FALSE;
    }

    /* Any remaining horizontal space is given to the message and status areas,
       and placed as padding around the map. This means that the tiles region
       will either be the desired size, or 1 character wider (which will cause a
       half-character black space around the map in tiles mode, which is what we
       want; in character mode, the map cannot be centered because the window
       width is odd, so it'll end up half a character off-center). */
    if (x_remaining) {
        ui_flags.mapwidth += x_remaining;
        ui_flags.map_padding = x_remaining / 2;
    }

    /* Any remaining vertical space is used for the extra window, if we could
       make it at least 2 lines high. Otherwise, it's given to the message area,
       because it has to go /somewhere/, even if this makes the message area
       taller than the user wanted. */
    if (y_remaining >= (ui_flags.draw_horizontal_frame_lines ? 3 : 2)) {
        if (ui_flags.draw_horizontal_frame_lines)
            y_remaining--;
        ui_flags.extraheight = y_remaining;
    } else {
        ui_flags.msgheight += y_remaining;
    }

    /* If we have a brokenly small terminal... */
    if (LINES <= 1) {
        ui_flags.mapheight = 1;
        ui_flags.msgheight = 1;
    }
}

void
setup_palette(void)
{
    static const uncursed_palette16 palette_default =
        { UNCURSED_DEFAULT_COLORS };

    static const uncursed_palette16 palette_alt1 = {{
        {0x00,0x00,0x00},{0xCC,0x00,0x00},{0x4E,0x9A,0x06},{0xC4,0xA0,0x00},
        {0x34,0x65,0xA4},{0x75,0x50,0x7B},{0x06,0x98,0x9A},{0xD3,0xD7,0xCF},
        {0x55,0x57,0x53},{0xEF,0x29,0x29},{0x8A,0xE2,0x34},{0xFC,0xE9,0x4F},
        {0x73,0x9F,0xCF},{0xAD,0x7F,0xA8},{0x34,0xE2,0xE2},{0xEE,0xEE,0xEC}
        }};

    static const uncursed_palette16 palette_alt2 = {{
        {0x00,0x00,0x00},{0xCC,0x00,0x00},{0x3E,0xAA,0x06},{0xC4,0xC0,0x00},
        {0x34,0x65,0xFF},{0xA5,0x70,0x9B},{0x06,0xB8,0xBA},{0xEE,0xEE,0xEE},
        {0x88,0x88,0x88},{0xEF,0x29,0x29},{0x8A,0xE2,0x34},{0xFC,0xE9,0x4F},
        {0x73,0x9F,0xFF},{0xAD,0x7F,0xA8},{0x34,0xE2,0xE2},{0xFF,0xFF,0xFF},
        }};

    static const uncursed_palette16 palette_alt3 = {{
        {0x00,0x00,0x00},{0xDC,0x32,0x2F},{0x85,0x99,0x00},{0xB5,0x89,0x00},
        {0x26,0x8B,0xD2},{0xF3,0x36,0x82},{0x2A,0xA1,0x98},{0xEE,0xE8,0xD5},
        {0x88,0x88,0x88},{0xEF,0x29,0x29},{0x8A,0xE2,0x34},{0xFC,0xE9,0x4F},
        {0x73,0x9F,0xFF},{0xAD,0x7F,0xA8},{0x34,0xE2,0xE2},{0xFF,0xFF,0xFF},
        }};
    static const uncursed_palette16 palette_saturated = {{
        {0x00,0x00,0x00}, {0xbb,0x00,0x00}, {0x00,0xbb,0x00}, {0xcb,0x6e,0x00},
        {0x00,0x00,0xcc}, {0xb1,0x00,0xbc}, {0x00,0xbb,0xaa}, {0xbb,0xbb,0xbb},
        {0x55,0x55,0x55}, {0xff,0x99,0x33}, {0xbb,0xff,0x00}, {0xff,0xee,0x00},
        {0x00,0xaa,0xff}, {0xff,0x7f,0xbb}, {0x7f,0xff,0xff}, {0xff,0xff,0xff},
        }};
    static const uncursed_palette16 palette_tertiary = {{
        {0x22,0x40,0x3d}, {0xab,0x6d,0x63}, {0x86,0xa0,0x60}, {0xb5,0x87,0x52},
        {0x48,0x76,0xa9}, {0x74,0x53,0xb8}, {0x4e,0xa2,0x91}, {0xc0,0xb7,0xa7},
        {0x3d,0x73,0x6e}, {0xff,0xc4,0x61}, {0xc1,0xf8,0x94}, {0xff,0xed,0x76},
        {0x94,0xd3,0xf8}, {0xf8,0xac,0xd7}, {0x94,0xf8,0xe2}, {0xff,0xf3,0xde},
        }};
    static const uncursed_palette16 palette_equiluminous = {{
        {0x3f,0x3f,0x3f}, {0xdc,0x00,0x00}, {0x00,0xa2,0x00}, {0xa1,0x75,0x00},
        {0x61,0x61,0xff}, {0xbf,0x00,0xbf}, {0x00,0x95,0x95}, {0xbf,0xbf,0xbf},
        {0x7f,0x7f,0x7f}, {0xff,0x9a,0x9a}, {0x00,0xf3,0x00}, {0xc9,0xc9,0x00},
        {0xb5,0xb5,0xff}, {0xff,0x7c,0xff}, {0x00,0xe0,0xe0}, {0xff,0xff,0xff},
        }};
    static const uncursed_palette16 palette_typical_term = {{
        {0x2e,0x34,0x36}, {0xcc,0x00,0x00}, {0x4e,0x9a,0x06}, {0xc4,0xa0,0x00},
        {0x34,0x65,0xa4}, {0xff,0x00,0xe4}, {0x00,0xfb,0xff}, {0xd3,0xd7,0xcf},
        {0x56,0x56,0x54}, {0xee,0x30,0x30}, {0x8a,0xe2,0x34}, {0xfc,0xe9,0x4f},
        {0x72,0x9f,0xcf}, {0xb2,0x92,0xaf}, {0xa2,0xff,0xff}, {0xee,0xee,0xec},
        }};
    static const uncursed_palette16 palette_lin_console = {{
        {0x00,0x00,0x00}, {0xb2,0x18,0x18}, {0x18,0xb2,0x18}, {0xb2,0x68,0x18},
        {0x18,0x18,0xb2}, {0xb2,0x18,0xb2}, {0x18,0xb2,0xb2}, {0xb2,0xb2,0xb2},
        {0x68,0x68,0x68}, {0xff,0x54,0x54}, {0x54,0xff,0x54}, {0xff,0xff,0x54},
        {0x54,0x54,0xff}, {0xff,0x54,0xff}, {0x54,0xff,0xff}, {0xff,0xff,0xff},
        }};
    static const uncursed_palette16 palette_win_console = {{
        {0x00,0x00,0x00}, {0x80,0x00,0x00}, {0x00,0x80,0x00}, {0x80,0x80,0x00},
        {0x00,0x00,0x80}, {0x80,0x00,0x80}, {0x00,0x80,0x80}, {0xc0,0xc0,0xc0},
        {0x80,0x80,0x80}, {0xff,0x00,0x00}, {0x00,0xff,0x00}, {0xff,0xff,0x00},
        {0x00,0x00,0xff}, {0xff,0x00,0xff}, {0x00,0xff,0xff}, {0xff,0xff,0xff},
        }};
    static const uncursed_palette16 palette_dark_pastels = {{
        {0x3f,0x3f,0x3f}, {0x70,0x50,0x50}, {0x60,0xb4,0x8a}, {0xdf,0xaf,0x8f},
        {0x9a,0xb9,0xd7}, {0xdc,0x8c,0xc4}, {0x8c,0xd1,0xd3}, {0xdc,0xdc,0xcd},
        {0x70,0x90,0x80}, {0xdc,0xa3,0xa3}, {0x72,0xd5,0xa2}, {0xf0,0xdf,0xaf},
        {0x94,0xc0,0xf3}, {0xec,0x93,0xd5}, {0x93,0xe1,0xe3}, {0xff,0xff,0xff},
        }};

    switch (settings.palette) {
    case PALETTE_ALT1:
        uncursed_set_palette16(&palette_alt1);
        return;
    case PALETTE_ALT2:
        uncursed_set_palette16(&palette_alt2);
        return;
    case PALETTE_ALT3:
        uncursed_set_palette16(&palette_alt3);
        return;
    case PALETTE_SATURATED:
        uncursed_set_palette16(&palette_saturated);
        return;
    case PALETTE_TERTIARY:
        uncursed_set_palette16(&palette_tertiary);
        return;
    case PALETTE_EQUILUM:
        uncursed_set_palette16(&palette_equiluminous);
        return;
    case PALETTE_TTERMINAL:
        uncursed_set_palette16(&palette_typical_term);
        return;
    case PALETTE_LCONSOLE:
        uncursed_set_palette16(&palette_lin_console);
        return;
    case PALETTE_REDMOND:
        uncursed_set_palette16(&palette_win_console);
        return;
    case PALETTE_DKPASTELS:
        uncursed_set_palette16(&palette_dark_pastels);
        return;
    case PALETTE_DEFAULT:
        uncursed_set_palette16(&palette_default);
        return;
    case PALETTE_NONE:
    default:
        uncursed_reset_palette16();
        return;
    }
}

static void
setup_tiles(void)
{
    load_tile_file(settings.tileset, NULL);
    ui_flags.asciiborders = FALSE;

    if (!tiletable || !tiletable_is_cchar)
        return;

    int i = 0;
    for (i = 0; i < tiletable_len; i += 16) {
        /* The format of a tile table entry is 32 bits of tile number, 64 bits
           of substitutions, 11 bits of formatting, and 21 for the character.
           However, due to endianness considerations, we need to reverse the
           first 4, middle 8, and last 4 bits.

           We want to check that the character is always in the 32-126 range. */
        unsigned long unichar = 0;
        unichar |= (unsigned long)(tiletable[i + 12]) << 0;
        unichar |= (unsigned long)(tiletable[i + 13]) << 8;
        unichar |= (unsigned long)(tiletable[i + 14] & 0x1F) << 16;

        if (unichar < (unsigned long)L' ' || unichar > (unsigned long)L'~')
            if (unichar != 0xd800UL && unichar != 0)
                return;
    }

    ui_flags.asciiborders = TRUE;
}

static void
newwin_wrapper(WINDOW **win, int h, int w, int y, int x)
{
    *win = newwin_onscreen(h, w, y, x);
}

static void
resize_wrapper(WINDOW **win, int h, int w, int y, int x)
{
    /* We must run the resize before the move; otherwise, if the window becomes
       smaller but also moves downwards or rightwards, the move may fail and the
       resize would leave it out of bounds. */
    wresize(*win, h, w);
    mvwin(*win, y, x);

    /* Sanity checks to debug mistakes in window resizing. */
    assert(x+w <= COLS);
    assert(y+h <= LINES);
    assert(getbegx(*win) == x);
    assert(getbegy(*win) == y);
    assert(getmaxx(*win) == w);
    assert(getmaxy(*win) == h);
}

static void
create_or_resize_game_windows(void (*wrapper)(WINDOW **, int, int, int, int))
{
    layout_game_windows();

    int outerframewidth = !!ui_flags.draw_outer_frame_lines;
    int y = outerframewidth;
    int x = outerframewidth;

    werase(basewin);

    wrapper(&msgwin, ui_flags.msgheight, ui_flags.mapwidth, y, x);
    y += ui_flags.msgheight + !!ui_flags.draw_horizontal_frame_lines;
    if (LINES == 1)
        y = 0; /* don't crash on very very small terminals */
    wrapper(&mapwin, ui_flags.mapheight,
            ui_flags.mapwidth - 2 * ui_flags.map_padding,
            y, x + ui_flags.map_padding);
    y += ui_flags.mapheight + !!ui_flags.draw_horizontal_frame_lines;
    if (ui_flags.statusheight)
        statuswin = derwin(basewin, ui_flags.statusheight,
                           ui_flags.mapwidth, y, x);
    else
        statuswin = NULL;
    y += ui_flags.statusheight + !!ui_flags.draw_horizontal_frame_lines;
    if (ui_flags.extraheight)
        extrawin = derwin(basewin, ui_flags.extraheight,
                          ui_flags.mapwidth, y, x);
    else
        extrawin = NULL;

    if (ui_flags.sidebarwidth) {
        x += ui_flags.mapwidth + !!ui_flags.draw_vertical_frame_lines;
        sidebar = derwin(basewin, LINES - 2 * outerframewidth,
                         COLS - 2 * outerframewidth - x,
                         outerframewidth, x);
    } else
        sidebar = NULL;

    setup_tiles();
    draw_frame();

    if (tiletable && !tiletable_is_cchar)
        wset_tiles_region(mapwin, ui_flags.mapheight,
                          ui_flags.mapwidth - 2 * ui_flags.map_padding, 0, 0,
                          ROWNO, COLNO, 0, 0);
    else
        wdelete_tiles_region(mapwin);

    mark_mapwin_for_full_refresh();

    keypad(mapwin, TRUE);
    keypad(msgwin, TRUE);
    leaveok(mapwin, FALSE);
    leaveok(msgwin, FALSE);

    if (statuswin)
        leaveok(statuswin, TRUE);
    if (sidebar)
        leaveok(sidebar, TRUE);
    if (extrawin) {
        leaveok(extrawin, TRUE);
        werase(extrawin);
    }
}

/* A wrapper around newwin() that moves the window if necessary to ensure
   that it fits on the screen. */
WINDOW *
newwin_onscreen(int ysize, int xsize, int ymin, int xmin)
{
    if (ymin < 0)
        ymin = 0;
    if (xmin < 0)
        xmin = 0;
    if (ysize + ymin > LINES)
        ymin = LINES - ysize;
    if (xsize + xmin > COLS)
        xmin = COLS - xsize;
    return newwin(ysize, xsize, ymin, xmin);
}

void
create_game_windows(void)
{
    create_or_resize_game_windows(newwin_wrapper);
    ui_flags.ingame = TRUE;
    ui_flags.maphoverx = ui_flags.maphovery = -1;
    setup_showlines();
    redraw_game_windows();
}


static void
resize_game_windows(void)
{
    if (!ui_flags.ingame)
        return;

    /* statuswin and sidebar never accept input, so simply recreating those is
       easiest. */
    if (statuswin) {
        delwin(statuswin);
        statuswin = NULL;
    }
    if (sidebar) {
        cleanup_sidebar(FALSE);
        delwin(sidebar);
        sidebar = NULL;
    }
    if (extrawin) {
        delwin(extrawin);
        extrawin = NULL;
    }

    if (mapwin)
        wdelete_tiles_region(mapwin);

    create_or_resize_game_windows(resize_wrapper);

    if (statuswin)
        leaveok(statuswin, TRUE);
    if (sidebar)
        leaveok(sidebar, TRUE);
    if (extrawin)
        leaveok(extrawin, TRUE);

    redo_showlines();
    redraw_game_windows();
}


void
destroy_game_windows(void)
{
    if (ui_flags.ingame) {
        delwin(msgwin);
        delwin(mapwin);
        if (statuswin)
            delwin(statuswin);
        if (extrawin)
            delwin(extrawin);
        if (sidebar || ui_flags.sidebarwidth) {
            cleanup_sidebar(TRUE);
            if (sidebar)
                delwin(sidebar);
        }
        msgwin = mapwin = statuswin = extrawin = sidebar = NULL;
    }

    ui_flags.ingame = FALSE;
}


void
redraw_popup_windows(void)
{
    struct gamewin *gw;
    for (gw = firstgw; gw; gw = gw->next) {
        if (!gw->win) /* partially constructed */
            continue;
        gw->draw(gw);
        wnoutrefresh(gw->win);
    }
}

void
redraw_game_windows(void)
{
    if (loadwin) {
        delwin(loadwin);
        loadwin = NULL;
    }

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

        if (extrawin)
            wnoutrefresh(extrawin);

        draw_frame();
    }

    redraw_popup_windows();
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
    } else if (settings.tileset) {
        setup_tiles();                 /* for ASCII vs. Unicode border */
        if (basewin && ui_flags.initialized)
            wnoutrefresh(basewin);
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
}


/* When the player presses a key, we want to try to interpret it a zero-time
   command, "menu", or "save", in any context for which it has no other
   meaning. (Thus, you can use the Ctrl-C binding for "menu" to save at a wish
   prompt, for instance.) This means we need to know which keys are meaningful
   in which contexts. */
static nh_bool
key_is_meaningful_in_context(int key, enum keyreq_context context)
{
    if (key == KEY_SIGNAL)
        return TRUE;
    if (!game_is_running)
        return TRUE;

    switch (context) {
        /* Cases in which all input is meaningful... */
    case krc_get_command:
    case krc_interrupt_long_action:
    case krc_keybinding:
        /* ...or which get command arguments, in which case we can't interrupt,
           really */
    case krc_get_movecmd_direction:
    case krc_count:
        /* ...or where any button dismisses a prompt */
    case krc_more:            /* do we want this? */
    case krc_pause_map:
    case krc_notification:
        return TRUE;

        /* Cases in which all direction commands are meaningful, in addition
           to the usual meaningful keys. (To handle things like shift-left,
           control-h, etc.). */
    case krc_getpos:
    case krc_getdir:

        if (ui_flags.current_followmode == FM_WATCH &&
            !ui_flags.in_zero_time_command)
            return FALSE;

        {
            int range;
            if (key_to_dir(key, &range) != DIR_NONE)
                return TRUE;
        }
        /* otherwise fall through */

        /* Cases in which the meaningful inputs are all ASCII and/or
           keypad/function keys, and no input is meaningful when watching. */
    case krc_yn:
    case krc_ynq:
    case krc_yn_generic:
    case krc_getlin:
    case krc_menu:
    case krc_objmenu:
    case krc_query_key_inventory:
    case krc_query_key_inventory_nullable:
    case krc_query_key_inventory_or_floor:
    case krc_query_key_symbol:
    case krc_query_key_letter_reassignment:

        if (ui_flags.current_followmode == FM_WATCH &&
            !ui_flags.in_zero_time_command)
            return FALSE;

        return classify_key(key) == 1  || /* numpad */
            (key >= ' ' && key <= '~') || /* printable ASCII */
            /* editing keys that map to control-combinations */
            key == 8 || key == 10 || key == 13 || key == 27 ||
            /* codes meaningful to some prompt or other */
            key == KEY_BACKSPACE || key == KEY_DC || key == KEY_DOWN ||
            key == KEY_END || key == KEY_ENTER || key == KEY_ESCAPE ||
            key == KEY_HOME || key == KEY_LEFT || key == KEY_NPAGE ||
            key == KEY_PPAGE || key == KEY_RIGHT || key == KEY_UP ||
            /* not real keys */
            key == KEY_OTHERFD || key == KEY_SIGNAL || key == KEY_RESIZE ||
            key == KEY_UNHOVER || key > KEY_MAX;
    }

    /* should be unreachable */
    return TRUE;
}


int
nh_wgetch(WINDOW * win, enum keyreq_context context)
{
    int key = 0;

    /* If we put up a loading screen, redraw behind it. (This removes the
       loading screen as a side effect.) */
    if (loadwin)
        redraw_game_windows();

    draw_extrawin(context);

    /* If we have a message we want to display as soon as we're in-game, do it
       here (so that we can show it above message boxes in watch mode). This
       involves a recursive call, so we have to be careful to prevent an
       infinite regress. */
    if (ui_flags.ingame && ui_flags.gameload_message &&
        !ui_flags.in_zero_time_command) {
        const char *msg = ui_flags.gameload_message;

        ui_flags.in_zero_time_command = TRUE;
        ui_flags.gameload_message = NULL;

        curses_msgwin(msg, krc_notification);

        ui_flags.in_zero_time_command = FALSE;
    }

    do {
        curs_set(ui_flags.want_cursor);

        if (!ui_flags.ingame)
            ui_flags.queued_server_cancels = 0;

        if (ui_flags.queued_server_cancels && !ui_flags.in_zero_time_command) {
            ui_flags.queued_server_cancels--;
            key = KEY_SIGNAL;
        } else {
            wrefresh(win);
            key = wgetch(win);

            /* Slow network connections sometimes throw off the key timings,
               making ESC letter look like Alt-letter (the codes differ only in
               timing). If a user has set their terminal to Alt-is-Meta mode (or
               has a physical Meta key), or if the user wants to play very
               quickly and doesn't care about Alt- combinations, they may want
               to treat all incoming Alt-letter combinations as ESC letter. */
            if (settings.alt_is_esc && key == (KEY_ALT | (key & 0xff))) {
                ungetch(key & 0xff);
                key = KEY_ESCAPE;
            }
        }

        if (!ui_flags.ingame && key == KEY_SIGNAL) {
            /* We got a server cancel that arrived late. Just ignore it. */
            key = 0;
            continue;
        }

        if (ui_flags.ingame && ui_flags.in_zero_time_command &&
            key == KEY_SIGNAL) {
            /* Don't let the server knock us out of something local to the
               client (or effectively local). */
            ui_flags.queued_server_cancels = 1;
            key = 0;
            continue;
        }

        if (key == KEY_HANGUP) {
            nh_exit_game(EXIT_SAVE);

            /* If we're in a game, EXIT_SAVE will longjmp out to the normal game
               saved/over sequence, and eventually the control will get back
               here outside a game (if KEY_HANGUP is returned from any wgetch
               call, it will be returned from all future wgetch calls).

               If we're not in a game, EXIT_SAVE will return normally, and from
               there, we spam ESC until the program is closed. (You can't ESC
               out of the main menu, so we use a special flag for that.) */
            ui_flags.done_hup++;

            /* Sanity checks (at least the first of which has failed in
               practice): ensure that we're outside a game from the client's
               point of view, and ensure that we aren't stuck in some sort
               of menu that doesn't respond correctly to ESC. If either of
               those checks fails, just deinitialize the terminal and exit
               immediately. */
            if (ui_flags.ingame || ui_flags.done_hup > 100) {
                exit_curses_ui();
                nh_lib_exit();
                /* don't free_displaychars() in case we're in an inconsistent
                   state there; it writes to disk */
                exit(EXIT_FAILURE);
            }

            clear_extrawin(); /* kind-of redundant for multiple reasons */
            return KEY_ESCAPE;
        }

        if (key == KEY_UNHOVER || key >= KEY_MAX + 256) {
            /* A mouse action on the map.

               nh_wgetch is only called in two circumstances: from modal dialog
               boxes, and from get_map_key. In modal dialog boxes, we've
               disabled mouse actions from all other windows, including the map;
               thus the only one of these keys we could potentially get is
               KEY_UNHOVER, and maphoverx will be -1 already, so we just ignore
               it.

               When on the map, we record the hover position, then return the
               key so that get_map_key can process it. Clicking cancels a hover;
               this is to work around a bug in Konsole (which will report a
               hover if the mouse is dragged, and will not report an unhover if
               the mouse is subsequently moved away with no buttons held). */

            if (key == KEY_UNHOVER && ui_flags.maphoverx == -1)
                continue;

            if (key < KEY_MAX + 256 + (ROWNO * COLNO * 2)) {
                ui_flags.maphoverx = -1;
                ui_flags.maphovery = -1;
            } else {
                int xy = key - KEY_MAX - 256 - (ROWNO * COLNO * 2);
                ui_flags.maphoverx = xy % COLNO;
                ui_flags.maphovery = xy / COLNO;
            }
        }

        if (key == KEY_RESIZE) {
            key = 0;
            handle_resize();
            draw_extrawin(context);
        }

        if (key == KEY_OTHERFD) {
            key = 0;
            if (ui_flags.connected_to_server)
                nhnet_check_socket_fd();
        }

        if (key && !key_is_meaningful_in_context(key, context)) {
            /* Perhaps the player's trying to open the main menu, save the
               game, or the like? */
            clear_extrawin();
            handle_nested_key(key); /* might longjmp out */
            key = 0;
            draw_extrawin(context);
        }

    } while (!key);

    clear_extrawin();
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

    /* We need to set gw->win in particular to NULL. Otherwise, we might
       try to redraw the window while we're constructing it (say, because
       constructing it requires flushing messages). */
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
        /* P_MAP: pause to show the result of detection or similar. The second
           arg to get_map_key is TRUE to allow this to be dismissed by clicking
           the map. */
        if (get_map_key(FALSE, TRUE, krc_pause_map) == KEY_SIGNAL)
            uncursed_signal_getch();
    }
}


/* expand tabs into proper number of spaces */
static char *
tabexpand(char *sbuf, size_t sbuflen)
{
    /* This array is guaranteed to be large enough to hold the expanded
       string. */
    char buf[strlen(sbuf) * 8 + 1];
    char *bp, *s = sbuf;
    int idx;

    if (!*s)
        return sbuf;

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
    strncpy(sbuf, buf, sbuflen-1);
    sbuf[sbuflen-1] = '\0';
    return sbuf;
}


void
curses_display_buffer(const char *inbuf, nh_bool trymove)
{
    char *line, **lines, *lptr, nul = 0;
    char linebuf[BUFSZ * ROWNO];
    int lcount, i;
    struct nh_menulist menu;

    char buf[strlen(inbuf) + 1];
    strcpy(buf, inbuf);

    init_menulist(&menu);

    for (line = lptr = buf; *lptr; line = lptr) {
        lptr = strchr(line, '\n');
        if (!lptr)
            lptr = &nul; /* no trailing newline, naughty */
        else
            *(lptr++) = '\0';

        strncpy(linebuf, line, BUFSZ * ROWNO);
        if (strchr(linebuf, '\t') != 0)
            tabexpand(linebuf, sizeof linebuf);

        wrap_text(COLNO - 4, linebuf, &lcount, &lines);
        for (i = 0; i < lcount; ++i)
            add_menu_txt(&menu, lines[i], MI_TEXT);

        free_wrap(lines);
    }

    curses_display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                        NULL, null_menu_callback);
}


void
curses_raw_print(const char *str)
{
    endwin();
    fprintf(stderr, strchr(str, '\n') ? "%s" : "%s\n", str);
    refresh();

    curses_msgwin(str, krc_notification);
}


/* sleep for 50 ms */
void
curses_delay_output(void)
{
    if (settings.animation == ANIM_INSTANT ||
        settings.animation == ANIM_INTERRUPTIBLE)
        return;
    doupdate();
#if defined(WIN32)
    Sleep(45);
#else
    nanosleep(&(struct timespec){ .tv_nsec = 50 * 1000 * 1000}, NULL);
#endif
}


void
curses_load_progress(int progress)
{
    /*
     * 123456789012345678
     * Loading... 100.00%
     */
    if (!loadwin)
        loadwin = newdialog(1, 18, 0, NULL);
    wattrset(loadwin, 0);
    mvwprintw(loadwin, 1, 2, "Loading... %6.2f%%",
              (((float) progress) + 0.5f) / 100.0f);
    wrefresh(loadwin);
}

void
curses_server_cancel(void)
{
    uncursed_signal_getch();
}
