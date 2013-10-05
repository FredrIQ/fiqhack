/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include <time.h>

static void ccmd_shutdown(json_t * ignored);
static void ccmd_start_game(json_t * params);
static void ccmd_restore_game(json_t * params);
static void ccmd_exit_game(json_t * params);
static void ccmd_game_command(json_t * params);
static void ccmd_view_start(json_t * params);
static void ccmd_view_step(json_t * params);
static void ccmd_view_finish(json_t * params);
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

    {"start_game", ccmd_start_game, 0},
    {"restore_game", ccmd_restore_game, 0},
    {"exit_game", ccmd_exit_game, 0},
    {"game_command", ccmd_game_command, 0},
    {"view_start", ccmd_view_start, 0},
    {"view_step", ccmd_view_step, 0},
    {"view_finish", ccmd_view_finish, 0},
    {"list_games", ccmd_list_games, 0},

    {"get_drawing_info", ccmd_get_drawing_info, 1},
    {"get_roles", ccmd_get_roles, 1},
    {"get_topten", ccmd_get_topten, 1},
    {"get_commands", ccmd_get_commands, 1},
    {"get_obj_commands", ccmd_get_obj_commands, 1},
    {"describe_pos", ccmd_describe_pos, 1},

    {"set_option", ccmd_set_option, 0},
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

/* start_game: Start a new game
 * parameters: name, role, race, gend, align, playmode
 */
static void
ccmd_start_game(json_t * params)
{
    char filename[1024], basename[1024], path[1024];
    json_t *j_msg;
    const char *name;
    int role, race, gend, align, mode, fd, ret;
    long t;

    if (json_unpack
        (params, "{ss,si,si,si,si,si*}", "name", &name, "role", &role, "race",
         &race, "gender", &gend, "alignment", &align, "mode", &mode) == -1)
        exit_client("Bad set of parameters for start_game");

    /* reset cached display data from a previous game */
    reset_cached_diplaydata();

    if (mode == MODE_WIZARD && !user_info.can_debug)
        mode = MODE_EXPLORE;

    t = (long)time(NULL);
    snprintf(path, 1024, "%s/save/%s/", settings.workdir, user_info.username);
    snprintf(basename, 1024, "%ld_%s.nhgame", t, name);
    snprintf(filename, 1024, "%s%s", path, basename);

    mkdir(path, 0755);  /* should already exist unless something went wrong
                           while upgrading */
    fd = open(filename, O_EXCL | O_CREAT | O_RDWR, 0600);
    if (fd == -1)
        exit_client("Could not create the logfile");

    ret = nh_start_game(fd, name, role, race, gend, align, mode);
    if (ret) {
        struct nh_roles_info *ri = nh_get_roles();
        const char *rolename = (gend &&
                                ri->rolenames_f[role]) ? ri->
            rolenames_f[role] : ri->rolenames_m[role];
        gamefd = fd;
        gameid =
            db_add_new_game(user_info.uid, basename, rolename,
                            ri->racenames[race], ri->gendnames[gend],
                            ri->alignnames[align], mode, name,
                            player_info.level_desc);
        log_msg("%s has started a new game (%d) as %s", user_info.username,
                gameid, name);
        j_msg = json_pack("{si,si}", "return", ret, "gameid", gameid);
    } else {
        close(fd);
        unlink(filename);
        j_msg = json_pack("{si,si}", "return", ret, "gameid", -1);
    }

    client_msg("start_game", j_msg);
}


static void
ccmd_restore_game(json_t * params)
{
    int gid, fd, status;
    char filename[1024], basename[1024];

    if (json_unpack(params, "{si*}", "gameid", &gid) == -1)
        exit_client("Bad set of parameters for restore_game");

    if (!db_get_game_filename(user_info.uid, gid, basename, 1024)) {
        client_msg("restore_game", json_pack("{si}", "return", ERR_BAD_FILE));
        return;
    }

    snprintf(filename, 1024, "%s/save/%s/%s", settings.workdir,
             user_info.username, basename);
    fd = open(filename, O_RDWR);
    if (fd == -1) {
        client_msg("restore_game", json_pack("{si}", "return", ERR_BAD_FILE));
        return;
    }

    /* reset cached display data from a previous game */
    reset_cached_diplaydata();

    status = nh_restore_game(fd, NULL, FALSE);
    if (status == ERR_REPLAY_FAILED) {
        log_msg("Failed to restore saved game %d, file %s", gid, filename);
        if (srv_yn_function
            ("Restoring the game failed. Would you like to remove it from the list?",
             "yn", 'n') == 'y') {
            db_delete_game(user_info.uid, gid);
            log_msg("%s has chosen to remove game %d from the database",
                    user_info.username, gid);
        }
    }

    client_msg("restore_game", json_pack("{si}", "return", status));

    if (status == GAME_RESTORED) {
        gameid = gid;
        gamefd = fd;
        db_update_game(gameid, player_info.moves, player_info.z,
                       player_info.level_desc);
        log_msg("%s has restored game %d", user_info.username, gameid);
    }
}


