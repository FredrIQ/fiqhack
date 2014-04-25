/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-25 */
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
    int x, y, ask = 1;
    struct nh_getpos_result ngr;
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
            ngr = (*windowprocs.win_getpos) (x, y, force, goal);
            rv = ngr.howclosed;
        } while (force && (rv == NHCR_CLIENT_CANCEL ||
                           x < 0 || y < 0 || x > COLNO - 1 || y > ROWNO - 1));
        x = ngr.x;
        y = ngr.y;
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
    enum nh_direction dir;
    int dirint;

    if (log_replay_input(1, "D%d", &dirint))
        dir = dirint;
    else {
        log_replay_no_more_options();
        dir = (*windowprocs.win_getdir) (query, restricted);
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

    if (!*dz && (Stunned || (Confusion && !rn2(5))))
        confdir(dx, dy);

    return 1;
}


char
query_key(const char *query, int *count)
{
    struct nh_query_key_result qkr = {.count = -1};

    if (!log_replay_input(1, "K%d", &(qkr.key)) &&
        (!count || !log_replay_input(2, "K%d,%d", &(qkr.key), &(qkr.count)))) {
        log_replay_no_more_options();
        qkr = (*windowprocs.win_query_key) (query, !!count);
    }

    if (!count)
        log_record_input("K%d", qkr.key);
    else
        log_record_input("K%d,%d", qkr.key, qkr.count);

    log_time_line();

    if (count && qkr.count != -1)
        pline_nomore("<%s: %d %c>", query, qkr.count, qkr.key);
    else
        pline_nomore("<%s: %c>", query, qkr.key);

    if (count)
        *count = qkr.count;

    return qkr.key;
}


const char *
getlin(const char *query, boolean isarg)
{
    const char *res;
    if (!log_replay_line(&res)) {
        log_replay_no_more_options();
        (*windowprocs.win_getlin) (query, &res, msg_getlin_callback);
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
    /* This is a genuine QBUFSZ buffer because it communicates over the API. */
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

    if (how == PICK_NONE && log_replay_input(0, "M"))
        dmcd.nresults = 0;
    else if (log_replay_input(0, "M!"))
        dmcd.nresults = -1;
    else if (!log_replay_menu(FALSE, &dmcd)) {
        log_replay_no_more_options();
        (*windowprocs.win_display_menu) (
            &menu_copy, title, how, placement_hint, &dmcd,
            msg_display_menu_callback);
    }

    if (results)
        *results = dmcd.results;

    if (how == PICK_NONE) {
        log_record_input("M");
        log_time_line();
    } else if (dmcd.nresults == -1) {
        log_record_input("M!");
        log_time_line();
        pline_nomore("<%s: cancelled>", title ? title : "Untitled menu");
    } else {
        const char *buf = "(none selected)";

        log_record_menu(FALSE, &dmcd);
        log_time_line();

        if (dmcd.nresults == 1) {
            for (j = 0;
                 j < menu_copy.icount && item_copy[j].id != dmcd.results[0];
                 j++) {}
            buf = item_copy[j].caption;
        } else if (dmcd.nresults > 1)
            buf = msgprintf("(%d selected)", dmcd.nresults);

        pline_nomore("<%s: %s>", title ? title : "Untitled menu", buf);
    }

    return dmcd.nresults;
}

int
display_objects(struct nh_objlist *objlist, const char *title, int how,
                int placement_hint, const struct nh_objresult **results)
{
    struct display_objects_callback_data docd;

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

    if (how == PICK_NONE && log_replay_input(0, "O"))
        docd.nresults = 0;
    else if (log_replay_input(0, "O!"))
        docd.nresults = -1;
    else if (!log_replay_menu(TRUE, &docd)) {
        log_replay_no_more_options();
        (*windowprocs.win_display_objects) (
            &menu_copy, title, how, placement_hint, &docd,
            msg_display_objects_callback);
    }

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
        const char *buf = "(none selected)";

        log_record_menu(TRUE, &docd);
        log_time_line();

        /* TODO: Show the object letters that were selected. */
        if (docd.nresults >= 1)
            buf = msgprintf("(%d selected)", docd.nresults);

        pline_nomore("<%s: %s>", title ? title : "Untitled menu", buf);
    }

    return docd.nresults;
}

boolean
win_list_items(struct nh_objlist *objlist, boolean is_invent)
{
    struct nh_objlist zero_objlist;

    if (!windowprocs.win_list_items) {
        dealloc_objmenulist(objlist);
        return FALSE;
    }

    if (!objlist) {
        init_objmenulist(&zero_objlist);
        objlist = &zero_objlist;
    }

    return (*windowprocs.win_list_items) (objlist, is_invent);
}

/*windows.c*/

