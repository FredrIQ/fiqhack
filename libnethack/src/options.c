/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2016-03-08 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "common_options.h"
#include <ctype.h>

#define WINTYPELEN 16

static int change_inv_order(const char *op);
static struct nh_option_desc *new_opt_struct(void);

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

static const struct nh_listitem movecommand_list[] = {
    {uim_nointeraction, "nointeraction"},
    {uim_onlyitems, "onlyitems"},
    {uim_displace, "displace"},
    {uim_pacifist, "pacifist"},
    {uim_attackhostile, "attackhostile"},
    {uim_traditional, "traditional"},
    {uim_standard, "standard"},
    {uim_indiscriminate, "indiscriminate"},
    {uim_forcefight, "forcefight"}
};
static const struct nh_enum_option movecommand_spec =
    { movecommand_list, listlen(movecommand_list) };

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


/*
 * This is statically allocated, which means a pointer to it must never
 * escape this file (since nh_option_desc is supposed to be dynamically
 * allocated.
 *
 * The only valid use of this, therefore, is to copy it to a new
 * dynamically-allocated copy and go from there.
 */
static const struct nh_option_desc const_options[] = {
    {"autodig", "dig if moving and wielding digging tool",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"autodigdown", "autodig downwards tries to create a pit or hole",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"autopickup", "automatically pick up objects you move over",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"autopickup_rules",
     "rules to decide what to autopickup if autopickup is on",
     nh_birth_ingame, OPTTYPE_AUTOPICKUP_RULES, {.ar = &def_autopickup}},
    {"autoquiver",
     "when firing with an empty quiver, select something suitable",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"corridorbranch", "branching corridors do not stop farmove",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"disclose", "whether to disclose information at end of game",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = DISCLOSE_PROMPT_DEFAULT_YES}},
    {"fruit", "the name of a fruit you enjoy eating",
     nh_birth_ingame, OPTTYPE_STRING, {.s = NULL}},
    {"menustyle", "user interface for object selection",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = MENU_FULL}},
    {"movecommand", "what the movement keys do", nh_birth_ingame,
     OPTTYPE_ENUM, {.e = uim_standard}},
    {"multistage_equip", "equipping items can imply unequipping others",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"packorder", "the inventory order of the items in your pack",
     nh_birth_ingame, OPTTYPE_STRING, {.s = NULL}},
    {"pickup_burden", "maximum burden picked up before prompt",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = MOD_ENCUMBER}},
    {"pickup_thrown", "autopickup items you threw or fired",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"prayconfirm", "use confirmation prompt when #pray command issued",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"pushweapon", "offhand the old weapon when wielding a new one",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"show_uncursed", "always show uncursed status",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"showrace", "show yourself by your race rather than by role",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"sortpack", "group similar kinds of objects in inventory",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"sparkle", "display sparkly effect for resisted magical attacks",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"tombstone", "print tombstone when you die",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"travel_interrupt", "interrupt travel (_) when a hostile is in sight",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"verbose", "print more commentary during the game",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},

    {"name", "character name",
     nh_birth_lasting, OPTTYPE_STRING, {.s = NULL}},
    {"mode", "game mode",
     nh_birth_lasting, OPTTYPE_ENUM, {.e = MODE_NORMAL}},
    {"seed", "seed: 16 letters (nonscoring)/blank",
     nh_birth_lasting, OPTTYPE_STRING, {.s = NULL}},
    {"timezone", "time zone to use for time-dependent effects",
     nh_birth_lasting, OPTTYPE_ENUM, {.e = 0}},
    {"elbereth", "difficulty: the E-word repels monsters",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = TRUE}},
    {"reincarnation", "Special Rogue-like levels",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = TRUE}},
    {"seduction", "certain monsters may seduce you",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = TRUE}},
    {"bones", "allow bones levels",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = TRUE}},
    {"permablind", "spend the whole game blind",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = FALSE}},
    {"permahallu", "spend the whole game hallucinating",
     nh_birth_lasting, OPTTYPE_BOOL, {.b = FALSE}},
    {"polyinit", "play in monster form (non-scoring)",
     nh_birth_lasting, OPTTYPE_ENUM, {.e = -1}},

    {"legacy", "print introductory message",
     nh_birth_creation, OPTTYPE_BOOL, {.b = TRUE}},
    {"align", "your starting alignment",
     nh_birth_creation, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"gender", "your starting gender",
     nh_birth_creation, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"race", "your starting race",
     nh_birth_creation, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"role", "your starting role",
     nh_birth_creation, OPTTYPE_ENUM, {.e = ROLE_NONE}},
    {"catname", "the name of your (first) cat",
     nh_birth_creation, OPTTYPE_STRING, {.s = NULL}},
    {"dogname", "the name of your (first) dog",
     nh_birth_creation, OPTTYPE_STRING, {.s = NULL}},
    {"horsename", "the name of your (first) horse",
     nh_birth_creation, OPTTYPE_STRING, {.s = NULL}},
    {"pettype", "your preferred initial pet type",
     nh_birth_creation, OPTTYPE_ENUM, {.e = 0}},

    {NULL, NULL, nh_birth_ingame, OPTTYPE_BOOL, {.s = NULL}}
};


