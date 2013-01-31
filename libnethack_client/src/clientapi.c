/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2012. */
/* The NetHack client lib may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhclient.h"

struct nh_window_procs windowprocs, alt_windowprocs;
int current_game;
struct nh_option_desc *option_lists[OPTION_LIST_COUNT];

#ifdef UNIX
# include <signal.h>
static struct sigaction oldaction;
#endif


void
nhnet_lib_init(const struct nh_window_procs *winprocs)
{
#ifdef WIN32
    WSADATA wsa_data;

    WSAStartup(0x0202, &wsa_data);
#else
    /* ignore SIGPIPE */
    struct sigaction ignoreaction;

    memset(&ignoreaction, 0, sizeof (struct sigaction));
    ignoreaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &ignoreaction, &oldaction);
#endif

    windowprocs = *winprocs;
    alt_windowprocs = *winprocs;
}


void
nhnet_lib_exit(void)
{
    if (nhnet_connected())
        nhnet_disconnect();

    xmalloc_cleanup();
    conn_err = FALSE;

#ifdef WIN32
    WSACleanup();
#else
    /* restore previous signale handler for SIGPIPE */
    sigaction(SIGPIPE, &oldaction, NULL);
#endif
}


nh_bool
nhnet_exit_game(int exit_type)
{
    json_t *jmsg;
    int ret;

    if (!nhnet_active())
        return nh_exit_game(exit_type);

    xmalloc_cleanup();

    if (!api_entry())
        return 0;

    jmsg = json_pack("{si}", "exit_type", exit_type);
    jmsg = send_receive_msg("exit_game", jmsg);
    if (json_unpack(jmsg, "{si!}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_exit_game");
        ret = 0;
    }
    json_decref(jmsg);

    current_game = 0;

    api_exit();
    return ret;
}


