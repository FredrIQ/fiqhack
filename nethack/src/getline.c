/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

#include <stdio.h>
#include <errno.h>

struct extcmd_hook_args {
    const char **namelist;
    const char **desclist;
    int listlen;
};

static nh_bool ext_cmd_getlin_hook(struct win_getline *, void *);


static void
buf_insert(char *buf, int pos, char key)
{
    int len = strlen(buf);      /* len must be signed, it may go to -1 */

    while (len >= pos) {
        buf[len + 1] = buf[len];
        len--;
    }
    buf[pos] = key;
}


static void
buf_delete(char *buf, int pos)
{
    int len = strlen(buf);

    while (pos < len) {
        buf[pos] = buf[pos + 1];
        pos++;
    }
}


static void
draw_getline_inner(struct gamewin *gw, int echo)
{
    struct win_getline *glw = (struct win_getline *)gw->extra;
    int width, height, i, offset = 0;
    size_t len = strlen(glw->buf);
    int output_count;
    char **output;

    nh_window_border(gw->win, 2);

    width = getmaxx(gw->win);
    height = getmaxy(gw->win);

    wrap_text(COLNO - 4, glw->query, &output_count, &output);
    for (i = 0; i < output_count; i++)
        mvwaddstr(gw->win, i + 1, 2, output[i]);
    free_wrap(output);

    if (glw->pos > width - 4)
        offset = glw->pos - (width - 4);
    wmove(gw->win, height - 2, 2);
    wattron(gw->win, A_UNDERLINE);
    if (echo)
        waddnstr(gw->win, &glw->buf[offset], width - 4);
    else
        for (i = 0; i < (int)len; i++)
            wprintw(gw->win, "*");
    for (i = len; i < width - 4; i++)
        wprintw(gw->win, "%c", (A_UNDERLINE != A_NORMAL) ? ' ' : '_');
    wattroff(gw->win, A_UNDERLINE);
    wmove(gw->win, height - 2, glw->pos - offset + 2);
    wnoutrefresh(gw->win);
}

void
draw_getline(struct gamewin *gw)
{
    draw_getline_inner(gw, 1);
}

static void
draw_getline_noecho(struct gamewin *gw)
{
    draw_getline_inner(gw, 0);
}

static void
resize_getline(struct gamewin *gw)
{
    int height, width;

    getmaxyx(gw->win, height, width);
    newdialog(height, width, 2, gw->win);
}

