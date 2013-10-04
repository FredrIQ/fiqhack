/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DECL_H
# define DECL_H

extern int (*occupation) (void);
extern int (*afternmv) (void);

extern int bases[MAXOCLASSES];

extern int multi;
extern char multi_txt[BUFSZ];
extern int occtime;

# define WARNCOUNT 6    /* number of different warning levels */

extern int x_maze_max, y_maze_max;
extern int otg_temp;

extern struct dgn_topology {    /* special dungeon levels for speed */
    d_level d_oracle_level;
    d_level d_bigroom_level;    /* unused */
    d_level d_rogue_level;
    d_level d_medusa_level;
    d_level d_stronghold_level;
    d_level d_valley_level;
    d_level d_wiz1_level;
    d_level d_wiz2_level;
    d_level d_wiz3_level;
    d_level d_juiblex_level;
    d_level d_orcus_level;
    d_level d_baalzebub_level;  /* unused */
    d_level d_asmodeus_level;   /* unused */
    d_level d_portal_level;     /* only in goto_level() [do.c] */
    d_level d_sanctum_level;
    d_level d_earth_level;
    d_level d_water_level;
    d_level d_fire_level;
    d_level d_air_level;
    d_level d_astral_level;
    xchar d_tower_dnum;
    xchar d_sokoban_dnum;
    xchar d_mines_dnum, d_quest_dnum;
    d_level d_qstart_level, d_qlocate_level, d_nemesis_level;
    d_level d_knox_level;
} dungeon_topology;

/* macros for accesing the dungeon levels by their old names */
# define oracle_level           (dungeon_topology.d_oracle_level)
# define bigroom_level          (dungeon_topology.d_bigroom_level)
# define rogue_level            (dungeon_topology.d_rogue_level)
# define medusa_level           (dungeon_topology.d_medusa_level)
# define stronghold_level       (dungeon_topology.d_stronghold_level)
# define valley_level           (dungeon_topology.d_valley_level)
# define wiz1_level             (dungeon_topology.d_wiz1_level)
# define wiz2_level             (dungeon_topology.d_wiz2_level)
# define wiz3_level             (dungeon_topology.d_wiz3_level)
# define juiblex_level          (dungeon_topology.d_juiblex_level)
# define orcus_level            (dungeon_topology.d_orcus_level)
# define baalzebub_level        (dungeon_topology.d_baalzebub_level)
# define asmodeus_level         (dungeon_topology.d_asmodeus_level)
# define portal_level           (dungeon_topology.d_portal_level)
# define sanctum_level          (dungeon_topology.d_sanctum_level)
# define earth_level            (dungeon_topology.d_earth_level)
# define water_level            (dungeon_topology.d_water_level)
# define fire_level             (dungeon_topology.d_fire_level)
# define air_level              (dungeon_topology.d_air_level)
# define astral_level           (dungeon_topology.d_astral_level)
# define tower_dnum             (dungeon_topology.d_tower_dnum)
# define sokoban_dnum           (dungeon_topology.d_sokoban_dnum)
# define mines_dnum             (dungeon_topology.d_mines_dnum)
# define quest_dnum             (dungeon_topology.d_quest_dnum)
# define qstart_level           (dungeon_topology.d_qstart_level)
# define qlocate_level          (dungeon_topology.d_qlocate_level)
# define nemesis_level          (dungeon_topology.d_nemesis_level)
# define knox_level             (dungeon_topology.d_knox_level)

# define xdnstair               (dnstair.sx)
# define ydnstair               (dnstair.sy)
# define xupstair               (upstair.sx)
# define yupstair               (upstair.sy)

extern int branch_id;
extern coord inv_pos;
extern dungeon dungeons[];
extern s_level *sp_levchn;

# define dunlev_reached(x)      (dungeons[(x)->dnum].dunlev_ureached)

# include "quest.h"
extern struct q_score quest_status;

extern char pl_character[PL_CSIZ];
extern char pl_race;    /* character's race */

extern char pl_fruit[PL_FSIZ];
extern int current_fruit;
extern struct fruit *ffruit;

extern struct sinfo {
    int game_running;   /* ok to call nh_do_move */
    int restoring;      /* game is currently non-interactive (user input via
                           log restore) */
    int viewing;        /* non-interactive: viewing a game replay */
    int gameover;       /* self explanatory? */
    int forced_exit;    /* exit was triggered by nh_exit_game */
    int stopprint;      /* inhibit further end of game disclosure */
    int something_worth_saving; /* in case of panic */
    int panicking;      /* `panic' is in progress */
# if defined(WIN32)
    int exiting;        /* an exit handler is executing */
# endif
    int in_impossible;
# ifdef PANICLOG
    int in_paniclog;
# endif
} program_state;

