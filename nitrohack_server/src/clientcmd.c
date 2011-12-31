/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include <time.h>

static void ccmd_shutdown(json_t *ignored);
static void ccmd_start_game(json_t *params);
static void ccmd_restore_game(json_t *params);
static void ccmd_exit_game(json_t *params);
static void ccmd_game_command(json_t *params);
static void ccmd_view_start(json_t *params);
static void ccmd_view_step(json_t *params);
static void ccmd_view_finish(json_t *params);
static void ccmd_list_games(json_t *params);
static void ccmd_get_drawing_info(json_t *params);
static void ccmd_get_roles(json_t *params);
static void ccmd_get_topten(json_t *params);
static void ccmd_get_commands(json_t *params);
static void ccmd_get_obj_commands(json_t *params);
static void ccmd_describe_pos(json_t *params);
static void ccmd_set_option(json_t *params);
static void ccmd_get_options(json_t *params);
static void ccmd_get_pl_prompt(json_t *params);
static void ccmd_get_root_pl_prompt(json_t *params);

const struct client_command clientcmd[] = {
    {"shutdown",	ccmd_shutdown},
    
    {"start_game",	ccmd_start_game},
    {"restore_game",	ccmd_restore_game},
    {"exit_game",	ccmd_exit_game},
    {"game_command",	ccmd_game_command},
    {"view_start",	ccmd_view_start},
    {"view_step",	ccmd_view_step},
    {"view_finish",	ccmd_view_finish},
    {"list_games",	ccmd_list_games},
    
    {"get_drawing_info",ccmd_get_drawing_info},
    {"get_roles",	ccmd_get_roles},
    {"get_topten",	ccmd_get_topten},
    {"get_commands",	ccmd_get_commands},
    {"get_obj_commands",ccmd_get_obj_commands},
    {"describe_pos",	ccmd_describe_pos},
    
    {"set_option",	ccmd_set_option},
    {"get_options",	ccmd_get_options},
    
    {"get_pl_prompt",	ccmd_get_pl_prompt},
    {"get_root_pl_prompt",ccmd_get_root_pl_prompt},
    
    {NULL, NULL}
};


/* shutdown: The client is done and the server process is no longer needed. */
static void ccmd_shutdown(json_t *ignored)
{
    /* pretend we got a signal and use the same shutdown code */
    termination_flag = 3;
}

/* start_game: Start a new game
 * parameters: name, role, race, gend, align, playmode
 */
static void ccmd_start_game(json_t *params)
{
    char filename[1024];
    json_t *j_msg;
    const char *name;
    int role, race, gend, align, mode, fd, ret;
    long t;
    
    if (json_unpack(params, "{ss,si,si,si,si,si!}", "name", &name, "role", &role,
	"race", &race, "gender", &gend, "alignment", &align, "mode", &mode) == -1)
	exit_client("Bad set of parameters for start_game");
    
    if (mode == MODE_WIZARD && !user_info.can_debug)
	mode = MODE_EXPLORE;
    
    t = (long)time(NULL);
    snprintf(filename, 1024, "%s/save/%s/%ld_%s.nhgame", settings.workdir,
	     user_info.username, t, name);
    fd = open(filename, O_EXCL | O_CREAT | O_RDWR, 0600);
    if (fd == -1)
	exit_client("Could not create the logfile");
    
    ret = nh_start_game(fd, name, role, race, gend, align, mode);
    if (ret) {
	gamefd = fd;
	gameid = db_add_new_game(user_info.uid, filename, role, race, gend, align, mode, name);
	j_msg = json_pack("{si,si}", "return", ret, "gameid", gameid);
    } else {
	close(fd);
	unlink(filename);
	j_msg = json_pack("{si,si}", "return", ret, "gameid", -1);
    }
    
    client_msg("start_game", j_msg);
    json_decref(j_msg);
}


