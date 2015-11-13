/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include "menulist.h"

static void srv_raw_print(const char *str);
static void srv_pause(enum nh_pause_reason r);
static void srv_update_status(struct nh_player_info *pi);
static void srv_print_message(enum msg_channel msgc, const char *msg);
static void srv_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux,
                              int uy);
static void srv_delay_output(void);
static void srv_load_progress(int progress);
static void srv_level_changed(int displaymode);
static void srv_outrip(struct nh_menulist *ml, nh_bool tombstone,
                       const char *name, int gold, const char *killbuf,
                       int end_how, int year);
static void srv_request_command(nh_bool debug, nh_bool completed,
                                nh_bool interrupted, void *callbackarg,
                                void (*)(const struct nh_cmd_and_arg *arg,
                                         void *callbackarg));
static void srv_display_menu(struct nh_menulist *ml, const char *title,
                             int how, int placement_hint, void *callbackarg,
                             void (*)(const int *, int, void *));
static void srv_display_objects(struct nh_objlist *objlist, const char *title,
                                int how, int placement_hint, void *callbackarg,
                                void (*)(const struct nh_objresult *,
                                         int, void *));
static void srv_list_items(struct nh_objlist *objlist, nh_bool invent);
static struct nh_query_key_result srv_query_key(
    const char *query, enum nh_query_key_flags flags, nh_bool allow_count);
static struct nh_getpos_result srv_getpos(int xorig, int yorig,
                                          nh_bool force, const char *goal);
static enum nh_direction srv_getdir(const char *query, nh_bool restricted);
static void srv_getline(const char *query, void *callbackarg,
                        void (*callback)(const char *, void *));
static void srv_server_cancel(void);

/*---------------------------------------------------------------------------*/

struct nh_player_info player_info;
static struct nh_dbuf_entry prev_dbuf[ROWNO][COLNO];
static int prev_invent_icount, prev_floor_icount;
static struct nh_objitem *prev_invent;
static const struct nh_dbuf_entry zero_dbuf;    /* an entry of all zeroes */
static json_t *display_data, *jinvent_items, *jfloor_items;

struct nh_window_procs server_windowprocs = {
    srv_pause,
    srv_display_buffer,
    srv_update_status,
    srv_print_message,
    srv_request_command,
    srv_display_menu,
    srv_display_objects,
    srv_list_items,
    srv_update_screen,
    srv_raw_print,
    srv_query_key,
    srv_getpos,
    srv_getdir,
    srv_yn_function,
    srv_getline,
    srv_delay_output,
    srv_load_progress,
    srv_level_changed,
    srv_outrip,
    srv_server_cancel,
};

/*---------------------------------------------------------------------------*/

static json_t *
client_request(const char *funcname, json_t * request_msg)
{
    json_t *jret, *jobj;
    void *iter;
    const char *key;
    int i;

    client_msg(funcname, request_msg);

    /* client response */
    jret = read_input();
    if (!jret)
        exit_client("Incorrect or damaged response", 0);

    jobj = json_object_get(jret, funcname);
    while (!jobj || !json_is_object(jobj)) {
        iter = json_object_iter(jret);
        key = json_object_iter_key(iter);
        for (i = 0; clientcmd[i].name; i++)
            if (!strcmp(clientcmd[i].name, key))
                break;

        if (clientcmd[i].name) {
            /* The received object contains a valid command in the toplevel
               context. For some commands, we can and should process them even
               with the game waiting for input. Otherwise, tell the client to
               behave itself. */
            if (clientcmd[i].can_run_async)
                clientcmd[i].func(json_object_iter_value(iter));
            else {
                exit_client("Command sent out of sequence", 0);
                break;
            }
        } else {
            exit_client("Incorrect or damaged response", 0);
            break;
        }
        json_decref(jret);
        jret = read_input();
        jobj = json_object_get(jret, funcname);
    }

    if (jobj)
        json_incref(jobj);
    json_decref(jret);

    return jobj;
}


static void
add_display_data(const char *key, json_t * data)
{
    json_t *tmpobj;

    if (!display_data)
        display_data = json_array();

    tmpobj = json_object();
    json_object_set_new(tmpobj, key, data);
    json_array_append_new(display_data, tmpobj);
}


