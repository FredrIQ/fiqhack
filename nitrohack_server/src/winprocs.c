/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"


static void srv_raw_print(const char *str);
static void srv_pause(enum nh_pause_reason r);
static void srv_display_buffer(char *buf, nh_bool trymove);
static void srv_update_status(struct nh_player_info *pi);
static void srv_print_message(int turn, const char *msg);
static void srv_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO]);
static void srv_delay_output(void);
static void srv_level_changed(int displaymode);
static void srv_outrip(struct nh_menuitem *items,int icount, nh_bool tombstone,
			   char *name, int gold, char *killbuf, int end_how, int year);
static int srv_display_menu(struct nh_menuitem *items, int icount,
				const char *title, int how, int *results);
static int srv_display_objects(struct nh_objitem *items, int icount, const char *title,
			int how, struct nh_objresult *pick_list);
static nh_bool srv_list_items(struct nh_objitem *items, int icount, nh_bool invent);
static char srv_query_key(const char *query, int *count);
static int srv_getpos(int *x, int *y, nh_bool force, const char *goal);
static enum nh_direction srv_getdir(const char *query, nh_bool restricted);
static char srv_yn_function(const char *query, const char *rset, char defchoice);
static void srv_getline(const char *query, char *buf);

static void srv_alt_raw_print(const char *str);
static void srv_alt_pause(enum nh_pause_reason r);
static void srv_alt_display_buffer(char *buf, nh_bool trymove);
static void srv_alt_update_status(struct nh_player_info *pi);
static void srv_alt_print_message(int turn, const char *msg);
static void srv_alt_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO]);
static void srv_alt_delay_output(void);
static void srv_alt_level_changed(int displaymode);
static void srv_alt_outrip(struct nh_menuitem *items,int icount, nh_bool tombstone,
			   char *name, int gold, char *killbuf, int end_how, int year);
static nh_bool srv_alt_list_items(struct nh_objitem *items, int icount, nh_bool invent);

/*---------------------------------------------------------------------------*/

static struct nh_player_info prev_player_info;
static struct nh_dbuf_entry prev_dbuf[ROWNO][COLNO];
static int prev_invent_icount;
static struct nh_objitem *prev_invent;
static const struct nh_dbuf_entry zero_dbuf; /* an entry of all zeroes */
static json_t *display_data, *jinvent_items, *jfloor_items;
static int altproc;

struct nh_window_procs server_windowprocs = {
    srv_pause,
    srv_display_buffer,
    srv_update_status,
    srv_print_message,
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
    srv_level_changed,
    srv_outrip,
};


/* alternative window procs for replay*/
struct nh_window_procs server_alt_windowprocs = {
    srv_alt_pause,
    srv_alt_display_buffer,
    srv_alt_update_status,
    srv_alt_print_message,
    NULL,
    NULL,
    srv_alt_list_items,
    srv_alt_update_screen,
    srv_alt_raw_print,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    srv_alt_delay_output,
    srv_alt_level_changed,
    srv_alt_outrip,
};

/*---------------------------------------------------------------------------*/

static json_t *client_request(const char *funcname, json_t *request_msg)
{
    json_t *jret, *jobj;
    
    client_msg(funcname, request_msg);
    json_decref(request_msg);
    
    /* client response */
    jret = read_input();

    jobj = json_object_get(jret, funcname);
    if (!jobj || !json_is_object(jobj))
	exit_client("Incorrect response");
    
    json_incref(jobj);
    json_decref(jret);
    
    return jobj;
}


static void add_display_data(const char *key, json_t *data)
{
    json_t *tmpobj;
    char keystr[BUFSZ];
    
    if (altproc) {
	snprintf(keystr, BUFSZ-1, "alt_%s", key);
	keystr[BUFSZ-1] = '\0';
	key = keystr;
    }
    
    if (!display_data)
	display_data = json_array();
    
    tmpobj = json_object();
    json_object_set_new(tmpobj, key, data);
    json_array_append_new(display_data, tmpobj);
}