static void ccmd_restore_game(json_t *params)
{
    int gid, fd, status;
    char filename[1024];
    json_t *jmsg;
    
    if (json_unpack(params, "{si!}", "gameid", &gid) == -1)
	exit_client("Bad set of parameters for restore_game");
    
    if (!db_get_game_filename(user_info.uid, gid, filename, 1024) ||
	(fd = open(filename, O_RDWR)) == -1) {
	jmsg = json_pack("{si}", "return", ERR_BAD_FILE);
	client_msg("restore_game", jmsg);
	json_decref(jmsg);
	return;
    }
    
    status = nh_restore_game(fd, NULL, FALSE);
    
    jmsg = json_pack("{si}", "return", status);
    client_msg("restore_game", jmsg);
    json_decref(jmsg);
    
    if (status == GAME_RESTORED) {
	gameid = gid;
	gamefd = fd;
	db_update_game_ts(gameid);
    }
}


static void ccmd_exit_game(json_t *params)
{
    int etype, status;
    json_t *jmsg;
    
    if (json_unpack(params, "{si!}", "exit_type", &etype) == -1)
	exit_client("Bad set of parameters for exit_game");
    
    status = nh_exit_game(etype);
    if (status) {
	db_update_game_ts(gameid);
	gameid = 0;
	close(gamefd);
	gamefd = -1;
    }
    
    jmsg = json_pack("{si}", "return", status);
    client_msg("exit_game", jmsg);
    json_decref(jmsg);
}


static void ccmd_game_command(json_t *params)
{
    json_t *jmsg, *jarg;
    int count, result, gid;
    const char *cmd;
    struct nh_cmd_arg arg;
    
    if (json_unpack(params, "{ss,so,si}", "command", &cmd, "arg", &jarg,
	"count", &count) == -1)
	exit_client("Bad set of parameters for game_command");
    
    if (json_unpack(jarg, "{si*}", "argtype", &arg.argtype) == -1)
	exit_client("Bad parameter arg in game_command");
    
    switch (arg.argtype) {
	case CMD_ARG_DIR:
	    if (json_unpack(params, "{si*}", "d", &arg.d) == -1)
		exit_client("Bad direction arg in game_command");
	    break;
	    
	case CMD_ARG_POS:
	    if (json_unpack(params, "{si,si*}", "x", &arg.pos.x, "y", &arg.pos.y) == -1)
		exit_client("Bad position arg in game_command");
	    break;
	    
	case CMD_ARG_OBJ:
	    if (json_unpack(params, "{si*}", "invlet", &arg.invlet) == -1)
		exit_client("Bad invlet arg in game_command");
	    break;
	    
	case CMD_ARG_NONE:
	default:
	    break;
    }
    
    if (cmd[0] == '\0')
	cmd = NULL;
    
    db_update_game_ts(gameid);
    result = nh_command(cmd, count, &arg);
    
    gid = gameid;
    if (result >= GAME_OVER) {
	close(gamefd);
	gamefd = -1;
	gameid = 0;
    }
    
    jmsg = json_pack("{si}", "return", result);
    client_msg("game_command", jmsg);
    json_decref(jmsg);
    
    /* move the finished game to its final resting place */
    if (result == GAME_OVER) {
	char full_name[1024], final_name[1024], *filename;
	if (!db_get_game_filename(user_info.uid, gid, full_name, 1024))
	    return;

	filename = strrchr(full_name, '/');
	if (!filename)
	    return;
	snprintf(final_name, 1024, "%s/completed/%s", settings.workdir, filename+1);
	rename(full_name, final_name);
    }
}


static void ccmd_view_start(json_t *params)
{
    int ret, gid, fd;
    struct nh_replay_info info;
    char filename[1024];
    json_t *jmsg;
    
    if (json_unpack(params, "{si!}", "gameid", &gid) == -1)
	exit_client("Bad set of parameters for view_start");
    
    if (!db_get_game_filename(user_info.uid, gid, filename, 1024) ||
	(fd = open(filename, O_RDWR)) == -1) {
	jmsg = json_pack("{si}", "return", FALSE);
	client_msg("view_start", jmsg);
	json_decref(jmsg);
	return;
    }
    
    ret = nh_view_replay_start(fd, &server_alt_windowprocs, &info);
    
    jmsg = json_pack("{si,s:{ss,si,si,si,si}}", "return", ret, "info", "nextcmd",
		     info.nextcmd, "actions", info.actions, "max_actions",
		     info.max_actions, "moves", info.moves, "max_moves", info.max_moves);
    
    client_msg("view_start", jmsg);
    json_decref(jmsg);
}


