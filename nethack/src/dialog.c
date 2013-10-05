/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>
#include <limits.h>


static enum nh_direction last_dir;

WINDOW *
newdialog(int height, int width)
{
    int starty, startx;
    WINDOW *win;

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
        width = getmaxx(msgwin) + (ui_flags.draw_frame ? 2 : 0);
        startx = 0;
        starty = getmaxy(msgwin) - (ui_flags.draw_frame ? 0 : 1);
    } else {
        /* out of game, keep dialogs centred */
        starty = (LINES - height) / 2;
        startx = (COLS - width) / 2;
    }

    win = newwin(height, width, starty, startx);
    keypad(win, TRUE);
    meta(win, TRUE);
    wattron(win, FRAME_ATTRS);
    box(win, 0, 0);
    wattroff(win, FRAME_ATTRS);
    return win;
}


void
reset_last_dir(void)
{
    last_dir = DIR_NONE;
}


enum nh_direction
curses_getdir(const char *query, nh_bool restricted)
{
    int key;
    enum nh_direction dir;
    char qbuf[QBUFSZ];
    nh_bool repeat_hint = check_prev_cmd_same();
    int curr_key = get_current_cmd_key();

    snprintf(qbuf, QBUFSZ, "%s%s%s%s", query ? query : "In what direction?",
             repeat_hint ? " (" : "",
             repeat_hint ? curses_keyname(curr_key) : "",
             repeat_hint ? " to repeat)" : "");
    key = curses_msgwin(qbuf);
    if (key == '.' || key == 's') {
        last_dir = DIR_SELF;
        return DIR_SELF;
    } else if (key == KEY_ESCAPE) {
        return DIR_NONE;
    }

    dir = key_to_dir(key);
    if (dir == DIR_NONE) {
        /* Repeat last direction if the command key for this action is used in
           this direction prompt. */
        if (curr_key == key && last_dir != DIR_NONE)
            dir = last_dir;
        else
            curses_msgwin("What a strange direction!");
    } else {
        last_dir = dir;
    }

    return dir;
}

char
curses_yn_function(const char *query, const char *resp, char def)
{
    int width, height, key, output_count, idx;
    WINDOW *win;
    char prompt[QBUFSZ];
    char *rb, respbuf[QBUFSZ];
    char **output;

    strcpy(respbuf, resp);
    /* any acceptable responses that follow <esc> aren't displayed */
    if ((rb = strchr(respbuf, '\033')) != 0)
        *rb = '\0';
    sprintf(prompt, "%s [%s] ", query, respbuf);
    if (def)
        sprintf(prompt + strlen(prompt), "(%c) ", def);

    wrap_text(COLNO - 5, prompt, &output_count, &output);

    width = COLNO - 1;
    height = output_count + 2;
    win = newdialog(height, width);
    for (idx = 0; idx < output_count; idx++)
        mvwprintw(win, idx + 1, 2, output[idx]);
    wrefresh(win);

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
        } else if (strchr(quit_chars, key)) {
            key = def;
            break;
        }

        if (!strchr(resp, key))
            key = 0;

    } while (!key);

    delwin(win);
    free_wrap(output);
    redraw_game_windows();
    return key;
}


char
curses_query_key(const char *query, int *count)
{
    int width, height, key;
    WINDOW *win;
    int cnt = 0;
    nh_bool hascount = FALSE;

    height = 3;
    width = strlen(query) + 4;
    win = newdialog(height, width);
    mvwprintw(win, 1, 2, query);
    wrefresh(win);

    key = nh_wgetch(win);
    while ((isdigit(key) || key == KEY_BACKSPACE) && count != NULL) {
        if (isdigit(key)) {
            hascount = TRUE;
            /* prevent int overflow */
            if (cnt < INT_MAX / 10 ||
                (cnt == INT_MAX / 10 && (key - '0') <= INT_MAX % 10))
                cnt = 10 * cnt + (key - '0');
            else
                cnt = INT_MAX;
        } else {
            hascount = (cnt > 0);
            cnt /= 10;
        }
        key = nh_wgetch(win);
        hascount = TRUE;
    }

    if (count != NULL) {
        if (!hascount && !cnt)
            cnt = -1;   /* signal to caller that no count was given */
        *count = cnt;
    }

    delwin(win);
    redraw_game_windows();
    return key;
}


int
curses_msgwin(const char *msg)
{
    int key, len;
    int width = strlen(msg) + 4;
    int prevcurs = curs_set(0);
    WINDOW *win = newdialog(3, width);

    len = strlen(msg);
    while (isspace(msg[len - 1]))
        len--;

    mvwaddnstr(win, 1, 2, msg, len);
    wrefresh(win);
    key = nh_wgetch(win);       /* wait for any key */

    delwin(win);
    curs_set(prevcurs);
    redraw_game_windows();

    return key;
}
