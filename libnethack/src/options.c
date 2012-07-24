/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include <ctype.h>

#define WINTYPELEN 16

static int change_inv_order(char *op);
static struct nh_autopickup_rules *copy_autopickup_rules(
  const struct nh_autopickup_rules *in);

/* -------------------------------------------------------------------------- */

/* output array for parse_autopickup_rules. */
static struct nh_autopickup_rule ap_rules_array[AUTOPICKUP_MAX_RULES];
static struct nh_autopickup_rules ap_rules_static;

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
static const struct nh_autopickup_rules def_autopickup =
    { def_ap_ruleset, SIZE(def_ap_ruleset) };


#define VTRUE (void*)TRUE
#define VFALSE (void*)FALSE

static const struct nh_option_desc const_options[] = {
    {"autodig", "dig if moving and wielding digging tool", OPTTYPE_BOOL,
     {VFALSE}},
    {"autopickup", "automatically pick up objects you move over", OPTTYPE_BOOL,
     {VTRUE}},
    {"autopickup_rules",
     "rules to decide what to autopickup if autopickup is on",
     OPTTYPE_AUTOPICKUP_RULES, {(void *)&def_autopickup}},
    {"autoquiver",
     "when firing with an empty quiver, select something suitable",
     OPTTYPE_BOOL, {VFALSE}},
    {"comment", "has no effect", OPTTYPE_STRING, {""}},
    {"confirm", "ask before hitting tame or peaceful monsters", OPTTYPE_BOOL,
     {VTRUE}},
    {"disclose", "whether to disclose information at end of game", OPTTYPE_ENUM,
     {(void *)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"fruit", "the name of a fruit you enjoy eating", OPTTYPE_STRING,
     {"slime mold"}},
    {"lit_corridor", "show a dark corridor as lit if in sight", OPTTYPE_BOOL,
     {VFALSE}},
    {"menustyle", "user interface for object selection", OPTTYPE_ENUM,
     {(void *)MENU_FULL}},
    {"packorder", "the inventory order of the items in your pack",
     OPTTYPE_STRING, {"$\")[%?+!=/(*`0_"}},
    {"pickup_burden", "maximum burden picked up before prompt", OPTTYPE_ENUM,
     {(void *)MOD_ENCUMBER}},
    {"pickup_thrown", "autopickup items you threw or fired", OPTTYPE_BOOL,
     {VTRUE}},
    {"prayconfirm", "use confirmation prompt when #pray command issued",
     OPTTYPE_BOOL, {VTRUE}},
    {"pushweapon",
     "when wielding a new weapon, put your previous weapon into the secondary weapon slot",
     OPTTYPE_BOOL, {VFALSE}},
    {"runmode", "display frequency when `running' or `travelling'",
     OPTTYPE_ENUM, {(void *)RUN_LEAP}},
    {"safe_pet", "prevent you from (knowingly) attacking your pet(s)",
     OPTTYPE_BOOL, {VTRUE}},
    {"show_uncursed", "always show uncursed status", OPTTYPE_BOOL, {VFALSE}},
    {"showrace", "show yourself by your race rather than by role", OPTTYPE_BOOL,
     {VFALSE}},
    {"sortpack", "group similar kinds of objects in inventory", OPTTYPE_BOOL,
     {VTRUE}},
    {"sparkle", "display sparkly effect for resisted magical attacks",
     OPTTYPE_BOOL, {VTRUE}},
    {"tombstone", "print tombstone when you die", OPTTYPE_BOOL, {VTRUE}},
    {"travel_interrupt", "interrupt travel (_) when a hostile is in sight",
     OPTTYPE_BOOL, {VTRUE}},
    {"verbose", "print more commentary during the game", OPTTYPE_BOOL, {VTRUE}},

    {NULL, NULL, OPTTYPE_BOOL, {NULL}}
};


static const struct nh_option_desc const_birth_options[] = {
    {"elbereth", "difficulty: the E-word repels monsters", OPTTYPE_BOOL,
     {VTRUE}},
    {"reincarnation", "Special Rogue-like levels", OPTTYPE_BOOL, {VTRUE}},
    {"seduction", "certain monsters may seduce you", OPTTYPE_BOOL, {VTRUE}},
    {"bones", "allow bones levels", OPTTYPE_BOOL, {VTRUE}},
    {"permablind", "spend the whole game blind", OPTTYPE_BOOL, {FALSE}},
    {"permahallu", "spend the whole game hallucinating", OPTTYPE_BOOL, {FALSE}},
    {"legacy", "print introductory message", OPTTYPE_BOOL, {VTRUE}},
    {"align", "your starting alignment", OPTTYPE_ENUM, {(void *)ROLE_NONE}},
    {"gender", "your starting gender", OPTTYPE_ENUM, {(void *)ROLE_NONE}},
    {"race", "your starting race", OPTTYPE_ENUM, {(void *)ROLE_NONE}},
    {"role", "your starting role", OPTTYPE_ENUM, {(void *)ROLE_NONE}},
    {"catname", "the name of your (first) cat", OPTTYPE_STRING, {NULL}},
    {"dogname", "the name of your (first) dog", OPTTYPE_STRING, {NULL}},
    {"horsename", "the name of your (first) horse", OPTTYPE_STRING, {NULL}},
    {"pettype", "your preferred initial pet type", OPTTYPE_ENUM, {0}},

    {NULL, NULL, OPTTYPE_BOOL, {NULL}}
};


/* associate boolean options with variables directly */
static const struct nh_boolopt_map boolopt_map[] = {
    {"autodig", &flags.autodig},
    {"autopickup", &flags.pickup},
    {"autoquiver", &flags.autoquiver},
    {"confirm", &flags.confirm},
    {"female", &flags.female},
    {"legacy", &flags.legacy},
    {"lit_corridor", &flags.lit_corridor},
    {"pickup_thrown", &iflags.pickup_thrown},
    {"prayconfirm", &flags.prayconfirm},
    {"pushweapon", &flags.pushweapon},
    {"safe_pet", &flags.safe_dog},
    {"show_uncursed", &iflags.show_uncursed},
    {"showrace", &iflags.showrace},
    {"sortpack", &flags.sortpack},
    {"sparkle", &flags.sparkle},
    {"travel_interrupt", &iflags.travel_interrupt},
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


struct nh_option_desc *ui_options;
struct nh_boolopt_map *ui_boolopt_map;

boolean(*ui_option_callback) (struct nh_option_desc *);

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


static struct nh_option_desc *
find_option(struct nh_option_desc *optlist, const char *name)
{
    int i;

    for (i = 0; optlist[i].name; i++)
        if (!strcmp(name, optlist[i].name))
            return &optlist[i];

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
    options = clone_optlist(const_options);
    birth_options = clone_optlist(const_birth_options);

    build_role_spec();
    build_race_spec();

    /* initialize option definitions */
    find_option(options, "comment")->s.maxlen = BUFSZ;
    find_option(options, "disclose")->e = disclose_spec;
    find_option(options, "fruit")->s.maxlen = PL_FSIZ;
    find_option(options, "menustyle")->e = menustyle_spec;
    find_option(options, "pickup_burden")->e = pickup_burden_spec;
    find_option(options, "packorder")->s.maxlen = MAXOCLASSES;
    find_option(options, "runmode")->e = runmode_spec;
    find_option(options, "autopickup_rules")->a = autopickup_spec;

    find_option(birth_options, "align")->e = align_spec;
    find_option(birth_options, "gender")->e = gender_spec;
    find_option(birth_options, "role")->e = role_spec;
    find_option(birth_options, "race")->e = race_spec;
    find_option(birth_options, "pettype")->e = pettype_spec;
    find_option(birth_options, "catname")->s.maxlen = PL_PSIZ;
    find_option(birth_options, "dogname")->s.maxlen = PL_PSIZ;
    find_option(birth_options, "horsename")->s.maxlen = PL_PSIZ;

    /* If no config file exists, these values will not get set until they have
       already been used during game startup.  (-1) is a much better default,
       as 0 will always cause a lawful male Archologist to be created */
    flags.init_align = flags.init_gend = flags.init_race = flags.init_role = -1;
}


void
cleanup_opt_struct(void)
{
    free((void *)role_spec.choices);
    role_spec.choices = NULL;
    free((void *)race_spec.choices);
    race_spec.choices = NULL;
    free_optlist(options);
    options = NULL;
    free_optlist(birth_options);
    birth_options = NULL;
}


void
initoptions(void)
{
    int i;

    iflags.travelcc.x = iflags.travelcc.y = -1;
    flags.warnlevel = 1;
    flags.warntype = 0L;

    /* init flags.inv_order this way, as setting it via the option requires a
       preexisting order */
    memcpy(flags.inv_order, def_inv_order, sizeof flags.inv_order);

    fruitadd(obj_descr[SLIME_MOLD].oc_name);
    strncpy(pl_fruit, obj_descr[SLIME_MOLD].oc_name, PL_FSIZ);

    /* init from option definitions */
    for (i = 0; birth_options[i].name; i++)
        nh_set_option(birth_options[i].name, birth_options[i].value, FALSE);

    for (i = 0; options[i].name; i++)
        nh_set_option(options[i].name, options[i].value, FALSE);

    if (!active_birth_options)
        /* at this point the user may no longer change their birth options.
           active_birth_options will recieve birth option changes made during
           log replay, so that we can show the user what birth options the
           loaded game was started with */
        active_birth_options = clone_optlist(birth_options);
    else
        /* the switch to alternate birth options has already happened, so make
           sure those settings are active instead. */
        for (i = 0; active_birth_options[i].name; i++)
            nh_set_option(active_birth_options[i].name,
                          active_birth_options[i].value, FALSE);
}


static boolean
option_value_ok(struct nh_option_desc *option, union nh_optvalue value)
{
    int i;

    switch (option->type) {
    case OPTTYPE_BOOL:
        if (value.b == ! !value.b)
            return TRUE;
        break;

    case OPTTYPE_INT:
        if (value.i >= option->i.min && value.i <= option->i.max)
            return TRUE;
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < option->e.numchoices; i++)
            if (value.e == option->e.choices[i].id)
                return TRUE;
        break;

    case OPTTYPE_STRING:
        if (!value.s)
            break;

        if (strlen(value.s) > option->s.maxlen)
            break;

        if (!*value.s)
            value.s = NULL;

        return TRUE;

    case OPTTYPE_AUTOPICKUP_RULES:
        if (value.ar && value.ar->num_rules > AUTOPICKUP_MAX_RULES)
            break;
        return TRUE;
    }

    return FALSE;
}


static union nh_optvalue
string_to_optvalue(struct nh_option_desc *option, char *str)
{
    union nh_optvalue value;
    int i;

    value.i = -99999;

    switch (option->type) {
    case OPTTYPE_BOOL:
        if (!strcmp(str, "TRUE") || !strcmp(str, "true") || !strcmp(str, "1"))
            value.b = TRUE;
        else if (!strcmp(str, "FALSE") || !strcmp(str, "false") ||
                 !strcmp(str, "0"))
            value.b = FALSE;
        else
            value.i = 2;        /* intentionally invalid */

        break;

    case OPTTYPE_INT:
        sscanf(str, "%d", &value.i);
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < option->e.numchoices; i++)
            if (!strcmp(str, option->e.choices[i].caption))
                value.e = option->e.choices[i].id;
        break;

    case OPTTYPE_STRING:
        if (*str)
            value.s = str;
        else
            value.s = NULL;
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        value.ar = parse_autopickup_rules(str);
        break;
    }

    return value;
}


