/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-30 */
/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/*
 * Standard template for these functions:
 *
 * if (log_replay_input(...)) [use that value]
 * else {log_replay_no_more_options(); [ask for the value] }
 *
 * log_record_input(...)
 *
 * suppress_more();
 * pline([a text description of the input])
 *
 * if (!program_state.in_zero_time_command && isarg) {
 *   flags.last_arg = ...
 * }
 *
 * return ...;
 *
 * Note that an explicit in_zero_time_command check isn't needed on
 * log_replay_input and log_record_input, because they check themselves.
 */

enum nh_client_response
getpos(coord *cc, boolean force, const char *goal, boolean isarg)
{
    int x, y, ask = 1;
    enum nh_client_response rv = NHCR_CLIENT_CANCEL;
    char c[2];

    char accept_chars[] = {
        [NHCR_ACCEPTED] = '.',
        [NHCR_CONTINUE] = ',',
        [NHCR_MOREINFO] = ':',
        [NHCR_MOREINFO_CONTINUE] = ';',
    };
    enum nh_client_response accept_codes[] = {
        ['.'] = NHCR_ACCEPTED,
        [','] = NHCR_CONTINUE,
        [':'] = NHCR_MOREINFO,
        [';'] = NHCR_MOREINFO_CONTINUE,
    };

    if (log_replay_input(0, "P!"))
        ask = 0;
    else if (log_replay_input(3, "P%d,%d%1[.,:;]", &x, &y, c) && *c) {
        cc->x = x;
        cc->y = y;
        rv = accept_codes[(int)*c];
        ask = 0;
    } else
        log_replay_no_more_options();

    x = cc->x;
    y = cc->y;

    flush_screen();

    if (ask) {
        do {
            rv = (*windowprocs.win_getpos) (&x, &y, force, goal);
        } while (force && (rv == NHCR_CLIENT_CANCEL ||
                           x < 1 || y < 1 || x > COLNO || y > ROWNO));
    }

    if (rv == NHCR_CLIENT_CANCEL)
        log_record_input("P!");
    else
        log_record_input("P%d,%d%c", x, y, accept_chars[rv]);

    suppress_more();
    if (rv == NHCR_CLIENT_CANCEL)
        pline("<position: (cancelled)>");
    else
        pline("<position: (%d, %d)>", cc->x, cc->y);

    cc->x = x;
    cc->y = y;

    if (isarg && !program_state.in_zero_time_command) {
        if (rv == -1)
            flags.last_arg.argtype &= ~CMD_ARG_POS;
        else {
            flags.last_arg.argtype |= CMD_ARG_POS;
            flags.last_arg.pos.x = cc->x;
            flags.last_arg.pos.y = cc->y;
        }
    }

    return rv;
}


int
getdir(const char *s, schar * dx, schar * dy, schar * dz, boolean isarg)
{
    static const char *const dirnames[] = {
        "no direction", "west", "northwest", "north", "northeast",
        "east", "southeast", "south", "southwest", "up", "down", "self"
    };
    const char *query = s ? s : "In what direction?";
    boolean restricted = u.umonnum == PM_GRID_BUG;
    enum nh_direction dir;
    int dirint;

    if (!log_replay_input(0, "D%d", &dirint))
        dir = (*windowprocs.win_getdir) (query, restricted);
    else {
        log_replay_no_more_options();
        dir = dirint;
    }

    log_record_input("D%d", (int)dir);

    if (!program_state.in_zero_time_command && isarg)
        flags.last_arg.argtype &= ~CMD_ARG_DIR;

    suppress_more();
    pline("<%s: %s>", query, dirnames[dir + 1]);

    *dz = 0;
    if (!dir_to_delta(dir, dx, dy, dz))
        return 0;

    if (*dx && *dy && u.umonnum == PM_GRID_BUG) {
        *dx = *dy = 0;
        return 0;
    }

    if (isarg && !program_state.in_zero_time_command) {
        flags.last_arg.argtype |= CMD_ARG_DIR;
        flags.last_arg.dir = dir;
    }

    if (!*dz && (Stunned || (Confusion && !rn2(5))))
        confdir(dx, dy);

    return 1;
}


char
query_key(const char *query, int *count)
{
    int key;

    if (count)
        *count = -1;

    if (!log_replay_input(1, "K%d", &key) &&
        (!count || !log_replay_input(2, "K%d,%d", &key, count))) {
        log_replay_no_more_options();
        key = (*windowprocs.win_query_key) (query, count);
    }

    if (!count || *count == -1)
        log_record_input("K%d", key);
    else
        log_record_input("K%d,%d", key, *count);

    suppress_more();
    if (count && *count != -1)
        pline("<%s: %d %c>", query, *count, key);
    else
        pline("<%s: %c>", query, key);

    return key;
}


