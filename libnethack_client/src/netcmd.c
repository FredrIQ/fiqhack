/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright (c) Daniel Thaler, 2012. */
/* The NetHack client lib may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhclient.h"
#include "menulist.h"

struct netcmd {
    const char *name;
    json_t *(*func) (json_t *params, int display_only);
};

static json_t *cmd_raw_print(json_t *params, int display_only);
static json_t *cmd_pause(json_t *params, int display_only);
static json_t *cmd_display_buffer(json_t *params, int display_only);
static json_t *cmd_update_status(json_t *params, int display_only);
static json_t *cmd_print_message(json_t *params, int display_only);
static json_t *cmd_print_message_nonblocking(json_t *params, int display_only);
static json_t *cmd_update_screen(json_t *params, int display_only);
static json_t *cmd_delay_output(json_t *params, int display_only);
static json_t *cmd_level_changed(json_t *params, int display_only);
static json_t *cmd_outrip(json_t *params, int display_only);
static json_t *cmd_request_command(json_t *params, int display_only);
static json_t *cmd_display_menu(json_t *params, int display_only);
static json_t *cmd_display_objects(json_t *params, int display_only);
static json_t *cmd_list_items(json_t *params, int display_only);
static json_t *cmd_query_key(json_t *params, int display_only);
static json_t *cmd_getpos(json_t *params, int display_only);
static json_t *cmd_getdir(json_t *params, int display_only);
static json_t *cmd_yn_function(json_t *params, int display_only);
static json_t *cmd_getline(json_t *params, int display_only);
static json_t *cmd_server_error(json_t *params, int display_only);


/*---------------------------------------------------------------------------*/


static struct netcmd netcmd_list[] = {
    {"raw_print", cmd_raw_print},
    {"pause", cmd_pause},
    {"display_buffer", cmd_display_buffer},
    {"update_status", cmd_update_status},
    {"print_message", cmd_print_message},
    {"update_screen", cmd_update_screen},
    {"delay_output", cmd_delay_output},
    {"level_changed", cmd_level_changed},
    {"outrip", cmd_outrip},

    {"request_command", cmd_request_command},
    {"display_menu", cmd_display_menu},
    {"display_objects", cmd_display_objects},
    {"list_items", cmd_list_items},
    {"query_key", cmd_query_key},
    {"getpos", cmd_getpos},
    {"getdir", cmd_getdir},
    {"yn", cmd_yn_function},
    {"getline", cmd_getline},
    {"print_message_nonblocking", cmd_print_message_nonblocking},

    {"server_error", cmd_server_error},
    {NULL, NULL}
};

/*---------------------------------------------------------------------------*/

json_t *
handle_netcmd(const char *key, const char *expected, json_t * jmsg)
{
    int i;
    json_t *ret_msg = NULL;

    for (i = 0; netcmd_list[i].name; i++) {
        if (!strcmp(key, netcmd_list[i].name)) {
            ret_msg = netcmd_list[i].func(jmsg, FALSE);
            break;
        }
    }
    json_decref(jmsg);

    char ucbuf[strlen(key) + strlen(expected) + sizeof
               "Unknown command '' received from server (expecting '')"];
    sprintf(ucbuf,
            "Unknown command '%s' received from server (expecting '%s')",
            key, expected);

    if (!netcmd_list[i].name)
        print_error(ucbuf);

    return ret_msg;
}


void
handle_display_list(json_t * display_list)
{
    int i, j, count;
    json_t *jwrap, *jobj;
    void *iter;
    const char *key;

    if (!json_is_array(display_list)) {
        print_error("Invalid display list data type.");
        return;
    }

    count = json_array_size(display_list);
    for (i = 0; i < count; i++) {
        jwrap = json_array_get(display_list, i);
        iter = json_object_iter(jwrap);
        if (!iter) {
            print_error("Odd: Empty display list entry.");
            continue;
        }

        key = json_object_iter_key(iter);
        jobj = json_object_iter_value(iter);

        for (j = 0; netcmd_list[j].name; j++) {
            if (!strcmp(key, netcmd_list[j].name)) {
                netcmd_list[j].func(jobj, TRUE);
                break;
            }
        }

        if (json_object_iter_next(jwrap, iter))
            print_error("Unsupported: more than one command in a"
                        " single display list entry.");

        if (!netcmd_list[j].name)
            print_error("Unknown display list entry type.");
    }
}