json_t *get_display_data(void)
{
    if (jfloor_items) {
	add_display_data("list_items", jfloor_items);
	jfloor_items = NULL;
    }
    if (jinvent_items) {
	add_display_data("list_items", jinvent_items);
	jinvent_items = NULL;
    }
    return display_data;
}


/*
 * Callbacks which provide information that must be sent to the user.
 */
static void srv_raw_print(const char *str)
{
    json_t *jobj = json_string(str);
    add_display_data("raw_print", jobj);
}


static void srv_pause(enum nh_pause_reason r)
{
    json_t *jobj = json_integer(r);
    /* since the display may stop here, the sidebar info should be up-to-date
     * sidebar data is added to the output as a side-effect of get_display_data() */
    get_display_data();
    add_display_data("pause", jobj);
}


static void srv_display_buffer(char *buf, nh_bool trymove)
{
    json_t *jobj = json_pack("{ss,si}", "buf", buf, "trymove", trymove);
    add_display_data("pause", jobj);
}


static void srv_update_status(struct nh_player_info *pi)
{
    json_t *jobj, *jarr;
    int i;
    
    if (!memcmp(&prev_player_info, pi, sizeof(struct nh_player_info)))
	return;
    
    prev_player_info = *pi;
    jobj = json_object();
    json_object_set_new(jobj, "plname", json_string(pi->plname));
    json_object_set_new(jobj, "rank", json_string(pi->rank));
    json_object_set_new(jobj, "level_desc", json_string(pi->level_desc));
    json_object_set_new(jobj, "x", json_integer(pi->x));
    json_object_set_new(jobj, "y", json_integer(pi->y));
    json_object_set_new(jobj, "z", json_integer(pi->z));
    json_object_set_new(jobj, "score", json_integer(pi->score));
    json_object_set_new(jobj, "xp", json_integer(pi->xp));
    json_object_set_new(jobj, "gold", json_integer(pi->gold));
    json_object_set_new(jobj, "moves", json_integer(pi->moves));
    json_object_set_new(jobj, "max_rank_sz", json_integer(pi->max_rank_sz));
    json_object_set_new(jobj, "st", json_integer(pi->st));
    json_object_set_new(jobj, "st_extra", json_integer(pi->st_extra));
    json_object_set_new(jobj, "dx", json_integer(pi->dx));
    json_object_set_new(jobj, "co", json_integer(pi->co));
    json_object_set_new(jobj, "in", json_integer(pi->in));
    json_object_set_new(jobj, "wi", json_integer(pi->wi));
    json_object_set_new(jobj, "ch", json_integer(pi->ch));
    json_object_set_new(jobj, "align", json_integer(pi->align));
    json_object_set_new(jobj, "nr_items", json_integer(pi->nr_items));
    json_object_set_new(jobj, "hp", json_integer(pi->hp));
    json_object_set_new(jobj, "hpmax", json_integer(pi->hpmax));
    json_object_set_new(jobj, "en", json_integer(pi->en));
    json_object_set_new(jobj, "enmax", json_integer(pi->enmax));
    json_object_set_new(jobj, "ac", json_integer(pi->ac));
    json_object_set_new(jobj, "level", json_integer(pi->level));
    json_object_set_new(jobj, "coinsym", json_integer(pi->coinsym));
    json_object_set_new(jobj, "monnum", json_integer(pi->monnum));
    json_object_set_new(jobj, "cur_monnum", json_integer(pi->cur_monnum));
    json_object_set_new(jobj, "enhance_possible", json_integer(pi->enhance_possible));
    
    jarr = json_array();
    for (i = 0; i < pi->nr_items; i++)
	json_array_append_new(jarr, json_string(pi->statusitems[i]));
    json_object_set_new(jobj, "statusitems", jarr);

    add_display_data("player_info", jobj);
}


static void srv_print_message(int turn, const char *msg)
{
    json_t *jobj = json_object();
    json_object_set_new(jobj, "turn", json_integer(turn));
    json_object_set_new(jobj, "msg", json_string(msg));
    add_display_data("print_message", jobj);
}


