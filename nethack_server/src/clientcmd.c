/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include "common_options.h"
#include <time.h>

static void ccmd_shutdown(json_t * ignored);
static void ccmd_create_game(json_t * params);
static void ccmd_play_game(json_t * params);
static void ccmd_exit_game(json_t * params);
static void ccmd_list_games(json_t * params);
static void ccmd_get_drawing_info(json_t * params);
static void ccmd_get_roles(json_t * params);
static void ccmd_get_topten(json_t * params);
static void ccmd_get_commands(json_t * params);
static void ccmd_get_obj_commands(json_t * params);
static void ccmd_describe_pos(json_t * params);
static void ccmd_set_option(json_t * params);
static void ccmd_get_options(json_t * params);
static void ccmd_get_pl_prompt(json_t * params);
static void ccmd_get_root_pl_prompt(json_t * params);
static void ccmd_set_email(json_t * params);
static void ccmd_set_password(json_t * params);

const struct client_command clientcmd[] = {
    {"shutdown", ccmd_shutdown, 0},

    {"create_game", ccmd_create_game, 0},
    {"play_game", ccmd_play_game, 0},
    {"exit_game", ccmd_exit_game, 1},
    {"list_games", ccmd_list_games, 0},

    {"get_drawing_info", ccmd_get_drawing_info, 1},
    {"get_roles", ccmd_get_roles, 1},
    {"get_topten", ccmd_get_topten, 1},
    {"get_commands", ccmd_get_commands, 1},
    {"get_obj_commands", ccmd_get_obj_commands, 1},
    {"describe_pos", ccmd_describe_pos, 1},

    {"set_option", ccmd_set_option, 1},
    {"get_options", ccmd_get_options, 1},

    {"get_pl_prompt", ccmd_get_pl_prompt, 0},
    {"get_root_pl_prompt", ccmd_get_root_pl_prompt, 0},

    {"set_email", ccmd_set_email, 1},
    {"set_password", ccmd_set_password, 1},

    {NULL, NULL}
};


/* shutdown: The client is done and the server process is no longer needed. */
static void
ccmd_shutdown(json_t * ignored)
{
    json_t *jmsg;

    /* pretend we got a signal and use the same shutdown code */
    termination_flag = 3;
    jmsg = json_pack("{si}", "return", 1);
    client_msg("shutdown", jmsg);
}


/* duplicated in clientapi.c */
static struct nh_listitem *
read_json_list(json_t * jarr)
{
    struct nh_listitem *list;
    json_t *jobj;
    int size, i;
    const char *txt;

    size = json_array_size(jarr);
    list = malloc(size * sizeof (struct nh_listitem));

    for (i = 0; i < size; i++) {
        jobj = json_array_get(jarr, i);
        if (json_unpack(jobj, "{si,ss!}", "id", &list[i].id, "txt", &txt) == -1)
            continue;
        list[i].caption = strdup(txt);
    }

    return list;
}