static json_t *
cmd_raw_print(json_t *params, int display_only)
{
    if (!json_is_string(params)) {
        print_error("Incorrect parameter type in cmd_raw_print");
        return NULL;
    }

    client_windowprocs.win_raw_print(json_string_value(params));
    return NULL;
}


static json_t *
cmd_pause(json_t *params, int display_only)
{
    if (!json_is_integer(params)) {
        print_error("Incorrect parameter type in cmd_pause");
        return NULL;
    }

    client_windowprocs.win_pause(json_integer_value(params));
    return NULL;
}


static json_t *
cmd_display_buffer(json_t *params, int display_only)
{
    const char *buf;
    int trymove;

    if (json_unpack(params, "{ss,si!}", "buf", &buf, "trymove",
                    &trymove) == -1) {
        print_error("Incorrect parameters in cmd_display_buffer");
        return NULL;
    }

    client_windowprocs.win_display_buffer(buf, trymove);
    return NULL;
}


static json_t *
cmd_update_status(json_t *params, int display_only)
{
    static struct nh_player_info player;
    int i;
    json_t *p;

    if ((p = json_object_get(params, "plname")))
        strncpy(player.plname, json_string_value(p), PL_NSIZ - 1);
    if ((p = json_object_get(params, "rank")))
        strncpy(player.rank, json_string_value(p), PL_NSIZ - 1);
    if ((p = json_object_get(params, "racename")))
        strncpy(player.racename, json_string_value(p), PL_NSIZ - 1);
    if ((p = json_object_get(params, "rolename")))
        strncpy(player.rolename, json_string_value(p), PL_NSIZ - 1);
    if ((p = json_object_get(params, "gendername")))
        strncpy(player.gendername, json_string_value(p), PL_NSIZ - 1);
    if ((p = json_object_get(params, "level_desc")))
        strncpy(player.level_desc, json_string_value(p), COLNO - 1);
    if ((p = json_object_get(params, "x")))
        player.x = json_integer_value(p);
    if ((p = json_object_get(params, "y")))
        player.y = json_integer_value(p);
    if ((p = json_object_get(params, "z")))
        player.z = json_integer_value(p);
    if ((p = json_object_get(params, "score")))
        player.score = json_integer_value(p);
    if ((p = json_object_get(params, "xp")))
        player.xp = json_integer_value(p);
    if ((p = json_object_get(params, "gold")))
        player.gold = json_integer_value(p);
    if ((p = json_object_get(params, "moves")))
        player.moves = json_integer_value(p);
    if ((p = json_object_get(params, "max_rank_sz")))
        player.max_rank_sz = json_integer_value(p);
    if ((p = json_object_get(params, "st")))
        player.st = json_integer_value(p);
    if ((p = json_object_get(params, "st_extra")))
        player.st_extra = json_integer_value(p);
    if ((p = json_object_get(params, "dx")))
        player.dx = json_integer_value(p);
    if ((p = json_object_get(params, "co")))
        player.co = json_integer_value(p);
    if ((p = json_object_get(params, "in")))
        player.in = json_integer_value(p);
    if ((p = json_object_get(params, "wi")))
        player.wi = json_integer_value(p);
    if ((p = json_object_get(params, "ch")))
        player.ch = json_integer_value(p);
    if ((p = json_object_get(params, "align")))
        player.align = json_integer_value(p);
    if ((p = json_object_get(params, "hp")))
        player.hp = json_integer_value(p);
    if ((p = json_object_get(params, "hpmax")))
        player.hpmax = json_integer_value(p);
    if ((p = json_object_get(params, "en")))
        player.en = json_integer_value(p);
    if ((p = json_object_get(params, "enmax")))
        player.enmax = json_integer_value(p);
    if ((p = json_object_get(params, "ac")))
        player.ac = json_integer_value(p);
    if ((p = json_object_get(params, "level")))
        player.level = json_integer_value(p);
    if ((p = json_object_get(params, "coinsym")))
        player.coinsym = json_integer_value(p);
    if ((p = json_object_get(params, "monnum")))
        player.monnum = json_integer_value(p);
    if ((p = json_object_get(params, "cur_monnum")))
        player.cur_monnum = json_integer_value(p);
    if ((p = json_object_get(params, "can_enhance")))
        player.can_enhance = json_integer_value(p);
    if ((p = json_object_get(params, "statusitems"))) {
        player.nr_items = json_array_size(p);
        if (player.nr_items > STATUSITEMS_MAX)
            player.nr_items = STATUSITEMS_MAX;
        for (i = 0; i < player.nr_items; i++)
            strncpy(player.statusitems[i],
                    json_string_value(json_array_get(p, i)), ITEMLEN - 1);
    }

    client_windowprocs.win_update_status(&player);
    return NULL;
}