static void srv_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO])
{
    int i, x, y, samedbe, samecols, zerodbe, zerocols;
    json_t *jdbuf, *dbufcol, *dbufent;
    
    samecols = 0;
    zerocols = 0;
    jdbuf = json_array();
    for (x = 0; x < COLNO; x++) {
	samedbe = 0;
	zerodbe = 0;
	dbufcol = json_array();
	for (y = 0; y < ROWNO; y++) {
	    if (!memcmp(&dbuf[y][x], &prev_dbuf[y][x], sizeof(dbuf[y][x]))) {
		samedbe++;
		json_array_append_new(dbufcol, json_integer(-1));
	    } else if (!memcmp(&dbuf[y][x], &zero_dbuf, sizeof(dbuf[y][x]))) {
		zerodbe++;
		json_array_append_new(dbufcol, json_integer(0));
	    } else {
		dbufent = json_pack("{si,si,si,si,si,si,si,si,si}",
				    "effect", dbuf[y][x].effect,
				    "bg", dbuf[y][x].bg,
				    "trap", dbuf[y][x].trap,
				    "obj", dbuf[y][x].obj,
				    "obj_mn", dbuf[y][x].obj_mn,
				    "mon", dbuf[y][x].mon,
				    "monflags", dbuf[y][x].monflags,
				    "invis", dbuf[y][x].invis,
				    "visible", dbuf[y][x].visible);
		json_array_append_new(dbufcol, dbufent);
	    }
	}
	
	if (samedbe == ROWNO) { /* entire column is unchanged */
	    samecols++;
	    json_array_append_new(jdbuf, json_integer(-1));
	} else if (zerodbe == ROWNO) { /* entire column is zero */
	    zerocols++;
	    json_array_append_new(jdbuf, json_integer(0));
	} else
	    json_array_append(jdbuf, dbufcol);
	
	json_decref(dbufcol);
    }
    
    if (samecols == COLNO) {
	add_display_data("update_screen", json_integer(-1));
    } else if (zerocols == COLNO) {
	add_display_data("update_screen", json_integer(0));
    } else
	add_display_data("update_screen", jdbuf);
    
    for (i = 0; i < ROWNO; i++)
	memcpy(&prev_dbuf[i], &dbuf[i], sizeof(dbuf[i]));
}


static void srv_delay_output(void)
{
    add_display_data("delay_output", json_object());
}


static void srv_level_changed(int displaymode)
{
    add_display_data("level_changed", json_integer(displaymode));
}


static json_t *json_menuitem(struct nh_menuitem *mi)
{
    json_t *jobj = json_pack("{ss,si,si,si,si,si}", "caption", mi->caption,
		     "id", mi->id, "role", mi->role, "accel", mi->accel,
		     "group_accel", mi->group_accel, "selected", mi->selected);
    return jobj;
}


static void srv_outrip(struct nh_menuitem *items,int icount, nh_bool tombstone,
			   char *name, int gold, char *killbuf, int end_how, int year)
{
    int i;
    json_t *jobj, *jarr;
    
    jarr = json_array();
    for (i = 0; i < icount; i++)
	json_array_append_new(jarr, json_menuitem(&items[i]));
    jobj = json_pack("{so,si,si,si,si,ss,ss}", "items", jarr, "icount", icount,
		     "tombstone", tombstone, "gold", gold, "year", year,
		     "name", name, "killbuf", killbuf);
    
    add_display_data("outrip", jobj);
    
    db_set_game_done(gameid, end_how);
}


/*
 * Callbacks that require user input
 */