json_t *
get_display_data(void)
{
    json_t *dd;

    if (jfloor_items) {
        add_display_data("list_items", jfloor_items);
        jfloor_items = NULL;
    }
    if (jinvent_items) {
        add_display_data("list_items", jinvent_items);
        jinvent_items = NULL;
    }
    dd = display_data;
    display_data = NULL;
    return dd;
}


/*
 * Callbacks which provide information that must be sent to the user.
 */
static void
srv_raw_print(const char *str)
{
    json_t *jobj = json_string(str);

    add_display_data("raw_print", jobj);
}


static void
srv_pause(enum nh_pause_reason r)
{
    json_t *jobj = json_integer(r);

    /* since the display may stop here, the sidebar info should be up-to-date
       sidebar data is added to the output as a side-effect of
       get_display_data() */
    display_data = get_display_data();
    add_display_data("pause", jobj);
}


void
srv_display_buffer(const char *buf, nh_bool trymove)
{
    json_t *jobj = json_pack("{ss,si}", "buf", buf, "trymove", trymove);

    add_display_data("display_buffer", jobj);
}


static void
srv_update_status(struct nh_player_info *pi)
{
    json_t *jobj, *jarr;
    struct nh_player_info *oi = &player_info;
    int i, all;

    if (!memcmp(&player_info, pi, sizeof (struct nh_player_info)))
        return;

    all = !player_info.plname[0];

    /* only send fields that have changed since the last transmission */
    jobj = json_object();
    if (all) {
        json_object_set_new(jobj, "plname", json_string(pi->plname));
        json_object_set_new(jobj, "coinsym", json_integer(pi->coinsym));
        json_object_set_new(jobj, "max_rank_sz", json_integer(pi->max_rank_sz));
    }
    if (all || strcmp(pi->rank, oi->rank))
        json_object_set_new(jobj, "rank", json_string(pi->rank));
    if (all || strcmp(pi->racename, oi->racename))
        json_object_set_new(jobj, "racename", json_string(pi->racename));
    if (all || strcmp(pi->rolename, oi->rolename))
        json_object_set_new(jobj, "rolename", json_string(pi->rolename));
    if (all || strcmp(pi->gendername, oi->gendername))
        json_object_set_new(jobj, "gendername", json_string(pi->gendername));
    if (all || strcmp(pi->level_desc, oi->level_desc))
        json_object_set_new(jobj, "level_desc", json_string(pi->level_desc));
    if (all || pi->x != oi->x)
        json_object_set_new(jobj, "x", json_integer(pi->x));
    if (all || pi->y != oi->y)
        json_object_set_new(jobj, "y", json_integer(pi->y));
    if (all || pi->z != oi->z)
        json_object_set_new(jobj, "z", json_integer(pi->z));
    if (all || pi->score != oi->score)
        json_object_set_new(jobj, "score", json_integer(pi->score));
    if (all || pi->xp != oi->xp)
        json_object_set_new(jobj, "xp", json_integer(pi->xp));
    if (all || pi->gold != oi->gold)
        json_object_set_new(jobj, "gold", json_integer(pi->gold));
    if (all || pi->moves != oi->moves)
        json_object_set_new(jobj, "moves", json_integer(pi->moves));
    if (all || pi->st != oi->st)
        json_object_set_new(jobj, "st", json_integer(pi->st));
    if (all || pi->st_extra != oi->st_extra)
        json_object_set_new(jobj, "st_extra", json_integer(pi->st_extra));
    if (all || pi->dx != oi->dx)
        json_object_set_new(jobj, "dx", json_integer(pi->dx));
    if (all || pi->co != oi->co)
        json_object_set_new(jobj, "co", json_integer(pi->co));
    if (all || pi->in != oi->in)
        json_object_set_new(jobj, "in", json_integer(pi->in));
    if (all || pi->wi != oi->wi)
        json_object_set_new(jobj, "wi", json_integer(pi->wi));
    if (all || pi->ch != oi->ch)
        json_object_set_new(jobj, "ch", json_integer(pi->ch));
    if (all || pi->align != oi->align)
        json_object_set_new(jobj, "align", json_integer(pi->align));
    if (all || pi->hp != oi->hp)
        json_object_set_new(jobj, "hp", json_integer(pi->hp));
    if (all || pi->hpmax != oi->hpmax)
        json_object_set_new(jobj, "hpmax", json_integer(pi->hpmax));
    if (all || pi->en != oi->en)
        json_object_set_new(jobj, "en", json_integer(pi->en));
    if (all || pi->enmax != oi->enmax)
        json_object_set_new(jobj, "enmax", json_integer(pi->enmax));
    if (all || pi->ac != oi->ac)
        json_object_set_new(jobj, "ac", json_integer(pi->ac));
    if (all || pi->level != oi->level)
        json_object_set_new(jobj, "level", json_integer(pi->level));
    if (all || pi->monnum != oi->monnum)
        json_object_set_new(jobj, "monnum", json_integer(pi->monnum));
    if (all || pi->cur_monnum != oi->cur_monnum)
        json_object_set_new(jobj, "cur_monnum", json_integer(pi->cur_monnum));
    if (all || pi->can_enhance != oi->can_enhance)
        json_object_set_new(jobj, "can_enhance", json_integer(pi->can_enhance));
    if (all ||
        memcmp(pi->statusitems, oi->statusitems, sizeof (pi->statusitems))) {
        jarr = json_array();
        for (i = 0; i < pi->nr_items; i++)
            json_array_append_new(jarr, json_string(pi->statusitems[i]));
        json_object_set_new(jobj, "statusitems", jarr);
    }
    player_info = *pi;

    add_display_data("update_status", jobj);
}