static json_t *
cmd_print_message(json_t *params, int display_only)
{
    int turn;
    const char *msg;

    if (json_unpack(params, "{si,ss!}", "turn", &turn, "msg", &msg) == -1) {
        print_error("Incorrect parameters in cmd_print_message");
        return NULL;
    }

    client_windowprocs.win_print_message(turn, msg);
    return NULL;
}

static json_t *
cmd_print_message_nonblocking(json_t *params, int display_only)
{
    int turn;
    const char *msg;

    if (json_unpack(params, "{si,ss!}", "turn", &turn, "msg", &msg) == -1) {
        print_error("Incorrect parameters in cmd_print_message_nonblocking");
        return NULL;
    }

    client_windowprocs.win_print_message_nonblocking(turn, msg);
    return NULL;
}

static struct nh_dbuf_entry dbuf[ROWNO][COLNO];
static json_t *
cmd_update_screen(json_t *params, int display_only)
{
    int ux, uy;
    int x, y, effect, bg, trap, obj, obj_mn, mon, monflags, branding, invis,
        visible;
    json_t *jdbuf, *col, *elem;
    int ok = 1;

    if (json_unpack(params, "{si,si,so!}", "ux", &ux, "uy", &uy, "dbuf", &jdbuf)
        == -1) {
        print_error("Incorrect parameters in cmd_update_screen");
        return NULL;
    }

    if (json_is_integer(jdbuf)) {
        if (json_integer_value(jdbuf) == 0) {
            memset(dbuf, 0, sizeof (struct nh_dbuf_entry) * ROWNO * COLNO);
            client_windowprocs.win_update_screen(dbuf, ux, uy);
        } else
            print_error("Incorrect parameter in cmd_update_screen");
        return NULL;
    }

    if (!json_is_array(jdbuf)) {
        print_error("Incorrect parameter in cmd_update_screen");
        return NULL;
    }

    if (json_array_size(jdbuf) != COLNO)
        print_error("Wrong number of columns in cmd_update_screen");
    for (x = 0; x < COLNO; x++) {
        col = json_array_get(jdbuf, x);
        if (json_is_integer(col)) {
            if (json_integer_value(col) == 0) {
                for (y = 0; y < ROWNO; y++)
                    memset(&dbuf[y][x], 0, sizeof (struct nh_dbuf_entry));
            } else if (json_integer_value(col) != 1) {
                print_error("Strange column value in cmd_update_screen");
                ok = 0;
            }
            continue;
        }

        if (!json_is_array(col) || json_array_size(col) != ROWNO) {
            print_error("Wrong column data type in cmd_update_screen");
            ok = 0;
            continue;
        }

        for (y = 0; y < ROWNO; y++) {
            elem = json_array_get(col, y);

            if (json_is_integer(elem)) {
                if (json_integer_value(elem) == 0)
                    memset(&dbuf[y][x], 0, sizeof (struct nh_dbuf_entry));
                else if (json_integer_value(elem) != 1) {
                    print_error("Strange element value in cmd_update_screen");
                    ok = 0;
                }
                continue;
            }

            if (json_unpack
                (elem, "[i,i,i,i,i,i,i,i,i,i!]", &effect, &bg, &trap, &obj,
                 &obj_mn, &mon, &monflags, &branding, &invis, &visible) == -1)
                print_error("Strange element data in cmd_update_screen");
            dbuf[y][x].effect = effect;
            dbuf[y][x].bg = bg;
            dbuf[y][x].trap = trap;
            dbuf[y][x].obj = obj;
            dbuf[y][x].obj_mn = obj_mn;
            dbuf[y][x].mon = mon;
            dbuf[y][x].monflags = monflags;
            dbuf[y][x].branding = branding;
            dbuf[y][x].invis = invis;
            dbuf[y][x].visible = visible;
        }
    }

    if (ok)
        client_windowprocs.win_update_screen(dbuf, ux, uy);
    return NULL;
}