static int srv_display_menu(struct nh_menuitem *items, int icount,
				const char *title, int how, int *results)
{
    int i, ret;
    json_t *jobj, *jarr;
    
    jarr = json_array();
    for (i = 0; i < icount; i++)
	json_array_append_new(jarr, json_menuitem(&items[i]));
    jobj = json_pack("{so,si,si,ss}", "items", jarr, "icount", icount,
		     "how", how, "title", title);
    
    if (how == PICK_NONE) {
	add_display_data("display_menu", jobj);
	return 0;
    }
    
    jobj = client_request("display_menu", jobj);
    if (json_unpack(jobj, "{si,so!}", "return", &ret, "results", jarr) == -1)
	exit_client("Bad parameter for display_menu");
    
    for (i = 0; i < json_array_size(jarr); i++)
	results[i] = json_integer_value(json_array_get(jarr, i));
    
    json_decref(jobj);
    return ret;
}


static json_t *json_objitem(struct nh_objitem *oi)
{
    json_t *jobj;
    jobj = json_pack("{ss,si,si,si,si,si,si,si,si,si,si}", "caption", oi->caption,
		     "id", oi->id, "role", oi->role, "count", oi->count,
		     "otype", oi->otype, "oclass", oi->oclass,
		     "weight", oi->weight, "buc", oi->buc, "accel", oi->accel,
		     "group_accel", oi->group_accel, "worn", oi->worn);
    return jobj;
}


static int srv_display_objects(struct nh_objitem *items, int icount, const char *title,
			int how, struct nh_objresult *pick_list)
{
    int i, ret;
    json_t *jobj, *jarr, *jobj2;
    
    jarr = json_array();
    for (i = 0; i < icount; i++)
	json_array_append_new(jarr, json_objitem(&items[i]));
    jobj = json_pack("{so,si,si,ss}", "items", jarr, "icount", icount,
		     "how", how, "title", title);
    
    if (how == PICK_NONE) {
	add_display_data("display_objects", jobj);
	return 0;
    }
    
    jobj = client_request("display_objects", jobj);
    if (json_unpack(jobj, "{si,so!}", "return", &ret, "pick_list", jarr) == -1)
	exit_client("Bad parameter for display_objects");
    
    for (i = 0; i < json_array_size(jarr); i++) {
	jobj2 = json_array_get(jarr, i);
	if (json_unpack(jobj2, "{si,si!}", "id", &pick_list[i].id, "count",
	    &pick_list[i].count) == -1)
	    exit_client("Bad pick_list in display_objects");
    }
    
    json_decref(jobj);
    return ret;
}


static nh_bool srv_list_items(struct nh_objitem *items, int icount, nh_bool invent)
{
    int i;
    json_t *jobj, *jarr;
    
    if (invent && prev_invent && icount == prev_invent_icount &&
	!memcmp(items, prev_invent, sizeof(struct nh_objitem) * icount))
	return TRUE;
    
    if (invent) {
	prev_invent_icount = icount;
	free(prev_invent);
	prev_invent = malloc(sizeof(struct nh_objitem) * icount);
	memcpy(prev_invent, items, sizeof(struct nh_objitem) * icount);
    }
    
    jarr = json_array();
    for (i = 0; i < icount; i++)
	json_array_append_new(jarr, json_objitem(&items[i]));
    jobj = json_pack("{so,si,si}", "items", jarr, "icount", icount, "invent", invent);
    
    /* there cold be lots of list_item calls after each other if the player is
     * picking up or dropping large numbers of items. We only care about the
     * last state. */
    if (invent) {
	if (jinvent_items)
	    json_decref(jinvent_items);
	jinvent_items = jobj;
    } else {
	if (jfloor_items)
	    json_decref(jfloor_items);
	jfloor_items = jobj;
    }
    
    /* If list_items returns TRUE, the dialog "Things that are here" is not shown.
     * The return value doesn't matter at all if the list doesn't concern
     * items on the floor (invent == TRUE) or if the list is empty.
     * For items on the floor it is technically not always correct to return
     * TRUE (it should actually depend on whether the client has a sidebar or
     * not), but sending a message to the client just for this seems rather silly */
    return TRUE;
}