static void ccmd_view_step(json_t *params)
{
    enum replay_control action;
    int count, ret;
    struct nh_replay_info info;
    json_t *jmsg;
    
    if (json_unpack(params, "{si,si!}", "action", &action, "count", &count) == -1)
	exit_client("Bad set of parameters for view_step");
    
    ret = nh_view_replay_step(&info, action, count);
    
    jmsg = json_pack("{si,s:{ss,si,si,si,si}}", "return", ret, "info", "nextcmd",
		     info.nextcmd, "actions", info.actions, "max_actions",
		     info.max_actions, "moves", info.moves, "max_moves", info.max_moves);
    
    client_msg("view_start", jmsg);
    json_decref(jmsg);
}


static void ccmd_view_finish(json_t *params)
{
    void *iter;
    json_t *jmsg;
    
    iter = json_object_iter(params);
    if (iter)
	exit_client("non-empty parameter list for view_finish");
    
    nh_view_replay_finish();
    
    jmsg = json_object();
    client_msg("view_start", jmsg);
    json_decref(jmsg);
}


static void ccmd_list_games(json_t *params)
{
    int completed, limit, count, i, fd;
    struct gamefile_info *files;
    enum nh_log_status status;
    struct nh_game_info gi;
    json_t *jmsg, *jarr, *jobj;
    
    if (json_unpack(params, "{si,si!}", "completed", &completed, "limit", &limit) == -1)
	exit_client("Bad parameters for list_games");
    
    /* step 1: get a list of files from the db. */
    files = db_list_games(completed, user_info.uid, limit, &count);
    
    jarr = json_array();
    /* step 2: get extra info for each file. */
    for (i = 0; i < count; i++) {
	fd = open(files[i].filename, O_RDWR);
	if (fd == -1) { /* to error out or not, that is the question... */
	    log_msg("Game file %s could not be opened in ccmd_list_games.");
	    continue;
	}
	
	status = nh_get_savegame_status(fd, &gi);
	jobj = json_pack("{si,si,si,ss,ss,ss,ss,ss}", "gameid", files[i].gid,
			 "status", status, "playmode", gi.playmode,
			 "plname", gi.name, "plrole", gi.plrole, "plrace", gi.plrace,
			 "plgend", gi.plgend, "plalign", gi.plalign);
	if (status == LS_SAVED) {
	    json_object_set_new(jobj, "level_desc", json_string(gi.level_desc));
	    json_object_set_new(jobj, "moves", json_integer(gi.moves));
	    json_object_set_new(jobj, "depth", json_integer(gi.depth));
	    json_object_set_new(jobj, "has_amulet", json_integer(gi.has_amulet));
	} else if (status == LS_DONE) {
	    json_object_set_new(jobj, "death", json_string(gi.death));
	}
	json_array_append_new(jarr, jobj);
	
	free((void*)files[i].username);
	free((void*)files[i].filename);
	
	close(fd);
    }
    free(files);
    
    jmsg = json_pack("{so}", "games", jarr);
    
    client_msg("list_games", jmsg);
    json_decref(jmsg);
}


static json_t *json_symarray(struct nh_symdef *array, int len)
{
    int i;
    json_t *jarr, *jobj;
    
    jarr = json_array();
    for (i = 0; i < len; i++) {
	jobj = json_pack("{ss,si,si}", "sn", array[i].symname, "ch", array[i].ch,
			 "co", array[i].color);
	json_array_append_new(jarr, jobj);
    }
    return jarr;
}