static json_t *
cmd_delay_output(json_t *params, int display_only)
{
    client_windowprocs.win_delay();
    return NULL;
}


static json_t *
cmd_level_changed(json_t *params, int display_only)
{
    if (!json_is_integer(params)) {
        print_error("Incorrect parameter type in cmd_level_changed");
        return NULL;
    }

    client_windowprocs.win_level_changed(json_integer_value(params));
    return NULL;
}


static void
json_read_menuitem(json_t * jobj, struct nh_menuitem *mi)
{
    int accel, group_accel, selected;
    const char *caption;

    if (json_unpack
        (jobj, "{ss,si,si,si,si,si!}", "caption", &caption, "id", &mi->id,
         "role", &mi->role, "accel", &accel, "group_accel", &group_accel,
         "selected", &selected) == -1)
        print_error("Bad menuitem object encountered");
    strncpy(mi->caption, caption, BUFSZ - 1);
    /* char sized struct members can't be set through int pointers */
    mi->accel = accel;
    mi->group_accel = group_accel;
    mi->selected = selected;
}


static json_t *
cmd_outrip(json_t *params, int display_only)
{
    struct nh_menulist menu;
    int end_how, tombstone, gold, year, i;
    const char *name, *killbuf;
    json_t *jarr;

    if (json_unpack
        (params, "{so,si,si,si,si,si,ss,ss!}", "items", &jarr,
         "icount", &(menu.icount), "tombstone", &tombstone, "gold", &gold,
         "year", &year, "how", &end_how, "name", &name,
         "killbuf", &killbuf) == -1) {
        print_error("Incorrect parameter type in cmd_outrip");
        return NULL;
    }

    if (!json_is_array(jarr) || json_array_size(jarr) != menu.icount) {
        print_error("Damaged items array in cmd_outrip");
        return NULL;
    }

    menu.size = menu.icount;
    menu.items = malloc(menu.icount * sizeof (struct nh_menuitem));
    for (i = 0; i < menu.icount; i++)
        json_read_menuitem(json_array_get(jarr, i), menu.items + i);

    client_windowprocs.win_outrip(&menu, tombstone, name, gold,
                                  killbuf, end_how, year);

    return NULL;
}

static void
cmd_request_command_inner(const struct nh_cmd_and_arg *cmd, 
                          void *json_t_to_fill)
{
    json_t **json_t_p = json_t_to_fill;
    json_t *jarg;

    jarg = json_object();

    if (cmd->arg.argtype & CMD_ARG_DIR)
        json_object_set_new(jarg, "d", json_integer(cmd->arg.dir));
    if (cmd->arg.argtype & CMD_ARG_POS) {
        json_object_set_new(jarg, "x", json_integer(cmd->arg.pos.x));
        json_object_set_new(jarg, "y", json_integer(cmd->arg.pos.y));
    }
    if (cmd->arg.argtype & CMD_ARG_OBJ)
        json_object_set_new(jarg, "invlet", json_integer(cmd->arg.invlet));
    if (cmd->arg.argtype & CMD_ARG_STR)
        json_object_set_new(jarg, "str", json_string(cmd->arg.str));
    if (cmd->arg.argtype & CMD_ARG_SPELL)
        json_object_set_new(jarg, "spelllet", json_integer(cmd->arg.spelllet));
    if (cmd->arg.argtype & CMD_ARG_LIMIT)
        json_object_set_new(jarg, "limit", json_integer(cmd->arg.limit));

    *json_t_p = json_pack("{ss,so}", "command", cmd->cmd, "arg", jarg);
}

static json_t *
cmd_request_command(json_t *params, int display_only)
{
    int debug;
    int completed;
    int interrupted;
    json_t *jobj;

    if (json_unpack(params, "{sb,sb,sb*}", "debug", &debug, "completed",
                    &completed, "interrupted", &interrupted) == -1) {
        print_error("Incorrect parameter type in cmd_request_command");
        return NULL;
    }

    client_windowprocs.win_request_command(debug, completed, interrupted,
                                           &jobj, cmd_request_command_inner);
    return jobj;
}


