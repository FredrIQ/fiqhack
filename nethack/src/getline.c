/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
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

static nh_bool ext_cmd_getlin_hook(char *, void *);


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

    while (len >= pos) {
        buf[pos] = buf[pos + 1];
        pos++;
    }
}


static void
draw_getline_inner(struct gamewin *gw, int echo)
{
    struct win_getline *glw = (struct win_getline *)gw->extra;
    int width, i, offset = 0;
    size_t len = strlen(glw->buf);

    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0, 0);
    wattroff(gw->win, FRAME_ATTRS);

    width = getmaxx(gw->win);
    mvwaddnstr(gw->win, 1, 2, glw->query, width - 4);
    if (glw->pos > width - 4)
        offset = glw->pos - (width - 4);

    wmove(gw->win, 2, 2);
    wattron(gw->win, A_UNDERLINE);
    if (echo)
        waddnstr(gw->win, &glw->buf[offset], width - 4);
    else
        for (i = 0; i < (int)len; i++)
            wprintw(gw->win, "*");
    for (i = len; i < width - 4; i++)
        wprintw(gw->win, "%c", (A_UNDERLINE != A_NORMAL) ? ' ' : '_');
    wattroff(gw->win, A_UNDERLINE);
    wmove(gw->win, 2, glw->pos - offset + 2);
    wrefresh(gw->win);
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
    int height, width, startx, starty;

    getmaxyx(gw->win, height, width);
    if (height > LINES)
        height = LINES;
    if (width > COLS)
        width = COLS;

    /* TODO: this does pretty much the same thing as newdialog() in terms of
       planning the window size.  Maybe merge? */

    if (game_is_running) {
        startx = 0;
        // starty = max(0, (getmaxy(msgwin) - height) / 2);
        starty = 0;
    } else {
        starty = (LINES - height) / 2;
        startx = (COLS - width) / 2;
    }

    mvwin(gw->win, starty, startx);
    wresize(gw->win, height, width);
}


/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
static void
hooked_curses_getlin(const char *query, char *buf, getlin_hook_proc hook,
                     void *hook_proc_arg, int echo)
{
    struct gamewin *gw;
    struct win_getline *gldat;
    int height, width, key, prev_curs;
    size_t len = 0;
    nh_bool done = FALSE;
    nh_bool autocomplete = FALSE;

    prev_curs = curs_set(1);

    height = 4;
    width = COLS - 2;
    gw = alloc_gamewin(sizeof (struct win_getline));
    gw->win = newdialog(height, width);
    gw->draw = echo ? draw_getline : draw_getline_noecho;
    gw->resize = resize_getline;
    gldat = (struct win_getline *)gw->extra;
    gldat->buf = buf;
    gldat->query = query;

    buf[0] = 0;
    while (!done) {
        draw_getline_inner(gw, echo);
        errno = 0;
        key = nh_wgetch(gw->win);

        switch (key) {
        case KEY_ESC:
            buf[0] = (char)key;
            buf[1] = 0;
            done = TRUE;
            break;

        case KEY_ENTER:
        case '\r':
            done = TRUE;
            break;

        case KEY_BACKSPACE:    /* different terminals send different codes... */
        case KEY_BACKDEL:
#if defined(PDCURSES)
        case 8:        /* ^H */
#endif
            if (gldat->pos == 0)
                continue;
            gldat->pos--;
            /* fall through */
        case KEY_DC:
            if (len == 0)
                continue;
            len--;
            buf_delete(buf, gldat->pos);
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
                key == KEY_BACKDEL || gldat->pos >= BUFSZ - 2)
                continue;
            buf_insert(buf, gldat->pos, key);
            gldat->pos++;
            len++;

            if (hook) {
                if (autocomplete)
                    /* discard previous completion before looking for a new one 
                     */
                    buf[gldat->pos] = '\0';

                autocomplete = (*hook) (buf, hook_proc_arg);
                len = strlen(buf);      /* (*hook) may modify buf */
            } else
                autocomplete = FALSE;

            break;
        }
    }

    curs_set(prev_curs);

    delwin(gw->win);
    delete_gamewin(gw);
    redraw_game_windows();
}

void
curses_getline(const char *query, char *buffer)
{
    hooked_curses_getlin(query, buffer, NULL, NULL, 1);
}