/* duplicated in clientapi.c */
static void
read_json_option(json_t * jobj, struct nh_option_desc *opt)
{
    json_t *joptval, *joptdesc, *jelem;
    const char *name, *helptxt, *strval;
    int size, i;
    struct nh_autopickup_rule *r;

    name = NULL;

    memset(opt, 0, sizeof (struct nh_option_desc));
    if (json_unpack
        (jobj, "{ss,ss,si,so,so,si!}", "name", &name, "helptxt", &helptxt,
         "type", &opt->type, "value", &joptval, "desc", &joptdesc,
         "birth", &opt->birth_option) == -1) {
        memset(opt, 0, sizeof (struct nh_option_desc));
        log_msg("broken option specification for option %s",
                name ? name : "unknown");
        return;
    }
    opt->name = strdup(name);
    opt->helptxt = strdup(helptxt);

    switch (opt->type) {
    case OPTTYPE_BOOL:
        opt->value.b = json_integer_value(joptval);
        break;

    case OPTTYPE_INT:
        opt->value.i = json_integer_value(joptval);
        json_unpack(joptdesc, "{si,si!}", "max", &opt->i.max, "min",
                    &opt->i.min);
        break;

    case OPTTYPE_ENUM:
        opt->value.e = json_integer_value(joptval);

        size = json_array_size(joptdesc);
        opt->e.numchoices = size;
        opt->e.choices = read_json_list(joptdesc);
        break;

    case OPTTYPE_STRING:
        opt->value.s = strdup(json_string_value(joptval));
        opt->s.maxlen = json_integer_value(joptdesc);
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        size = json_array_size(joptdesc);
        opt->a.numclasses = size;
        opt->a.classes = read_json_list(joptdesc);

        size = json_array_size(joptval);
        if (!size)
            break;
        opt->value.ar = malloc(sizeof (struct nh_autopickup_rules));
        opt->value.ar->num_rules = size;
        opt->value.ar->rules =
            malloc(size * sizeof (struct nh_autopickup_rule));
        for (i = 0; i < size; i++) {
            r = &opt->value.ar->rules[i];
            jelem = json_array_get(joptval, i);
            json_unpack(jelem, "{ss,si,si,si!}", "pattern", &strval, "oclass",
                        &r->oclass, "buc", &r->buc, "action", &r->action);
            memset(r->pattern, 0, sizeof (r->pattern));
            strncpy(r->pattern, strval, sizeof (r->pattern) - 1);
            r->pattern[sizeof (r->pattern) - 1] = 0;
        }

        break;
    }
}


/*
 * create_game: Start a new game
 * parameters: name, role, race, gend, align, playmode
 */
