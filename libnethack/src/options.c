/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2013-12-22 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "common_options.h"
#include <ctype.h>

#define WINTYPELEN 16

static int change_inv_order(char *op);

/* -------------------------------------------------------------------------- */

#define listlen(list) (sizeof(list)/sizeof(list[0]))

static const struct nh_listitem disclose_list[] = {
    {DISCLOSE_NO_WITHOUT_PROMPT, "no"},
    {DISCLOSE_PROMPT_DEFAULT_NO, "ask, default no"},
    {DISCLOSE_PROMPT_DEFAULT_YES, "ask, default yes"},
    {DISCLOSE_YES_WITHOUT_PROMPT, "yes"}
};
static const struct nh_enum_option disclose_spec =
    { disclose_list, listlen(disclose_list) };

static const struct nh_listitem menustyle_list[] = {
    {MENU_PARTIAL, "partial"},
    {MENU_FULL, "full"}
};
static const struct nh_enum_option menustyle_spec =
    { menustyle_list, listlen(menustyle_list) };

static const struct nh_listitem pickup_burden_list[] = {
    {UNENCUMBERED, "unencumbered"},
    {SLT_ENCUMBER, "burdened"},
    {MOD_ENCUMBER, "stressed"},
    {HVY_ENCUMBER, "strained"},
    {EXT_ENCUMBER, "overtaxed"},
    {OVERLOADED, "overloaded"}
};
static const struct nh_enum_option pickup_burden_spec =
    { pickup_burden_list, listlen(pickup_burden_list) };

static const struct nh_listitem runmode_list[] = {
    {RUN_CRAWL, "crawl"},
    {RUN_STEP, "step"},
    {RUN_LEAP, "leap"},
    {RUN_TPORT, "teleport"}
};
static const struct nh_enum_option runmode_spec =
    { runmode_list, listlen(runmode_list) };

static const struct nh_listitem mode_list[] = {
    {MODE_NORMAL, "normal"},
    {MODE_EXPLORE, "explore"},
    {MODE_WIZARD, "debug"}
};
static const struct nh_enum_option mode_spec =
    { mode_list, listlen(mode_list) };

static const struct nh_listitem align_list[] = {
    {0, "lawful"},
    {1, "neutral"},
    {2, "chaotic"},
    {ROLE_NONE, "ask"},
    {ROLE_RANDOM, "random"}
};
static const struct nh_enum_option align_spec =
    { align_list, listlen(align_list) };

static const struct nh_listitem gender_list[] = {
    {0, "male"},
    {1, "female"},
    {ROLE_NONE, "ask"},
    {ROLE_RANDOM, "random"}
};
static const struct nh_enum_option gender_spec =
    { gender_list, listlen(gender_list) };

static struct nh_enum_option race_spec = { NULL, 0 };
static struct nh_enum_option role_spec = { NULL, 0 };

static const struct nh_listitem pettype_list[] = {
    {'c', "cat"},
    {'d', "dog"},
    {'h', "horse"},
    {'n', "no pet"},
    {0, "random"}
};
static const struct nh_enum_option pettype_spec =
    { pettype_list, listlen(pettype_list) };

static const struct nh_listitem ap_object_class_list[] = {
    {OCLASS_ANY, "any"},
    {GOLD_SYM, "gold"},
    {AMULET_SYM, "amulets"},
    {WEAPON_SYM, "weapons"},
    {ARMOR_SYM, "armor"},
    {RING_SYM, "rings"},
    {TOOL_SYM, "tools"},
    {FOOD_SYM, "food"},
    {POTION_SYM, "potions"},
    {SCROLL_SYM, "scrolls"},
    {SPBOOK_SYM, "spellbooks"},
    {WAND_SYM, "wands"},
    {GEM_SYM, "gems"},
    {ROCK_SYM, "large stones"},
    {BALL_SYM, "iron balls"},
    {CHAIN_SYM, "chains"}
};
static const struct nh_autopick_option autopickup_spec =
    { ap_object_class_list, listlen(ap_object_class_list) };


