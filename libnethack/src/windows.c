/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2016-06-10 */
/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static boolean
force_servercancel(void)
{
    return program_state.followmode == FM_WATCH &&
        !program_state.in_zero_time_command;
}

/*
 * Standard template for these functions:
 *
 * if (!log_want_replay([expected first character])) [ask for the value]
 * else if (log_replay_input(...)) [use that value]
 * else log_replay_no_more_options();
 *
 * if ([there was a server cancel] ||
 *     ([we just prompted the user] && force_servercancel()))
 *    [go back to the start and try again]
 *
 * log_record_input(...)
 * log_time_line()
 *
 * pline_nomore([a text description of the input])
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
    int x = COLNO, y = ROWNO, ask = 1;
    struct nh_getpos_result ngr;
    enum nh_client_response rv = NHCR_SERVER_CANCEL;
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

    while (rv == NHCR_SERVER_CANCEL) {
        ask = !log_want_replay('P');
        if (!ask) {
            if (log_replay_input(0, "P!")) {
                rv = NHCR_CLIENT_CANCEL;
            } else if (log_replay_input(3, "P%d,%d%1[.,:;]", &x, &y, c) && *c) {
                cc->x = x;
                cc->y = y;
                rv = accept_codes[(int)*c];
            } else
                log_replay_no_more_options();
        }

        x = cc->x;
        y = cc->y;

        flush_screen();

        if (ask) {
            do {
                ngr = (*windowprocs.win_getpos) (x, y, force, goal);
                rv = ngr.howclosed;
            } while (force && (rv == NHCR_CLIENT_CANCEL || x < 0 || y < 0 ||
                               x > COLNO - 1 || y > ROWNO - 1));
            x = ngr.x;
            y = ngr.y;
            if (force_servercancel())
                rv = NHCR_SERVER_CANCEL;
        }
    }

    if (rv == NHCR_CLIENT_CANCEL)
        log_record_input("P!");
    else
        log_record_input("P%d,%d%c", x, y, accept_chars[rv]);

    log_time_line();

    if (rv == NHCR_CLIENT_CANCEL)
        pline_nomore("<position: (cancelled)>");
    else
        pline_nomore("<position: (%d, %d)>", x, y);

    cc->x = x;
    cc->y = y;

    if (isarg && !program_state.in_zero_time_command) {
        if (rv == NHCR_CLIENT_CANCEL)
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
    enum nh_direction dir = DIR_SERVERCANCEL;
    int dirint;

    while (dir == DIR_SERVERCANCEL) {
        if (!log_want_replay('D')) {
            dir = (*windowprocs.win_getdir) (query, restricted);
            if (force_servercancel())
                dir = DIR_SERVERCANCEL;
        } else if (log_replay_input(1, "D%d", &dirint)) {
            dir = dirint;
        } else
            log_replay_no_more_options();
    }

    log_record_input("D%d", (int)dir);

    log_time_line();

    if (!program_state.in_zero_time_command && isarg)
        flags.last_arg.argtype &= ~CMD_ARG_DIR;

    pline_nomore("<%s: %s>", query, dirnames[dir + 1]);

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

    turnstate.intended_dx = *dx;
    turnstate.intended_dy = *dy;
    if (!*dz && (Stunned || (Confusion && !rn2(5))))
        confdir(dx, dy);

    return 1;
}


char
query_key(const char *query, enum nh_query_key_flags qkflags, int *count)
{
    struct nh_query_key_result qkr = {.key = SERVERCANCEL_CHAR, .count = -1};

    while (qkr.key == SERVERCANCEL_CHAR) {
        if (!log_want_replay('K')) {
            qkr = (*windowprocs.win_query_key) (query, qkflags, !!count);
            if (force_servercancel())
                qkr.key = SERVERCANCEL_CHAR;
        } else if (!(!count && log_replay_input(1, "K%d", &(qkr.key))) &&
                   !(count && log_replay_input(2, "K%d,%d",
                                               &(qkr.key), &(qkr.count))))
            log_replay_no_more_options();
    }

    if (!count)
        log_record_input("K%d", qkr.key);
    else
        log_record_input("K%d,%d", qkr.key, qkr.count);

    log_time_line();

    if (count && qkr.count != -1)
        pline_nomore("<%s: %d %c>", query, qkr.count, qkr.key);
    else if (count && strchr(quitchars, qkr.key))
        pline_nomore("<%s: cancelled>", query);
    else
        pline_nomore("<%s: %c>", query, qkr.key);

    if (count)
        *count = qkr.count;

    return qkr.key;
}


const char *
getlin(const char *query, boolean isarg)
{
    static const char servercancel_res[] = {SERVERCANCEL_CHAR, 0};
    const char *res = servercancel_res;

    while (*res == SERVERCANCEL_CHAR) {
        if (!log_want_replay('L')) {
            (*windowprocs.win_getlin) (query, &res, msg_getlin_callback);
            if (force_servercancel())
                res = servercancel_res;
        } else if (!log_replay_line(&res))
            log_replay_no_more_options();
    }

    log_record_line(res);

    log_time_line();

    pline_nomore("<%s: %s>", query, res[0] == '\033' ? "(escaped)" : res);

    if (isarg && !program_state.in_zero_time_command) {
        if (*res == '\033')
            flags.last_arg.argtype &= ~CMD_ARG_STR;
        else {
            flags.last_arg.argtype |= CMD_ARG_STR;
            if (flags.last_str_buf)
                free(flags.last_str_buf);
            flags.last_str_buf = malloc(strlen(res) + 1);
            strcpy(flags.last_str_buf, res);
            flags.last_arg.str = flags.last_str_buf;
        }
    }

    return res;
}


/* Validates the parameters for the generic yes/no function to prevent the core
   from sending too long a prompt string to the window port causing a buffer
   overflow there. */