static void
lengthen_getlin_buffer(struct win_getline *gldat, size_t newlen)
{
    if (newlen < gldat->buf_alloclen)
        return;
    if (newlen < gldat->buf_alloclen * 2)
        newlen = gldat->buf_alloclen * 2;

    gldat->buf = realloc(gldat->buf, newlen + 1);
    gldat->buf_alloclen = newlen + 1;
}

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
static void
hooked_curses_getlin(const char *query, void *callbackarg,
                     void (*callback)(const char *, void *),
                     getlin_hook_proc hook, void *hook_proc_arg, int echo)
{
    struct gamewin *gw;
    struct win_getline *gldat;
    int height, width, key, prev_curs;
    size_t len = 0;
    nh_bool done = FALSE;
    nh_bool autocomplete = FALSE;
    char **output;
    int output_count;

    if (isendwin() || COLS < COLNO || LINES < ROWNO) {
        callback("\x1b", callbackarg);
        return;
    }

    prev_curs = nh_curs_set(1);

    /* wrap text just for determining dimensions */
    wrap_text(COLNO - 4, query, &output_count, &output);
    free_wrap(output);
    height = output_count + 3;
    width = COLNO;

    gw = alloc_gamewin(sizeof (struct win_getline), TRUE);
    gw->win = newdialog(height, width, 2, 0);
    gw->draw = echo ? draw_getline : draw_getline_noecho;
    gw->resize = resize_getline;
    gldat = (struct win_getline *)gw->extra;
    gldat->query = query;
    gldat->buf = NULL;
    gldat->buf_alloclen = 0;
    lengthen_getlin_buffer(gldat, 0);
    gldat->buf[0] = '\0';
    /* Tell delete_gamewin to free gldat->buf if we get an exception. */
    gw->dyndata = &(gldat->buf);

    while (!done) {
        draw_getline_inner(gw, echo);
        errno = 0;
        key = nh_wgetch(gw->win, krc_getlin);

        switch (key) {
        case KEY_ESCAPE:
        case '\x1b':
            lengthen_getlin_buffer(gldat, 1);
            gldat->buf[0] = (char)key;
            gldat->buf[1] = 0;
            done = TRUE;
            break;

        case KEY_SIGNAL:
            lengthen_getlin_buffer(gldat, 1);
            gldat->buf[0] = SERVERCANCEL_CHAR;
            gldat->buf[1] = 0;
            done = TRUE;
            break;            

        case KEY_ENTER:
        case '\r':
            done = TRUE;
            break;

        case KEY_BACKSPACE:
        case 8:
            if (gldat->pos == 0)
                continue;
            gldat->pos--;
            /* fall through */
        case KEY_DC:
            if (len == 0)
                continue;
            len--;
            buf_delete(gldat->buf, gldat->pos);
            break;

        case KEY_LEFT:
            if (gldat->pos > 0)
                gldat->pos--;
            break;

        case KEY_RIGHT:
            if (gldat->pos < len)
                gldat->pos++;
            break;

        case KEY_HOME:
            gldat->pos = 0;
            break;

        case KEY_END:
            gldat->pos = len;
            break;

        case ERR:
            if (errno != 0) {
                perror("wgetch");
            } else {
                fprintf(stderr, "wgetch: Unspecified Error\n");
            }
            exit(EXIT_FAILURE);

        default:
            if (' ' > (unsigned)key || (unsigned)key >= 128 ||
                key == KEY_BACKSPACE || gldat->pos >= BUFSZ - 2)
                continue;
            lengthen_getlin_buffer(gldat, len + 1);
            buf_insert(gldat->buf, gldat->pos, key);
            gldat->pos++;
            len++;

            if (hook) {
                if (autocomplete)
                    /* discard previous completion before looking for a new one 
                     */
                    gldat->buf[gldat->pos] = '\0';

                autocomplete = (*hook) (gldat, hook_proc_arg);
                len = strlen(gldat->buf);      /* (*hook) may modify buf */
            } else
                autocomplete = FALSE;

            break;
        }
    }

    nh_curs_set(prev_curs);

    char *bufcopy = gldat->buf;
    gw->dyndata = NULL; /* don't free gldat->buf */ 
    delete_gamewin(gw); /* frees gldat */

    redraw_game_windows();

    callback(bufcopy, callbackarg);
    free(bufcopy);
}

void
curses_getline(const char *query, void *callbackarg,
               void (*callback)(const char *, void *))
{
    hooked_curses_getlin(query, callbackarg, callback, NULL, NULL, 1);
}

void
curses_getline_pw(const char *query, void *callbackarg,
                  void (*callback)(const char *, void *))
{
    hooked_curses_getlin(query, callbackarg, callback, NULL, NULL, 0);
}


/*
 * Implement extended command completion by using this hook into
 * curses_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return TRUE if we've extended the string at base.  Otherwise return FALSE.
 * Assumptions:
 *
 *      + we don't change the characters that are already in base
 *      + we expand base's buffer if necessary
 */
static nh_bool
ext_cmd_getlin_hook(struct win_getline *base, void *hook_arg)
{
    int oindex, com_index;
    struct extcmd_hook_args *hpa = hook_arg;

    com_index = -1;
    for (oindex = 0; oindex < hpa->listlen; oindex++) {
        if (!strncasecmp(base->buf, hpa->namelist[oindex], strlen(base->buf))) {
            if (com_index == -1)        /* no matches yet */
                com_index = oindex;
            else        /* more than 1 match */
                return FALSE;
        }
    }
    if (com_index >= 0) {
        lengthen_getlin_buffer(base, strlen(hpa->namelist[com_index]));
        strcpy(base->buf, hpa->namelist[com_index]);
        return TRUE;
    }

    return FALSE;       /* didn't match anything */
}