static void
srv_print_message(enum msg_channel msgc, const char *msg)
{
    json_t *jobj = json_pack("{si,ss}", "channel", msgc, "msg", msg);

    add_display_data("print_message", jobj);
}

static void
srv_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux, int uy)
{
    int i, x, y, samedbe, samecols, zerodbe, zerocols, is_same, is_zero;
    json_t *jmsg, *jdbuf, *dbufcol, *dbufent;

    samecols = 0;
    zerocols = 0;
    jdbuf = json_array();
    for (x = 0; x < COLNO; x++) {
        samedbe = 0;
        zerodbe = 0;
        dbufcol = json_array();
        for (y = 0; y < ROWNO; y++) {
            /* an entry may be both the same as before and zero. Check for both
               so that both conditions can be counted */
            is_zero = is_same = FALSE;
            if (!memcmp(&dbuf[y][x], &zero_dbuf, sizeof (dbuf[y][x]))) {
                zerodbe++;
                is_zero = TRUE;
                json_array_append_new(dbufcol, json_integer(0));
            }
            if (!memcmp(&dbuf[y][x], &prev_dbuf[y][x], sizeof (dbuf[y][x]))) {
                samedbe++;
                is_same = TRUE;
                if (!is_zero)
                    json_array_append_new(dbufcol, json_integer(1));
            }
            if (!is_same && !is_zero) {
                /* It pains me to make this an array rather than a struct, but
                   it does cause much less data to be sent. */
                dbufent =
                    json_pack("[i,i,i,i,i,i,i,i,i,i]", dbuf[y][x].effect,
                              dbuf[y][x].bg, dbuf[y][x].trap, dbuf[y][x].obj,
                              dbuf[y][x].obj_mn, dbuf[y][x].mon,
                              dbuf[y][x].monflags, dbuf[y][x].branding,
                              dbuf[y][x].invis, dbuf[y][x].visible);
                json_array_append_new(dbufcol, dbufent);
            }
        }

        is_zero = is_same = FALSE;
        if (zerodbe == ROWNO) { /* entire column is zero */
            zerocols++;
            is_zero = TRUE;
            json_array_append_new(jdbuf, json_integer(0));
        }
        if (samedbe == ROWNO) { /* entire column is unchanged */
            samecols++;
            is_same = TRUE;
            if (!is_zero)
                json_array_append_new(jdbuf, json_integer(1));
        }
        if (!is_same && !is_zero)
            json_array_append(jdbuf, dbufcol);
        json_decref(dbufcol);
    }

    if (samecols == COLNO) {
        json_decref(jdbuf);
        return; /* no point in sending out a message that nothing changed */
    } else if (zerocols == COLNO) {
        json_decref(jdbuf);
        jmsg =
            json_pack("{si,si,so}", "ux", ux, "uy", uy, "dbuf",
                      json_integer(0));
    } else
        jmsg = json_pack("{si,si,so}", "ux", ux, "uy", uy, "dbuf", jdbuf);

    add_display_data("update_screen", jmsg);

    for (i = 0; i < ROWNO; i++)
        memcpy(&prev_dbuf[i], &dbuf[i], sizeof (dbuf[i]));
}