void
getlin(const char *query, char *bufp, boolean isarg)
{
    if (!log_replay_line(bufp)) {
        log_replay_no_more_options();
        (*windowprocs.win_getlin) (query, bufp);
    }

    log_record_line(bufp);

    suppress_more();
    pline("<%s: %s>", query, bufp[0] == '\033' ? "(escaped)" : bufp);

    if (isarg && !program_state.in_zero_time_command) {
        if (*bufp == '\033')
            flags.last_arg.argtype &= ~CMD_ARG_STR;
        else {
            flags.last_arg.argtype |= CMD_ARG_STR;
            strcpy(flags.last_arg.str, bufp);
        }
    }
}


/* Validates the parameters for the generic yes/no function to prevent the core
   from sending too long a prompt string to the window port causing a buffer
   overflow there. */
char
yn_function(const char *query, const char *resp, char def)
{
    char qbuf[QBUFSZ], key;
    unsigned truncspot, reduction = sizeof (" [N]  ?") + 1;

    if (resp)
        reduction += strlen(resp) + sizeof (" () ");

    if (strlen(query) >= (QBUFSZ - reduction)) {
        paniclog("Query truncated: ", query);
        reduction += sizeof ("...");
        truncspot = QBUFSZ - reduction;
        strncpy(qbuf, query, (int)truncspot);
        qbuf[truncspot] = '\0';
        strcat(qbuf, "...");
    } else
        strcpy(qbuf, query);

    if (!log_replay_input(1, "Y%c", &key)) {
        log_replay_no_more_options();
        key = (*windowprocs.win_yn_function) (qbuf, resp, def);
    }

    log_record_input("Y%c", key);

    suppress_more();
    pline("<%s [%s]: %c>", qbuf, resp, key);
    return key;
}


int
display_menu(struct nh_menulist *menu, const char *title, int how,
             int placement_hint, int *results)
{
    int n, j;

    if (how == PICK_NONE && log_replay_input(0, "M"))
        n = 0;
    else if (log_replay_input(0, "M!"))
        n = -1;
    else if (!log_replay_menu(FALSE, &n, results)) {
        log_replay_no_more_options();
        n = (*windowprocs.win_display_menu) (menu, title, how,
                                             placement_hint, results);
    }

    if (how == PICK_NONE)
        log_record_input("M");
    else if (n == -1) {
        log_record_input("M!");
        suppress_more();
        pline("<%s: cancelled>", title ? title : "List of objects");
    } else {
        char buf[BUFSZ] = "(none selected)";

        log_record_menu(FALSE, n, results);

        if (n == 1) {
            for (j = 0;
                 j < menu->icount && menu->items[j].id != results[0];
                 j++) {}
            strcpy(buf, menu->items[j].caption);
        } else if (n > 1)
            sprintf(buf, "(%d selected)", n);

        suppress_more();
        pline("<%s: %s>", title ? title : "Untitled menu", buf);
    }

    return n;
}


int
display_objects(struct nh_objitem *items, int icount, const char *title,
                int how, int placement_hint, struct nh_objresult *pick_list)
{
    int n, j;

    if (how == PICK_NONE && log_replay_input(0, "O"))
        n = 0;
    else if (log_replay_input(0, "O!"))
        n = -1;
    else if (!log_replay_menu(TRUE, &n, pick_list)) {
        log_replay_no_more_options();
        n = (*windowprocs.win_display_objects) (items, icount, title, how,
                                                placement_hint, pick_list);
    }

    if (how == PICK_NONE)
        log_record_input("O");
    else if (n == -1) {
        log_record_input("O!");
        suppress_more();
        pline("<%s: cancelled>", title ? title : "List of objects");
    } else {
        char buf[BUFSZ] = "(none selected)";

        log_record_menu(TRUE, n, pick_list);

        if (n == 1) {
            for (j = 0; j < icount && items[j].id != pick_list[0].id; j++) {
            }
            sprintf(buf, "%c", items[j].accel);
        } else if (n > 1)
            sprintf(buf, "(%d selected)", n);
        suppress_more();
        pline("<%s: %s>", title ? title : "List of objects", buf);
    }
    return n;
}

boolean
win_list_items(struct nh_objitem * items, int icount, boolean is_invent)
{
    if (!windowprocs.win_list_items)
        return FALSE;

    return (*windowprocs.win_list_items) (items, icount, is_invent);
}

/*windows.c*/