static void
ccmd_exit_game(json_t * params)
{
    int etype, status;

    if (json_unpack(params, "{si*}", "exit_type", &etype) == -1)
        exit_client("Bad set of parameters for exit_game");

    status = nh_exit_game(etype);
    if (status) {
        db_update_game(gameid, player_info.moves, player_info.z,
                       player_info.level_desc);
        log_msg("%s has closed game %d", user_info.username, gameid);
        gameid = 0;
        close(gamefd);
        gamefd = -1;
    }

    client_msg("exit_game", json_pack("{si}", "return", status));
}


static void
ccmd_game_command(json_t * params)
{
    json_t *jarg;
    int count, result, gid;
    const char *cmd;
    struct nh_cmd_arg arg;

    if (json_unpack
        (params, "{ss,so,si*}", "command", &cmd, "arg", &jarg, "count",
         &count) == -1)
        exit_client("Bad set of parameters for game_command");

    if (json_unpack(jarg, "{si*}", "argtype", &arg.argtype) == -1)
        exit_client("Bad parameter arg in game_command");

    switch (arg.argtype) {
    case CMD_ARG_DIR:
        if (json_unpack(jarg, "{si*}", "d", &arg.d) == -1)
            exit_client("Bad direction arg in game_command");
        break;

    case CMD_ARG_POS:
        if (json_unpack(jarg, "{si,si*}", "x", &arg.pos.x, "y", &arg.pos.y) ==
            -1)
            exit_client("Bad position arg in game_command");
        break;

    case CMD_ARG_OBJ:
        if (json_unpack(jarg, "{si*}", "invlet", &arg.invlet) == -1)
            exit_client("Bad invlet arg in game_command");
        break;

    case CMD_ARG_NONE:
    default:
        break;
    }

    if (cmd[0] == '\0')
        cmd = NULL;

    result = nh_command(cmd, count, &arg);

    gid = gameid;
    if (result >= GAME_OVER) {
        close(gamefd);
        log_msg("Game %d (by %s) closed: game %s.", gameid, user_info.username,
                result == GAME_SAVED ? "saved" : "ended");
        gamefd = -1;
        gameid = 0;
    }

    client_msg("game_command", json_pack("{si}", "return", result));
    db_update_game(gameid, player_info.moves, player_info.z,
                   player_info.level_desc);

    /* move the finished game to its final resting place */
    if (result == GAME_OVER) {
        char basename[1024], filename[1024], final_name[1024];
        int len;
        char buf[BUFSZ];

        /* get the topten entry for the current game */
        struct nh_topten_entry *tte = nh_get_topten(&len, buf, NULL, 0, 0, 0);

        if (!db_get_game_filename(user_info.uid, gid, basename, 1024))
            return;

        snprintf(filename, 1024, "%s/save/%s/%s", settings.workdir,
                 user_info.username, basename);
        snprintf(final_name, 1024, "%s/completed/%s", settings.workdir,
                 basename);
        rename(filename, final_name);
        db_add_topten_entry(gid, tte->points, tte->hp, tte->maxhp, tte->deaths,
                            tte->end_how, tte->death, tte->entrytxt);
    }
}


static void
ccmd_view_start(json_t * params)
{
    int ret, gid, fd;
    struct nh_replay_info info;
    char basename[1024], filename[1024];
    json_t *jmsg;

    if (json_unpack(params, "{si*}", "gameid", &gid) == -1)
        exit_client("Bad set of parameters for view_start");

    if (!db_get_game_filename(0, gid, basename, 1024)) {
        log_msg("Client requested nonexistent game %d for viewing", gid);
        jmsg = json_pack("{si}", "return", FALSE);
        client_msg("view_start", jmsg);
        return;
    }

    snprintf(filename, 1024, "%s/save/%s/%s", settings.workdir,
             user_info.username, basename);
    fd = open(filename, O_RDWR);
    if (fd == -1) {
        snprintf(filename, 1024, "%s/completed/%s", settings.workdir, basename);
        fd = open(filename, O_RDWR);
    }
    if (fd == -1) {
        log_msg("failed to open game %d (file %s) for viewing", gid, basename);
        jmsg = json_pack("{si}", "return", FALSE);
        client_msg("view_start", jmsg);
        return;
    }

    ret = nh_view_replay_start(fd, &server_alt_windowprocs, &info);

    jmsg =
        json_pack("{si,s:{ss,si,si,si,si}}", "return", ret, "info", "nextcmd",
                  info.nextcmd, "actions", info.actions, "max_actions",
                  info.max_actions, "moves", info.moves, "max_moves",
                  info.max_moves);
    client_msg("view_start", jmsg);
}