extern char tune[6];

# define MAXLINFO (MAXDUNGEON * MAXLEVEL)
extern boolean restoring;

extern const char quitchars[];
extern const char vowels[];
extern const char ynchars[];
extern const char ynqchars[];
extern const char ynaqchars[];

extern const char disclosure_options[];

extern int smeq[];
extern int saved_cmd;

# define KILLED_BY_AN    0
# define KILLED_BY       1
# define NO_KILLER_PREFIX 2
extern int killer_format;
extern const char *killer;
extern const char *delayed_killer;
extern int done_money;
extern char killer_buf[BUFSZ];
extern const char *configfile;
extern char plname[PL_NSIZ];
extern char dogname[];
extern char catname[];
extern char horsename[];
extern char preferred_pet;
extern const char *occtxt;      /* defined when occupation != NULL */
extern const char *nomovemsg;
extern const char nul[];

extern const schar xdir[], ydir[], zdir[];

extern schar tbx, tby;  /* set in mthrowu.c */

extern struct multishot {
    int n, i;
    short o;
    boolean s;
} m_shot;

extern struct dig_info {        /* apply.c, hack.c */
    int effort;
    int lastdigtime;
    d_level level;
    coord pos;
    boolean down, chew, warned, quiet;
} digging;

extern unsigned int stetho_last_used_move;
extern int stetho_last_used_movement;

extern unsigned int moves;
extern long wailmsg;

extern boolean in_mklev;
extern boolean stoned;
extern boolean unweapon;
extern boolean mrg_to_wielded;
extern struct obj *current_wand;

extern boolean in_steed_dismounting;

extern const int shield_static[];

# include "spell.h"
extern struct spell spl_book[]; /* sized in decl.c */

# include "color.h"

extern const char def_oc_syms[MAXOCLASSES];     /* default class symbols */
extern const char def_monsyms[MAXMCLASSES];     /* default class symbols */

# include "obj.h"
extern struct obj *invent, *uarm, *uarmc, *uarmh, *uarms, *uarmg, *uarmf,
    *uarmu,       /* under-wear, so to speak */
    *uskin, *uamul, *uleft, *uright, *ublindf, *uwep, *uswapwep, *uquiver;
extern int lastinvnr;

extern struct obj *uchain;      /* defined only when punished */
extern struct obj *uball;
extern struct obj *book;
extern struct obj zeroobj;      /* init'd and defined in decl.c */

# include "role.h"
# include "you.h"
extern struct you u;

# include "onames.h"
# ifndef PM_H   /* (pm.h has already been included via youprop.h) */
#  include "pm.h"
# endif

extern struct monst youmonst;   /* init'd and defined in decl.c */
extern struct monst *mydogs, *migrating_mons;

extern struct mvitals {
    uchar born;
    uchar died;
    uchar mvflags;
} mvitals[NUMMONS];

/* The names of the colors used for gems, etc. */
extern const char *const c_obj_colors[];

extern const char *const the_your[];

/* material strings */
extern const char *const materialnm[];

/* Monster name articles */
# define ARTICLE_NONE   0
# define ARTICLE_THE    1
# define ARTICLE_A      2
# define ARTICLE_YOUR   3

/* Monster name suppress masks */
# define SUPPRESS_IT            0x01
# define SUPPRESS_INVISIBLE     0x02
# define SUPPRESS_HALLUCINATION 0x04
# define SUPPRESS_SADDLE        0x08
# define EXACT_NAME             0x0F

/* Vision */
extern boolean vision_full_recalc;      /* TRUE if need vision recalc */
extern char **viz_array;        /* could see/in sight row pointers */

/* xxxexplain[] is in drawing.c */
extern const char *const monexplain[], *const oclass_names[];

/* used in files.c; xxconf.h can override if needed */
# ifndef FQN_MAX_FILENAME
#  define FQN_MAX_FILENAME 512
# endif


extern char *fqn_prefix[PREFIX_COUNT];
extern const char *const fqn_prefix_names[PREFIX_COUNT];

struct object_pick {
    struct obj *obj;
    int count;
};

struct menulist {
    struct nh_menuitem *items;
    int size;
    int icount;
};

extern struct permonst pm_leader, pm_guardian, pm_nemesis;
extern struct permonst pm_you_male, pm_you_female;