static char srv_query_key(const char *query, int *count)
{
    int ret, c;
    json_t *jobj;
    
    jobj = json_pack("{ss,sb}", "query", query, "allow_count", !!count);
    
    jobj = client_request("query_key", jobj);
    if (json_unpack(jobj, "{si,si,si!}", "return", &ret, "count", &c) == -1)
	exit_client("Bad parameters for query_key");
    json_decref(jobj);
    
    if (count)
	*count = c;
    
    return ret;
}


static int srv_getpos(int *x, int *y, nh_bool force, const char *goal)
{
    int ret;
    json_t *jobj;
    
    jobj = json_pack("{ss,si,si,si}", "goal", goal, "force", force, "x", *x, "y", *y);
    jobj = client_request("getpos", jobj);
    
    if (json_unpack(jobj, "{si,si,si!}", "return", &ret, "x", x, "y", y) == -1)
	exit_client("Bad parameters for getpos");
    
    json_decref(jobj);
    return ret;
}


static enum nh_direction srv_getdir(const char *query, nh_bool restricted)
{
    int ret;
    json_t *jobj;
    
    jobj = json_pack("{ss,si}", "query", query, "restricted", restricted);
    jobj = client_request("getdir", jobj);
    
    if (json_unpack(jobj, "{si!}", "return", &ret) == -1)
	exit_client("Bad parameters for getdir");
    
    json_decref(jobj);
    return ret;
}


static char srv_yn_function(const char *query, const char *set, char def)
{
    int ret;
    json_t *jobj;
    
    jobj = json_pack("{ss,ss,si}", "query", query, "set", set, "def", def);
    jobj = client_request("yn", jobj);
    
    if (json_unpack(jobj, "{si!}", "return", &ret) == -1)
	exit_client("Bad parameters for yn");
    
    json_decref(jobj);
    return ret;
}


static void srv_getline(const char *query, char *buf)
{
    json_t *jobj;
    const char *str;
    
    jobj = json_pack("{ss}", "query", query);
    jobj = client_request("getline", jobj);
    
    if (json_unpack(jobj, "{ss!}", "line", &str) == -1)
	exit_client("Bad parameters for getline");
    
    strncpy(buf, str, BUFSZ - 1);
    json_decref(jobj);
}

/*---------------------------------------------------------------------------*/

static void srv_alt_raw_print(const char *str){
    altproc = TRUE;
    srv_raw_print(str);
    altproc = FALSE;
}


static void srv_alt_pause(enum nh_pause_reason r)
{
    altproc = TRUE;
    srv_pause(r);
    altproc = FALSE;
}


static void srv_alt_display_buffer(char *buf, nh_bool trymove)
{
    altproc = TRUE;
    srv_display_buffer(buf, trymove);
    altproc = FALSE;
}


static void srv_alt_update_status(struct nh_player_info *pi)
{
    altproc = TRUE;
    srv_update_status(pi);
    altproc = FALSE;
}


static void srv_alt_print_message(int turn, const char *msg)
{
    altproc = TRUE;
    srv_print_message(turn, msg);
    altproc = FALSE;
}


static void srv_alt_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO])
{
    altproc = TRUE;
    srv_update_screen(dbuf);
    altproc = FALSE;
}


static void srv_alt_delay_output(void)
{
    altproc = TRUE;
    srv_delay_output();
    altproc = FALSE;
}


static void srv_alt_level_changed(int displaymode)
{
    altproc = TRUE;
    srv_level_changed(displaymode);
    altproc = FALSE;
}


static void srv_alt_outrip(struct nh_menuitem *items, int icount, nh_bool tombstone,
			   char *name, int gold, char *killbuf, int end_how, int year)
{
    altproc = TRUE;
    srv_alt_outrip(items, icount, tombstone, name, gold, killbuf, end_how, year);
    altproc = FALSE;
}


static nh_bool srv_alt_list_items(struct nh_objitem *items, int icount, nh_bool invent)
{
    int ret;
    altproc = TRUE;
    ret = srv_list_items(items, icount, invent);
    altproc = FALSE;
    return ret;
}

/* winprocs.c */