static void ccmd_get_drawing_info(json_t *params)
{
    json_t *jobj;
    struct nh_drawing_info *di;
    void *iter;
    
    iter = json_object_iter(params);
    if (iter)
	exit_client("non-empty parameter list for get_drawing_info");
    
    di = nh_get_drawing_info();
    jobj = json_pack("{si,si,si,si,si,si,si,si}",
		     "num_bgelements", di->num_bgelements,
		     "num_traps", di->num_traps,
		     "num_objects", di->num_objects,
		     "num_monsters", di->num_monsters,
		     "num_warnings", di->num_warnings,
		     "num_expltypes", di->num_expltypes,
		     "num_zaptypes", di->num_zaptypes,
		     "num_effects", di->num_effects);
    json_object_set_new(jobj, "bgelements", json_symarray(di->bgelements, di->num_bgelements));
    json_object_set_new(jobj, "traps", json_symarray(di->traps, di->num_traps));
    json_object_set_new(jobj, "objects", json_symarray(di->objects, di->num_objects));
    json_object_set_new(jobj, "monsters", json_symarray(di->monsters, di->num_monsters));
    json_object_set_new(jobj, "warnings", json_symarray(di->warnings, di->num_warnings));
    json_object_set_new(jobj, "expltypes", json_symarray(di->expltypes, di->num_expltypes));
    json_object_set_new(jobj, "zaptypes", json_symarray(di->zaptypes, di->num_zaptypes));
    json_object_set_new(jobj, "effects", json_symarray(di->effects, di->num_effects));
    json_object_set_new(jobj, "explsyms", json_symarray(di->explsyms, NUMEXPCHARS));
    json_object_set_new(jobj, "zapsyms", json_symarray(di->zapsyms, NUMZAPCHARS));
    json_object_set_new(jobj, "swallowsyms", json_symarray(di->swallowsyms, NUMSWALLOWCHARS));
    json_object_set_new(jobj, "invis", json_symarray(di->invis, 1));
    
    client_msg("get_drawing_info", jobj);
    json_decref(jobj);
}


static void ccmd_get_roles(json_t *params)
{
    int i, len;
    struct nh_roles_info *ri;
    json_t *jmsg, *jarr, *j_tmp;
    void *iter;
    
    iter = json_object_iter(params);
    if (iter)
	exit_client("non-empty parameter list for get_roles");
    
    ri = nh_get_roles();
    jmsg = json_object();
    json_object_set_new(jmsg, "num_roles", json_integer(ri->num_roles));
    json_object_set_new(jmsg, "num_races", json_integer(ri->num_races));
    json_object_set_new(jmsg, "num_genders", json_integer(ri->num_genders));
    json_object_set_new(jmsg, "num_aligns", json_integer(ri->num_aligns));
    json_object_set_new(jmsg, "def_role", json_integer(ri->def_role));
    json_object_set_new(jmsg, "def_race", json_integer(ri->def_race));
    json_object_set_new(jmsg, "def_gend", json_integer(ri->def_gend));
    json_object_set_new(jmsg, "def_align", json_integer(ri->def_align));
    
    /* rolenames_m */
    jarr = json_array();
    for (i = 0; i < ri->num_roles; i++) {
	j_tmp = ri->rolenames_m[i] ? json_string(ri->rolenames_m[i]) : json_null();
	json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "rolenames_m", jarr);
    
    /* rolenames_f */
    jarr = json_array();
    for (i = 0; i < ri->num_roles; i++) {
	j_tmp = ri->rolenames_f[i] ? json_string(ri->rolenames_f[i]) : json_null();
	json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "rolenames_f", jarr);
    
    /* racenames */
    jarr = json_array();
    for (i = 0; i < ri->num_races; i++) {
	j_tmp = ri->racenames[i] ? json_string(ri->racenames[i]) : json_null();
	json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "racenames", jarr);
    
    /* gendnames */
    jarr = json_array();
    for (i = 0; i < ri->num_genders; i++) {
	j_tmp = ri->gendnames[i] ? json_string(ri->gendnames[i]) : json_null();
	json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "gendnames", jarr);
    
    /* alignnames */
    jarr = json_array();
    for (i = 0; i < ri->num_aligns; i++) {
	j_tmp = ri->alignnames[i] ? json_string(ri->alignnames[i]) : json_null();
	json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "alignnames", jarr);
    
    /* matrix */
    len = ri->num_roles * ri->num_races * ri->num_genders * ri->num_aligns;
    jarr = json_array();
    for (i = 0; i < len; i++) {
	json_array_append_new(jarr, json_integer(ri->matrix[i]));
    }
    json_object_set_new(jmsg, "matrix", jarr);
    
    client_msg("get_roles", jmsg);
    json_decref(jmsg);
}