/* associate boolean options with variables directly */
static const struct nhlib_boolopt_map boolopt_map[] = {
    {"autodig", &flags.autodig},
    {"autodigdown", &flags.autodigdown},
    {"autopickup", &flags.pickup},
    {"autoquiver", &flags.autoquiver},
    {"corridorbranch", &flags.corridorbranch},
    {"multistage_equip", &flags.cblock},
    {"legacy", &flags.legacy},
    {"pickup_thrown", &flags.pickup_thrown},
    {"prayconfirm", &flags.prayconfirm},
    {"pushweapon", &flags.pushweapon},
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


/* Most environment variables will eventually be printed in an error message if
   they don't work. Error messages now tend to be bounds-checked correctly, but
   we nonetheless don't want to parrot maliciously long messages back to the
   user because it spams their terminals. */
char *
nh_getenv(const char *ev)
{
    char *getev = getenv(ev);

    if (getev && strlen(getev) <= 128) /* same as 4.2 */
        return getev;
    else
        return NULL;
}


static void
build_role_spec(void)
{
    if (role_spec.choices)
        return;

    int i;
    struct nh_listitem *choices;

    /* build list of roles */
    for (i = 0; roles[i].name.m || roles[i].name.f; i++)
        ; /* just count em */
    role_spec.numchoices = i + 2;
    choices = malloc((i + 2) * sizeof (struct nh_listitem));
    for (i = 0; roles[i].name.m || roles[i].name.f; i++) {
        choices[i].id = i;
        if (roles[i].name.m)
            choices[i].caption = roles[i].name.m;
        else
            choices[i].caption = roles[i].name.f;
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
    if (race_spec.choices)
        return;

    int i;
    struct nh_listitem *choices;

    /* build list of races */
    for (i = 0; races[i].noun; i++)
        ;   /* just count em */
    race_spec.numchoices = i + 2;
    choices = malloc((i + 2) * sizeof (struct nh_listitem));
    for (i = 0; races[i].noun; i++) {
        choices[i].id = i;
        choices[i].caption = races[i].noun;
    }
    choices[i].id = ROLE_NONE;
    choices[i].caption = "ask";
    choices[i + 1].id = ROLE_RANDOM;
    choices[i + 1].caption = "random";

    race_spec.choices = choices;
}


struct nh_option_desc *
new_opt_struct(void)
{
    struct nh_option_desc *options = nhlib_clone_optlist(const_options);

    build_role_spec();
    build_race_spec();

    /* initialize option definitions */
    nhlib_find_option(options, "disclose")->e = disclose_spec;
    nhlib_find_option(options, "menustyle")->e = menustyle_spec;
    nhlib_find_option(options, "movecommand")->e = movecommand_spec;
    nhlib_find_option(options, "pickup_burden")->e = pickup_burden_spec;
    nhlib_find_option(options, "autopickup_rules")->a = autopickup_spec;

    nhlib_find_option(options, "name")->s.maxlen = PL_NSIZ;
    nhlib_find_option(options, "seed")->s.maxlen = RNG_SEED_SIZE_BASE64;
    nhlib_find_option(options, "mode")->e = mode_spec;
    nhlib_find_option(options, "timezone")->e = timezone_spec;
    nhlib_find_option(options, "polyinit")->e = polyinit_spec;
    nhlib_find_option(options, "align")->e = align_spec;
    nhlib_find_option(options, "gender")->e = gender_spec;
    nhlib_find_option(options, "role")->e = role_spec;
    nhlib_find_option(options, "race")->e = race_spec;
    nhlib_find_option(options, "pettype")->e = pettype_spec;
    nhlib_find_option(options, "catname")->s.maxlen = PL_PSIZ;
    nhlib_find_option(options, "dogname")->s.maxlen = PL_PSIZ;
    nhlib_find_option(options, "horsename")->s.maxlen = PL_PSIZ;

    struct nh_option_desc *fruit = nhlib_find_option(options, "fruit");
    const char *def_fruit = "slime mold";
    fruit->s.maxlen = PL_FSIZ;
    fruit->value.s = malloc(strlen(def_fruit)+1);
    strcpy(fruit->value.s, def_fruit);

    struct nh_option_desc *packorder = nhlib_find_option(options, "packorder");
    const char *def_packorder = "$\")[%?+!=/(*`0_";
    packorder->s.maxlen = MAXOCLASSES;
    packorder->value.s = malloc(strlen(def_packorder)+1);
    strcpy(packorder->value.s, def_packorder);

    return options;
}


void
initoptions(void)
{
    flags.mon_generation = 1;

    flags.travelcc.x = flags.travelcc.y = -1;

    /* init flags.inv_order this way, as setting it via the option requires a
       preexisting order */
    memcpy(flags.inv_order, def_inv_order, sizeof flags.inv_order);

    fruitadd(obj_descr[SLIME_MOLD].oc_name);
    strncpy(gamestate.fruits.curname, obj_descr[SLIME_MOLD].oc_name, PL_FSIZ);
}


boolean
set_option(const char *name, union nh_optvalue value,
           struct newgame_options *ngo)
{
    struct nh_option_desc *option = NULL, *options = new_opt_struct();
    boolean ret = FALSE;

    /* can't change options for other players */
    if (program_state.followmode != FM_PLAY)
        goto free;

    if (options)
        option = nhlib_find_option(options, name);

    if (!option || (option->birth_option && program_state.game_running))
        goto free;

    if (!nhlib_option_value_ok(option, value))
        goto free;

    nhlib_copy_option_value(option, value);

    if (option->type == OPTTYPE_BOOL) {
        boolean *bvar = nhlib_find_boolopt(boolopt_map, option->name);

        if (!bvar) {
            impossible("no boolean for option '%s'", option->name);
            goto free;
        }
        *bvar = option->value.b;

        ret = TRUE;
        goto free;
    } else if (!strcmp("disclose", option->name)) {
        flags.end_disclose = option->value.e;
    } else if (!strcmp("fruit", option->name)) {
        strncpy(gamestate.fruits.curname, option->value.s, PL_FSIZ-1);
        gamestate.fruits.curname[PL_FSIZ - 1] = '\0';
        if (objects)    /* don't do fruitadd before the game is running */
            fruitadd(gamestate.fruits.curname);
    } else if (!strcmp("menustyle", option->name)) {
        flags.menu_style = option->value.e;
    } else if (!strcmp("movecommand", option->name)) {
        flags.interaction_mode = option->value.e;
    } else if (!strcmp("packorder", option->name)) {
        if (!change_inv_order(option->value.s))
            goto free;
    } else if (!strcmp("pickup_burden", option->name)) {
        flags.pickup_burden = option->value.e;
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
        strncpy(u.uplname, option->value.s, PL_NSIZ-1);
        (u.uplname)[PL_NSIZ - 1] = '\0';
    } else if (!strcmp("seed", option->name)) {
        /* note: does not NUL-terminate a max-length string, this is
           intentional */
        strncpy(flags.setseed, option->value.s, RNG_SEED_SIZE_BASE64);
    } else if (!strcmp("catname", option->name)) {
        if (!ngo)
            panic("catname set outside newgame sequence");
        strncpy(ngo->catname, option->value.s, PL_PSIZ-1);
        ngo->catname[PL_PSIZ - 1] = '\0';
    } else if (!strcmp("dogname", option->name)) {
        if (!ngo)
            panic("dogname set outside newgame sequence");
        strncpy(ngo->dogname, option->value.s, PL_PSIZ-1);
        ngo->dogname[PL_PSIZ - 1] = '\0';
    } else if (!strcmp("horsename", option->name)) {
        if (!ngo)
            panic("horsename set outside newgame sequence");
        strncpy(ngo->horsename, option->value.s, PL_PSIZ-1);
        ngo->horsename[PL_PSIZ - 1] = '\0';
    } else if (!strcmp("pettype", option->name)) {
        if (!ngo)
            panic("preferred_pet set outside newgame sequence");
        ngo->preferred_pet = (char)option->value.e;
    } else if (!strcmp("timezone", option->name)) {
        flags.timezone = option->value.e;
    } else if (!strcmp("polyinit", option->name)) {
        flags.polyinit_mnum = option->value.e;
    }

    else
        /* unrecognized option */
        goto free;

    /* assume that any recognized option has been handled. */
    ret = TRUE;

free:
    nhlib_free_optlist(options);

    return ret;
}


boolean
nh_set_option(const char *name, union nh_optvalue value)
{
    boolean rv;

    API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(FALSE);

    rv = set_option(name, value, NULL);

    API_EXIT();
    return rv;
}


struct nh_option_desc *
default_options(void)
{
    return new_opt_struct();
}


struct nh_option_desc *
nh_get_options(void)
{
    struct nh_option_desc *options = new_opt_struct();

    /* Outside of a game, don't attempt to recover options from state; all
       options should have default values. */
    if (!program_state.game_running)
        return options;

    struct nh_option_desc *option;
    for (option = options; option->name; ++option) {
        if (option->type == OPTTYPE_BOOL) {
            boolean *bvar = nhlib_find_boolopt(boolopt_map, option->name);

            if (!bvar) {
                impossible("no boolean for option '%s'", option->name);
                continue;
            }

            option->value.b = *bvar;
        }

        else if (!strcmp("disclose", option->name)) {
            option->value.e = flags.end_disclose;
        } else if (!strcmp("fruit", option->name)) {
            if (option->value.s)
                free(option->value.s);
            option->value.s = malloc(PL_FSIZ);

            strncpy(option->value.s, gamestate.fruits.curname, PL_FSIZ-1);
            option->value.s[PL_FSIZ - 1] = '\0';

        } else if (!strcmp("menustyle", option->name)) {
            option->value.e = flags.menu_style;
        } else if (!strcmp("movecommand", option->name)) {
            option->value.e = flags.interaction_mode;
        } else if (!strcmp("packorder", option->name)) {

            int i;

            if (option->value.s)
                free(option->value.s);

            option->value.s = malloc(MAXOCLASSES + 1);
            for (i = 0; i < MAXOCLASSES; ++i)
                option->value.s[i] = def_oc_syms[(int)flags.inv_order[i]];

            option->value.s[MAXOCLASSES] = '\0';

        } else if (!strcmp("pickup_burden", option->name)) {
            option->value.e = flags.pickup_burden;
        } else if (!strcmp("autopickup_rules", option->name)) {
            if (option->value.ar) {
                free(option->value.ar->rules);
                free(option->value.ar);
            }
            option->value.ar = nhlib_copy_autopickup_rules(flags.ap_rules);
        } else if (!strcmp("mode", option->name)) {
            option->value.e =
                flags.debug ? MODE_WIZARD :
                flags.explore ? MODE_EXPLORE : MODE_NORMAL;
        } else if (!strcmp("timezone", option->name)) {
            option->value.e = flags.timezone;
        } else if (!strcmp("polyinit", option->name)) {
            option->value.e = flags.polyinit_mnum;
        } else if (!strcmp("align", option->name)) {
            option->value.e = u.initalign;
        } else if (!strcmp("gender", option->name)) {
            option->value.e = u.initgend;
        } else if (!strcmp("race", option->name)) {
            option->value.e = u.initrace;
        } else if (!strcmp("role", option->name)) {
            option->value.e = u.initrole;
        } else if (!strcmp("name", option->name)) {

            if (option->value.s)
                free(option->value.s);
            option->value.s = malloc(PL_NSIZ);

            strncpy(option->value.s, u.uplname, PL_NSIZ-1);
            option->value.s[PL_NSIZ - 1] = '\0';

        } else if (!strcmp("seed", option->name)) {

            if (option->value.s)
                free(option->value.s);
            option->value.s = malloc(RNG_SEED_SIZE_BASE64 + 1);

            strncpy(option->value.s, flags.setseed, RNG_SEED_SIZE_BASE64);
            option->value.s[RNG_SEED_SIZE_BASE64] = '\0';

        } else if (!strcmp("catname", option->name) ||
                   !strcmp("dogname", option->name) ||
                   !strcmp("horsename", option->name)) {
            /* The client shouldn't be doing this, but we can't crash in
               response because that would be exploitable. Send it a null
               string instead. */
            if (option->value.s)
                free(option->value.s);
            option->value.s = malloc(1);
            option->value.s[0] = '\0';
        } else if (!strcmp("pettype", option->name)) {
            /* As the previous case, just we want an enum not a string. */
            option->value.e = 0;
        } else {
            impossible("unknown option '%s'", option->name);
            memset(&option->value, 0, sizeof option->value);
        }
    }

    return options;
}




/* Returns the fid of the fruit type; if that type already exists, it returns
   the fid of that one; if it does not exist, it adds a new fruit type to the
   chain and returns the new one. */
int
fruitadd(const char *str)
{
    int i;
    struct fruit *f;
    struct fruit *lastf = 0;
    int highest_fruit_id = 0;
    char buf[PL_FSIZ], *c;
    boolean user_specified = (str == gamestate.fruits.curname);

    /* if not user-specified, then it's a fruit name for a fruit on a bones
       level... */

    /* Note: every fruit has an id (spe for fruit objects) of at least 1; 0 is
       an error. */
    if (user_specified) {
        /* disallow naming after other foods (since it'd be impossible to tell
           the difference) */

        boolean found = FALSE, numeric = FALSE;

        for (i = bases[FOOD_CLASS]; objects[i].oc_class == FOOD_CLASS; i++) {
            if (i != SLIME_MOLD && !strcmp(OBJ_NAME(objects[i]),
                                           gamestate.fruits.curname)) {
                found = TRUE;
                break;
            }
        }
        for (c = gamestate.fruits.curname; *c >= '0' && *c <= '9'; c++)
            ;
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
            strcpy(buf, gamestate.fruits.curname);
            strcpy(gamestate.fruits.curname, "candied ");
            strncat(gamestate.fruits.curname + 8, buf, PL_FSIZ - 8 - 1);
        }
    }
    for (f = gamestate.fruits.chain; f; f = f->nextf) {
        lastf = f;
        if (f->fid > highest_fruit_id)
            highest_fruit_id = f->fid;
        if (!strncmp(str, f->fname, PL_FSIZ))
            goto nonew;
    }
    /* if adding another fruit would overflow spe, use a random fruit
       instead... we've got a lot to choose from.

       TODO: No idea what RNG this should be on (in particular, should it be on
       the display RNG?) */
    if (highest_fruit_id >= 127)
        return rnd(127);
    highest_fruit_id++;
    f = newfruit();
    memset(f, 0, sizeof (struct fruit));
    if (gamestate.fruits.chain)
        lastf->nextf = f;
    else
        gamestate.fruits.chain = f;
    strcpy(f->fname, str);
    f->fid = highest_fruit_id;
    f->nextf = 0;
nonew:
    if (user_specified)
        gamestate.fruits.current = highest_fruit_id;
    return f->fid;
}


static int
change_inv_order(const char *op)
{
    int oc_sym, num;
    char buf[MAXOCLASSES];
    const char *sp;

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

/*options.c*/