/* type of setjmp calls: sigsetjmp for UNIX, plain setjmp otherwise. */
# ifdef UNIX
#  define nh_jmp_buf sigjmp_buf
#  define nh_longjmp(buf, val) siglongjmp(buf, val)
#  define nh_setjmp(buf) sigsetjmp(buf, 1)
# else
#  define nh_jmp_buf jmp_buf
#  define nh_longjmp(buf, val) longjmp(buf, val)
#  define nh_setjmp(buf) setjmp(buf)
# endif

extern int exit_jmp_buf_valid;
extern nh_jmp_buf exit_jmp_buf;

extern struct artifact *artilist;
extern short disco[NUM_OBJECTS];

struct cmd_desc {
    const char *name;
    const char *desc;
    char defkey, altkey;
    boolean can_if_buried;
    const void *func;
    unsigned int flags;
    const char *text;
};

struct histevent {
    unsigned int when, hidden;
    char what[BUFSZ];
};

extern unsigned int histcount;
extern struct histevent *histevents;


extern unsigned long long turntime;
extern int current_timezone,
    replay_timezone;   /* difference from UTC in seconds */
extern unsigned int timer_id;

extern struct nh_option_desc *active_birth_options;
extern struct nh_option_desc *birth_options;
extern struct nh_option_desc *options;

# define MSGCOUNT 20

extern char toplines[MSGCOUNT][BUFSZ];
extern int curline;

# define add_menuitem(m, i, cap, acc, sel)\
    add_menu_item((m)->items, (m)->size, (m)->icount, i, cap, acc, sel)
# define add_menuheading(m, c)\
    add_menu_txt((m)->items, (m)->size, (m)->icount, c, MI_HEADING)
# define add_menutext(m, c)\
    add_menu_txt((m)->items, (m)->size, (m)->icount, c, MI_TEXT)

# define MEMFILE_HASHTABLE_SIZE 1009

# define MDIFF_SEEK 0
# define MDIFF_COPY 1
# define MDIFF_EDIT 2
# define MDIFF_INVALID 255

enum memfile_tagtype {
    MTAG_START, /* 0 */
    MTAG_WATERLEVEL,
    MTAG_DUNGEONSTRUCT,
    MTAG_BRANCH,
    MTAG_DUNGEON,
    MTAG_REGION,        /* 5 */
    MTAG_YOU,
    MTAG_VERSION,
    MTAG_WORMS,
    MTAG_ROOMS,
    MTAG_HISTORY,       /* 10 */
    MTAG_ORACLES,
    MTAG_TIMER,
    MTAG_TIMERS,
    MTAG_LIGHT,
    MTAG_LIGHTS,        /* 15 */
    MTAG_OBJ,
    MTAG_TRACK,
    MTAG_OCLASSES,
    MTAG_RNDMONST,
    MTAG_MON,   /* 20 */
    MTAG_STEAL,
    MTAG_ARTIFACT,
    MTAG_RNGSTATE,
    MTAG_LEVELS,
    MTAG_LEVEL, /* 25 */
    MTAG_MVITALS,
    MTAG_GAMESTATE,
    MTAG_DAMAGE,
    MTAG_DAMAGEVALUE,
    MTAG_TRAP,  /* 30 */
    MTAG_FRUIT,
    MTAG_ENGRAVING,
};
struct memfile_tag {
    struct memfile_tag *next;
    long tagdata;
    enum memfile_tagtype tagtype;
    int pos;
};
struct memfile {
    /* The basic information: the buffer, its length, and the file position */
    char *buf;
    int len;
    int pos;
    /* Difference memfiles are relative to another memfile; and they contain
       both the actual data in buf, and the diffed data in diffbuf */
    struct memfile *relativeto;
    char *diffbuf;
    int difflen;
    int diffpos;
    int relativepos;    /* pos corresponds to relativepos in diffbuf */
    /* Run-length-encoding of diffs. Either curcmd is MDIFF_INVALID and
       curcount is irrelevant, or curcmd is a command and curcount is a count
       matching that command. Note that we allow a negative "runlength" for
       seek, so we can encode both forwards and backwards seeks. */
    uint8_t curcmd;
    int16_t curcount;
    /* Tags to help in diffing. This is a hashtable for efficiency, using
       chaining in the case of collisions */
    struct memfile_tag *tags[MEMFILE_HASHTABLE_SIZE];
};

extern int logfile;

#endif /* DECL_H */