/* copy values carefully: copying pointers to strings on the stack is not good
 * return TRUE if the new value differs from the current value */
static boolean
copy_option_value(struct nh_option_desc *option, union nh_optvalue value)
{
    struct nh_autopickup_rules *aold, *anew;
    int i;

    switch (option->type) {
    case OPTTYPE_STRING:
        if (option->value.s == value.s ||
            (option->value.s && value.s && !strcmp(option->value.s, value.s)))
            return FALSE;       /* setting the option to it's current value;
                                   nothing to copy */

        if (option->value.s)
            free(option->value.s);
        option->value.s = NULL;
        if (value.s) {
            option->value.s = malloc(strlen(value.s) + 1);
            strcpy(option->value.s, value.s);
        }
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        aold = option->value.ar;
        anew = value.ar;

        if (!aold && !anew)
            return FALSE;

        /* check rule set equality */
        if (aold && anew && aold->num_rules == anew->num_rules) {
            /* compare each individual rule */
            for (i = 0; i < aold->num_rules; i++)
                if (strcmp(aold->rules[i].pattern, anew->rules[i].pattern) ||
                    aold->rules[i].oclass != anew->rules[i].oclass ||
                    aold->rules[i].buc != anew->rules[i].buc ||
                    aold->rules[i].action != anew->rules[i].action)
                    break;      /* rule difference found */
            if (i == aold->num_rules)
                return FALSE;
        }

        if (aold) {
            free(aold->rules);
            free(aold);
        }

        option->value.ar = copy_autopickup_rules(value.ar);
        break;

    case OPTTYPE_BOOL:
        if (option->value.b == value.b)
            return FALSE;
        option->value.b = value.b;
        break;

    case OPTTYPE_ENUM:
        if (option->value.e == value.e)
            return FALSE;
        option->value.e = value.e;
        break;

    case OPTTYPE_INT:
        if (option->value.i == value.i)
            return FALSE;
        option->value.i = value.i;
        break;
    }

    return TRUE;

}