static struct nh_autopickup_rule def_ap_ruleset[] = {
    {"*zorkmid*", OCLASS_ANY, B_DONT_CARE, AP_LEAVE},
    {"", GOLD_SYM, B_DONT_CARE, AP_GRAB},
    {"", SCROLL_SYM, B_DONT_CARE, AP_GRAB},
    {"", RING_SYM, B_DONT_CARE, AP_GRAB},
    {"", WAND_SYM, B_DONT_CARE, AP_GRAB},
    {"", POTION_SYM, B_DONT_CARE, AP_GRAB},
};
static struct nh_autopickup_rules def_autopickup =
    { def_ap_ruleset, SIZE(def_ap_ruleset) };

static const struct nh_option_desc const_options[] = {
    {"autodig", "dig if moving and wielding digging tool", FALSE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"autodigdown", "autodig downwards tries to create a pit or hole", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"autopickup", "automatically pick up objects you move over", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"autopickup_rules",
     "rules to decide what to autopickup if autopickup is on", FALSE,
     OPTTYPE_AUTOPICKUP_RULES, {.ar = &def_autopickup}},
    {"autoquiver",
     "when firing with an empty quiver, select something suitable", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"comment", "has no effect", FALSE, OPTTYPE_STRING, {.s = ""}},
    {"confirm", "ask before hitting tame or peaceful monsters", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"disclose", "whether to disclose information at end of game", FALSE,
     OPTTYPE_ENUM, {.e = DISCLOSE_PROMPT_DEFAULT_YES}},
    {"fruit", "the name of a fruit you enjoy eating", FALSE, OPTTYPE_STRING,
     {"slime mold"}},
    {"lit_corridor", "show a dark corridor as lit if in sight", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"menustyle", "user interface for object selection", FALSE, OPTTYPE_ENUM,
     {.e = MENU_FULL}},
    {"packorder", "the inventory order of the items in your pack", FALSE,
     OPTTYPE_STRING, {.s = "$\")[%?+!=/(*`0_"}},
    {"pickup_burden", "maximum burden picked up before prompt", FALSE,
     OPTTYPE_ENUM, {.e = MOD_ENCUMBER}},
    {"pickup_thrown", "autopickup items you threw or fired", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"prayconfirm", "use confirmation prompt when #pray command issued", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"pushweapon", "offhand the old weapon when wielding a new one", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"runmode", "display frequency when `running' or `travelling'", FALSE,
     OPTTYPE_ENUM, {.e = RUN_LEAP}},
    {"safe_pet", "prevent you from (knowingly) attacking your pet(s)", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"show_uncursed", "always show uncursed status", FALSE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"showrace", "show yourself by your race rather than by role", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"sortpack", "group similar kinds of objects in inventory", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"sparkle", "display sparkly effect for resisted magical attacks",
     OPTTYPE_BOOL, FALSE, {.b = TRUE}},
    {"tombstone", "print tombstone when you die", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"travel_interrupt", "interrupt travel (_) when a hostile is in sight",
     FALSE, OPTTYPE_BOOL, {.b = TRUE}},
    {"verbose", "print more commentary during the game", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},

    {"name", "character name", TRUE, OPTTYPE_STRING, {.s = NULL}},
    {"mode", "game mode", TRUE, OPTTYPE_ENUM, {.e = MODE_NORMAL}},
    {"elbereth", "difficulty: the E-word repels monsters", TRUE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"reincarnation", "Special Rogue-like levels", TRUE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"seduction", "certain monsters may seduce you", TRUE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"bones", "allow bones levels", TRUE, OPTTYPE_BOOL, {.b = TRUE}},
    {"permablind", "spend the whole game blind", TRUE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"permahallu", "spend the whole game hallucinating", TRUE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"legacy", "print introductory message", TRUE, OPTTYPE_BOOL, {.b = TRUE}},
    {"align", "your starting alignment", TRUE, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"gender", "your starting gender", TRUE, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"race", "your starting race", TRUE, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"role", "your starting role", TRUE, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"catname", "the name of your (first) cat", TRUE, OPTTYPE_STRING,
     {.s = NULL}},
    {"dogname", "the name of your (first) dog", TRUE, OPTTYPE_STRING,
     {.s = NULL}},
    {"horsename", "the name of your (first) horse", TRUE, OPTTYPE_STRING,
     {.s = NULL}},
    {"pettype", "your preferred initial pet type", TRUE, OPTTYPE_ENUM,
     {.e = 0}},

    {NULL, NULL, FALSE, OPTTYPE_BOOL, {.s = NULL}}
};