static void ccmd_get_topten(json_t *params)
{
    struct nh_topten_entry *scores;
    char buf[BUFSZ];
    const char *player;
    int listlen, top, around, own, i;
    json_t *jmsg, *jarr, *jobj;
    
    if (json_unpack(params, "{ss,si,si,si!}",
	"player", &player, "top", &top, "around", &around, "own", &own) == -1)
	exit_client("Bad parameters for get_topten");
    
    if (player && !player[0])
	player = NULL;
    
    scores = nh_get_topten(&listlen, buf, player, top, around, own);
    
    jarr = json_array();
    for (i = 0; i < listlen; i++) {
	jobj = json_pack("{si,si,si,si,si,si,si,si,si,si,si,si,si,ss,ss,ss,ss,ss,ss,ss,si}",
	    "rank", scores[i].rank,
	    "points", scores[i].points,
	    "maxlvl", scores[i].maxlvl,
	    "hp", scores[i].hp,
	    "maxhp", scores[i].maxhp,
	    "deaths", scores[i].deaths,
	    "ver_major", scores[i].ver_major,
	    "ver_minor", scores[i].ver_minor,
	    "patchlevel", scores[i].patchlevel,
	    "deathdate", scores[i].deathdate,
	    "birthdate", scores[i].birthdate,
	    "moves", scores[i].moves,
	    "end_how", scores[i].end_how,
	    "plrole", scores[i].plrole,
	    "plrace", scores[i].plrace,
	    "plgend", scores[i].plgend,
	    "plalign", scores[i].plalign,
	    "name", scores[i].name,
	    "death", scores[i].death,
	    "entrytxt", scores[i].entrytxt,
	    "highlight", scores[i].highlight);
	json_array_append_new(jarr, jobj);
    }
    jmsg = json_pack("{so,ss}", "toplist", jarr, "msg", buf);
    client_msg("get_roles", jmsg);
    json_decref(jmsg);
}


static void ccmd_get_commands(json_t *params)
{
    void *iter;
    int cmdcount, i;
    json_t *jarr, *jobj;
    struct nh_cmd_desc *cmdlist;
    
    iter = json_object_iter(params);
    if (iter)
	exit_client("non-empty parameter list for get_commands");
    
    cmdlist = nh_get_commands(&cmdcount);
    
    jarr = json_array();
    for (i = 0; i < cmdcount; i++) {
	jobj = json_pack("{ss,ss,si,si,si}",
			 "name", cmdlist[i].name,
			 "desc", cmdlist[i].desc,
			 "def", cmdlist[i].defkey,
			 "alt", cmdlist[i].altkey,
			 "flags", cmdlist[i].flags);
	json_array_append_new(jarr, jobj);
    }
    jobj = json_pack("{so,si}", "cmdlist", jarr, "cmdcount", cmdcount);
    
    client_msg("get_commands", jobj);
    json_decref(jobj);
}


static void ccmd_get_obj_commands(json_t *params)
{
    int invlet, cmdcount, i;
    json_t *jarr, *jobj;
    struct nh_cmd_desc *cmdlist;

    if (json_unpack(params, "{si!}", "invlet", &invlet) == -1)
	exit_client("Bad parameters for get_obj_commands");
    cmdlist = nh_get_object_commands(&cmdcount, invlet);
    
    jarr = json_array();
    for (i = 0; i < cmdcount; i++) {
	jobj = json_pack("{ss,ss,si,si,si}",
			 "name", cmdlist[i].name,
			 "desc", cmdlist[i].desc,
			 "def", cmdlist[i].defkey,
			 "alt", cmdlist[i].altkey,
			 "flags", cmdlist[i].flags);
	json_array_append_new(jarr, jobj);
    }
    jobj = json_pack("{so,si}", "cmdlist", jarr, "cmdcount", cmdcount);
    
    client_msg("get_obj_commands", jobj);
    json_decref(jobj);
}