char
yn_function(const char *query, const char *resp, char def)
{
    /* This is a genuine QBUFSZ buffer because it communicates over the API.
       TODO: Change the API so that this hardcoded limit doesn't exist. */
    char qbuf[QBUFSZ], key = SERVERCANCEL_CHAR;
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

    while (key == SERVERCANCEL_CHAR) {
        if (!log_want_replay('Y')) {
            if (program_state.followmode == FM_RECOVERQUIT)
                key = 'q';    /* skip the DYWYPI */
            else {
                key = (*windowprocs.win_yn_function) (qbuf, resp, def);
                if (force_servercancel())
                    key = SERVERCANCEL_CHAR;
            }
        } else if (!log_replay_input(1, "Y%c", &key))
            log_replay_no_more_options();
    }

    log_record_input("Y%c", key);

    log_time_line();

    pline_nomore("<%s [%s]: %c>", qbuf, resp, key);
    return key;
}


int
display_menu(struct nh_menulist *menu, const char *title, int how,
             int placement_hint, const int **results)
{
    int j;
    struct display_menu_callback_data dmcd;

    /* Memory management: we need a stack-allocated copy of the menu items in
       case terminate() gets called, and because we need to be able to refer to
       them even after win_display_menu (which deallocates the menu items)
       returns. */
    struct nh_menuitem item_copy[menu->icount ? menu->icount : 1];
    struct nh_menulist menu_copy = {.items = menu->icount ? item_copy : NULL,
                                    .icount = menu->icount,
                                    .size = 0}; /* size = 0: don't free it */

    if (menu->icount)
        memcpy(item_copy, menu->items, sizeof item_copy);
    dealloc_menulist(menu);

    do {
        if (!log_want_replay('M')) {
            (*windowprocs.win_display_menu) (
                &menu_copy, title, how, placement_hint, &dmcd,
                msg_display_menu_callback);
            if (force_servercancel())
                dmcd.nresults = -2;
        } else if (how == PICK_NONE && log_replay_input(0, "M"))
            dmcd.nresults = 0;
        else if (log_replay_input(0, "M!"))
            dmcd.nresults = -1;
        else if (!log_replay_menu(FALSE, &dmcd))
            log_replay_no_more_options();
    } while (dmcd.nresults == -2); /* server cancel */

    if (results)
        *results = dmcd.results;

    /* Save shim for "impossible" results on menus that have been saved into
       some save files; interpret "pick 0 letters" as "cancel"

       TODO: Fix the various implementations of win_display_menu not to
       return this sort of impossible result */
    if (dmcd.nresults < 1 && how == PICK_LETTER)
        dmcd.nresults = -1;

    if (how == PICK_NONE) {
        log_record_input("M");
        log_time_line();
    } else if (dmcd.nresults == -1) {
        log_record_input("M!");
        log_time_line();
        pline_nomore("<%s: cancelled>", title ? title : "Untitled menu");
    } else if (how == PICK_LETTER) {
        log_record_menu(FALSE, &dmcd);
        log_time_line();
        pline_nomore("<%s: %c>", title ? title : "Untitled menu",
                     dmcd.results[0] < 26 ? 'a' + dmcd.results[0] - 1:
                                            'A' + dmcd.results[0] - 27);
    } else {
        const char *buf = "(none selected)";

        log_record_menu(FALSE, &dmcd);
        log_time_line();

        if (dmcd.nresults == 1) {
            for (j = 0;
                 j < menu_copy.icount && item_copy[j].id != dmcd.results[0];
                 j++) {}
            buf = item_copy[j].caption;
        } else if (dmcd.nresults > 1) {
            buf = msgprintf("(%d selected)", dmcd.nresults);
        }

        pline_nomore("<%s: %s>", title ? title : "Untitled menu", buf);
    }

    return dmcd.nresults;
}