void
curses_getline_pw(const char *query, char *buffer)
{
    hooked_curses_getlin(query, buffer, NULL, NULL, 0);
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
 *      + base has enough room to hold our string
 */
static nh_bool
ext_cmd_getlin_hook(char *base, void *hook_arg)
{
    int oindex, com_index;
    struct extcmd_hook_args *hpa = hook_arg;

    com_index = -1;
    for (oindex = 0; oindex < hpa->listlen; oindex++) {
        if (!strncmp(base, hpa->namelist[oindex], strlen(base))) {
            if (com_index == -1)        /* no matches yet */
                com_index = oindex;
            else        /* more than 1 match */
                return FALSE;
        }
    }
    if (com_index >= 0) {
        strcpy(base, hpa->namelist[com_index]);
        return TRUE;
    }

    return FALSE;       /* didn't match anything */
}

/* remove excess whitespace from a string buffer (in place) */
static char *
mungspaces(char *bp)
{
    char c, *p, *p2;
    nh_bool was_space = TRUE;

    for (p = p2 = bp; (c = *p) != '\0'; p++) {
        if (c == '\t')
            c = ' ';
        if (c != ' ' || !was_space)
            *p2++ = c;
        was_space = (c == ' ');
    }
    if (was_space && p2 > bp)
        p2--;
    *p2 = '\0';
    return bp;
}


static int
extcmd_via_menu(const char **namelist, const char **desclist, int listlen)
{
    struct nh_menuitem *items;
    int icount, size, *pick_list;
    int *choices;
    char buf[BUFSZ];
    char cbuf[QBUFSZ], prompt[QBUFSZ], fmtstr[20];
    int i, n, nchoices, acount;
    int ret, biggest;
    int accelerator, prevaccelerator;
    int matchlevel = 0;

    size = 10;
    items = malloc(sizeof (struct nh_menuitem) * size);
    choices = malloc((listlen + 1) * sizeof (int));

    ret = 0;
    cbuf[0] = '\0';
    biggest = 0;
    while (!ret) {
        nchoices = n = 0;
        /* populate choices */
        for (i = 0; i < listlen; i++) {
            if (!matchlevel || !strncmp(namelist[i], cbuf, matchlevel)) {
                choices[nchoices++] = i;
                if (strlen(desclist[i]) > biggest) {
                    biggest = strlen(desclist[i]);
                    sprintf(fmtstr, "%%-%ds", biggest + 15);
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
        icount = 0;
        prevaccelerator = 0;
        acount = 0;
        for (i = 0; i < listlen && choices[i] >= 0; ++i) {
            accelerator = namelist[choices[i]][matchlevel];
            if (accelerator != prevaccelerator || nchoices < (ROWNO - 3)) {
                if (acount) {
                    /* flush the extended commands for that letter already in
                       buf */
                    sprintf(buf, fmtstr, prompt);
                    add_menu_item(items, size, icount, prevaccelerator, buf,
                                  prevaccelerator, FALSE);
                    acount = 0;
                }
            }
            prevaccelerator = accelerator;
            if (!acount || nchoices < (ROWNO - 3)) {
                sprintf(prompt, "%s [%s]", namelist[choices[i]],
                        desclist[choices[i]]);
            } else if (acount == 1) {
                sprintf(prompt, "%s or %s", namelist[choices[i - 1]],
                        namelist[choices[i]]);
            } else {
                strcat(prompt, " or ");
                strcat(prompt, namelist[choices[i]]);
            }
            ++acount;
        }
        if (acount) {
            /* flush buf */
            sprintf(buf, fmtstr, prompt);
            add_menu_item(items, size, icount, prevaccelerator, buf,
                          prevaccelerator, FALSE);
        }
        pick_list = malloc(sizeof (int) * icount);
        sprintf(prompt, "Extended Command: %s", cbuf);
        n = curses_display_menu(items, icount, prompt, PICK_ONE,
                                PLHINT_ANYWHERE, pick_list);

        if (n == 1) {
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
        free(pick_list);
    }

    free(choices);
    free(items);

    return ret;
}


/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
nh_bool
curses_get_ext_cmd(char *cmd_out, const char **namelist, const char **desclist,
                   int listlen)
{
    int i;
    struct extcmd_hook_args hpa = { namelist, desclist, listlen };

    if (settings.extmenu) {
        i = extcmd_via_menu(namelist, desclist, listlen);
        if (i == -1)
            return FALSE;
        strcpy(cmd_out, namelist[i]);
        return TRUE;
    }

    /* maybe a runtime option? */
    hooked_curses_getlin("extended command: (? for help)", cmd_out,
                         ext_cmd_getlin_hook, &hpa, 1);
    mungspaces(cmd_out);
    if (cmd_out[0] == 0 || cmd_out[0] == '\033')
        return FALSE;

    return TRUE;
}

/*getline.c*/