static void ccmd_describe_pos(json_t *params)
{
    struct nh_desc_buf db;
    int x, y;
    json_t *jmsg;

    if (json_unpack(params, "{si,si!}", "x", &x, "y", &y) == -1)
	exit_client("Bad parameters for describe_pos");
    
    nh_describe_pos(x, y, &db);
    jmsg = json_pack("{ss,ss,ss,ss,ss,ss,si}",
		     "bgdesc", db.bgdesc,
		     "trapdesc", db.trapdesc,
		     "objdesc", db.objdesc,
		     "mondesc", db.mondesc,
		     "invisdesc", db.invisdesc,
		     "effectdesc", db.effectdesc,
		     "objcount", db.objcount);
    client_msg("describe_pos", jmsg);
    json_decref(jmsg);
}


static const struct nh_option_desc *find_option(const char *optname,
	const struct nh_option_desc *list1, const struct nh_option_desc *list2)
{
    int i;
    const struct nh_option_desc *option = NULL;
    for (i = 0; list1[i].name && !option; i++)
	if (!strcmp(optname, list1[i].name))
	    option = &list1[i];
    for (i = 0; list2[i].name && !option; i++)
	if (!strcmp(optname, list2[i].name))
	    option = &list2[i];
    return option;
}


static json_t *json_list(const struct nh_listitem *list, int len)
{
    int i;
    json_t *jarr = json_array();
    for (i = 0; i < len; i++)
	json_array_append_new(jarr, json_pack("{si,ss}", "id", list[i].id,
					      "txt", list[i].caption));
    return jarr;
}


static json_t *json_option(const struct nh_option_desc *option)
{
    int i;
    json_t *jopt, *joptval, *joptdesc, *jobj;
    struct nh_autopickup_rule *r;
    
    switch (option->type) {
	case OPTTYPE_BOOL:
	    joptval = json_integer(option->value.b);
	    joptdesc = json_object();
	    break;
	    
	case OPTTYPE_INT:
	    joptval = json_integer(option->value.i);
	    joptdesc = json_pack("{si,si}", "max", option->i.max, "min", option->i.min);
	    break;
	    
	case OPTTYPE_ENUM:
	    joptval = json_integer(option->value.e);
	    joptdesc = json_list(option->e.choices, option->e.numchoices);
	    break;
	    
	case OPTTYPE_STRING:
	    joptval = json_string(option->value.s);
	    joptdesc = json_integer(option->s.maxlen);
	    break;
	    
	case OPTTYPE_AUTOPICKUP_RULES:
	    joptdesc = json_list(option->a.classes, option->a.numclasses);
	    joptval = json_array();
	    for (i = 0; option->value.ar && i < option->value.ar->num_rules; i++) {
		r = &option->value.ar->rules[i];
		jobj = json_pack("{ss,si,si,si}", "pattern", r->pattern, "oclass",
				 r->oclass, "buc", r->buc, "action", r->action);
		json_array_append_new(joptval, jobj);
	    }
	    break;
    }
    
    jopt = json_pack("{ss,ss,si,so,so}", "name", option->name, "helptxt",
		     option->helptxt, "type", option->type, "value", joptval,
		     "desc", joptdesc);
    return jopt;
}


