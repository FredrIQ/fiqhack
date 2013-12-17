/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#define IN_DECL_C
#include "hack.h"

/*
 * The following structure will be initialized at startup time with
 * the level numbers of some "important" things in the game.
 */
struct dgn_topology dungeon_topology;

int smeq[MAXNROFROOMS + 1];

int killer_format;
const char *killer;
const char *delayed_killer;
int done_money;
char killer_buf[BUFSZ];
const char *nomovemsg;
const char nul[40] = {0};     /* contains zeros */
char pl_character[PL_CSIZ];
char pl_race;

char pl_fruit[PL_FSIZ];
int current_fruit;
struct fruit *ffruit;

char tune[6];

const char quitchars[] = " \r\n\033";
const char vowels[] = "aeiouAEIOU";
const char ynchars[] = "yn";
const char ynqchars[] = "ynq";
const char ynaqchars[] = "ynaq";

const char disclosure_options[] = "iavgcs";

#if defined(WIN32)
char hackdir[PATHLEN];  /* where rumors, help, record are */
#endif


struct sinfo program_state;

const schar xdir[11] = { -1, -1, 0, 1, 1, 1, 0, -1, 0, 0, 0 };
const schar ydir[11] = { 0, -1, -1, -1, 0, 1, 1, 1, 0, 0, 0 };
const schar zdir[11] = { 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0 };

schar tbx, tby; /* mthrowu: target */

/* for xname handling of multiple shot missile volleys:
   number of shots, index of current one, validity check, shoot vs throw */
struct multishot m_shot = { 0, 0, STRANGE_OBJECT, FALSE };

struct dig_info digging;

int branch_id;
dungeon dungeons[MAXDUNGEON];   /* ini'ed by init_dungeon() */
s_level *sp_levchn;
coord inv_pos;  /* vibrating square position */

boolean in_mklev;
boolean stoned; /* done to monsters hit by 'c' */
boolean unweapon;

boolean in_steed_dismounting;

coord bhitpos;

struct level *levels[MAXLINFO];
struct level *level;    /* level map */

struct monst youmonst;
struct flag flags;
struct instance_flags iflags;
struct you u;

struct obj *invent;

const int shield_static[SHIELD_COUNT] = {
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,    /* 7 per row */
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
};

struct spell spl_book[MAXSPELL + 1];

unsigned int moves;
long wailmsg;

/* used to zero all elements of a struct obj, also as a flag to mean a
   non-object */
struct obj zeroobj;

/* originally from dog.c */
char dogname[PL_PSIZ];
char catname[PL_PSIZ];
char horsename[PL_PSIZ];
char preferred_pet;     /* '\0', 'c', 'd', 'n' (none) */

/* monsters that went down/up together with @ */
struct monst *mydogs;

/* monsters that are moving to another dungeon level */
struct monst *migrating_mons;

struct mvitals mvitals[NUMMONS];

const char *const c_obj_colors[] = {
    "black",          /* CLR_BLACK */
    "red",            /* CLR_RED */
    "green",          /* CLR_GREEN */
    "brown",          /* CLR_BROWN */
    "blue",           /* CLR_BLUE */
    "magenta",        /* CLR_MAGENTA */
    "cyan",           /* CLR_CYAN */
    "gray",           /* CLR_GRAY */
    "dark gray",      /* CLR_DARK_GRAY */
    "orange",         /* CLR_ORANGE */
    "bright green",   /* CLR_BRIGHT_GREEN */
    "yellow",         /* CLR_YELLOW */
    "bright blue",    /* CLR_BRIGHT_BLUE */
    "bright magenta", /* CLR_BRIGHT_MAGENTA */
    "bright cyan",    /* CLR_BRIGHT_CYAN */
    "white",          /* CLR_WHITE */
};

const char *const the_your[] = { "the", "your" };

/* NOTE: the order of these words exactly corresponds to the
   order of oc_material values #define'd in objclass.h. */
const char *const materialnm[] = {
    "mysterious", "liquid", "wax", "organic", "flesh",
    "paper", "cloth", "leather", "wooden", "bone", "dragonhide",
    "iron", "metal", "copper", "silver", "gold", "platinum", "mithril",
    "plastic", "glass", "gemstone", "stone"
};

/* Vision */
boolean vision_full_recalc;
char **viz_array;       /* used in cansee() and couldsee() macros */

char *fqn_prefix[PREFIX_COUNT] = { NULL, NULL, NULL, NULL, NULL };

const char *const fqn_prefix_names[PREFIX_COUNT] = {
    "bonesdir", "datadir", "scoredir", "lockdir", "troubledir"
};


