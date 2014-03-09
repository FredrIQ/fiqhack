/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-03-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#define IN_DECL_C
#include "hack.h"

/* Memory management, outside the game. */
struct xmalloc_block *api_blocklist = NULL;

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

char toplines[MSGCOUNT][BUFSZ];
int toplines_count[MSGCOUNT];
int curline;

/* When changing this, also change neutral_turnstate_tasks. */
static const struct turnstate default_turnstate = {
    .tracked = {0},
    .continue_message = TRUE,
    .vision_full_recalc = FALSE,
    .delay_flushing = FALSE,
    .migrating_pets = NULL,
    .migrating_objs = NULL,
    .helpless_timers = {},
    .helpless_causes = {},
    .helpless_endmsgs = {},
    .pray = { .align = A_NONE, .type = pty_invalid, .trouble = ptr_invalid },
    .move = { .dx = 0, .dy = 0, .stepped_on = {} },
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
    int i, j;

    /* We want to compare turnstate to the default for effective equality:
       integers are equal, strings are equal up to the first NUL, tagged
       unions are equal up to the tag and the bits containing its associated
       value, padding doesn't matter. */
    if (memcmp(turnstate.tracked, default_turnstate.tracked,
               sizeof (turnstate.tracked)))
        impossible("turnstate.tracked persisted between turns");
    if (!turnstate.continue_message)
        impossible("turnstate.continue_message persisted between turns");
    if (turnstate.vision_full_recalc)
        impossible("vision not recalculated when needed during a turn");
    if (turnstate.delay_flushing)
        impossible("flushing delayed over a turn");

    if (turnstate.migrating_pets) {
        int count = 0;
        while (turnstate.migrating_pets) {
            struct monst *mtmp = turnstate.migrating_pets;
            turnstate.migrating_pets = mtmp->nmon;
            dealloc_monst(mtmp);
            ++count;
        }
        impossible("%d pets still migrating between turns", count);
    }
    if (turnstate.migrating_objs) {
        int count = 0;
        while (turnstate.migrating_objs) {
            struct obj *otmp = turnstate.migrating_objs;
            obj_extract_self(otmp);
            obfree(otmp);
            ++count;
        }
        impossible("objects still migrating between turns");
    }

    for (i = hr_first; i <= hr_last; ++i) {
        if (turnstate.helpless_timers[i])
            impossible("helpless timer %d nonzero between turns", i);
        if (*turnstate.helpless_causes[i])
            impossible("helpless cause %d nonzero between turns", i);
        if (*turnstate.helpless_endmsgs[i])
            impossible("helpless endmsg %d nonzero between turns", i);
    }

    if (turnstate.pray.align != A_NONE)
        impossible("prayer alignment persisted between turns");
    if (turnstate.pray.type != pty_invalid)
        impossible("prayer type persisted between turns");
    if (turnstate.pray.trouble != ptr_invalid)
        impossible("prayer trouble persisted between turns");

    if (turnstate.move.dx || turnstate.move.dy)
        impossible("turnstate dx and dy persisted between turns");

    for (i = 0; i < COLNO; i++)
        for (j = 0; j < ROWNO; j++)
            if (turnstate.move.stepped_on[i][j])
                impossible("turnstate stepped-on persisted between turns");

    memcpy(&turnstate, &default_turnstate, sizeof turnstate);

    log_neutral_turnstate();
}

void
init_data(boolean including_program_state)
{
    init_turnstate();

    moves = 1;

    /* If including_program_state is not set, we don't init anything that
       isn't saved in the save file. */
    if (including_program_state) {
        memset(&program_state, 0, sizeof (program_state));
        memset(toplines, 0, sizeof (toplines));
        memset(toplines_count, 0, sizeof (toplines_count));

        viz_array = NULL;
    }

    memset(&flags, 0, sizeof (flags));
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
    memset(&inv_pos, 0, sizeof (inv_pos));

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
    migrating_mons = NULL;
    artilist = NULL;
    branch_id = 0;
    histevents = NULL;
    histcount = 0;
    timer_id = 1;
    curline = 0;

    flags.moonphase = 10;       /* invalid value, so that the first call to
                                   realtime_tasks will dtrt */
}

/*decl.c*/
