/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-06 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>
#include <limits.h>


/* Create a new dialog, or reposition an existing one, in an appropriate
   position for showing prompts. Also draw a border around it. */
WINDOW *
newdialog(int height, int width, int dismissable, WINDOW *win)
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
        fresh_message_line(TRUE);
        draw_msgwin();
        if (getmaxx(msgwin) < getmaxx(stdscr))
            width = getmaxx(msgwin) + (ui_flags.draw_outer_frame_lines ? 2 : 0);
        else
            width = getmaxx(stdscr);
        startx = 0;
        starty = getmaxy(msgwin) - (ui_flags.draw_outer_frame_lines ? 0 : 1);
    } else {
        /* out of game, keep dialogs centred */
        starty = (LINES - height) / 2;
        startx = (COLS - width) / 2;
    }

    redraw_popup_windows();

    if (!win)
        win = newwin_onscreen(height, width, starty, startx);
    else {
        mvwin(win, starty, startx);
        wresize(win, height, width);
    }

    werase(win);
    keypad(win, TRUE);
    meta(win, TRUE);
    nh_window_border(win, dismissable);
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
    wnoutrefresh(gw->win);

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

    gw->win = newdialog(wmw->layout_height, wmw->layout_width,
                        wmw->context == krc_notification ? 2 : 1, NULL);
}

static int
curses_msgwin_generic(const char *msg, int (*validator)(int, void *),
                      void *arg, nh_bool cursor_visible,
                      enum keyreq_context context)
{
    int rv;

    /* We don't know whether the window system is inited right now. So ask.
       isendwin() is one of the few uncursed functions that works no matter
       what. */
    if (isendwin()) {
        fprintf(stderr, "%s\n", msg);
        return validator('\x1b', arg);
    }

    /* Don't try to render this on a very small terminal. */
    if (COLS < COLNO || LINES < ROWNO)
        return validator('\x1b', arg);

    int prevcurs = nh_curs_set(cursor_visible);

    struct gamewin *gw = alloc_gamewin(sizeof (struct win_msgwin));
    struct win_msgwin *wmw = (struct win_msgwin *)gw->extra;
    wmw->msg = msg;
    wmw->context = context;
    gw->draw = draw_curses_msgwin;
    gw->resize = resize_curses_msgwin;

    gw->win = 0;
    /* resize_curses_msgwin sets the layout_{width,height}, and because gw->win
       is 0, allocates gw->win. */
    resize_curses_msgwin(gw);
    draw_curses_msgwin(gw);

    rv = -1;
    while (rv == -1)
        rv = validator(nh_wgetch(gw->win, context), arg);

    delete_gamewin(gw);

    nh_curs_set(prevcurs);
    redraw_game_windows();

    return rv;
}

static int
curses_inline_query(const char *msg, int (*validator)(int, void*),
                    void *arg, nh_bool cursor_visible,
                    enum keyreq_context context) {
    if (!game_is_running) {
        curses_impossible("Trying to create inline query outside game.");
        return curses_msgwin_generic(msg, validator, arg, cursor_visible,
                                     context);
    } else if (!msgwin) {
        curses_impossible("Trying to create inline query without msgwin.");
        return curses_msgwin_generic(msg, validator, arg, cursor_visible,
                                     context);
    }

    /* We use a temporary message as the game will send a summary of the
       decision along later. */
    curses_temp_message(msg);
    draw_msgwin();

    /* We do not respect cursor_visible here, since we want the cursor focused
       on the prompt. We need to leave a space after it, though. */
    int y, x;
    getyx(msgwin, y, x);
    if (x < COLNO - 1)
        wmove(msgwin, y, x + 1);

    int rv = -1;
    while (rv == -1)
        rv = validator(nh_wgetch(msgwin, context), arg);

    curses_clear_temp_messages();
    draw_msgwin();

    return rv;
}

static int
curses_getdir_validator(int key, void *unused)
{
    int dir;
    int range;

    (void) unused;
    if (key == KEY_ESCAPE || key == '\x1b')
        return DIR_NONE + 5;
    else if (key == KEY_SIGNAL)
        return DIR_SERVERCANCEL + 5;

    dir = key_to_dir(key, &range);
    if (dir != DIR_NONE)
        return dir + 5;
    else if (key == '5' || key == KEY_B2)
        return DIR_SELF + 5;

    return -1;
}