struct nhnet_game *
nhnet_list_games(int done, int show_all, int *count)
{
    int i, has_amulet;
    json_t *jmsg, *jarr, *jobj;
    struct nhnet_game *gb;
    struct nhnet_game *gamebuf = NULL;
    const char *plname, *plrole, *plrace, *plgend, *plalign, *level_desc,
        *death;

    if (!api_entry())
        return NULL;

    jmsg =
        json_pack("{si,si,si}", "limit", 0, "completed", done, "show_all",
                  show_all);
    jmsg = send_receive_msg("list_games", jmsg);
    if (json_unpack(jmsg, "{so!}", "games", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_list_games");
        *count = 0;
    } else {
        *count = json_array_size(jarr);
        gamebuf = xmalloc(*count * sizeof (struct nhnet_game));
        for (i = 0; i < *count; i++) {
            gb = &gamebuf[i];
            memset(gb, 0, sizeof (struct nhnet_game));
            jobj = json_array_get(jarr, i);
            if (json_unpack
                (jobj, "{si,si,si,ss,ss,ss,ss,ss*}", "gameid", &gb->gameid,
                 "status", &gb->status, "playmode", &gb->i.playmode, "plname",
                 &plname, "plrole", &plrole, "plrace", &plrace, "plgend",
                 &plgend, "plalign", &plalign) == -1) {
                print_error("Invalid game info object.");
                continue;
            }
            strncpy(gb->i.name, plname, PL_NSIZ - 1);
            strncpy(gb->i.plrole, plrole, PLRBUFSZ - 1);
            strncpy(gb->i.plrace, plrace, PLRBUFSZ - 1);
            strncpy(gb->i.plgend, plgend, PLRBUFSZ - 1);
            strncpy(gb->i.plalign, plalign, PLRBUFSZ - 1);

            if (gb->status == LS_SAVED) {
                json_unpack(jobj, "{ss,si,si,si*}", "level_desc", &level_desc,
                            "moves", &gb->i.moves, "depth", &gb->i.depth,
                            "has_amulet", &has_amulet);
                gb->i.has_amulet = has_amulet;
                strncpy(gb->i.level_desc, level_desc,
                        sizeof (gb->i.level_desc) - 1);
            } else if (gb->status == LS_DONE) {
                json_unpack(jobj, "{ss,si,si*}", "death", &death, "moves",
                            &gb->i.moves, "depth", &gb->i.depth);
                strncpy(gb->i.death, death, sizeof (gb->i.death) - 1);
            }
        }
    }
    json_decref(jmsg);

    api_exit();
    return gamebuf;
}


int
nhnet_restore_game(int gid, struct nh_window_procs *rwinprocs)
{
    int ret;
    json_t *jmsg;

    if (!api_entry())
        return ERR_NETWORK_ERROR;

    jmsg = json_pack("{si}", "gameid", gid);
    jmsg = send_receive_msg("restore_game", jmsg);
    if (json_unpack(jmsg, "{si!}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_restore_game");
        ret = ERR_NETWORK_ERROR;        /* we don't know the error actually
                                           was, any error code will do */
    }
    json_decref(jmsg);

    if (ret == GAME_RESTORED)
        current_game = gid;

    api_exit();
    return ret;
}


nh_bool
nhnet_start_game(const char *name, int role, int race, int gend, int align,
                 enum nh_game_modes playmode)
{
    int ret;
    json_t *jmsg;

    if (!api_entry())
        return 0;

    jmsg =
        json_pack("{ss,si,si,si,si,si}", "name", name, "role", role, "race",
                  race, "gender", gend, "alignment", align, "mode", playmode);
    jmsg = send_receive_msg("start_game", jmsg);
    if (json_unpack(jmsg, "{si,si!}", "return", &ret, "gameid", &current_game)
        == -1) {
        print_error("Incorrect return object in nhnet_start_game");
        ret = 0;
    }

    json_decref(jmsg);
    api_exit();
    return ret;
}


int
nhnet_command(const char * volatile cmd, int rep, struct nh_cmd_arg *arg)
{
    int ret;
    json_t *jmsg, *jarg;

    if (!nhnet_active())
        return nh_command(cmd, rep, arg);

    if (!api_entry())
        return ERR_NETWORK_ERROR;

    xmalloc_cleanup();

    switch (arg->argtype) {
    case CMD_ARG_DIR:
        jarg = json_pack("{si,si}", "argtype", arg->argtype, "d", arg->d);
        break;

    case CMD_ARG_POS:
        jarg =
            json_pack("{si,si,si}", "argtype", arg->argtype, "x", arg->pos.x,
                      "y", arg->pos.y);
        break;

    case CMD_ARG_OBJ:
        jarg =
            json_pack("{si,si}", "argtype", arg->argtype, "invlet",
                      arg->invlet);
        break;

    case CMD_ARG_NONE:
    default:
        jarg = json_pack("{si}", "argtype", arg->argtype);
        break;
    }

    jmsg =
        json_pack("{ss,so,si}", "command", cmd ? cmd : "", "arg", jarg, "count",
                  rep);
    jmsg = send_receive_msg("game_command", jmsg);
    if (json_unpack(jmsg, "{si!}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_command");
        ret = 0;
    }

    json_decref(jmsg);
    api_exit();
    return ret;
}


nh_bool
nhnet_view_replay_start(int fd, struct nh_window_procs * rwinprocs,
                        struct nh_replay_info * info)
{
    int ret;
    json_t *jmsg;
    const char *nextcmd;

    if (!nhnet_active())
        return nh_view_replay_start(fd, rwinprocs, info);

    if (!api_entry())
        return FALSE;

    alt_windowprocs = *rwinprocs;

    jmsg = send_receive_msg("view_start", json_pack("{si}", "gameid", fd));
    if (json_unpack
        (jmsg, "{si,s:{ss,si,si,si,si}}", "return", &ret, "info", "nextcmd",
         &nextcmd, "actions", &info->actions, "max_actions", &info->max_actions,
         "moves", &info->moves, "max_moves", &info->max_moves) == -1) {
        print_error("Incorrect return object in nhnet_view_replay_step");
        ret = 0;
    } else
        strncpy(info->nextcmd, nextcmd, sizeof (info->nextcmd) - 1);

    api_exit();
    return ret;
}


nh_bool
nhnet_view_replay_step(struct nh_replay_info * info, enum replay_control action,
                       int count)
{
    int ret;
    json_t *jmsg;
    const char *nextcmd;

    if (!nhnet_active())
        return nh_view_replay_step(info, action, count);

    if (!api_entry())
        return FALSE;

    jmsg =
        send_receive_msg("view_step",
                         json_pack("{si,si,s:{si,si,si,si}}", "action", action,
                                   "count", count, "info", "actions",
                                   info->actions, "max_actions",
                                   info->max_actions, "moves", info->moves,
                                   "max_moves", info->max_moves));
    if (json_unpack
        (jmsg, "{si,s:{ss,si,si,si,si}}", "return", &ret, "info", "nextcmd",
         &nextcmd, "actions", &info->actions, "max_actions", &info->max_actions,
         "moves", &info->moves, "max_moves", &info->max_moves) == -1) {
        print_error("Incorrect return object in nhnet_view_replay_step");
    } else
        strncpy(info->nextcmd, nextcmd, sizeof (info->nextcmd) - 1);

    api_exit();
    return ret;
}


void
nhnet_view_replay_finish(void)
{
    if (!nhnet_active())
        return nh_view_replay_finish();

    xmalloc_cleanup();

    alt_windowprocs = windowprocs;

    if (!api_entry())
        return;

    send_receive_msg("view_finish", json_object());

    api_exit();
}


struct nh_cmd_desc *
nhnet_get_commands(int *count)
{
    int i, defkey, altkey;
    json_t *jmsg, *jarr, *jobj;
    struct nh_cmd_desc *cmdlist = NULL;
    const char *name, *desc;

    if (!nhnet_active())
        return nh_get_commands(count);

    if (!api_entry())
        return 0;

    jmsg = json_object();
    jmsg = send_receive_msg("get_commands", jmsg);
    if (json_unpack(jmsg, "{so!}", "cmdlist", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_restore_game");
    } else {
        *count = json_array_size(jarr);
        cmdlist = xmalloc(*count * sizeof (struct nh_cmd_desc));

        for (i = 0; i < *count; i++) {
            jobj = json_array_get(jarr, i);
            json_unpack(jobj, "{ss,ss,si,si,si!}", "name", &name, "desc", &desc,
                        "def", &defkey, "alt", &altkey, "flags",
                        &cmdlist[i].flags);
            strcpy(cmdlist[i].name, name);
            strcpy(cmdlist[i].desc, desc);
            cmdlist[i].defkey = defkey;
            cmdlist[i].altkey = altkey;
        }
    }

    json_decref(jmsg);
    api_exit();
    return cmdlist;
}


struct nh_cmd_desc *
nhnet_get_object_commands(int *count, char invlet)
{
    int i, defkey, altkey;
    json_t *jmsg, *jarr, *jobj;
    struct nh_cmd_desc *cmdlist = NULL;
    const char *name, *desc;

    if (!nhnet_active())
        return nh_get_object_commands(count, invlet);

    if (!api_entry())
        return 0;

    jmsg = json_pack("{si}", "invlet", invlet);
    jmsg = send_receive_msg("get_obj_commands", jmsg);
    if (json_unpack(jmsg, "{so!}", "cmdlist", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_object_commands");
    } else {
        *count = json_array_size(jarr);
        cmdlist = xmalloc(*count * sizeof (struct nh_cmd_desc));

        for (i = 0; i < *count; i++) {
            jobj = json_array_get(jarr, i);
            json_unpack(jobj, "{ss,ss,si,si,si!}", "name", &name, "desc", &desc,
                        "def", &defkey, "alt", &altkey, "flags",
                        &cmdlist[i].flags);
            strcpy(cmdlist[i].name, name);
            strcpy(cmdlist[i].desc, desc);
            cmdlist[i].defkey = defkey;
            cmdlist[i].altkey = altkey;
        }
    }

    json_decref(jmsg);
    api_exit();
    return cmdlist;
}


static struct nh_symdef *
read_symdef_array(json_t * jarr)
{
    json_t *jobj;
    struct nh_symdef *arr;
    int i, size, ch;
    const char *symname;
    char *str;

    size = json_array_size(jarr);
    arr = xmalloc(size * sizeof (struct nh_symdef));

    for (i = 0; i < size; i++) {
        jobj = json_array_get(jarr, i);
        json_unpack(jobj, "[s,i,i!]", &symname, &ch, &arr[i].color);
        arr[i].ch = ch;
        str = xmalloc(strlen(symname) + 1);
        strcpy(str, symname);
        arr[i].symname = str;
    }

    return arr;
}


struct nh_drawing_info *
nhnet_get_drawing_info(void)
{
    json_t *jmsg, *jbg, *jtraps, *jobjs, *jmons, *jwarn, *jexpt, *jzapt, *jzaps,
        *jeff, *jexps, *jswal, *jinvis;
    struct nh_drawing_info *di;

    if (!nhnet_active())
        return nh_get_drawing_info();

    if (!api_entry())
        return 0;

    jmsg = send_receive_msg("get_drawing_info", json_object());
    di = xmalloc(sizeof (struct nh_drawing_info));
    if (json_unpack
        (jmsg,
         "{si,si,si,si,si,si,si,si,si,so,so,so,so,so,so,so,so,so,so,so,so!}",
         "num_bgelements", &di->num_bgelements, "num_traps", &di->num_traps,
         "num_objects", &di->num_objects, "num_monsters", &di->num_monsters,
         "num_warnings", &di->num_warnings, "num_expltypes", &di->num_expltypes,
         "num_zaptypes", &di->num_zaptypes, "num_effects", &di->num_effects,
         "feature_offset", &di->bg_feature_offset, "bgelements", &jbg, "traps",
         &jtraps, "objects", &jobjs, "monsters", &jmons, "warnings", &jwarn,
         "expltypes", &jexpt, "zaptypes", &jzapt, "effects", &jeff, "explsyms",
         &jexps, "zapsyms", &jzaps, "swallowsyms", &jswal, "invis",
         &jinvis) == -1 || !json_is_array(jbg) || !json_is_array(jobjs) ||
        !json_is_array(jmons) || !json_is_array(jwarn) || !json_is_array(jexpt)
        || !json_is_array(jzapt) || !json_is_array(jeff) ||
        !json_is_array(jexps) || !json_is_array(jswal) ||
        !json_is_array(jinvis)) {
        print_error("Incorrect return object in nhnet_get_drawing_info");
        di = NULL;
    } else {
        di->bgelements = read_symdef_array(jbg);
        di->traps = read_symdef_array(jtraps);
        di->objects = read_symdef_array(jobjs);
        di->monsters = read_symdef_array(jmons);
        di->warnings = read_symdef_array(jwarn);
        di->expltypes = read_symdef_array(jexpt);
        di->zaptypes = read_symdef_array(jzapt);
        di->effects = read_symdef_array(jeff);
        di->explsyms = read_symdef_array(jexps);
        di->zapsyms = read_symdef_array(jzaps);
        di->swallowsyms = read_symdef_array(jswal);
        di->invis = read_symdef_array(jinvis);
    }
    json_decref(jmsg);

    api_exit();
    return di;
}


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


static void
read_json_option(json_t * jobj, struct nh_option_desc *opt)
{
    json_t *joptval, *joptdesc, *jelem;
    const char *name, *helptxt, *strval;
    int size, i;
    struct nh_autopickup_rule *r;

    memset(opt, 0, sizeof (struct nh_option_desc));
    if (!json_unpack
        (jobj, "{ss,ss,si,so,so!}", "name", &name, "helptxt", &helptxt, "type",
         &opt->type, "value", &joptval, "desc", &joptdesc) == -1) {
        memset(opt, 0, sizeof (struct nh_option_desc));
        print_error("Broken option specification.");
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
            strncpy(r->pattern, strval, sizeof (r->pattern) - 1);
        }

        break;
    }
}


static void
free_option_data(struct nh_option_desc *opt)
{
    int i;

    free((void *)opt->name);
    free((void *)opt->helptxt);

    switch (opt->type) {
    case OPTTYPE_BOOL:
    case OPTTYPE_INT:
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < opt->e.numchoices; i++)
            free(opt->e.choices[i].caption);
        free((void *)opt->e.choices);
        break;

    case OPTTYPE_STRING:
        if (opt->value.s)
            free(opt->value.s);
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        for (i = 0; i < opt->a.numclasses; i++)
            free(opt->a.classes[i].caption);
        free((void *)opt->a.classes);

        if (opt->value.ar) {
            free(opt->value.ar->rules);
            free(opt->value.ar);
        }
        break;
    }
}


nh_bool
nhnet_set_option(const char *name, union nh_optvalue value, nh_bool isstr)
{
    int ret, i;
    json_t *jmsg, *joval, *jobj;
    struct nh_option_desc *gameopts, *birthopts, *opt;
    struct nh_autopickup_rule *r;

    ret = nh_set_option(name, value, isstr);
    if (!nhnet_active())
        return ret;

    if (!api_entry())
        return FALSE;

    gameopts = nhnet_get_options(GAME_OPTIONS);
    birthopts = nhnet_get_options(CURRENT_BIRTH_OPTIONS);
    opt = NULL;
    for (i = 0; gameopts[i].name && !opt; i++)
        if (!strcmp(name, gameopts[i].name))
            opt = &gameopts[i];
    for (i = 0; birthopts[i].name && !opt; i++)
        if (!strcmp(name, birthopts[i].name))
            opt = &birthopts[i];

    if (opt) {
        if (isstr || opt->type == OPTTYPE_STRING)
            joval = json_string(value.s);
        else if (opt->type == OPTTYPE_INT || opt->type == OPTTYPE_ENUM ||
                 opt->type == OPTTYPE_BOOL) {
            joval = json_integer(value.i);
        } else if (opt->type == OPTTYPE_AUTOPICKUP_RULES) {
            joval = json_array();
            for (i = 0; value.ar && i < value.ar->num_rules; i++) {
                r = &value.ar->rules[i];
                jobj =
                    json_pack("{ss,si,si,si}", "pattern", r->pattern, "oclass",
                              r->oclass, "buc", r->buc, "action", r->action);
                json_array_append_new(joval, jobj);
            }
        } else {
            /* Shouldn't happen; if it does, put an easily searchable string
               in as the value to ease debuging */
            joval = json_string("<unused>");
        }

        jmsg =
            json_pack("{ss,so,si}", "name", name, "value", joval, "isstr",
                      isstr);
        jmsg = send_receive_msg("set_option", jmsg);
        if (json_unpack(jmsg, "{si,so!}", "return", &ret, "option", &jobj) ==
            -1) {
            print_error("Bad response in nhnet_set_option");
        } else {
            free_option_data(opt);
            read_json_option(jobj, opt);
        }
        json_decref(jmsg);
    }
    api_exit();
    return ret;
}


struct nh_option_desc *
nhnet_get_options(enum nh_option_list list)
{
    struct nh_option_desc *olist;
    json_t *jmsg, *jarr, *jobj;
    int count, i;

    if (!nhnet_active())
        return nh_get_options(list);

    if (list < OPTION_LIST_COUNT && option_lists[list])
        return option_lists[list];

    if (!api_entry()) {
        olist = xmalloc(sizeof (struct nh_option_desc));
        memset(olist, 0, sizeof (struct nh_option_desc));
        return olist;
    }

    jmsg = send_receive_msg("get_options", json_pack("{si}", "list", list));
    if (json_unpack(jmsg, "{so!}", "options", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_options");
        olist = xmalloc(sizeof (struct nh_option_desc));
        memset(olist, 0, sizeof (struct nh_option_desc));
    } else {
        count = json_array_size(jarr);
        option_lists[list] = olist =
            malloc(sizeof (struct nh_option_desc) * (count + 1));
        memset(olist, 0, sizeof (struct nh_option_desc) * (count + 1));
        for (i = 0; i < count; i++) {
            jobj = json_array_get(jarr, i);
            read_json_option(jobj, &olist[i]);
        }
    }
    json_decref(jmsg);

    api_exit();
    return olist;
}


void
free_option_lists(void)
{
    int i, j;

    for (i = 0; i < OPTION_LIST_COUNT; i++)
        if (option_lists[i]) {
            for (j = 0; option_lists[i][j].name; j++)
                free_option_data(&option_lists[i][j]);
            free(option_lists[i]);
            option_lists[i] = NULL;
        }
}


void
nhnet_describe_pos(int x, int y, struct nh_desc_buf *bufs, int *is_in)
{
    const char *bgdesc, *trapdesc, *objdesc, *mondesc, *invisdesc, *effectdesc;
    json_t *jmsg;
    int in;

    if (!nhnet_active())
        return nh_describe_pos(x, y, bufs, is_in);

    if (!api_entry())
        return;

    jmsg =
        send_receive_msg("describe_pos",
                         json_pack("{si,si, si}", "x", x, "y", y, "is_in",
                                   is_in != NULL));
    json_unpack(jmsg, "{ss,ss,ss,ss,ss,ss,si,si!}", "bgdesc", &bgdesc,
                "trapdesc", &trapdesc, "objdesc", &objdesc, "mondesc", &mondesc,
                "invisdesc", &invisdesc, "effectdesc", &effectdesc, "objcount",
                &bufs->objcount, "in", &in);

    strncpy(bufs->bgdesc, bgdesc, BUFSZ - 1);
    strncpy(bufs->trapdesc, trapdesc, BUFSZ - 1);
    strncpy(bufs->objdesc, objdesc, BUFSZ - 1);
    strncpy(bufs->mondesc, mondesc, BUFSZ - 1);
    strncpy(bufs->invisdesc, invisdesc, BUFSZ - 1);
    strncpy(bufs->effectdesc, effectdesc, BUFSZ - 1);
    json_decref(jmsg);
    if (is_in)
        *is_in = in;

    api_exit();
}


static const char **
read_string_array(json_t * jarr)
{
    json_t *jstr;
    const char **array;
    const char *str;
    char *buf;
    int size, i;

    size = json_array_size(jarr);
    array = xmalloc(sizeof (char *) * size);
    for (i = 0; i < size; i++) {
        jstr = json_array_get(jarr, i);
        if (!json_is_string(jstr)) {
            array[i] = NULL;
            continue;
        }
        str = json_string_value(jstr);
        buf = xmalloc(strlen(str) + 1);
        strcpy(buf, str);
        array[i] = buf;
    }

    return array;
}


struct nh_roles_info *
nhnet_get_roles(void)
{
    struct nh_roles_info *ri = NULL;
    json_t *jmsg, *jroles_m, *jroles_f, *jraces, *jgenders, *jaligns, *jmatrix;
    nh_bool *matrix;
    int i, size;

    if (!nhnet_active())
        return nh_get_roles();

    if (!api_entry())
        return NULL;

    jmsg = send_receive_msg("get_roles", json_object());
    ri = xmalloc(sizeof (struct nh_roles_info));
    if (json_unpack
        (jmsg, "{si,si,si,si,si,si,si,si,so,so,so,so,so,so}", "num_roles",
         &ri->num_roles, "num_races", &ri->num_races, "num_genders",
         &ri->num_genders, "num_aligns", &ri->num_aligns, "def_role",
         &ri->def_role, "def_race", &ri->def_race, "def_gend", &ri->def_gend,
         "def_align", &ri->def_align, "rolenames_m", &jroles_m, "rolenames_f",
         &jroles_f, "racenames", &jraces, "gendnames", &jgenders, "alignnames",
         &jaligns, "matrix", &jmatrix) == -1 || !json_is_array(jroles_m) ||
        !json_is_array(jroles_f) || !json_is_array(jraces) ||
        !json_is_array(jgenders) || !json_is_array(jaligns) ||
        !json_is_array(jmatrix) || json_array_size(jroles_m) != ri->num_roles ||
        json_array_size(jroles_f) != ri->num_roles ||
        json_array_size(jraces) != ri->num_races ||
        json_array_size(jgenders) != ri->num_genders ||
        json_array_size(jaligns) != ri->num_aligns) {
        print_error("Incorrect return object in nhnet_get_roles");
        ri = NULL;
    } else {
        ri->rolenames_m = read_string_array(jroles_m);
        ri->rolenames_f = read_string_array(jroles_f);
        ri->racenames = read_string_array(jraces);
        ri->gendnames = read_string_array(jgenders);
        ri->alignnames = read_string_array(jaligns);

        size = json_array_size(jmatrix);
        matrix = xmalloc(size * sizeof (nh_bool));
        for (i = 0; i < size; i++)
            matrix[i] = json_integer_value(json_array_get(jmatrix, i));
        ri->matrix = matrix;
    }

    json_decref(jmsg);
    api_exit();
    return ri;
}


char *
nhnet_build_plselection_prompt(char *buf, int buflen, int rolenum, int racenum,
                               int gendnum, int alignnum)
{
    json_t *jmsg;
    char *str, *ret;

    if (!nhnet_active())
        return nh_build_plselection_prompt(buf, buflen, rolenum, racenum,
                                           gendnum, alignnum);

    if (!api_entry())
        return NULL;

    jmsg =
        json_pack("{si,si,si,si}", "role", rolenum, "race", racenum, "gend",
                  gendnum, "align", alignnum);
    jmsg = send_receive_msg("get_pl_prompt", jmsg);
    if (json_unpack(jmsg, "{ss!}", "prompt", &str) == -1) {
        print_error
            ("Incorrect return object in nhnet_build_plselection_prompt");
        ret = NULL;
    } else {
        strncpy(buf, str, buflen - 1);
        buf[buflen - 1] = '\0';
        ret = buf;
    }
    json_decref(jmsg);

    api_exit();
    return ret;
}


const char *
nhnet_root_plselection_prompt(char *buf, int buflen, int rolenum, int racenum,
                              int gendnum, int alignnum)
{
    json_t *jmsg;
    char *str, *ret;

    if (!nhnet_active())
        return nh_root_plselection_prompt(buf, buflen, rolenum, racenum,
                                          gendnum, alignnum);

    if (!api_entry())
        return NULL;

    jmsg =
        json_pack("{si,si,si,si}", "role", rolenum, "race", racenum, "gend",
                  gendnum, "align", alignnum);
    jmsg = send_receive_msg("get_root_pl_prompt", jmsg);
    if (json_unpack(jmsg, "{ss!}", "prompt", &str) == -1) {
        print_error("Incorrect return object in nhnet_root_plselection_prompt");
        ret = NULL;
    } else {
        strncpy(buf, str, buflen - 1);
        buf[buflen - 1] = '\0';
        ret = buf;
    }
    json_decref(jmsg);

    api_exit();
    return ret;
}


struct nh_topten_entry *
nhnet_get_topten(int *out_len, char *statusbuf, const char * volatile player,
                 int top, int around, nh_bool own)
{
    struct nh_topten_entry *ttlist;
    json_t *jmsg, *jarr, *jobj;
    const char *msg, *plrole, *plrace, *plgend, *plalign, *name, *death,
        *entrytxt;
    int len, i, highlight;

    if (!nhnet_active())
        return nh_get_topten(out_len, statusbuf, player, top, around, own);

    *out_len = 0;
    if (!api_entry())
        return NULL;

    jmsg =
        json_pack("{ss,si,si,si}", "player", player ? player : "", "top", top,
                  "around", around, "own", own);
    jmsg = send_receive_msg("get_topten", jmsg);
    if (json_unpack(jmsg, "{so,ss!}", "toplist", &jarr, "msg", &msg) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_topten");
        ttlist = NULL;
    } else {
        len = json_array_size(jarr);
        strncpy(statusbuf, msg, BUFSZ - 1);
        *out_len = len;
        ttlist = xmalloc((len + 1) * sizeof (struct nh_topten_entry));
        memset(ttlist, 0, (len + 1) * sizeof (struct nh_topten_entry));
        for (i = 0; i < len; i++) {
            jobj = json_array_get(jarr, i);
            json_unpack(jobj,
                        "{si,si,si,si,si,si,si,si,si,si,si,si,si,ss,ss,ss,ss,ss,ss,ss,si!}",
                        "rank", &ttlist[i].rank, "points", &ttlist[i].points,
                        "maxlvl", &ttlist[i].maxlvl, "hp", &ttlist[i].hp,
                        "maxhp", &ttlist[i].maxhp, "deaths", &ttlist[i].deaths,
                        "ver_major", &ttlist[i].ver_major, "ver_minor",
                        &ttlist[i].ver_minor, "patchlevel",
                        &ttlist[i].patchlevel, "deathdate",
                        &ttlist[i].deathdate, "birthdate", &ttlist[i].birthdate,
                        "moves", &ttlist[i].moves, "end_how",
                        &ttlist[i].end_how, "plrole", &plrole, "plrace",
                        &plrace, "plgend", &plgend, "plalign", &plalign, "name",
                        &name, "death", &death, "entrytxt", &entrytxt,
                        "highlight", &highlight);
            strncpy(ttlist[i].plrole, plrole, PLRBUFSZ - 1);
            strncpy(ttlist[i].plrace, plrace, PLRBUFSZ - 1);
            strncpy(ttlist[i].plgend, plgend, PLRBUFSZ - 1);
            strncpy(ttlist[i].plalign, plalign, PLRBUFSZ - 1);
            strncpy(ttlist[i].name, name, PL_NSIZ - 1);
            strncpy(ttlist[i].death, death, BUFSZ - 1);
            strncpy(ttlist[i].entrytxt, entrytxt, BUFSZ - 1);
            ttlist[i].highlight = highlight;
        }
    }
    json_decref(jmsg);

    api_exit();
    return ttlist;
}


int
nhnet_change_email(const char *email)
{
    int ret;
    json_t *jmsg;

    jmsg = json_pack("{ss}", "email", email);
    jmsg = send_receive_msg("set_email", jmsg);
    if (json_unpack(jmsg, "{si}", "return", &ret) == -1)
        return FALSE;
    return ret;
}


int
nhnet_change_password(const char *password)
{
    int ret;
    json_t *jmsg;

    jmsg = json_pack("{ss}", "password", password);
    jmsg = send_receive_msg("set_password", jmsg);
    if (json_unpack(jmsg, "{si}", "return", &ret) == -1)
        return FALSE;

    if (ret)
        strncpy(saved_password, password, 199);
    return ret;
}

/* clientapi.c */
