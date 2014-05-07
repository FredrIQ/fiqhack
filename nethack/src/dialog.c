/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-08 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>
#include <limits.h>


/* Create a new dialog, or reposition an existing one, in an appropriate
   position for showing prompts. Also draw a border aound it. */
WINDOW *
newdialog(int height, int width, WINDOW *win)
{
    int starty, startx;

    if (height < 3)
        height = 3;
    if (height > LINES)
        height = LINES;
    if (width > COLS)
        width = COLS;

    if (game_is_running) {
        /* instead of covering up messages, draw the dialog as if it were a
           message */
        fresh_message_line(FALSE);
        draw_msgwin();
        if (getmaxx(msgwin) < getmaxx(stdscr))
            width = getmaxx(msgwin) + (ui_flags.draw_frame ? 2 : 0);
        else
            width = getmaxx(stdscr);
        startx = 0;
        starty = getmaxy(msgwin) - (ui_flags.draw_frame ? 0 : 1);
    } else {
        /* out of game, keep dialogs centred */
        starty = (LINES - height) / 2;
        startx = (COLS - width) / 2;
    }

    if (!win)
        win = newwin(height, width, starty, startx);
    else {
        mvwin(win, starty, startx);
        wresize(win, height, width);
    }

    werase(win);
    keypad(win, TRUE);
    meta(win, TRUE);
    nh_box(win);
    return win;
}

static void
layout_curses_msgwin(struct win_msgwin *wmw, int *linecount, char ***lines,
                     nh_bool recalc)
{
    int width = strlen(wmw->msg) + 4;
    if (width > COLNO - 1)
        width = COLNO - 1;
    if (width > getmaxx(stdscr) - 2)
        width = getmaxx(stdscr) - 2;

    if (!recalc)
        width = wmw->layout_width;

    wrap_text(width - 4, wmw->msg, linecount, lines);

    if (recalc) {
        wmw->layout_width = width;
        wmw->layout_height = *linecount + 2;
    }
}

static void
draw_curses_msgwin(struct gamewin *gw)
{
    int i;

    int linecount;
    char **lines;
    struct win_msgwin *wmw = (struct win_msgwin *)gw->extra;

    layout_curses_msgwin(wmw, &linecount, &lines, 0);

    for (i = 0; i < linecount; i++)
        mvwaddstr(gw->win, 1 + i, 2, lines[i]);
    wrefresh(gw->win);

    free_wrap(lines);
}

static void
resize_curses_msgwin(struct gamewin *gw)
{
    int linecount;
    char **lines;
    struct win_msgwin *wmw = (struct win_msgwin *)gw->extra;

    layout_curses_msgwin(wmw, &linecount, &lines, 1);
    free_wrap(lines);

    gw->win = newdialog(wmw->layout_height, wmw->layout_width, gw->win);
}

static int
curses_msgwin_generic(const char *msg, int (*validator)(int, void *),
                      void *arg, nh_bool cursor_visible)
{
    int rv;

    /* We don't know whether the window system is inited right now. So ask.
       isendwin() is one of the few uncursed functions that works no matter
       what. */
    if (isendwin()) {
        fprintf(stderr, "%s\n", msg);
        return validator('\x1b', arg);
    }

    int prevcurs = curs_set(cursor_visible);

    struct gamewin *gw = alloc_gamewin(sizeof (struct win_msgwin));
    struct win_msgwin *wmw = (struct win_msgwin *)gw->extra;
    wmw->msg = msg;
    gw->draw = draw_curses_msgwin;
    gw->resize = resize_curses_msgwin;

    gw->win = 0;
    /* resize_curses_msgwin sets the layout_{width,height}, and because gw->win
       is 0, allocates gw->win. */
    resize_curses_msgwin(gw);
    draw_curses_msgwin(gw);

    rv = -1;
    while (rv == -1)
        rv = validator(nh_wgetch(gw->win), arg);

    delete_gamewin(gw);

    curs_set(prevcurs);
    redraw_game_windows();

    return rv;
}


enum nh_direction
curses_getdir(const char *query, nh_bool restricted)
{
    int key;
    enum nh_direction dir;
    char qbuf[QBUFSZ];

    snprintf(qbuf, QBUFSZ, "%s", query ? query : "In what direction?");
    key = curses_msgwin(qbuf);
    if (key == '.' || key == 's') {
        return DIR_SELF;
    } else if (key == KEY_ESCAPE) {
        return DIR_NONE;
    }

    dir = key_to_dir(key);
    if (dir == DIR_NONE) {
        curses_msgwin("What a strange direction!");
    }

    return dir;
}


static int
curses_yn_function_validator(int key, void *resp)
{
    if (key == '\x1b') {
        if (strchr(resp, 'q'))
            return 'q';
        if (strchr(resp, 'n'))
            return 'n';
        return -2; /* interpreted as default by curses_yn_function */
    } else if (strchr(quit_chars, key))
        return -2;

    if (!strchr(resp, key))
        return -1; /* reject the key */

    return key;
}

char
curses_yn_function(const char *query, const char *resp, char def)
{
    int key;
    char prompt[strlen(query) + strlen(resp) + 8];
    char *rb;
    char respbuf[strlen(resp) + 1];

    strcpy(respbuf, resp);
    /* any acceptable responses that follow <esc> aren't displayed */
    if ((rb = strchr(respbuf, '\033')) != 0)
        *rb = '\0';

    sprintf(prompt, "%s [%s] ", query, respbuf);
    if (def)
        sprintf(prompt + strlen(prompt), "(%c) ", def);

    strcpy(respbuf, resp);
    key = curses_msgwin_generic(prompt, curses_yn_function_validator,
                                respbuf, 1);
    if (key == -2)
        key = def;

    return key;
}


static int
curses_query_key_validator(int key, void *count)
{
    if (!count || (!isdigit(key) && key != KEY_BACKSPACE))
        return key;

    if (isdigit(key)) {
        if (*(int *)count == -1)
            *(int *)count = 0;
        if (*(int *)count < INT_MAX / 10 ||
            (*(int *)count == INT_MAX / 10 && (key - '0') <= INT_MAX % 10))
            *(int *)count = 10 * *(int *)count + (key - '0');
        else
            *(int *)count = INT_MAX;
    } else {
        if (*(int *)count <= 0)
            *(int *)count = -1;
        else
            *(int *)count /= 10;
    }

    return -1; /* prompt for another key */
}

struct nh_query_key_result
curses_query_key(const char *query, nh_bool allow_count)
{
    struct nh_query_key_result nqkr;

    nqkr.count = -1;

    nqkr.key = curses_msgwin_generic(query, curses_query_key_validator,
                                     allow_count ? &(nqkr.count) : NULL, 1);
    return nqkr;
}


static int
curses_msgwin_validator(int key, void *unused)
{
    (void) unused;
    return key;
}

int
curses_msgwin(const char *msg)
{
    return curses_msgwin_generic(msg, curses_msgwin_validator, NULL, 0);
}