static void
cmd_display_menu_inner(const int *results, int nresults, void *json_t_to_fill)
{
    json_t **json_t_p = json_t_to_fill;
    json_t *jarr;
    int i;

    if (!json_t_p)
        return;

    jarr = json_array();
    for (i = 0; i < nresults; i++)
        json_array_append_new(jarr, json_integer(results[i]));

    *json_t_p = json_pack("{so,si}", "results", jarr, "howclosed",
                          nresults == -1 ? NHCR_CLIENT_CANCEL : NHCR_ACCEPTED);
}

static json_t *
cmd_display_menu(json_t *params, int display_only)
{
    struct nh_menulist menu;
    int how, placement_hint;
    const char *title;
    json_t *jarr, *jobj = NULL;
    int i;

    if (json_unpack
        (params, "{so,si,ss,si!}", "items", &jarr, "how", &how,
         "title", &title, "plhint", &placement_hint) == -1) {
        print_error("Incorrect parameter type in cmd_display_menu");
        return NULL;
    }

    if (!json_is_array(jarr)) {
        print_error("Damaged items array in cmd_display_menu");
        return NULL;
    }

    if (!*title)
        title = NULL;

    menu.size = menu.icount = json_array_size(jarr);
    menu.items = malloc(menu.icount * sizeof (struct nh_menuitem));
    for (i = 0; i < menu.icount; i++)
        json_read_menuitem(json_array_get(jarr, i), menu.items + i);

    client_windowprocs.win_display_menu(&menu, title, how, placement_hint,
                                        display_only ? NULL : &jobj,
                                        cmd_display_menu_inner);
    return jobj;
}


static void
json_read_objitem(json_t * jobj, struct nh_objitem *oi)
{
    const char *caption;
    int accel, group_accel, worn;

    if (json_unpack
        (jobj, "[s,i,i,i,i,i,i,i,i,i,i!]", &caption, &oi->id, &oi->role,
         &oi->count, &oi->otype, &oi->oclass, &oi->weight, &oi->buc, &accel,
         &group_accel, &worn) == -1) {
        print_error("Bad objitem JSON object encountered");
    }
    strncpy(oi->caption, caption, BUFSZ - 1);
    oi->accel = accel;
    oi->group_accel = group_accel;
    oi->worn = worn;
}

static void
cmd_display_objects_inner(const struct nh_objresult *results, int nresults,
                          void *json_t_to_fill)
{
    json_t **json_t_p = json_t_to_fill;
    json_t *jarr, *jobj;
    int i;

    if (!json_t_p)
        return;

    jarr = json_array();
    for (i = 0; i < nresults; i++) {
        jobj = json_pack("{si,si}", "id", results[i].id,
                         "count", results[i].count);
        json_array_append_new(jarr, jobj);
    }

    *json_t_p = json_pack("{so,si}", "pick_list", jarr, "howclosed",
                          nresults == -1 ? NHCR_CLIENT_CANCEL : NHCR_ACCEPTED);
}

static json_t *
cmd_display_objects(json_t *params, int display_only)
{
    struct nh_objlist objlist;
    int i, how, placement_hint;
    const char *title;
    json_t *jarr, *jobj = NULL;

    if (json_unpack
        (params, "{so,si,ss,si!}", "items", &jarr, "how", &how,
         "title", &title, "plhint", &placement_hint) == -1) {
        print_error("Incorrect parameter type in cmd_display_menu");
        return NULL;
    }

    if (!json_is_array(jarr)) {
        print_error("Damaged items array in cmd_display_menu");
        return NULL;
    }

    if (!*title)
        title = NULL;

    objlist.size = objlist.icount = json_array_size(jarr);
    objlist.items = malloc(objlist.icount * sizeof (struct nh_objitem));

    for (i = 0; i < objlist.icount; i++)
        json_read_objitem(json_array_get(jarr, i), objlist.items + i);

    client_windowprocs.win_display_objects(&objlist, title, how, placement_hint,
                                           display_only ? NULL : &jobj,
                                           cmd_display_objects_inner);
    return jobj;
}


static json_t *
cmd_list_items(json_t *params, int display_only)
{
    struct nh_objlist objlist;
    int i, invent;
    json_t *jarr;

    if (json_unpack
        (params, "{so,si,si!}", "items", &jarr, "icount",
         &(objlist.icount), "invent", &invent) == -1) {
        print_error("Incorrect parameter type in cmd_list_items");
        return NULL;
    }

    if (!json_is_array(jarr) || json_array_size(jarr) != objlist.icount) {
        print_error("Damaged items array in cmd_list_items");
        return NULL;
    }

    objlist.items = malloc(objlist.icount * sizeof (struct nh_objitem));
    objlist.size = objlist.icount;

    for (i = 0; i < objlist.icount; i++)
        json_read_objitem(json_array_get(jarr, i), objlist.items + i);
    client_windowprocs.win_list_items(&objlist, invent);

    return NULL;
}