/* associate boolean options with variables directly */
static const struct nhlib_boolopt_map boolopt_map[] = {
    {"autodig", &flags.autodig},
    {"autodigdown", &flags.autodigdown},
    {"autopickup", &flags.pickup},
    {"autoquiver", &flags.autoquiver},
    {"confirm", &flags.confirm},
    {"legacy", &flags.legacy},
    {"lit_corridor", &flags.lit_corridor},
    {"pickup_thrown", &flags.pickup_thrown},
    {"prayconfirm", &flags.prayconfirm},
    {"pushweapon", &flags.pushweapon},
    {"safe_pet", &flags.safe_dog},
    {"show_uncursed", &flags.show_uncursed},
    {"showrace", &flags.showrace},
    {"sortpack", &flags.sortpack},
    {"sparkle", &flags.sparkle},
    {"travel_interrupt", &flags.travel_interrupt},
    {"tombstone", &flags.tombstone},
    {"verbose", &flags.verbose},

    /* birth options */
    {"elbereth", &flags.elbereth_enabled},
    {"reincarnation", &flags.rogue_enabled},
    {"seduction", &flags.seduce_enabled},
    {"bones", &flags.bones_enabled},
    {"permablind", &flags.permablind},
    {"permahallu", &flags.permahallu},
    {NULL, NULL}
};


static const char def_inv_order[MAXOCLASSES] = {
    COIN_CLASS, AMULET_CLASS, WEAPON_CLASS, ARMOR_CLASS, FOOD_CLASS,
    SCROLL_CLASS, SPBOOK_CLASS, POTION_CLASS, RING_CLASS, WAND_CLASS,
    TOOL_CLASS, GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, 0,
};


/* most environment variables will eventually be printed in an error
 * message if they don't work, and most error message paths go through
 * BUFSZ buffers, which could be overflowed by a maliciously long
 * environment variable.  if a variable can legitimately be long, or
 * if it's put in a smaller buffer, the responsible code will have to
 * bounds-check itself.
 */
char *
nh_getenv(const char *ev)
{
    char *getev = getenv(ev);

    if (getev && strlen(getev) <= (BUFSZ / 2))
        return getev;
    else
        return NULL;
}


static void
build_role_spec(void)
{
    int i;
    struct nh_listitem *choices;

    /* build list of roles */
    for (i = 0; roles[i].name.m || roles[i].name.f; i++) ; /* just count em */
    role_spec.numchoices = i + 2;
    choices = malloc((i + 2) * sizeof (struct nh_listitem));
    for (i = 0; roles[i].name.m || roles[i].name.f; i++) {
        choices[i].id = i;
        if (roles[i].name.m)
            choices[i].caption = (char *)roles[i].name.m;
        else
            choices[i].caption = (char *)roles[i].name.f;
    }
    choices[i].id = ROLE_NONE;
    choices[i].caption = "ask";
    choices[i + 1].id = ROLE_RANDOM;
    choices[i + 1].caption = "random";

    role_spec.choices = choices;
}

static void
build_race_spec(void)
{
    int i;
    struct nh_listitem *choices;

    /* build list of races */
    for (i = 0; races[i].noun; i++) ;   /* just count em */
    race_spec.numchoices = i + 2;
    choices = malloc((i + 2) * sizeof (struct nh_listitem));
    for (i = 0; races[i].noun; i++) {
        choices[i].id = i;
        choices[i].caption = (char *)races[i].noun;
    }
    choices[i].id = ROLE_NONE;
    choices[i].caption = "ask";
    choices[i + 1].id = ROLE_RANDOM;
    choices[i + 1].caption = "random";

    race_spec.choices = choices;
}