static void ccmd_set_option(json_t *params)
{
    const char *optname, *optstr;
    json_t *jmsg, *joval, *jopt;
    int isstr, i, ret;
    const struct nh_option_desc *gameopt, *birthopt, *option;
    union nh_optvalue value;
    struct nh_autopickup_rules ar = {NULL, 0};
    struct nh_autopickup_rule *r;
    
    if (json_unpack(params, "{ss,so,si!}", "name", &optname, "value", &joval,
	"isstr", &isstr) == -1)
	exit_client("Bad parameters for set_option");
    
    /* find the option_desc for the options that should be set; the option type
     * is required in order to decode the option value. */
    gameopt = nh_get_options(GAME_OPTIONS);
    birthopt = nh_get_options(gameid ? ACTIVE_BIRTH_OPTIONS : CURRENT_BIRTH_OPTIONS);
    option = find_option(optname, gameopt, birthopt);
    if (!option) {
	jmsg = json_pack("{si,so,so}", "return", 0, "option", json_null(),
			 "optstr", json_null());
	client_msg("set_option", jmsg);
	json_decref(jmsg);
	return;
    }
    
    /* decode the option value depending on the option type */
    if (isstr || option->type == OPTTYPE_STRING) {
	if (!json_is_string(joval))
	    exit_client("could not decode option string");
	value.s = (char*)json_string_value(joval);
    
    } else if (option->type == OPTTYPE_INT || option->type == OPTTYPE_INT ||
	       option->type == OPTTYPE_BOOL) {
	if (!json_is_integer(joval))
	    exit_client("could not decode option value");
	value.i = json_integer_value(joval);
    
    } else if (option->type == OPTTYPE_AUTOPICKUP_RULES) {
	if (!json_is_array(joval))
	    exit_client("could not decode option");
	
	value.ar = &ar;
	ar.num_rules = json_array_size(joval);
	ar.rules = malloc(sizeof(struct nh_autopickup_rule) * ar.num_rules);
	for (i = 0; i < ar.num_rules; i++) {
	    r = &ar.rules[i];
	    if (json_unpack(json_array_get(joval, i), "{ss,si,si,si}", "pattern",
			    &r->pattern, "oclass", &r->oclass, "buc", &r->buc,
			    "action", &r->action) == -1)
		exit_client("Error unpacking autopickup rule");
	}
    }
    
    ret = nh_set_option(optname, value, isstr);
    
    if (option->type == OPTTYPE_AUTOPICKUP_RULES)
	free(ar.rules);
    
    /* re-get the option and it's string representation */
    gameopt = nh_get_options(GAME_OPTIONS);
    birthopt = nh_get_options(gameid ? ACTIVE_BIRTH_OPTIONS : CURRENT_BIRTH_OPTIONS);
    option = find_option(optname, gameopt, birthopt);
    
    jopt = json_option(option);
    optstr = nh_get_option_string(option);
    
    /* return the altered option struct and the string representation to the
     * client. The intent is to save some network round trips and make a
     * separate get_option_string message unneccessary */
    jmsg = json_pack("{si,so,ss}", "return", ret, "option", jopt, "optstr", optstr);
    client_msg("set_option", jmsg);
    
    db_set_option(user_info.uid, optname, option->type, optstr);
    json_decref(jmsg);
}


static void ccmd_get_options(json_t *params)
{
    int list, i;
    const struct nh_option_desc *options;
    json_t *jmsg, *jarr, *jarr2;
    
    if (json_unpack(params, "{si!}", "list", &list) == -1)
	exit_client("Bad parameters for get_options");
    
    jarr = json_array();
    jarr2 = json_array();
    options = nh_get_options(list);
    for (i = 0; options[i].name; i++) {
	json_array_append_new(jarr, json_option(&options[i]));
	json_array_append_new(jarr2, json_string(nh_get_option_string(&options[i])));
    }
    jmsg = json_pack("{so,so}", "options", jarr, "option_strings", jarr2);
    client_msg("get_options", jmsg);
    json_decref(jmsg);
}


static void ccmd_get_pl_prompt(json_t *params)
{
    int rolenum, racenum, gendnum, alignnum;
    char buf[1024], *bp;
    json_t *jmsg;
    
    if (json_unpack(params, "{si,si,si,si!}", "role", &rolenum, "race",
	&racenum, "gend", &gendnum, "align", &alignnum) == -1)
	exit_client("Bad parameters for get_pl_prompt");
    
    bp = nh_build_plselection_prompt(buf, 1024, rolenum, racenum, gendnum, alignnum);
    jmsg = json_pack("{ss}", "prompt", bp);
    client_msg("get_pl_prompt", jmsg);
    json_decref(jmsg);
}


static void ccmd_get_root_pl_prompt(json_t *params)
{
    int rolenum, racenum, gendnum, alignnum;
    char buf[1024];
    const char *bp;
    json_t *jmsg;
    
    if (json_unpack(params, "{si,si,si,si!}", "role", &rolenum, "race",
	&racenum, "gend", &gendnum, "align", &alignnum) == -1)
	exit_client("Bad parameters for get_root_pl_prompt");
    
    bp = nh_root_plselection_prompt(buf, 1024, rolenum, racenum, gendnum, alignnum);
    jmsg = json_pack("{ss}", "prompt", bp);
    client_msg("get_root_pl_prompt", jmsg);
    json_decref(jmsg);
}

/* clientcmd.c */