int
display_objects(struct nh_objlist *objlist, const char *title, int how,
                int placement_hint, const struct nh_objresult **results)
{
    struct display_objects_callback_data docd;
    int i, j;

    /* Memory management: we need a stack-allocated copy of the menu items in
       case terminate() gets called, and because we need to be able to refer to
       them even after win_display_menu (which deallocates the menu items)
       returns. */
    struct nh_objitem item_copy[objlist->icount ? objlist->icount : 1];
    struct nh_objlist menu_copy = {.items = objlist->icount ? item_copy : NULL,
                                   .icount = objlist->icount,
                                   .size = 0}; /* size = 0: don't free it */

    if (objlist->icount)
        memcpy(item_copy, objlist->items, sizeof item_copy);
    dealloc_objmenulist(objlist);

    do {
        if (!log_want_replay('O')) {
            (*windowprocs.win_display_objects) (
                &menu_copy, title, how, placement_hint, &docd,
                msg_display_objects_callback);
            if (force_servercancel())
                docd.nresults = -2;
        } else if (how == PICK_NONE && log_replay_input(0, "O"))
            docd.nresults = 0;
        else if (log_replay_input(0, "O!"))
            docd.nresults = -1;
        else if (!log_replay_menu(TRUE, &docd))
            log_replay_no_more_options();
    } while (docd.nresults == -2); /* server cancel */

    if (results)
        *results = docd.results;

    if (how == PICK_NONE) {
        log_record_input("O");
        log_time_line();
    } else if (docd.nresults == -1) {
        log_record_input("O!");
        log_time_line();
        pline_nomore("<%s: cancelled>", title ? title : "List of objects");
    } else {
        const char *buf = "no selections";

        log_record_menu(TRUE, &docd);
        log_time_line();

        char selected[docd.nresults + 1];
        char *sp = selected;
        for (j = 0; j < docd.nresults; j++) {
            for (i = 0; i < menu_copy.icount; i++) {
                if (menu_copy.items[i].id == docd.results[j].id) {
                    if (!menu_copy.items[i].accel) {
                        buf = msgprintf("(%d selected)", docd.nresults);
                        goto no_inventory_letters;
                    }
                    *(sp++) = menu_copy.items[i].accel;
                    break; /* don't buffer overflow on ID clash */
                }
            }
        }
        *sp = 0;

        if (docd.nresults >= 1)
            buf = msg_from_string(selected);

    no_inventory_letters:
        pline_nomore("<%s: %s>", title ? title : "Untitled menu", buf);
    }

    return docd.nresults;
}

void
win_list_items(struct nh_objlist *objlist, boolean is_invent)
{
    struct nh_objlist zero_objlist;

    if (!windowprocs.win_list_items) {
        if (objlist)
            dealloc_objmenulist(objlist);
        return;
    }

    if (!objlist) {
        init_objmenulist(&zero_objlist);
        objlist = &zero_objlist;
    }

    (*windowprocs.win_list_items) (objlist, is_invent);
}

/*windows.c*/