static boolean
set_option(const char *name, union nh_optvalue value, boolean isstring)
{
    boolean is_ui = FALSE;
    struct nh_option_desc *option = NULL;

    if (options)
        option = find_option(options, name);

    if (!option && !program_state.game_running && birth_options)
        option = find_option(birth_options, name);

    if (!option && ui_options) {
        option = find_option(ui_options, name);
        is_ui = TRUE;
    }

    if (!option)
        return FALSE;

    /* if this option change affects game options (!is_ui) and happens during a 
       replay (program_state.viewing) and the change isn't triggered by the
       replay (!program_state.restoring) */
    if (!is_ui && program_state.viewing && !program_state.restoring)
        return FALSE;   /* Nope, sorry. That would mess up the replay */

    if (isstring)
        value = string_to_optvalue(option, value.s);

    if (!option_value_ok(option, value))
        return FALSE;

    if (copy_option_value(option, value) && !is_ui)
        log_option(option);     /* prev value != new value */

    if (option->type == OPTTYPE_BOOL) {
        int i;
        boolean *bvar = NULL;
        const struct nh_boolopt_map *boolmap = boolopt_map;

        if (is_ui)
            boolmap = ui_boolopt_map;

        for (i = 0; boolmap[i].optname && !bvar; i++)
            if (!strcmp(option->name, boolmap[i].optname))
                bvar = boolmap[i].addr;

        if (!bvar)
            /* shouldn't happen */
            return FALSE;

        *bvar = option->value.b;
        if (is_ui && ui_option_callback)
            /* allow the ui to "see" changes to booleans, but the return value
               doesn't mattter as the option was set here. */
            ui_option_callback(option);
        return TRUE;
    } else if (is_ui)
        return ui_option_callback(option);
    /* regular non-boolean options */
    else if (!strcmp("comment", option->name)) {
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
        iflags.runmode = option->value.e;
    } else if (!strcmp("autopickup_rules", option->name)) {
        if (iflags.ap_rules) {
            free(iflags.ap_rules->rules);
            free(iflags.ap_rules);
            iflags.ap_rules = NULL;
        }
        iflags.ap_rules = copy_autopickup_rules(option->value.ar);
    }
    /* birth options */
    else if (!strcmp("align", option->name)) {
        flags.init_align = option->value.e;
    } else if (!strcmp("gender", option->name)) {
        flags.init_gend = option->value.e;
    } else if (!strcmp("race", option->name)) {
        flags.init_race = option->value.e;
    } else if (!strcmp("role", option->name)) {
        flags.init_role = option->value.e;
    }

    else if (!strcmp("catname", option->name)) {
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

    if (!api_entry_checkpoint())
        return FALSE;

    rv = set_option(name, value, isstring);

    api_exit();
    return rv;
}


struct nh_option_desc *
nh_get_options(enum nh_option_list list)
{
    switch (list) {
    case CURRENT_BIRTH_OPTIONS:
        return birth_options;
    case ACTIVE_BIRTH_OPTIONS:
        return active_birth_options;
    case GAME_OPTIONS:
        return options;
    default:
        return NULL;
    }
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


struct nh_option_desc *
clone_optlist(const struct nh_option_desc *in)
{
    int i;
    struct nh_option_desc *out;

    for (i = 0; in[i].name; i++) ;
    i++;
    out = malloc(sizeof (struct nh_option_desc) * i);
    memcpy(out, in, sizeof (struct nh_option_desc) * i);

    for (i = 0; in[i].name; i++) {
        if (in[i].type == OPTTYPE_STRING && in[i].value.s)
            out[i].value.s = strdup(in[i].value.s);
        else if (in[i].type == OPTTYPE_AUTOPICKUP_RULES && in[i].value.ar)
            out[i].value.ar = copy_autopickup_rules(in[i].value.ar);
    }

    return out;
}


void
free_optlist(struct nh_option_desc *opt)
{
    int i;

    if (!opt)
        return;

    for (i = 0; opt[i].name; i++) {
        if (opt[i].type == OPTTYPE_STRING && opt[i].value.s)
            free(opt[i].value.s);
        else if (opt[i].type == OPTTYPE_AUTOPICKUP_RULES && opt[i].value.ar) {
            free(opt[i].value.ar->rules);
            free(opt[i].value.ar);
        }
    }

    free(opt);
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


int
dotogglepickup(void)
{
    union nh_optvalue val;

    val.b = !flags.pickup;
    set_option("autopickup", val, FALSE);

    pline("Autopickup: %s.", flags.pickup ? "ON" : "OFF");
    return 0;
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


/* convenience function: allows the ui to share option handling code */
void
nh_setup_ui_options(struct nh_option_desc *uioptions,
                    struct nh_boolopt_map *boolmap,
                    boolean(*callback) (struct nh_option_desc *))
{
    ui_options = uioptions;
    ui_boolopt_map = boolmap;
    ui_option_callback = callback;
}


static struct nh_autopickup_rules *
copy_autopickup_rules(const struct nh_autopickup_rules *in)
{
    struct nh_autopickup_rules *out;
    int size;

    if (!in || !in->num_rules)
        return NULL;

    out = malloc(sizeof (struct nh_autopickup_rules));
    out->num_rules = in->num_rules;
    size = out->num_rules * sizeof (struct nh_autopickup_rule);
    out->rules = malloc(size);
    memcpy(out->rules, in->rules, size);

    return out;
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


struct nh_autopickup_rules *
parse_autopickup_rules(const char *str)
{
    struct nh_autopickup_rules *out;
    char *copy, *semi;
    const char *start;
    int i, rcount = 0;
    unsigned int action, buc;

    if (!str || !*str)
        return NULL;

    start = str;
    while ((semi = strchr(start, ';'))) {
        start = ++semi;
        rcount++;
    }

    if (!rcount)
        return NULL;

    out = &ap_rules_static;
    out->rules = ap_rules_array;
    out->num_rules = rcount;

    i = 0;
    start = copy = strdup(str);
    while ((semi = strchr(start, ';')) && i < rcount) {
        *semi++ = '\0';
        sscanf(start, "(\"%39[^,],%d,%u,%u);", out->rules[i].pattern,
               &out->rules[i].oclass, &buc, &action);
        /* since %[ in sscanf requires a nonempty match, we allowed it to match
           the closing '"' of the rule. Remove that now. */
        out->rules[i].pattern[strlen(out->rules[i].pattern) - 1] = '\0';
        out->rules[i].buc = (enum nh_bucstatus)buc;
        out->rules[i].action = (enum autopickup_action)action;
        i++;
        start = semi;
    }

    free(copy);
    return out;
}


/*options.c*/