static void
ccmd_create_game(json_t * params)
{
    char filename[1024], basename[1024], path[1024];
    json_t *j_msg, *jarr, *jobj;
    int fd, ret, count, i, debug = 0;
    long t;

    if (json_unpack (params, "{so!}", "options", &jarr) == -1 ||
        !json_is_array(jarr))
        exit_client("Bad set of parameters for create_game", 0);

    /* reset cached display data from a previous game */
    reset_cached_displaydata();

    struct nh_option_desc *opts;
    count = json_array_size(jarr);
    opts = calloc(sizeof (struct nh_option_desc), (count + 1));
    for (i = 0; i < count; i++) {
        jobj = json_array_get(jarr, i);
        read_json_option(jobj, &opts[i]);
    }
    opts[i].name = 0;

    struct nh_option_desc *modeopt = nhlib_find_option(opts, "mode");
    struct nh_option_desc *nameopt = nhlib_find_option(opts, "name");
    if (modeopt && modeopt->value.e == MODE_WIZARD) {
        if (user_info.can_debug)
            debug = 1;
        else
            modeopt->value.e = MODE_EXPLORE;
    } else if (!nameopt && (!modeopt || modeopt->value.e != MODE_EXPLORE))
        exit_client("No character name provided", 0);

    const char *name = nameopt ? nameopt->value.s :
        debug ? "wizard" : "explorer";

    if (strspn(name, "abcdefghijklmnopqrstuvwxyz"
               "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(name))
        exit_client("Character name contains unwanted characters", 0);

    t = (long)time(NULL);
    snprintf(path, 1024, "%s/save/%s/", settings.workdir, user_info.username);
    snprintf(basename, 1024, "%ld_%s.nhgame", t, name);
    basename[1000] = '\0';
    snprintf(filename, 1024, "%s%s", path, basename);

    mkdir(path, 0755);  /* should already exist unless something went wrong
                           while upgrading */
    fd = open(filename, O_EXCL | O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        log_msg("%s tried to create a new game (%d) as %s, but the file could "
                "not be opened", user_info.username, gameid, name);
        exit_client("Could not create the logfile", SIGABRT);
    }

    ret = nh_create_game(fd, opts);
    close(fd);

    if (ret == NHCREATE_OK) {

        struct nh_option_desc
            *roleopt = nhlib_find_option(opts, "role"),
            *raceopt = nhlib_find_option(opts, "race"),
            *alignopt = nhlib_find_option(opts, "align"),
            *gendopt = nhlib_find_option(opts, "gender"),
            *modeopt = nhlib_find_option(opts, "mode");
        struct nh_roles_info *ri = nh_get_roles();

        int role = roleopt->value.e;
        int race = raceopt->value.e;
        int gend = gendopt->value.e;
        int align = alignopt->value.e;
        int mode = modeopt->value.e;

        const char *rolename = (gend &&
                                ri->rolenames_f[role]) ?
            ri->rolenames_f[role] : ri->rolenames_m[role];
        gameid =
            db_add_new_game(user_info.uid, basename, rolename,
                            ri->racenames[race], ri->gendnames[gend],
                            ri->alignnames[align], mode, name,
                            player_info.level_desc);
        log_msg("%s has created a new game (%d) as %s", user_info.username,
                gameid, name);
        j_msg = json_pack("{si}", "return", gameid);
    } else {
        unlink(filename);
        log_msg("%s tried to create a new game (%d) as %s, but the creation %s",
                user_info.username, gameid, name, ret == NHCREATE_FAIL ?
                "failed" : ret == NHCREATE_INVALID ? "had incorrect options" :
                "did not happen for an unknown reason");
        j_msg = json_pack("{si}", "return", ret);
    }

    nhlib_free_optlist(opts);

    client_msg("create_game", j_msg);
}


static const char *const play_status_names[] = {
    [GAME_DETACHED] = "game detached",
    [GAME_OVER] = "game ended",
    [GAME_ALREADY_OVER] = "watched/replayed game ended",
    [REPLAY_FINISHED] = "reached end of replay",
    [RESTART_PLAY] = "connection was disrupted and needs reconnecting",
    [CLIENT_RESTART] = "client is reloading the game",
    [ERR_BAD_ARGS] = "game ID did not exist",
    [ERR_BAD_FILE] = "file on disk was unreadable",
    [ERR_IN_PROGRESS] = "locking issues",
    [ERR_RESTORE_FAILED] = "manual recovery required",
    [ERR_RECOVER_REFUSED] = "automatic recovery refused",
};
static void
ccmd_play_game(json_t * params)
{
    int gid, fd, status, followmode;
    char filename[1024];
    enum getgame_result ggr;
    struct nh_game_info unused;
    enum nh_log_status logstatus;

    if (json_unpack(params, "{si,si*}", "gameid", &gid,
                    "followmode", &followmode) == -1 ||
        (followmode != FM_PLAY && followmode != FM_REPLAY &&
         followmode != FM_WATCH))
        exit_client("Bad set of parameters for play_game", 0);

    /* TODO: For followmode == FM_PLAY, there's no check that that's actually
       a legal thing to do! We don't want players playing each other's games. */

    ggr = db_get_game_filename(gid, filename, 1024); 
    if (ggr == GGR_NOT_FOUND) {
        log_msg("User '%s' tried to load game %d not in the database",
                user_info.username, gid);
        client_msg("play_game", json_pack("{si}", "return", ERR_BAD_FILE));
        return;
    }

    fd = open(filename, O_RDWR);
    if (fd == -1) {
        log_msg("User '%s' tried to load unreadable file '%s'");
        client_msg("play_game", json_pack("{si}", "return", ERR_BAD_FILE));
        return;
    }

    /* Special case: if the game is incomplete according to db_get_game_filename
       but complete according to the game engine, go into recoverquit mode. */
    logstatus = nh_get_savegame_status(fd, &unused);
    if (logstatus == LS_DONE && ggr == GGR_INCOMPLETE)
        followmode = FM_RECOVERQUIT;

    /* reset cached display data from a previous game */
    reset_cached_displaydata();

    const char *verb = followmode == FM_PLAY ? "play" :
        followmode == FM_REPLAY ? "replay" :
        followmode == FM_WATCH ? "watch" :
        followmode == FM_RECOVERQUIT ? "recoverquit" : "?";

    log_msg("User '%s' started to %s game %d, file %s",
            user_info.username, verb, gid, filename);
    gameid = gid;
    gamefd = fd;
    status = nh_play_game(fd, followmode);
    gameid = -1;
    gamefd = -1;
    log_msg("User '%s' stopped %sing game %d, file %s: %s",
            user_info.username, verb, gid, filename, play_status_names[status]);

    if (status == ERR_RESTORE_FAILED) {
        log_msg("Failed to restore saved game %d, file %s", gid, filename);
        if (srv_yn_function
            ("Restoring the game failed. Would you like to remove it from the "
             "list?",
             "yn", 'n') == 'y') {
            db_delete_game(user_info.uid, gid);
            log_msg("%s has chosen to remove game %d from the database",
                    user_info.username, gid);
        }
    }

    client_msg("play_game", json_pack("{si}", "return", status));

    db_update_game(gid, player_info.moves, player_info.z,
                   player_info.level_desc);

    /* move the finished game to its final resting place */
    if (status == GAME_OVER) {
        char filename[1024], final_name[1024];
        int len;
        char buf[BUFSZ];

        /* get the topten entry for the current game */
        struct nh_topten_entry *tte = nh_get_topten(&len, buf, NULL, 0, 0, 0);

        if (!db_get_game_filename(gid, filename, 1024))
            return;

        db_add_topten_entry(gid, tte->points, tte->hp, tte->maxhp, tte->deaths,
                            tte->end_how, tte->death, tte->entrytxt);

        if (!db_get_game_filename(gid, final_name, 1024))
            return;

        rename(filename, final_name);
    }
}


static void
ccmd_exit_game(json_t * params)
{
    int etype, status;

    if (json_unpack(params, "{si*}", "exit_type", &etype) == -1)
        exit_client("Bad set of parameters for exit_game", 0);

    status = nh_exit_game(etype);
    if (status) {
        db_update_game(gameid, player_info.moves, player_info.z,
                       player_info.level_desc);
        log_msg("%s has closed game %d", user_info.username, gameid);
        gameid = -1;
        close(gamefd);
        gamefd = -1;
    }

    client_msg("exit_game", json_pack("{sb}", "return", status));
}


static void
ccmd_list_games(json_t * params)
{
    int completed, limit, show_all, count, i, fd;
    struct gamefile_info *files;
    enum nh_log_status status;
    struct nh_game_info gi;
    json_t *jarr, *jobj;

    if (json_unpack
        (params, "{sb,si*}", "completed", &completed, "limit", &limit) == -1)
        exit_client("Bad parameters for list_games", 0);
    if (json_unpack(params, "{sb*}", "show_all", &show_all) == -1)
        show_all = 0;

    if (limit > 100)
        limit = 100; /* try to prevent DOS to some extent */

    /* step 1: get a list of files from the db. */
    files =
        db_list_games(completed, show_all ? -user_info.uid : user_info.uid,
                      limit, &count);

    jarr = json_array();
    /* step 2: get extra info for each file. */
    for (i = 0; i < count; i++) {
        fd = open(files[i].filename, O_RDWR);
        if (fd == -1) {
            log_msg("Game file %s could not be opened in ccmd_list_games.",
                    files[i].filename);
            continue;
        }

        status = nh_get_savegame_status(fd, &gi);
        jobj = json_pack(
            "{si,si,si,ss,ss,ss,ss,ss,ss}", "gameid", files[i].gid, "status",
            status, "playmode", gi.playmode, "plname", gi.name, "plrole",
            gi.plrole, "plrace", gi.plrace, "plgend", gi.plgend, "plalign",
            gi.plalign, "game_state", gi.game_state);
        json_array_append_new(jarr, jobj);
        free((void *)files[i].filename);

        close(fd);
    }
    free(files);

    client_msg("list_games", json_pack("{so}", "games", jarr));
}


static json_t *
json_symarray(const struct nh_symdef *array, int len)
{
    int i;
    json_t *jarr, *jobj;

    jarr = json_array();
    for (i = 0; i < len; i++) {
        jobj =
            json_pack("[s,i,i]", array[i].symname, array[i].ch, array[i].color);
        json_array_append_new(jarr, jobj);
    }
    return jarr;
}


static void
ccmd_get_drawing_info(json_t * params)
{
    json_t *jobj;
    struct nh_drawing_info *di;
    void *iter;

    iter = json_object_iter(params);
    if (iter)
        exit_client("non-empty parameter list for get_drawing_info", 0);

    di = nh_get_drawing_info();
    jobj =
        json_pack("{si,si,si,si,si,si,si,si,si}", "num_bgelements",
                  di->num_bgelements, "num_traps", di->num_traps, "num_objects",
                  di->num_objects, "num_monsters", di->num_monsters,
                  "num_warnings", di->num_warnings, "num_expltypes",
                  di->num_expltypes, "num_zaptypes", di->num_zaptypes,
                  "num_effects", di->num_effects, "feature_offset",
                  di->bg_feature_offset);
    json_object_set_new(jobj, "bgelements",
                        json_symarray(di->bgelements, di->num_bgelements));
    json_object_set_new(jobj, "traps", json_symarray(di->traps, di->num_traps));
    json_object_set_new(jobj, "objects",
                        json_symarray(di->objects, di->num_objects));
    json_object_set_new(jobj, "monsters",
                        json_symarray(di->monsters, di->num_monsters));
    json_object_set_new(jobj, "warnings",
                        json_symarray(di->warnings, di->num_warnings));
    json_object_set_new(jobj, "expltypes",
                        json_symarray(di->expltypes, di->num_expltypes));
    json_object_set_new(jobj, "zaptypes",
                        json_symarray(di->zaptypes, di->num_zaptypes));
    json_object_set_new(jobj, "effects",
                        json_symarray(di->effects, di->num_effects));
    json_object_set_new(jobj, "explsyms",
                        json_symarray(di->explsyms, NUMEXPCHARS));
    json_object_set_new(jobj, "zapsyms",
                        json_symarray(di->zapsyms, NUMZAPCHARS));
    json_object_set_new(jobj, "swallowsyms",
                        json_symarray(di->swallowsyms, NUMSWALLOWCHARS));
    json_object_set_new(jobj, "invis", json_symarray(di->invis, 1));

    client_msg("get_drawing_info", jobj);
}


static void
ccmd_get_roles(json_t * params)
{
    int i, len;
    struct nh_roles_info *ri;
    json_t *jmsg, *jarr, *j_tmp;
    void *iter;

    iter = json_object_iter(params);
    if (iter)
        exit_client("non-empty parameter list for get_roles", 0);

    ri = nh_get_roles();
    jmsg =
        json_pack("{si,si,si,si}", "num_roles", ri->num_roles,
                  "num_races", ri->num_races, "num_genders", ri->num_genders,
                  "num_aligns", ri->num_aligns);

    /* rolenames_m */
    jarr = json_array();
    for (i = 0; i < ri->num_roles; i++) {
        j_tmp =
            ri->rolenames_m[i] ? json_string(ri->rolenames_m[i]) : json_null();
        json_array_append_new(jarr, j_tmp);
    }
    json_object_set_new(jmsg, "rolenames_m", jarr);

    /* rolenames_f */
    jarr = json_array();
    for (i = 0; i < ri->num_roles; i++) {
        j_tmp =
            ri->rolenames_f[i] ? json_string(ri->rolenames_f[i]) : json_null();
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
        j_tmp =
            ri->alignnames[i] ? json_string(ri->alignnames[i]) : json_null();
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
}


static void
ccmd_get_topten(json_t * params)
{
    struct nh_topten_entry *scores;
    char buf[BUFSZ];
    const char *player;
    int listlen, top, around, own, i;
    json_t *jmsg, *jarr, *jobj;

    if (json_unpack
        (params, "{ss,si,si,si*}", "player", &player, "top", &top, "around",
         &around, "own", &own) == -1)
        exit_client("Bad parameters for get_topten", 0);

    if (player && !player[0])
        player = NULL;

    scores = nh_get_topten(&listlen, buf, player, top, around, own);

    jarr = json_array();
    for (i = 0; i < listlen; i++) {
        jobj =
            json_pack
            ("{si,si,si,si,si,si,si,si,si,si,si,si,si,ss,ss,ss,ss,ss,ss,ss,si}",
             "rank", scores[i].rank, "points", scores[i].points, "maxlvl",
             scores[i].maxlvl, "hp", scores[i].hp, "maxhp", scores[i].maxhp,
             "deaths", scores[i].deaths, "ver_major", scores[i].ver_major,
             "ver_minor", scores[i].ver_minor, "patchlevel",
             scores[i].patchlevel, "deathdate", scores[i].deathdate,
             "birthdate", scores[i].birthdate, "moves", scores[i].moves,
             "end_how", scores[i].end_how, "plrole", scores[i].plrole, "plrace",
             scores[i].plrace, "plgend", scores[i].plgend, "plalign",
             scores[i].plalign, "name", scores[i].name, "death",
             scores[i].death, "entrytxt", scores[i].entrytxt, "highlight",
             scores[i].highlight);
        json_array_append_new(jarr, jobj);
    }
    jmsg = json_pack("{so,ss}", "toplist", jarr, "msg", buf);
    client_msg("get_topten", jmsg);
}


static void
ccmd_get_commands(json_t * params)
{
    void *iter;
    int cmdcount, i;
    json_t *jarr, *jobj;
    struct nh_cmd_desc *cmdlist;

    iter = json_object_iter(params);
    if (iter)
        exit_client("non-empty parameter list for get_commands", 0);

    cmdlist = nh_get_commands(&cmdcount);

    jarr = json_array();
    for (i = 0; i < cmdcount; i++) {
        jobj =
            json_pack("{ss,ss,si,si,si}", "name", cmdlist[i].name, "desc",
                      cmdlist[i].desc, "def", cmdlist[i].defkey, "alt",
                      cmdlist[i].altkey, "flags", cmdlist[i].flags);
        json_array_append_new(jarr, jobj);
    }
    client_msg("get_commands", json_pack("{so}", "cmdlist", jarr));
}


static void
ccmd_get_obj_commands(json_t * params)
{
    int invlet, cmdcount, i;
    json_t *jarr, *jobj;
    struct nh_cmd_desc *cmdlist;

    if (json_unpack(params, "{si*}", "invlet", &invlet) == -1)
        exit_client("Bad parameters for get_obj_commands", 0);
    cmdlist = nh_get_object_commands(&cmdcount, invlet);

    jarr = json_array();
    for (i = 0; i < cmdcount; i++) {
        jobj =
            json_pack("{ss,ss,si,si,si}", "name", cmdlist[i].name, "desc",
                      cmdlist[i].desc, "def", cmdlist[i].defkey, "alt",
                      cmdlist[i].altkey, "flags", cmdlist[i].flags);
        json_array_append_new(jarr, jobj);
    }
    client_msg("get_obj_commands", json_pack("{so}", "cmdlist", jarr));
}


static void
ccmd_describe_pos(json_t * params)
{
    struct nh_desc_buf db;
    int x, y, is_in;
    json_t *jmsg;

    if (json_unpack(params, "{si,si,si*}", "x", &x, "y", &y, "is_in", &is_in) ==
        -1)
        exit_client("Bad parameters for describe_pos", 0);

    nh_describe_pos(x, y, &db, is_in ? &is_in : NULL);
    jmsg =
        json_pack("{ss,ss,ss,ss,ss,ss,si,sb,sb}", "bgdesc", db.bgdesc,
                  "trapdesc", db.trapdesc, "objdesc", db.objdesc, "mondesc",
                  db.mondesc, "invisdesc", db.invisdesc, "effectdesc",
                  db.effectdesc, "objcount", db.objcount, "in", is_in,
                  "feature_described", db.feature_described);
    client_msg("describe_pos", jmsg);
}


/* duplicated in clientapi.c */
static json_t *
json_list(const struct nh_listitem *list, int len)
{
    int i;
    json_t *jarr = json_array();

    for (i = 0; i < len; i++)
        json_array_append_new(jarr,
                              json_pack("{si,ss}", "id", list[i].id, "txt",
                                        list[i].caption));
    return jarr;
}


/* duplicated in clientapi.c */
static json_t *
json_option(const struct nh_option_desc *option)
{
    int i;
    json_t *jopt, *joptval, *joptdesc, *jobj;
    json_error_t jerr;
    struct nh_autopickup_rule *r;

    switch (option->type) {
    case OPTTYPE_BOOL:
        joptval = json_integer(option->value.b);
        joptdesc = json_object();
        break;

    case OPTTYPE_INT:
        joptval = json_integer(option->value.i);
        joptdesc =
            json_pack("{si,si}", "max", option->i.max, "min", option->i.min);
        break;

    case OPTTYPE_ENUM:
        joptval = json_integer(option->value.e);
        joptdesc = json_list(option->e.choices, option->e.numchoices);
        break;

    case OPTTYPE_STRING:
        joptval = json_string(option->value.s);
        if (!joptval)
            joptval = json_string("");
        joptdesc = json_integer(option->s.maxlen);
        break;
        
    case OPTTYPE_AUTOPICKUP_RULES:
        joptdesc = json_list(option->a.classes, option->a.numclasses);
        joptval = json_array();
        for (i = 0; option->value.ar && i < option->value.ar->num_rules; i++) {
            r = &option->value.ar->rules[i];
            jobj =
                json_pack("{ss,si,si,si}", "pattern", r->pattern, "oclass",
                          r->oclass, "buc", r->buc, "action", r->action);
            json_array_append_new(joptval, jobj);
        }
        break;

    default:
        joptdesc = json_string("<error: no description for option>");
        joptval = json_string("<error>");
        break;
    }

    jopt =
        json_pack_ex(&jerr, 0, "{ss,ss,si,so,so,si}", "name", option->name,
                     "helptxt", option->helptxt, "type", option->type,
                     "value", joptval, "desc", joptdesc,
                     "birth", option->birth_option);

    if (!jopt)
        log_msg("Could not encode option %s: %s", option->name,
                *jerr.text ? jerr.text : !joptval ? "missing joptval" :
                !joptdesc ? "missing joptdesc" : "unknown error");

    return jopt;
}


static void
ccmd_set_option(json_t * params)
{
    const char *optname, *pattern;
    json_t *jmsg, *joval, *jopt;
    int  i, ret;
    struct nh_option_desc *opts;
    const struct nh_option_desc *option;
    union nh_optvalue value;
    struct nh_autopickup_rules ar = { NULL, 0 };
    struct nh_autopickup_rule *r;

    if (json_unpack
        (params, "{ss,so*}", "name", &optname, "value", &joval) == -1)
        exit_client("Bad parameters for set_option", 0);

    /* find the option_desc for the options that should be set; the option type
       is required in order to decode the option value. */
    opts = nh_get_options();
    option = nhlib_const_find_option(opts, optname);
    if (!option) {
        jmsg = json_pack("{si,so}", "return", FALSE, "option", json_object());
        client_msg("set_option", jmsg);
        nhlib_free_optlist(opts);
        return;
    }

    /* decode the option value depending on the option type */
    if (option->type == OPTTYPE_STRING) {
        if (!json_is_string(joval))
            exit_client("could not decode option string", 0);
        value.s = (char *)json_string_value(joval);

    } else if (option->type == OPTTYPE_INT || option->type == OPTTYPE_ENUM ||
               option->type == OPTTYPE_BOOL) {
        if (!json_is_integer(joval))
            exit_client("could not decode option value", 0);
        value.i = json_integer_value(joval);

    } else if (option->type == OPTTYPE_AUTOPICKUP_RULES) {
        if (!json_is_array(joval))
            exit_client("could not decode option", 0);

        ar.num_rules = json_array_size(joval);
        ar.rules = malloc(sizeof (struct nh_autopickup_rule) * ar.num_rules);
        if (ar.num_rules) {
            value.ar = &ar;
            for (i = 0; i < ar.num_rules; i++) {
                r = &ar.rules[i];
                if (json_unpack
                    (json_array_get(joval, i), "{ss,si,si,si}", "pattern",
                     &pattern, "oclass", &r->oclass, "buc", &r->buc, "action",
                     &r->action) == -1)
                    exit_client("Error unpacking autopickup rule", 0);
                memset(r->pattern, 0, sizeof (r->pattern));
                strncpy(r->pattern, pattern, sizeof (r->pattern) - 1);
                r->pattern[sizeof (r->pattern) - 1] = 0;
            }
        } else
            value.ar = NULL;
    }

    ret = nh_set_option(optname, value);

    if (option->type == OPTTYPE_AUTOPICKUP_RULES)
        free(ar.rules);

    nhlib_free_optlist(opts);
    opts = nh_get_options();
    option = nhlib_const_find_option(opts, optname);

    jopt = json_option(option);

    /* return the altered option struct and the string representation to the
       client. The intent is to save some network round trips and make a
       separate get_option_string message unneccessary */
    jmsg = json_pack("{si,so}", "return", ret, "option", jopt);
    client_msg("set_option", jmsg);

    nhlib_free_optlist(opts);
}


static void
ccmd_get_options(json_t * params)
{
    int i;
    struct nh_option_desc *options;
    json_t *jmsg, *jarr;

    void *iter = json_object_iter(params);
    if (iter)
        exit_client("non-empty parameter list for get_options", 0);

    jarr = json_array();
    options = nh_get_options();
    for (i = 0; options[i].name; i++) {
        json_array_append_new(jarr, json_option(&options[i]));
    }
    jmsg = json_pack("{so}", "options", jarr);
    client_msg("get_options", jmsg);
    nhlib_free_optlist(options);
}


static void
ccmd_get_pl_prompt(json_t * params)
{
    int rolenum, racenum, gendnum, alignnum;
    char buf[1024], *bp;

    if (json_unpack
        (params, "{si,si,si,si*}", "role", &rolenum, "race", &racenum, "gend",
         &gendnum, "align", &alignnum) == -1)
        exit_client("Bad parameters for get_pl_prompt", 0);

    bp = nh_build_plselection_prompt(buf, 1024, rolenum, racenum, gendnum,
                                     alignnum);
    client_msg("get_pl_prompt", json_pack("{ss}", "prompt", bp));
}


static void
ccmd_get_root_pl_prompt(json_t * params)
{
    int rolenum, racenum, gendnum, alignnum;
    char buf[1024];
    const char *bp;

    if (json_unpack
        (params, "{si,si,si,si*}", "role", &rolenum, "race", &racenum, "gend",
         &gendnum, "align", &alignnum) == -1)
        exit_client("Bad parameters for get_root_pl_prompt", 0);

    bp = nh_root_plselection_prompt(buf, 1024, rolenum, racenum, gendnum,
                                    alignnum);
    client_msg("get_root_pl_prompt", json_pack("{ss}", "prompt", bp));
}


void
ccmd_set_email(json_t * params)
{
    const char *str;
    int ret = FALSE;

    if (json_unpack(params, "{ss*}", "email", &str) != -1)
        ret = db_set_user_email(user_info.uid, str);

    client_msg("set_email", json_pack("{si}", "return", ret));
}


void
ccmd_set_password(json_t * params)
{
    const char *str;
    int ret = FALSE;

    if (json_unpack(params, "{ss*}", "password", &str) != -1)
        ret = db_set_user_password(user_info.uid, str);

    client_msg("set_password", json_pack("{si}", "return", ret));
}

/* clientcmd.c */
