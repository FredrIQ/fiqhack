/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

int (*afternmv) (void);
int (*occupation) (void);

int bases[MAXOCLASSES];

int multi;
char multi_txt[BUFSZ];
int occtime;

int x_maze_max, y_maze_max;     /* initialized in main, used in mkmaze.c */
int otg_temp;   /* used by object_to_glyph() [otg] */

/*
 * The following structure will be initialized at startup time with
 * the level numbers of some "important" things in the game.
 */
struct dgn_topology dungeon_topology;

#include "quest.h"
struct q_score quest_status;

int smeq[MAXNROFROOMS + 1];

int saved_cmd;
int killer_format;
const char *killer;
const char *delayed_killer;
int done_money;
char killer_buf[BUFSZ];
const char *nomovemsg;
const char nul[40];     /* contains zeros */
char plname[PL_NSIZ];   /* player name */
char pl_character[PL_CSIZ];
char pl_race;

char pl_fruit[PL_FSIZ];
int current_fruit;
struct fruit *ffruit;

char tune[6];

const char *occtxt;
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

unsigned int stetho_last_used_move;
int stetho_last_used_movement;

int branch_id;
dungeon dungeons[MAXDUNGEON];   /* ini'ed by init_dungeon() */
s_level *sp_levchn;
coord inv_pos;  /* vibrating square position */

boolean in_mklev;
boolean stoned; /* done to monsters hit by 'c' */
boolean unweapon;
boolean mrg_to_wielded;

                         /* weapon picked is merged with wielded one */
struct obj *current_wand;       /* wand currently zapped/applied */

boolean in_steed_dismounting;

coord bhitpos;

struct level *levels[MAXLINFO];
struct level *level;    /* level map */

struct monst youmonst;
struct flag flags;
struct instance_flags iflags;
struct you u;

struct obj *invent, *uwep, *uarm, *uswapwep, *uquiver,  /* quiver */
   *uarmu,      /* under-wear, so to speak */
   *uskin,      /* dragon armor, if a dragon */
   *uarmc, *uarmh, *uarms, *uarmg, *uarmf, *uamul, *uright, *uleft, *ublindf,
    *uchain, *uball;
int lastinvnr;  /* 0 ... 51 */

const int shield_static[SHIELD_COUNT] = {
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,    /* 7 per row */
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
};

struct spell spl_book[MAXSPELL + 1];

unsigned int moves;
long wailmsg;

/* last/current book being read */
struct obj *book;

/* used to zero all elements of a struct obj */
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
    "black",    /* CLR_BLACK */
    "red",      /* CLR_RED */
    "green",    /* CLR_GREEN */
    "brown",    /* CLR_BROWN */
    "blue",     /* CLR_BLUE */
    "magenta",  /* CLR_MAGENTA */
    "cyan",     /* CLR_CYAN */
    "gray",     /* CLR_GRAY */
    "transparent",      /* no_color */
    "orange",   /* CLR_ORANGE */
    "bright green",     /* CLR_BRIGHT_GREEN */
    "yellow",   /* CLR_YELLOW */
    "bright blue",      /* CLR_BRIGHT_BLUE */
    "bright magenta",   /* CLR_BRIGHT_MAGENTA */
    "bright cyan",      /* CLR_BRIGHT_CYAN */
    "white",    /* CLR_WHITE */
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
int current_timezone, replay_timezone;
unsigned int timer_id = 1;

/* If one game (A) is started and then saved, followed by game B with different
 * birth_options, after which game A is restored, then A must run with it's
 * original birth_options, rather than the most recent birth_options which were
 * set for game B. */
struct nh_option_desc *active_birth_options;
struct nh_option_desc *birth_options;
struct nh_option_desc *options;

char toplines[MSGCOUNT][BUFSZ];
int curline;


void
init_data(void)
{
    boolean in_restore = program_state.restoring;

    /* iflags may already contain valid, important data, because init_data()
       runs as part of the game init sequence after options have been set, etc. 
     */
    boolean nolog = iflags.disable_log;
    struct nh_autopickup_rules *rules = iflags.ap_rules;

    moves = 1;

    memset(&program_state, 0, sizeof (program_state));
    memset(&flags, 0, sizeof (flags));
    memset(&iflags, 0, sizeof (iflags));
    memset(&quest_status, 0, sizeof (quest_status));
    memset(&levels, 0, sizeof (levels));
    memset(bases, 0, sizeof (bases));
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
    memset(multi_txt, 0, sizeof (multi_txt));

    level = NULL;
    multi = occtime = killer_format = 0;
    afternmv = NULL;
    occupation = NULL;
    killer = NULL;
    ffruit = NULL;
    current_fruit = 0;
    sp_levchn = NULL;
    in_mklev = stoned = unweapon = mrg_to_wielded = FALSE;
    current_wand = invent = uwep = uarm = uswapwep = uquiver = uarmu = uskin =
        uarmc = uarmh = uarms = uarmg = uarmf = uamul = uright = uleft =
        ublindf = uchain = uball = NULL;
    book = NULL;
    in_steed_dismounting = FALSE;
    wailmsg = 0;
    bhitpos.x = bhitpos.y = 0;
    preferred_pet = 0;
    migrating_mons = mydogs = NULL;
    vision_full_recalc = FALSE;
    viz_array = NULL;
    artilist = NULL;
    stetho_last_used_movement = 0;
    stetho_last_used_move = -1;
    branch_id = 0;
    histevents = NULL;
    histcount = 0;
    timer_id = 1;

    program_state.restoring = in_restore;
    iflags.disable_log = nolog;
    iflags.ap_rules = rules;
    flags.moonphase = 10;       /* invalid value, so that the first call to
                                   realtime_tasks will dtrt */
    lastinvnr = 51;
    flags.soundok = 1;
}

/*decl.c*/