/* quest monsters need some fixups depending on your race and role, but
 * mons is const. We'll use these copies instead. */
struct permonst pm_leader, pm_guardian, pm_nemesis;
struct permonst pm_you_male, pm_you_female;

int exit_jmp_buf_valid;
nh_jmp_buf exit_jmp_buf;

struct artifact *artilist;
short disco[NUM_OBJECTS];       /* discovered objects */

unsigned int histcount;
struct histevent *histevents;

unsigned long long turntime;
unsigned int timer_id = 1;

/* If one game (A) is started and then saved, followed by game B with different
 * birth_options, after which game A is restored, then A must run with its
 * original birth_options, rather than the most recent birth_options which were
 * set for game B. */
struct nh_option_desc *active_birth_options;
struct nh_option_desc *birth_options;
struct nh_option_desc *options;

char toplines[MSGCOUNT][BUFSZ];
int toplines_count[MSGCOUNT];
int curline;

/* When changing this, also change neutral_turnstate_tasks. */
static const struct turnstate default_turnstate = {
    .occupation = NULL,
    .multi = 0,
    .afternmv = NULL,
    .multi_txt = "",
    .occupation_txt = "",
    .saved_cmd = -1,
    .saved_arg = { CMD_ARG_NONE },
    .tracked = {0},
};

struct turnstate turnstate;

void
init_turnstate(void)
{
    memcpy(&turnstate, &default_turnstate, sizeof turnstate);
}

void
neutral_turnstate_tasks(void)
{
    /* We want to compare turnstate to the default for effective equality:
       integers are equal, strings are equal up to the first NUL, tagged
       unions are equal up to the tag and the bits containing its associated
       value, padding doesn't matter. */
#define CHECKEQ(x, y) do {                                              \
        if (turnstate.x != y)                                           \
            impossible("turnstate."#x" persisted between turns");       \
    } while(0)

    CHECKEQ(occupation, 0);
    CHECKEQ(multi, 0);
    CHECKEQ(afternmv, 0);
    CHECKEQ(multi_txt[0], 0);
    CHECKEQ(occupation_txt[0], 0);
    CHECKEQ(saved_cmd, -1);
    CHECKEQ(saved_arg.argtype, CMD_ARG_NONE);

#undef CHECKEQ

    if (memcmp(turnstate.tracked, default_turnstate.tracked,
               sizeof (turnstate.tracked)))
        impossible("turnstate.tracked persisted between turns");

    /* TODO: clean up memory */

    log_command_result();

void
init_data(void)
{
    /* iflags may already contain valid, important data, because init_data()
       runs as part of the game init sequence after options have been set, etc. 
     */
    struct nh_autopickup_rules *rules = iflags.ap_rules;

    init_turnstate();

    moves = 1;

    memset(&program_state, 0, sizeof (program_state));
    memset(&flags, 0, sizeof (flags));
    memset(&iflags, 0, sizeof (iflags));
    memset(&u.quest_status, 0, sizeof (u.quest_status));
    memset(&levels, 0, sizeof (levels));
    memset(&u, 0, sizeof (u));
    memset(dogname, 0, sizeof (dogname));
    memset(catname, 0, sizeof (catname));
    memset(horsename, 0, sizeof (horsename));
    memset(&youmonst, 0, sizeof (youmonst));
    memset(&zeroobj, 0, sizeof (zeroobj));
    memset(mvitals, 0, sizeof (mvitals));
    memset(spl_book, 0, sizeof (spl_book));
    memset(disco, 0, sizeof (disco));
    memset(&digging, 0, sizeof (digging));
    memset(&inv_pos, 0, sizeof (inv_pos));
    memset(toplines, 0, sizeof (toplines));
    memset(toplines_count, 0, sizeof (toplines_count));

    level = NULL;
    killer_format = 0;
    killer = NULL;
    ffruit = NULL;
    current_fruit = 0;
    sp_levchn = NULL;
    in_mklev = stoned = unweapon = FALSE;
    invent = NULL;
    in_steed_dismounting = FALSE;
    wailmsg = 0;
    bhitpos.x = bhitpos.y = 0;
    preferred_pet = 0;
    migrating_mons = mydogs = NULL;
    vision_full_recalc = FALSE;
    viz_array = NULL;
    artilist = NULL;
    branch_id = 0;
    histevents = NULL;
    histcount = 0;
    timer_id = 1;
    curline = 0;

    iflags.ap_rules = rules;
    flags.moonphase = 10;       /* invalid value, so that the first call to
                                   realtime_tasks will dtrt */
    flags.soundok = 1;
}

/*decl.c*/