void
init_opt_struct(void)
{
    if (options)
        nhlib_free_optlist(options);

    options = nhlib_clone_optlist(const_options);

    build_role_spec();
    build_race_spec();

    /* initialize option definitions */
    nhlib_find_option(options, "comment")->s.maxlen = BUFSZ;
    nhlib_find_option(options, "disclose")->e = disclose_spec;
    nhlib_find_option(options, "fruit")->s.maxlen = PL_FSIZ;
    nhlib_find_option(options, "menustyle")->e = menustyle_spec;
    nhlib_find_option(options, "pickup_burden")->e = pickup_burden_spec;
    nhlib_find_option(options, "packorder")->s.maxlen = MAXOCLASSES;
    nhlib_find_option(options, "runmode")->e = runmode_spec;
    nhlib_find_option(options, "autopickup_rules")->a = autopickup_spec;

    nhlib_find_option(options, "name")->s.maxlen = PL_PSIZ;
    nhlib_find_option(options, "mode")->e = mode_spec;
    nhlib_find_option(options, "align")->e = align_spec;
    nhlib_find_option(options, "gender")->e = gender_spec;
    nhlib_find_option(options, "role")->e = role_spec;
    nhlib_find_option(options, "race")->e = race_spec;
    nhlib_find_option(options, "pettype")->e = pettype_spec;
    nhlib_find_option(options, "catname")->s.maxlen = PL_PSIZ;
    nhlib_find_option(options, "dogname")->s.maxlen = PL_PSIZ;
    nhlib_find_option(options, "horsename")->s.maxlen = PL_PSIZ;

    /* If no config file exists, these values will not get set until they have
       already been used during game startup.  (-1) is a much better default,
       as 0 will always cause a lawful male Archologist to be created */
    u.initalign = u.initgend = u.initrace = u.initrole = -1;
}


void
cleanup_opt_struct(void)
{
    free((void *)role_spec.choices);
    role_spec.choices = NULL;
    free((void *)race_spec.choices);
    race_spec.choices = NULL;
    nhlib_free_optlist(options);
    options = NULL;
}


void
initoptions(void)
{
    int i;

    flags.mon_generation = 1;

    iflags.travelcc.x = iflags.travelcc.y = -1;
    flags.warnlevel = 1;

    /* init flags.inv_order this way, as setting it via the option requires a
       preexisting order */
    memcpy(flags.inv_order, def_inv_order, sizeof flags.inv_order);

    fruitadd(obj_descr[SLIME_MOLD].oc_name);
    strncpy(pl_fruit, obj_descr[SLIME_MOLD].oc_name, PL_FSIZ);

    for (i = 0; options[i].name; i++)
        nh_set_option(options[i].name, options[i].value, FALSE);
}