static void
srv_delay_output(void)
{
    add_display_data("delay_output", json_object());
}


static void
srv_level_changed(int displaymode)
{
    add_display_data("level_changed", json_integer(displaymode));
}


static json_t *
json_menuitem(struct nh_menuitem *mi)
{
    json_t *jobj = json_pack(
        "{ss,si,si,si,si,si,si}", "caption", mi->caption, "id", mi->id,
        "role", mi->role, "accel", mi->accel, "group_accel", mi->group_accel,
        "selected", mi->selected, "level", (int)(mi->level));

    return jobj;
}


static void
srv_outrip(struct nh_menulist *ml, nh_bool tombstone, const char *name,
           int gold, const char *killbuf, int end_how, int year)
{
    int i;
    json_t *jobj, *jarr;

    jarr = json_array();
    for (i = 0; i < ml->icount; i++)
        json_array_append_new(jarr, json_menuitem(ml->items + i));
    jobj = json_pack("{so,si,si,si,si,si,ss,ss}", "items", jarr,
                     "icount", ml->icount, "tombstone", tombstone, "gold", gold,
                     "year", year, "how", end_how, "name", name,
                     "killbuf", killbuf);

    dealloc_menulist(ml);

    add_display_data("outrip", jobj);
}


/*
 * Callbacks that require user input
 */

static void
srv_request_command(nh_bool debug, nh_bool completed, nh_bool interrupted,
                    void *callbackarg,
                    void (*callback)(const struct nh_cmd_and_arg *, void *))
{
    json_t *jarg, *jobj;
    const char *cmd, *str;
    struct nh_cmd_and_arg ncaa;

    jobj = json_pack("{sb,sb,sb}", "debug", debug, "completed", completed,
                     "interrupted", interrupted);
    jobj = client_request("request_command", jobj);

    if (json_unpack(jobj, "{ss,so!}", "command", &cmd, "arg", &jarg))
        exit_client("Bad set of parameters for request_command", 0);

    ncaa.arg.argtype = 0;

    if (json_unpack(jarg, "{si*}", "d", &(ncaa.arg.dir)) != -1)
        ncaa.arg.argtype |= CMD_ARG_DIR;
    if (json_unpack(jarg, "{si,si*}",
                    "x", &(ncaa.arg.pos.x), "y", &(ncaa.arg.pos.y)) != -1)
        ncaa.arg.argtype |= CMD_ARG_POS;
    if (json_unpack(jarg, "{si*}", "invlet", &(ncaa.arg.invlet)) != -1)
        ncaa.arg.argtype |= CMD_ARG_OBJ;
    if (json_unpack(jarg, "{ss*}", "str", &str) != -1 && strlen(str) < BUFSZ) {
        ncaa.arg.str = str;
        ncaa.arg.argtype |= CMD_ARG_STR;
    }
    if (json_unpack(jarg, "{si*}", "spelllet", &(ncaa.arg.spelllet)) != -1)
        ncaa.arg.argtype |= CMD_ARG_SPELL;
    if (json_unpack(jarg, "{si*}", "limit", &(ncaa.arg.limit)) != -1)
        ncaa.arg.argtype |= CMD_ARG_LIMIT;

    ncaa.cmd = cmd;
    /* sanitize for implausible/malicious input */
    if (strlen(cmd) >= 60 ||
        strspn(cmd, "abcdefghijklmnopqrstuvwxyz") != strlen(cmd))
        ncaa.cmd = "invalid";

    callback(&ncaa, callbackarg);
}


