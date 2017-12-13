/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2012. */
/* The NetHack client lib may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhclient.h"
#include "xmalloc.h"

struct nh_window_procs client_windowprocs;
int current_game;

#ifdef UNIX
# include <signal.h>
static struct sigaction oldaction;
#endif

/* We define the lifetime of pointers returned by clientapi.c as being until the
   next call into clientapi.c (except for nhnet_input_aborted, which explicitly
   does not deallocate the pointers). */
static struct xmalloc_block *xm_blocklist = NULL;

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

    client_windowprocs = *winprocs;
}


void
nhnet_lib_exit(void)
{
    xmalloc_cleanup(&xm_blocklist);

    if (nhnet_connected())
        nhnet_disconnect();

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

    xmalloc_cleanup(&xm_blocklist);

    if (!nhnet_active())
        return nh_exit_game(exit_type);

    if (!current_game)
        return 1;

    if (!api_entry())
        return 1;

    jmsg = json_pack("{si}", "exit_type", exit_type);
    jmsg = send_receive_msg("exit_game", jmsg);
    if (json_unpack(jmsg, "{sb!}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_exit_game");
        ret = 1;
    }
    json_decref(jmsg);

    current_game = 0;

    api_exit();
    return ret;
}


struct nhnet_game *
nhnet_list_games(int done, int show_all, int *count)
{
    int i;
    json_t *jmsg, *jarr, *jobj;
    struct nhnet_game *gb;
    struct nhnet_game *volatile gamebuf = NULL;
    const char *plname, *plrole, *plrace, *plgend, *plalign, *game_state;

    *count = 0; /* if we error out, we want this to be initialized */

    if (!api_entry())
        return NULL;

    xmalloc_cleanup(&xm_blocklist);

    jmsg =
        json_pack("{si,sb,sb}", "limit", 0, "completed", done, "show_all",
                  show_all);
    jmsg = send_receive_msg("list_games", jmsg);
    if (json_unpack(jmsg, "{so!}", "games", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_list_games");
        *count = 0;
    } else {
        *count = json_array_size(jarr);
        gamebuf = xmalloc(&xm_blocklist, *count * sizeof (struct nhnet_game));
        for (i = 0; i < *count; i++) {
            gb = &gamebuf[i];
            memset(gb, 0, sizeof (struct nhnet_game));
            jobj = json_array_get(jarr, i);
            if (json_unpack
                (jobj, "{si,si,si,ss,ss,ss,ss,ss,ss*}", "gameid", &gb->gameid,
                 "status", &gb->status, "playmode", &gb->i.playmode, "plname",
                 &plname, "plrole", &plrole, "plrace", &plrace, "plgend",
                 &plgend, "plalign", &plalign, "game_state", &game_state) ==
                -1) {
                print_error("Invalid game info object.");
                continue;
            }
            strncpy(gb->i.name, plname, PL_NSIZ - 1);
            strncpy(gb->i.plrole, plrole, PLRBUFSZ - 1);
            strncpy(gb->i.plrace, plrace, PLRBUFSZ - 1);
            strncpy(gb->i.plgend, plgend, PLRBUFSZ - 1);
            strncpy(gb->i.plalign, plalign, PLRBUFSZ - 1);
            strncpy(gb->i.game_state, game_state, COLNO - 1);
        }
    }
    json_decref(jmsg);

    api_exit();
    return gamebuf;
}


int
nhnet_play_game(int gid, enum nh_followmode followmode)
{
    int ret;
    json_t *jmsg;

    if (!nhnet_active())
        return nh_play_game(gid, followmode);

    if (!api_entry())
        return ERR_NETWORK_ERROR;

    xmalloc_cleanup(&xm_blocklist);

    jmsg = json_pack("{si,si}", "gameid", gid, "followmode", followmode);
    current_game = gid;
    jmsg = send_receive_msg("play_game", jmsg);
    current_game = 0;
    if (json_unpack(jmsg, "{si!}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_play_game");
        ret = ERR_NETWORK_ERROR;        /* we don't know the error actually
                                           was, any error code will do */
    }
    json_decref(jmsg);
    api_exit();
    return ret;
}


static json_t *
json_list(const struct nh_listitem *list, int len)
{
    int i;
    json_t *jarr = json_array();

    xmalloc_cleanup(&xm_blocklist);

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

    xmalloc_cleanup(&xm_blocklist);

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
        json_pack("{ss,ss,ss,si,so,so,si}", "name", option->name, "group",
                  option->group, "helptxt", option->helptxt,
                  "type", option->type, "value", joptval,
                  "desc", joptdesc, "birth", option->birth_option);
    return jopt;
}


enum nh_create_response
nhnet_create_game(struct nh_option_desc *opts_orig)
{
    json_t *jmsg, *jarr;
    int ret, i;
    struct nh_option_desc *volatile opts = opts_orig;

    if (!api_entry())
        return 0;

    xmalloc_cleanup(&xm_blocklist);

    jarr = json_array();
    for (i = 0; opts[i].name; i++)
        json_array_append_new(jarr, json_option(&opts[i]));

    jmsg = json_pack("{so}", "options", jarr);
    jmsg = send_receive_msg("create_game", jmsg);
    if (json_unpack(jmsg, "{si}", "return", &ret) == -1) {
        print_error("Incorrect return object in nhnet_create_game");
        ret = NHCREATE_FAIL;
    }

    json_decref(jmsg);
    api_exit();
    return ret;
}


struct nh_cmd_desc *
nhnet_get_commands(int *count)
{
    int i, defkey, altkey;
    json_t *jmsg, *jarr, *jobj;
    struct nh_cmd_desc *volatile cmdlist = NULL;
    const char *name, *desc;

    if (!nhnet_active())
        return nh_get_commands(count);

    if (!api_entry())
        return 0;

    xmalloc_cleanup(&xm_blocklist);

    jmsg = json_object();
    jmsg = send_receive_msg("get_commands", jmsg);
    if (json_unpack(jmsg, "{so!}", "cmdlist", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_commands");
    } else {
        *count = json_array_size(jarr);
        cmdlist = xmalloc(&xm_blocklist, *count * sizeof (struct nh_cmd_desc));

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
    struct nh_cmd_desc *volatile cmdlist = NULL;
    const char *name, *desc;

    if (!nhnet_active())
        return nh_get_object_commands(count, invlet);

    if (!api_entry())
        return 0;

    xmalloc_cleanup(&xm_blocklist);

    jmsg = json_pack("{si}", "invlet", invlet);
    jmsg = send_receive_msg("get_obj_commands", jmsg);
    if (json_unpack(jmsg, "{so!}", "cmdlist", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_object_commands");
    } else {
        *count = json_array_size(jarr);
        cmdlist = xmalloc(&xm_blocklist, *count * sizeof (struct nh_cmd_desc));

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
    arr = xmalloc(&xm_blocklist, size * sizeof (struct nh_symdef));

    for (i = 0; i < size; i++) {
        jobj = json_array_get(jarr, i);
        json_unpack(jobj, "[s,i,i!]", &symname, &ch, &arr[i].color);
        arr[i].ch = ch;
        str = xmalloc(&xm_blocklist, strlen(symname) + 1);
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

    xmalloc_cleanup(&xm_blocklist);

    jmsg = send_receive_msg("get_drawing_info", json_object());
    di = xmalloc(&xm_blocklist, sizeof (struct nh_drawing_info));
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

    xmalloc_cleanup(&xm_blocklist);

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
    const char *name, *helptxt, *strval, *group;
    int size, i;
    struct nh_autopickup_rule *r;

    memset(opt, 0, sizeof (struct nh_option_desc));
    if (json_unpack
        (jobj, "{ss,ss,ss,si,so,so,si!}", "name", &name, "group", &group,
         "helptxt", &helptxt, "type", &opt->type, "value", &joptval,
         "desc", &joptdesc, "birth", &opt->birth_option) == -1) {
        memset(opt, 0, sizeof (struct nh_option_desc));
        print_error("Broken option specification.");
        return;
    }
    opt->name = strdup(name);
    opt->helptxt = strdup(helptxt);
    opt->group = strdup(group);

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
            r->pattern[sizeof (r->pattern) - 1] = '\0';
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
    free((void *)opt->group);

    switch (opt->type) {
    case OPTTYPE_BOOL:
    case OPTTYPE_INT:
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < opt->e.numchoices; i++)
            free((void *)opt->e.choices[i].caption);
        free((void *)opt->e.choices);
        break;

    case OPTTYPE_STRING:
        if (opt->value.s)
            free(opt->value.s);
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        for (i = 0; i < opt->a.numclasses; i++)
            free((void *)opt->a.classes[i].caption);
        free((void *)opt->a.classes);

        if (opt->value.ar) {
            free(opt->value.ar->rules);
            free(opt->value.ar);
        }
        break;
    }
}


nh_bool
nhnet_set_option(const char *name, union nh_optvalue value)
{
    int ret, i;
    json_t *jmsg, *joval, *jobj;
    struct nh_option_desc *opts, *opt;
    struct nh_autopickup_rule *r;

    ret = nh_set_option(name, value);
    if (!nhnet_active())
        return ret;

    if (!api_entry())
        return FALSE;

    xmalloc_cleanup(&xm_blocklist);

    opts = nhnet_get_options();
    opt = NULL;
    for (i = 0; opts[i].name && !opt; i++)
        if (!strcmp(name, opts[i].name))
            opt = &opts[i];

    if (opt) {
        if (opt->type == OPTTYPE_STRING)
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
            /* Shouldn't happen; if it does, put an easily searchable string in 
               as the value to ease debugging */
            joval = json_string("<unused>");
        }

        jmsg =
            json_pack("{ss,so}", "name", name, "value", joval);
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
nhnet_get_options(void)
{
    struct nh_option_desc *olist;
    json_t *jmsg, *jarr, *jobj;
    int count, i;

    if (!nhnet_active())
        return nh_get_options();

    if (!api_entry()) {
        olist = malloc( sizeof (struct nh_option_desc));
        memset(olist, 0, sizeof (struct nh_option_desc));
        return olist;
    }

    jmsg = send_receive_msg("get_options", json_object());
    if (json_unpack(jmsg, "{so!}", "options", &jarr) == -1 ||
        !json_is_array(jarr)) {
        print_error("Incorrect return object in nhnet_get_options");
        olist = malloc(sizeof (struct nh_option_desc));
        memset(olist, 0, sizeof (struct nh_option_desc));
    } else {
        count = json_array_size(jarr);
        olist = malloc(sizeof (struct nh_option_desc) * (count + 1));
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
nhnet_describe_pos(int x, int y, struct nh_desc_buf *bufs, int *is_in)
{
    const char *bgdesc, *trapdesc, *objdesc, *mondesc, *invisdesc, *effectdesc;
    json_t *jmsg;
    int in;

    if (!nhnet_active()) {
        nh_describe_pos(x, y, bufs, is_in);
        return;
    }

    if (!api_entry())
        return;

    xmalloc_cleanup(&xm_blocklist);

    jmsg =
        send_receive_msg("describe_pos",
                         json_pack("{si,si,si}", "x", x, "y", y, "is_in",
                                   is_in != NULL));
    json_unpack(jmsg, "{ss,ss,ss,ss,ss,ss,si,sb,sb!}", "bgdesc", &bgdesc,
                "trapdesc", &trapdesc, "objdesc", &objdesc, "mondesc", &mondesc,
                "invisdesc", &invisdesc, "effectdesc", &effectdesc,
                "feature_described", &bufs->feature_described, "objcount",
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
    array = xmalloc(&xm_blocklist, sizeof (char *) * size);
    for (i = 0; i < size; i++) {
        jstr = json_array_get(jarr, i);
        if (!json_is_string(jstr)) {
            array[i] = NULL;
            continue;
        }
        str = json_string_value(jstr);
        buf = xmalloc(&xm_blocklist, strlen(str) + 1);
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

    xmalloc_cleanup(&xm_blocklist);

    jmsg = send_receive_msg("get_roles", json_object());
    ri = xmalloc(&xm_blocklist, sizeof (struct nh_roles_info));
    if (json_unpack
        (jmsg, "{si,si,si,si,so,so,so,so,so,so}", "num_roles",
         &ri->num_roles, "num_races", &ri->num_races, "num_genders",
         &ri->num_genders, "num_aligns", &ri->num_aligns, "rolenames_m",
         &jroles_m, "rolenames_f", &jroles_f, "racenames", &jraces, "gendnames",
         &jgenders, "alignnames", &jaligns, "matrix", &jmatrix) == -1 ||
        !json_is_array(jroles_m) || !json_is_array(jroles_f) ||
        !json_is_array(jraces) || !json_is_array(jgenders) ||
        !json_is_array(jaligns) || !json_is_array(jmatrix) ||
        json_array_size(jroles_m) != ri->num_roles ||
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
        matrix = xmalloc(&xm_blocklist, size * sizeof (nh_bool));
        for (i = 0; i < size; i++)
            matrix[i] = json_integer_value(json_array_get(jmatrix, i));
        ri->matrix = matrix;
    }

    json_decref(jmsg);
    api_exit();
    return ri;
}


char *
nhnet_build_plselection_prompt(char *const buf, int buflen, int rolenum,
                               int racenum, int gendnum, int alignnum)
{
    json_t *jmsg;
    char *str, *ret;

    if (!nhnet_active())
        return nh_build_plselection_prompt(buf, buflen, rolenum, racenum,
                                           gendnum, alignnum);

    if (!api_entry())
        return NULL;

    xmalloc_cleanup(&xm_blocklist);

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
nhnet_root_plselection_prompt(char *const buf, int buflen, int rolenum,
                              int racenum, int gendnum, int alignnum)
{
    json_t *jmsg;
    char *str, *ret;

    if (!nhnet_active())
        return nh_root_plselection_prompt(buf, buflen, rolenum, racenum,
                                          gendnum, alignnum);

    if (!api_entry())
        return NULL;

    xmalloc_cleanup(&xm_blocklist);

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
nhnet_get_topten(int *out_len, char *statusbuf, const char *volatile player,
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

    xmalloc_cleanup(&xm_blocklist);

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
        ttlist = xmalloc(&xm_blocklist,
                         (len + 1) * sizeof (struct nh_topten_entry));
        memset(ttlist, 0, (len + 1) * sizeof (struct nh_topten_entry));
        for (i = 0; i < len; i++) {
            jobj = json_array_get(jarr, i);
            json_unpack(jobj,
                        "{si,si,si,si,si,si,si,si,si,si,si,si,si,ss,ss,ss,ss,"
                        "ss,ss,ss,si!}",
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

    xmalloc_cleanup(&xm_blocklist);

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

    xmalloc_cleanup(&xm_blocklist);

    jmsg = json_pack("{ss}", "password", password);
    jmsg = send_receive_msg("set_password", jmsg);
    if (json_unpack(jmsg, "{si}", "return", &ret) == -1)
        return FALSE;

    if (ret)
        strncpy(saved_password, password, 199);
    return ret;
}

/* clientapi.c */