static boolean
set_option(const char *name, union nh_optvalue value, boolean isstring)
{
    struct nh_option_desc *option = NULL;

    /* if this option change affects game options and happens during a 
       replay (program_state.viewing) and the change isn't triggered by the
       replay (!program_state.restoring) */
    if (program_state.viewing && !program_state.restoring)
        return FALSE;   /* Nope, sorry. That would mess up the replay */

    if (options)
        option = nhlib_find_option(options, name);

    if (!option || (option->birth_option && program_state.game_running))
        return FALSE;

    if (isstring)
        value = nhlib_string_to_optvalue(option, value.s);

    if (!nhlib_option_value_ok(option, value))
        return FALSE;

    if (nhlib_copy_option_value(option, value))
        log_option(option);

    /* We may have allocated a new copy of the autopickup rules. */
    if (isstring && option->type == OPTTYPE_AUTOPICKUP_RULES) {
        free(value.ar->rules);
        free(value.ar);
    }

    if (option->type == OPTTYPE_BOOL) {
        boolean *bvar = nhlib_find_boolopt(boolopt_map, option->name);

        if (!bvar) {
            impossible("no boolean for option '%s'", option->name);
            return FALSE;
        }
        *bvar = option->value.b;
        return TRUE;
    } else if (!strcmp("comment", option->name)) {
        /* do nothing */
    } else if (!strcmp("disclose", option->name)) {
        flags.end_disclose = option->value.e;
    } else if (!strcmp("fruit", option->name)) {
        strncpy(pl_fruit, option->value.s, PL_FSIZ);
        if (objects)    /* don't do fruitadd before the game is running */
            fruitadd(pl_fruit);
    } else if (!strcmp("menustyle", option->name)) {
        flags.menu_style = option->value.e;
    } else if (!strcmp("packorder", option->name)) {
        if (!change_inv_order(option->value.s))
            return FALSE;
    } else if (!strcmp("pickup_burden", option->name)) {
        flags.pickup_burden = option->value.e;
    } else if (!strcmp("runmode", option->name)) {
        flags.runmode = option->value.e;
    } else if (!strcmp("autopickup_rules", option->name)) {
        if (flags.ap_rules) {
            free(flags.ap_rules->rules);
            free(flags.ap_rules);
            flags.ap_rules = NULL;
        }
        flags.ap_rules = nhlib_copy_autopickup_rules(option->value.ar);
    }
    /* birth options */
    else if (!strcmp("mode", option->name)) {
        flags.debug = (option->value.e == MODE_WIZARD);
        flags.explore = (option->value.e == MODE_EXPLORE);
    } else if (!strcmp("align", option->name)) {
        u.initalign = option->value.e;
    } else if (!strcmp("gender", option->name)) {
        u.initgend = option->value.e;
    } else if (!strcmp("race", option->name)) {
        u.initrace = option->value.e;
    } else if (!strcmp("role", option->name)) {
        u.initrole = option->value.e;
    }

    else if (!strcmp("name", option->name)) {
        strncpy(plname, option->value.s, PL_PSIZ);
    } else if (!strcmp("catname", option->name)) {
        strncpy(catname, option->value.s, PL_PSIZ);
    } else if (!strcmp("dogname", option->name)) {
        strncpy(dogname, option->value.s, PL_PSIZ);
    } else if (!strcmp("horsename", option->name)) {
        strncpy(horsename, option->value.s, PL_PSIZ);
    } else if (!strcmp("pettype", option->name)) {
        preferred_pet = (char)option->value.e;
    }

    else
        /* unrecognized option */
        return FALSE;

    /* assume that any recognized option has been handled. */
    return TRUE;
}


boolean
nh_set_option(const char *name, union nh_optvalue value, boolean isstring)
{
    boolean rv;

    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(FALSE);

    rv = set_option(name, value, isstring);

    API_EXIT();
    return rv;
}


struct nh_option_desc *
nh_get_options(void)
{
    return options;
}


const char *
nh_get_option_string(const struct nh_option_desc *option)
{
    char valbuf[10], *outstr;
    char *valstr = NULL;
    int i;
    boolean freestr = FALSE;

    switch (option->type) {
    case OPTTYPE_BOOL:
        valstr = option->value.b ? "true" : "false";
        break;

    case OPTTYPE_ENUM:
        valstr = "(invalid)";
        for (i = 0; i < option->e.numchoices; i++)
            if (option->value.e == option->e.choices[i].id)
                valstr = option->e.choices[i].caption;
        break;

    case OPTTYPE_INT:
        sprintf(valbuf, "%d", option->value.i);
        valstr = valbuf;
        break;

    case OPTTYPE_STRING:
        if (!option->value.s)
            valstr = "";
        else
            valstr = option->value.s;
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        freestr = TRUE;
        valstr = autopickup_to_string(option->value.ar);
        break;

    default:   /* custom option type defined by the client? */
        return NULL;
    }

    /* copy the string to xmalloced memory so that we can forget about the
       pointer here */
    outstr = xmalloc(strlen(valstr) + 1);
    strcpy(outstr, valstr);
    if (freestr)
        free(valstr);
    return outstr;
}


/* Returns the fid of the fruit type; if that type already exists, it
 * returns the fid of that one; if it does not exist, it adds a new fruit
 * type to the chain and returns the new one.
 */