static void
srv_display_menu(struct nh_menulist *ml, const char *title, int how,
                 int placement_hint, void *callbackarg,
                 void (*callback)(const int *, int, void *))
{
    int i, ret;
    json_t *jobj, *jarr;

    jarr = json_array();
    for (i = 0; i < ml->icount; i++)
        json_array_append_new(jarr, json_menuitem(ml->items + i));
    jobj =
        json_pack("{so,si,ss,si}", "items", jarr, "how", how,
                  "title", title ? title : "", "plhint", placement_hint);

    dealloc_menulist(ml);

    jobj = client_request("display_menu", jobj);
    if (json_unpack(jobj, "{si,so!}", "howclosed", &ret, "results", &jarr) == -1
        || !json_is_array(jarr))
        exit_client("Bad parameter for display_menu", 0);

    int results[json_array_size(jarr) < 1 ? 1 :
                json_array_size(jarr)];
    for (i = 0; i < json_array_size(jarr); i++)
        results[i] = json_integer_value(json_array_get(jarr, i));

    callback(results, ret == NHCR_CLIENT_CANCEL ? -1 :
             json_array_size(jarr), callbackarg);

    json_decref(jobj);
}


static json_t *
json_objitem(struct nh_objitem *oi)
{
    json_t *jobj;

    /* This array should have been an object, but transmission size prevents
       that. */
    jobj =
        json_pack("[s,i,i,i,i,i,i,i,i,i,i]", oi->caption, oi->id, oi->role,
                  oi->count, oi->otype, oi->oclass, oi->weight, oi->buc,
                  oi->accel, oi->group_accel, oi->worn);
    return jobj;
}


static void
srv_display_objects(struct nh_objlist *objlist, const char *title,
                    int how, int placement_hint, void *callbackarg,
                    void (*callback)(const struct nh_objresult *, int, void *))
{
    int i, ret;
    json_t *jobj, *jarr, *jobj2;

    jarr = json_array();
    for (i = 0; i < objlist->icount; i++)
        json_array_append_new(jarr, json_objitem(objlist->items + i));
    jobj =
        json_pack("{so,si,ss,si}", "items", jarr, "how", how,
                  "title", title ? title : "", "plhint", placement_hint);

    dealloc_objmenulist(objlist);

    jobj = client_request("display_objects", jobj);
    if (json_unpack(jobj, "{si,so!}", "howclosed", &ret, "pick_list", &jarr)
        == -1 || !json_is_array(jarr))
        exit_client("Bad parameter for display_objects", 0);

    struct nh_objresult pick_list[json_array_size(jarr) > 0 ?
                                  json_array_size(jarr) : 1];
    for (i = 0; i < json_array_size(jarr); i++) {
        jobj2 = json_array_get(jarr, i);
        if (json_unpack
            (jobj2, "{si,si!}", "id", &pick_list[i].id, "count",
             &pick_list[i].count) == -1)
            exit_client("Bad pick_list in display_objects", 0);
    }

    callback(pick_list, ret == NHCR_CLIENT_CANCEL ? -1 :
             json_array_size(jarr), callbackarg);

    json_decref(jobj);
}


static void
srv_list_items(struct nh_objlist *objlist, nh_bool invent)
{
    int i;
    json_t *jobj, *jarr;

    if (invent && prev_invent && objlist->icount == prev_invent_icount &&
        !memcmp(objlist->items, prev_invent,
                sizeof (struct nh_objitem) * objlist->icount)) {
        dealloc_objmenulist(objlist);
        return;
    }

    if (!invent && objlist->icount == 0 && prev_floor_icount == 0) {
        dealloc_objmenulist(objlist);
        return;
    }

    if (invent) {
        prev_invent_icount = objlist->icount;
        free(prev_invent);
        prev_invent = malloc(sizeof (struct nh_objitem) * objlist->icount);
        memcpy(prev_invent, objlist->items,
               sizeof (struct nh_objitem) * objlist->icount);
    } else
        prev_floor_icount = objlist->icount;

    jarr = json_array();
    for (i = 0; i < objlist->icount; i++)
        json_array_append_new(jarr, json_objitem(objlist->items + i));
    jobj =
        json_pack("{so,si,si}", "items", jarr, "icount",
                  objlist->icount, "invent", invent);

    dealloc_objmenulist(objlist);

    /* there could be lots of list_item calls after each other if the player is
       picking up or dropping large numbers of items. We only care about the
       last state. */
    if (invent) {
        if (jinvent_items)
            json_decref(jinvent_items);
        jinvent_items = jobj;
    } else {
        if (jfloor_items)
            json_decref(jfloor_items);
        jfloor_items = jobj;
    }
}