static json_t *
cmd_query_key(json_t *params, int display_only)
{
    const char *query;
    int allow_count, flags;
    struct nh_query_key_result ret;

    if (display_only) {
        print_error
            ("cmd_query_key called when a return value was not expected.");
        return NULL;
    }

    if (json_unpack
        (params, "{ss,si,sb!}", "query", &query, "flags", &flags,
         "allow_count", &allow_count) == -1) {
        print_error("Incorrect parameter type in cmd_query_key");
        return NULL;
    }

    ret = client_windowprocs.win_query_key(query, flags, allow_count);

    return json_pack("{si,si}", "return", ret.key, "count", ret.count);
}


static json_t *
cmd_getpos(json_t *params, int display_only)
{
    const char *goal;
    int x, y, force;
    struct nh_getpos_result ret;

    if (display_only) {
        print_error("cmd_getpos called when a return value was not expected.");
        return NULL;
    }

    if (json_unpack
        (params, "{ss,si,si,si!}", "goal", &goal, "force", &force, "x", &x, "y",
         &y) == -1) {
        print_error("Incorrect parameter type in cmd_getpos");
        return NULL;
    }

    ret = client_windowprocs.win_getpos(x, y, force, goal);
    return json_pack("{si,si,si}", "return", ret.howclosed,
                     "x", ret.x, "y", ret.y);
}


static json_t *
cmd_getdir(json_t *params, int display_only)
{
    const char *query;
    int restr, ret;

    if (display_only) {
        print_error("cmd_getdir called when a return value was not expected.");
        return NULL;
    }

    if (json_unpack(params, "{ss,si!}", "query", &query, "restricted", &restr)
        == -1) {
        print_error("Incorrect parameter type in cmd_getdir");
        return NULL;
    }

    ret = client_windowprocs.win_getdir(query, restr);
    return json_pack("{si}", "return", ret);
}


static json_t *
cmd_yn_function(json_t *params, int display_only)
{
    const char *query, *set;
    int def, ret;

    if (display_only) {
        print_error
            ("cmd_yn_function called when a return value was not expected.");
        return NULL;
    }

    if (json_unpack
        (params, "{ss,ss,si!}", "query", &query, "set", &set, "def",
         &def) == -1) {
        print_error("Incorrect parameter type in cmd_yn_function");
        return NULL;
    }

    ret = client_windowprocs.win_yn_function(query, set, def);
    return json_pack("{si}", "return", ret);
}


static void
cmd_getline_inner(const char *str, void *json_t_to_fill)
{
    json_t **json_t_p = json_t_to_fill;
    *json_t_p = json_pack("{ss}", "line", str);
}

static json_t *
cmd_getline(json_t *params, int display_only)
{
    const char *query;
    json_t *jobj;

    if (display_only) {
        print_error("cmd_getline called when a return value was not expected.");
        return NULL;
    }

    if (json_unpack(params, "{ss!}", "query", &query) == -1) {
        print_error("Incorrect parameter type in cmd_getline");
        return NULL;
    }

    client_windowprocs.win_getlin(query, &jobj, cmd_getline_inner);
    return jobj;
}


/* an error ocurred */
static json_t *
cmd_server_error(json_t *params, int display_only)
{
    int is_error;
    const char *msg;
    char errmsg[BUFSZ];

    if (json_unpack(params, "{sb,ss!}", "error", &is_error, "message", &msg) ==
        -1)
        return NULL;

    /* the error field in the response indicates the server's view of the
       client communication state. If error == FALSE, the problem is internal
       to the server and the client presumably did nothing wrong. In that case
       retrying the last command is OK */
    error_retry_ok = !is_error;

    if (is_error) {
        snprintf(errmsg, BUFSZ, "Server reports error: %s", msg);
        print_error(errmsg);
    }

    if (!restart_connection()) {
        print_error("Connection to server could not be re-established.");
        nhnet_disconnect();
        if (ex_jmp_buf_valid)
            longjmp(ex_jmp_buf, 1);
    }

    return NULL;
}

/* netcmd.c */