int
fruitadd(const char *str)
{
    int i;
    struct fruit *f;
    struct fruit *lastf = 0;
    int highest_fruit_id = 0;
    char buf[PL_FSIZ], *c;
    boolean user_specified = (str == pl_fruit);

    /* if not user-specified, then it's a fruit name for a fruit on a bones
       level... */

    /* Note: every fruit has an id (spe for fruit objects) of at least 1; 0 is
       an error. */
    if (user_specified) {
        /* disallow naming after other foods (since it'd be impossible to tell
           the difference) */

        boolean found = FALSE, numeric = FALSE;

        for (i = bases[FOOD_CLASS]; objects[i].oc_class == FOOD_CLASS; i++) {
            if (i != SLIME_MOLD && !strcmp(OBJ_NAME(objects[i]), pl_fruit)) {
                found = TRUE;
                break;
            }
        }
        for (c = pl_fruit; *c >= '0' && *c <= '9'; c++) ;
        if (isspace(*c) || *c == 0)
            numeric = TRUE;

        if (found || numeric || !strncmp(str, "cursed ", 7) ||
            !strncmp(str, "uncursed ", 9) || !strncmp(str, "blessed ", 8) ||
            !strncmp(str, "partly eaten ", 13) ||
            (!strncmp(str, "tin of ", 7) &&
             (!strcmp(str + 7, "spinach") || name_to_mon(str + 7) >= LOW_PM)) ||
            !strcmp(str, "empty tin") ||
            ((!strncmp(str + strlen(str) - 7, " corpse", 7) ||
              !strncmp(str + strlen(str) - 4, " egg", 4)) &&
             name_to_mon(str) >= LOW_PM)) {
            strcpy(buf, pl_fruit);
            strcpy(pl_fruit, "candied ");
            strncat(pl_fruit + 8, buf, PL_FSIZ - 8 - 1);
        }
    }
    for (f = ffruit; f; f = f->nextf) {
        lastf = f;
        if (f->fid > highest_fruit_id)
            highest_fruit_id = f->fid;
        if (!strncmp(str, f->fname, PL_FSIZ))
            goto nonew;
    }
    /* if adding another fruit would overflow spe, use a random fruit
       instead... we've got a lot to choose from. */
    if (highest_fruit_id >= 127)
        return rnd(127);
    highest_fruit_id++;
    f = newfruit();
    memset(f, 0, sizeof (struct fruit));
    if (ffruit)
        lastf->nextf = f;
    else
        ffruit = f;
    strcpy(f->fname, str);
    f->fid = highest_fruit_id;
    f->nextf = 0;
nonew:
    if (user_specified)
        current_fruit = highest_fruit_id;
    return f->fid;
}


static int
change_inv_order(char *op)
{
    int oc_sym, num;
    char *sp, buf[BUFSZ];

    num = 0;

    for (sp = op; *sp; sp++) {
        oc_sym = def_char_to_objclass(*sp);
        /* reject bad or duplicate entries */
        if (oc_sym == MAXOCLASSES || oc_sym == RANDOM_CLASS ||
            oc_sym == ILLOBJ_CLASS || !strchr(flags.inv_order, oc_sym) ||
            strchr(sp + 1, *sp))
            return 0;
        /* retain good ones */
        buf[num++] = (char)oc_sym;
    }
    buf[num] = '\0';

    /* fill in any omitted classes, using previous ordering */
    for (sp = flags.inv_order; *sp; sp++)
        if (!strchr(buf, *sp)) {
            buf[num++] = *sp;
            buf[num] = '\0';    /* explicitly terminate for next strchr() */
        }

    strcpy(flags.inv_order, buf);
    return 1;
}


char *
autopickup_to_string(const struct nh_autopickup_rules *ar)
{
    int size, i;
    char *buf, *bp, pattern[40];

    if (!ar || !ar->num_rules) {
        buf = strdup("");
        return buf;
    }

    /* at this point, size is an upper bound on the stringified length of ar 3
       stringified small numbers + a pattern with up to 40 chars < 64 chars */
    size = 64 * ar->num_rules;
    buf = malloc(size);
    buf[0] = '\0';

    for (i = 0; i < ar->num_rules; i++) {
        strncpy(pattern, ar->rules[i].pattern, sizeof (pattern));

        /* remove '"' and ';' from the pattern by replacing them by '?' (single 
           character wildcard), to simplify parsing */
        bp = pattern;
        while (*bp) {
            if (*bp == '"' || *bp == ';')
                *bp = '?';
            bp++;
        }

        snprintf(eos(buf), 64, "(\"%s\",%d,%u,%u);", pattern,
                 ar->rules[i].oclass, ar->rules[i].buc, ar->rules[i].action);
    }

    return buf;
}

/*options.c*/