static void
ccmd_view_step(json_t * params)
{
    enum replay_control action;
    int count, ret;
    struct nh_replay_info info;
    json_t *jmsg;

    if (json_unpack
        (params, "{si,si,s:{si,si,si,si*}*}", "action", &action, "count",
         &count, "info", "actions", &info.actions, "max_actions",
         &info.max_actions, "moves", &info.moves, "max_moves",
         &info.max_moves) == -1)
        exit_client("Bad set of parameters for view_step");

    info.nextcmd[0] = '\0';
    ret = nh_view_replay_step(&info, action, count);

    jmsg =
        json_pack("{si,s:{ss,si,si,si,si}}", "return", ret, "info", "nextcmd",
                  info.nextcmd, "actions", info.actions, "max_actions",
                  info.max_actions, "moves", info.moves, "max_moves",
                  info.max_moves);
    client_msg("view_step", jmsg);
}


static void
ccmd_view_finish(json_t * params)
{
    void *iter;

    iter = json_object_iter(params);
    if (iter)
        exit_client("non-empty parameter list for view_finish");

    nh_view_replay_finish();

    client_msg("view_finish", json_object());
}


static void
ccmd_list_games(json_t * params)
{
    char filename[1024];
    int completed, limit, show_all, count, i, fd;
    struct gamefile_info *files;
    enum nh_log_status status;
    struct nh_game_info gi;
    json_t *jarr, *jobj;

    if (json_unpack
        (params, "{si,si*}", "completed", &completed, "limit", &limit) == -1)
        exit_client("Bad parameters for list_games");
    if (json_unpack(params, "{si*}", "show_all", &show_all) == -1)
        show_all = 0;

    /* step 1: get a list of files from the db. */
    files =
        db_list_games(completed, show_all ? 0 : user_info.uid, limit, &count);

    jarr = json_array();
    /* step 2: get extra info for each file. */
    for (i = 0; i < count; i++) {
        if (completed)
            snprintf(filename, 1024, "%s/completed/%s", settings.workdir,
                     files[i].filename);
        else
            snprintf(filename, 1024, "%s/save/%s/%s", settings.workdir,
                     user_info.username, files[i].filename);
        fd = open(filename, O_RDWR);
        if (fd == -1) {
            log_msg("Game file %s could not be opened in ccmd_list_games.",
                    files[i].filename);
            continue;
        }

        status = nh_get_savegame_status(fd, &gi);
        jobj =
            json_pack("{si,si,si,ss,ss,ss,ss,ss}", "gameid", files[i].gid,
                      "status", status, "playmode", gi.playmode, "plname",
                      gi.name, "plrole", gi.plrole, "plrace", gi.plrace,
                      "plgend", gi.plgend, "plalign", gi.plalign);
        if (status == LS_SAVED) {
            json_object_set_new(jobj, "level_desc", json_string(gi.level_desc));
            json_object_set_new(jobj, "moves", json_integer(gi.moves));
            json_object_set_new(jobj, "depth", json_integer(gi.depth));
            json_object_set_new(jobj, "has_amulet",
                                json_integer(gi.has_amulet));
        } else if (status == LS_DONE) {
            json_object_set_new(jobj, "death", json_string(gi.death));
            json_object_set_new(jobj, "moves", json_integer(gi.moves));
            json_object_set_new(jobj, "depth", json_integer(gi.depth));
        }
        json_array_append_new(jarr, jobj);

        free((void *)files[i].username);
        free((void *)files[i].filename);

        close(fd);
    }
    free(files);

    client_msg("list_games", json_pack("{so}", "games", jarr));
}


static json_t *
json_symarray(struct nh_symdef *array, int len)
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
        exit_client("non-empty parameter list for get_drawing_info");

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
        exit_client("non-empty parameter list for get_roles");

    ri = nh_get_roles();
    jmsg =
        json_pack("{si,si,si,si,si,si,si,si}", "num_roles", ri->num_roles,
                  "num_races", ri->num_races, "num_genders", ri->num_genders,
                  "num_aligns", ri->num_aligns, "def_role", ri->def_role,
                  "def_race", ri->def_race, "def_gend", ri->def_gend,
                  "def_align", ri->def_align);

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
        exit_client("Bad parameters for get_topten");

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
        exit_client("non-empty parameter list for get_commands");

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
        exit_client("Bad parameters for get_obj_commands");
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
        exit_client("Bad parameters for describe_pos");

    nh_describe_pos(x, y, &db, is_in ? &is_in : NULL);
    jmsg =
        json_pack("{ss,ss,ss,ss,ss,ss,si,si}", "bgdesc", db.bgdesc, "trapdesc",
                  db.trapdesc, "objdesc", db.objdesc, "mondesc", db.mondesc,
                  "invisdesc", db.invisdesc, "effectdesc", db.effectdesc,
                  "objcount", db.objcount, "in", is_in);
    client_msg("describe_pos", jmsg);
}