static int
extcmd_via_menu(const char **namelist, const char **desclist, int listlen)
{
    struct nh_menulist menu;
    char buf[BUFSZ];
    char cbuf[QBUFSZ], prompt[QBUFSZ];
    int i, nchoices, acount;
    int ret, biggest;
    int accelerator, prevaccelerator;
    int matchlevel = 0;

    int choices[listlen + 1];

    ret = 0;
    cbuf[0] = '\0';
    biggest = 0;
    while (!ret) {
        nchoices = 0;
        /* populate choices */
        for (i = 0; i < listlen; i++) {
            if (!matchlevel || !strncmp(namelist[i], cbuf, matchlevel)) {
                choices[nchoices++] = i;
                if (strlen(desclist[i]) > biggest) {
                    biggest = strlen(desclist[i]);
                }
            }
        }
        choices[nchoices] = -1;

        /* if we're down to one, we have our selection so get out of here */
        if (nchoices == 1) {
            for (i = 0; i < listlen; i++)
                if (!strncmp(namelist[i], cbuf, matchlevel)) {
                    ret = i;
                    break;
                }
            break;
        }

        /* otherwise... */
        init_menulist(&menu);

        prevaccelerator = 0;
        acount = 0;
        for (i = 0; i < listlen && choices[i] >= 0; ++i) {
            accelerator = namelist[choices[i]][matchlevel];
            if (accelerator != prevaccelerator || nchoices < (ROWNO - 3)) {
                if (acount) {
                    /* flush the extended commands for that letter already in
                       buf */
                    snprintf(buf, ARRAY_SIZE(buf), "%-*s", biggest + 15, prompt);
                    add_menu_item(&menu, prevaccelerator, buf,
                                  prevaccelerator, FALSE);
                    acount = 0;
                }
            }
            prevaccelerator = accelerator;
            if (!acount || nchoices < (ROWNO - 3)) {
                snprintf(prompt, ARRAY_SIZE(prompt), "%s [%s]", namelist[choices[i]],
                        desclist[choices[i]]);
            } else if (acount == 1) {
                snprintf(prompt, ARRAY_SIZE(prompt), "%s or %s", namelist[choices[i - 1]],
                        namelist[choices[i]]);
            } else {
                strcat(prompt, " or ");
                strcat(prompt, namelist[choices[i]]);
            }
            ++acount;
        }
        if (acount) {
            /* flush buf */
            snprintf(buf, ARRAY_SIZE(buf), "%-*s", biggest + 15, prompt);
            add_menu_item(&menu, prevaccelerator, buf, prevaccelerator, FALSE);
        }

        int pick_list[1];
        snprintf(prompt, ARRAY_SIZE(prompt), "Extended Command: %s", cbuf);
        curses_display_menu(&menu, prompt, PICK_ONE, PLHINT_ANYWHERE,
                            pick_list, curses_menu_callback);

        if (*pick_list != CURSES_MENU_CANCELLED) {
            if (matchlevel > (QBUFSZ - 2)) {
                ret = -1;
            } else {
                cbuf[matchlevel++] = (char)pick_list[0];
                cbuf[matchlevel] = '\0';
            }
        } else {
            if (matchlevel) {
                ret = 0;
                matchlevel = 0;
            } else
                ret = -1;
        }
    }

    return ret;
}


/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
void
curses_get_ext_cmd(const char **namelist, const char **desclist, int listlen,
                   void *callback_arg, void (*callback)(const char *, void *))
{
    int i;
    struct extcmd_hook_args hpa = { namelist, desclist, listlen };

    if (settings.extmenu) {

        i = extcmd_via_menu(namelist, desclist, listlen);
        if (i == -1) {
            callback("\033", callback_arg);
            return;
        }
        callback(namelist[i], callback_arg);

    } else {

        /* maybe a runtime option? */
        hooked_curses_getlin("extended command: (? for help)", callback_arg,
                             callback, ext_cmd_getlin_hook, &hpa, 1);
    }
}

/*getline.c*/