enum nh_direction
curses_getdir(const char *query, nh_bool restricted)
{
    int rv;
    char qbuf[QBUFSZ];

    snprintf(qbuf, QBUFSZ, "%s", query ? query : "In what direction?");
    if (settings.prompt_inline)
        rv = curses_inline_query(qbuf, curses_getdir_validator, NULL, 0,
                                 krc_getdir);
    else
        rv = curses_msgwin_generic(qbuf, curses_getdir_validator, NULL, 0,
                                   krc_getdir);

    return rv - 5;
}


static int
curses_yn_function_validator(int key, void *resp)
{
    if (key == KEY_SIGNAL)
        return SERVERCANCEL_CHAR;

    if (key == KEY_ESCAPE || key == '\x1b') {
        if (strchr(resp, 'q'))
            return 'q';
        if (strchr(resp, 'n'))
            return 'n';
        return -2; /* interpreted as default by curses_yn_function */
    } else if (strchr(quit_chars, key))
        return -2;

    if (key > 128)
        return -1;

    if (!strchr(resp, key))
        return -1; /* reject the key */

    return key;
}

static char
curses_yn_function_core(const char *query, const char *resp, char def,
                        nh_bool internal)
{
    int key;
    /*
     * "query [resp] (y) \0"
     *       12    3456789
     */
    char prompt[strlen(query) + strlen(resp) + 9];
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
    if (!internal && settings.prompt_inline)
        key = curses_inline_query(prompt, curses_yn_function_validator,
                                  respbuf, 1,
                                  strcmp(respbuf, "yn") == 0 ? krc_yn :
                                  strcmp(respbuf, "ynq") == 0 ? krc_ynq :
                                  krc_yn_generic);
    else
        key = curses_msgwin_generic(prompt, curses_yn_function_validator,
                                    respbuf, 1,
                                    strcmp(respbuf, "yn") == 0 ? krc_yn :
                                    strcmp(respbuf, "ynq") == 0 ? krc_ynq :
                                    krc_yn_generic);
    if (key == -2)
        key = def;

    return key;
}

char
curses_yn_function_game(const char *query, const char *resp, char def)
{
    return curses_yn_function_core(query, resp, def, FALSE);
}

char
curses_yn_function_internal(const char *query, const char *resp, char def)
{
    return curses_yn_function_core(query, resp, def, TRUE);
}


static int
curses_query_key_validator(int key, void *count)
{
    if (key == KEY_SIGNAL)
        return SERVERCANCEL_CHAR;

    if (key == KEY_ESCAPE || key == '\x1b')
        return '\x1b';

    if (key > 128)
        return -1;

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

static struct nh_query_key_result
curses_query_key_core(const char *query, enum nh_query_key_flags flags,
                      nh_bool allow_count, nh_bool internal)
{
    struct nh_query_key_result nqkr;

    nqkr.count = -1;

    if (!internal && settings.prompt_inline)
        nqkr.key = curses_inline_query(query, curses_query_key_validator,
                                       allow_count ? &(nqkr.count) : NULL, 1,
                                       flags + krc_query_key_inventory);
    else
        nqkr.key = curses_msgwin_generic(query, curses_query_key_validator,
                                         allow_count ? &(nqkr.count) : NULL, 1,
                                         flags + krc_query_key_inventory);
    return nqkr;
}

struct nh_query_key_result
curses_query_key_game(const char *query, enum nh_query_key_flags flags,
                      nh_bool allow_count)
{
    return curses_query_key_core(query, flags, allow_count, FALSE);
}

struct nh_query_key_result
curses_query_key_internal(const char *query, enum nh_query_key_flags flags,
                          nh_bool allow_count)
{
    return curses_query_key_core(query, flags, allow_count, TRUE);
}


static int
curses_msgwin_validator(int key, void *unused)
{
    (void) unused;
    if (key == KEY_SIGNAL) {
        /* Clear the message immediately, and signal the next user input
           that happens */
        if (ui_flags.ingame)
            uncursed_signal_getch();
    }
    if (key > KEY_MAX || key == KEY_UNHOVER)
        return -1;
    return key;
}

int
curses_msgwin(const char *msg, enum keyreq_context context)
{
    return curses_msgwin_generic(msg, curses_msgwin_validator, NULL, 0,
                                 context);
}