static const struct nh_option_desc *
find_option(const char *optname, const struct nh_option_desc *list1,
            const struct nh_option_desc *list2)
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


static json_t *
json_option(const struct nh_option_desc *option)
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
        joptdesc =
            json_pack("{si,si}", "max", option->i.max, "min", option->i.min);
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
        json_pack("{ss,ss,si,so,so}", "name", option->name, "helptxt",
                  option->helptxt, "type", option->type, "value", joptval,
                  "desc", joptdesc);
    return jopt;
}


static void
ccmd_set_option(json_t * params)
{
    const char *optname, *optstr, *pattern;
    json_t *jmsg, *joval, *jopt;
    int isstr, i, ret;
    const struct nh_option_desc *gameopt, *birthopt, *option;
    union nh_optvalue value;
    struct nh_autopickup_rules ar = { NULL, 0 };
    struct nh_autopickup_rule *r;

    if (json_unpack
        (params, "{ss,so,si*}", "name", &optname, "value", &joval, "isstr",
         &isstr) == -1)
        exit_client("Bad parameters for set_option");

    /* find the option_desc for the options that should be set; the option type
       is required in order to decode the option value. */
    gameopt = nh_get_options(GAME_OPTIONS);
    birthopt =
        nh_get_options(gameid ? ACTIVE_BIRTH_OPTIONS : CURRENT_BIRTH_OPTIONS);
    option = find_option(optname, gameopt, birthopt);
    if (!option) {
        jmsg = json_pack("{si,so}", "return", FALSE, "option", json_object());
        client_msg("set_option", jmsg);
        return;
    }

    /* decode the option value depending on the option type */
    if (isstr || option->type == OPTTYPE_STRING) {
        if (!json_is_string(joval))
            exit_client("could not decode option string");
        value.s = (char *)json_string_value(joval);

    } else if (option->type == OPTTYPE_INT || option->type == OPTTYPE_ENUM ||
               option->type == OPTTYPE_BOOL) {
        if (!json_is_integer(joval))
            exit_client("could not decode option value");
        value.i = json_integer_value(joval);

    } else if (option->type == OPTTYPE_AUTOPICKUP_RULES) {
        if (!json_is_array(joval))
            exit_client("could not decode option");

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
                    exit_client("Error unpacking autopickup rule");
                strncpy(r->pattern, pattern, sizeof (r->pattern) - 1);
            }
        } else
            value.ar = NULL;
    }

    ret = nh_set_option(optname, value, isstr);

    if (option->type == OPTTYPE_AUTOPICKUP_RULES)
        free(ar.rules);

    gameopt = nh_get_options(GAME_OPTIONS);
    birthopt =
        nh_get_options(gameid ? ACTIVE_BIRTH_OPTIONS : CURRENT_BIRTH_OPTIONS);
    option = find_option(optname, gameopt, birthopt);

    jopt = json_option(option);
    optstr = nh_get_option_string(option);

    if (ret == TRUE)
        db_set_option(user_info.uid, optname, option->type, optstr);
    /* return the altered option struct and the string representation to the
       client. The intent is to save some network round trips and make a
       separate get_option_string message unneccessary */
    jmsg = json_pack("{si,so}", "return", ret, "option", jopt);
    client_msg("set_option", jmsg);
}


static void
ccmd_get_options(json_t * params)
{
    int list, i;
    const struct nh_option_desc *options;
    json_t *jmsg, *jarr;

    if (json_unpack(params, "{si*}", "list", &list) == -1)
        exit_client("Bad parameters for get_options");

    jarr = json_array();
    options = nh_get_options(list);
    for (i = 0; options[i].name; i++)
        json_array_append_new(jarr, json_option(&options[i]));
    jmsg = json_pack("{so}", "options", jarr);
    client_msg("get_options", jmsg);
}


static void
ccmd_get_pl_prompt(json_t * params)
{
    int rolenum, racenum, gendnum, alignnum;
    char buf[1024], *bp;

    if (json_unpack
        (params, "{si,si,si,si*}", "role", &rolenum, "race", &racenum, "gend",
         &gendnum, "align", &alignnum) == -1)
        exit_client("Bad parameters for get_pl_prompt");

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
        exit_client("Bad parameters for get_root_pl_prompt");

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