static struct nh_query_key_result
srv_query_key(const char *query, enum nh_query_key_flags flags,
              nh_bool allow_count)
{
    int ret, c;
    json_t *jobj;

    jobj = json_pack("{ss,si,sb}", "query", query, "flags", (int)flags,
                     "allow_count", (int)allow_count);

    jobj = client_request("query_key", jobj);
    if (json_unpack(jobj, "{si,si!}", "return", &ret, "count", &c) == -1)
        exit_client("Bad parameters for query_key", 0);
    json_decref(jobj);

    return (struct nh_query_key_result){.key = ret, .count = c};
}


static struct nh_getpos_result
srv_getpos(int origx, int origy, nh_bool force, const char *goal)
{
    int ret, x, y;
    json_t *jobj;

    jobj = json_pack("{ss,si,si,si}", "goal", goal, "force", force,
                     "x", origx, "y", origy);
    jobj = client_request("getpos", jobj);

    if (json_unpack(jobj, "{si,si,si!}",
                    "return", &ret, "x", &x, "y", &y) == -1)
        exit_client("Bad parameters for getpos", 0);

    json_decref(jobj);
    return (struct nh_getpos_result){.howclosed = ret, .x = x, .y = y};
}


static enum nh_direction
srv_getdir(const char *query, nh_bool restricted)
{
    int ret;
    json_t *jobj;

    jobj = json_pack("{ss,si}", "query", query, "restricted", restricted);
    jobj = client_request("getdir", jobj);

    if (json_unpack(jobj, "{si!}", "return", &ret) == -1)
        exit_client("Bad parameters for getdir", 0);

    json_decref(jobj);
    return ret;
}


char
srv_yn_function(const char *query, const char *set, char def)
{
    int ret;
    json_t *jobj;

    jobj = json_pack("{ss,ss,si}", "query", query, "set", set, "def", def);
    jobj = client_request("yn", jobj);

    if (json_unpack(jobj, "{si!}", "return", &ret) == -1)
        exit_client("Bad parameters for yn", 0);

    json_decref(jobj);
    return ret;
}


static void
srv_getline(const char *query, void *callbackarg,
            void (*callback)(const char *, void *))
{
    json_t *jobj;
    const char *str;

    jobj = json_pack("{ss}", "query", query);
    jobj = client_request("getline", jobj);

    if (json_unpack(jobj, "{ss!}", "line", &str) == -1)
        exit_client("Bad parameters for getline", 0);

    callback(str, callbackarg);
    json_decref(jobj);
}

static void
srv_load_progress(int progress)
{
    /* Just like with server cancels, we only use a small fraction of
       the normal sending routines, because no response is expected, we can't
       send display data, etc.. */
    char load_progress_msg[sizeof "{\"load_progress\":{\"progress\":10000}}"];
    snprintf(load_progress_msg, sizeof load_progress_msg,
             "{\"load_progress\":{\"progress\":%d}}", progress);
    send_string_to_client(load_progress_msg, FALSE);
}

static void
srv_server_cancel(void)
{
    client_server_cancel_msg();
}

/*---------------------------------------------------------------------------*/

void
reset_cached_displaydata(void)
{
    if (display_data)
        json_decref(display_data);
    if (jinvent_items)
        json_decref(jinvent_items);
    if (jfloor_items)
        json_decref(jfloor_items);
    display_data = jinvent_items = jfloor_items = NULL;

    if (prev_invent)
        free(prev_invent);
    prev_invent = NULL;
    prev_invent_icount = prev_floor_icount = 0;

    memset(&player_info, 0, sizeof (player_info));
    memset(&prev_dbuf, 0, sizeof (prev_dbuf));
}

/* winprocs.c */
