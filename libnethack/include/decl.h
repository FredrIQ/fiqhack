/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DECL_H
# define DECL_H

# include "flag.h"
# include "dungeon.h"
# include "monsym.h"
# include "xmalloc.h"

# define WARNCOUNT 6    /* number of different warning levels */

/* Read-only data */
extern const int x_maze_max, y_maze_max;

extern const int bases[MAXOCLASSES];

extern const char quitchars[];
extern const char vowels[];
extern const char ynchars[];
extern const char ynqchars[];
extern const char ynaqchars[];

extern const char disclosure_options[];

extern const char nul[];

extern const schar xdir[], ydir[], zdir[];

/* Dungeon topology */

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

extern int branch_id;
extern coord inv_pos;
extern dungeon dungeons[];
extern s_level *sp_levchn;

# define dunlev_reached(x)      (dungeons[(x)->dnum].dunlev_ureached)

extern char pl_character[PL_CSIZ];
extern char pl_race;    /* character's race */

extern char pl_fruit[PL_FSIZ];
extern int current_fruit;
extern struct fruit *ffruit;

extern char tune[6];

# define MAXLINFO (MAXDUNGEON * MAXLEVEL)


extern int smeq[];

# define KILLED_BY_AN    0
# define KILLED_BY       1
# define KILLED_BY_THE   2
# define NO_KILLER_PREFIX 3
extern int killer_format;
extern const char *killer;         /* message (in the sense of messages.c) */
extern const char *delayed_killer; /* must be a string literal */
extern int done_money;
extern const char *configfile;
extern char dogname[];
extern char catname[];
extern char horsename[];
extern char preferred_pet;

extern struct xmalloc_block *api_blocklist;

extern schar tbx, tby;  /* set in mthrowu.c */

extern struct multishot {
    int n, i;
    short o;
    boolean s;
} m_shot;

extern unsigned int moves;
extern long wailmsg;

extern boolean in_mklev;
extern boolean stoned;
extern boolean unweapon;
extern boolean mrg_to_wielded;

extern boolean in_steed_dismounting;

extern const int shield_static[];

# include "spell.h"
extern struct spell spl_book[]; /* sized in decl.c */

# include "color.h"

extern const char def_oc_syms[MAXOCLASSES];     /* default class symbols */
extern const char def_monsyms[MAXMCLASSES];     /* default class symbols */

# include "obj.h"
extern int lastinvnr;

extern struct obj *invent;
extern struct obj zeroobj;      /* init'd and defined in decl.c */

# include "role.h"
# include "you.h"
extern struct you u;

# include "onames.h"
# ifndef PM_H   /* (pm.h has already been included via youprop.h) */
#  include "pm.h"
# endif

extern struct monst youmonst;   /* init'd and defined in decl.c */
extern struct monst *migrating_mons;

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
    int (*func)(const struct nh_cmd_arg *);
    unsigned int flags;
};

struct histevent {
    unsigned int when, hidden;
    char what[BUFSZ];
};

extern unsigned int histcount;
extern struct histevent *histevents;


extern unsigned long long turntime;
extern unsigned int timer_id;

# define MSGCOUNT 30

extern char toplines[MSGCOUNT][BUFSZ];
extern int toplines_count[MSGCOUNT];
extern int curline;

# define add_menuitem(m, i, cap, acc, sel)      \
    add_menu_item((m), i, cap, acc, sel)
# define add_menuheading(m, c)                  \
    add_menu_txt((m), c, MI_HEADING)
# define add_menutext(m, c)                     \
    add_menu_txt((m), c, MI_TEXT)

# define MEMFILE_HASHTABLE_SIZE 1009

/* Seeks tend to have small integer arguments, whereas copies very often have
   all-bits-1 as an argument. Thus, using the value of 3 for MDIFF_COPY means
   that a "no-change" diff will be all-bits-1 up until the end, making it a
   little more compressible. */
# define MDIFF_SEEK 0
# define MDIFF_EDIT 2
# define MDIFF_COPY 3
# define MDIFF_INVALID 255

enum memfile_tagtype {
    MTAG_START,             /* 0 */
    MTAG_WATERLEVEL,
    MTAG_DUNGEONSTRUCT,
    MTAG_BRANCH,
    MTAG_DUNGEON,
    MTAG_REGION,            /* 5 */
    MTAG_YOU,
    MTAG_VERSION,
    MTAG_WORMS,
    MTAG_ROOMS,
    MTAG_HISTORY,           /* 10 */
    MTAG_ORACLES,
    MTAG_TIMER,
    MTAG_TIMERS,
    MTAG_LIGHT,
    MTAG_LIGHTS,            /* 15 */
    MTAG_OBJ,
    MTAG_TRACK,
    MTAG_OCLASSES,
    MTAG_RNDMONST,
    MTAG_MON,               /* 20 */
    MTAG_STEAL,
    MTAG_ARTIFACT,
    MTAG_RNGSTATE,
    MTAG_LEVEL,
    MTAG_LEVELS,            /* 25 */
    MTAG_MVITALS,
    MTAG_GAMESTATE,
    MTAG_DAMAGE,
    MTAG_DAMAGEVALUE,
    MTAG_TRAP,              /* 30 */
    MTAG_FRUIT,
    MTAG_ENGRAVING,
    MTAG_FLAGS,
    MTAG_STAIRWAYS,
    MTAG_LFLAGS,            /* 35 */
    MTAG_LOCATIONS,
    MTAG_OPTION,
    MTAG_OPTIONS,
    MTAG_AUTOPICKUP_RULE,
    MTAG_AUTOPICKUP_RULES,  /* 40 */
    MTAG_DUNGEON_TOPOLOGY,
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

enum target_location_units {
    TLU_BYTES,
    TLU_TURNS,
    TLU_EOF
};

extern struct sinfo {
    int game_running;   /* ok to call nh_do_move */
    int viewing;        /* replaying or watching a game */
    int gameover;       /* self explanatory? */
    int stopprint;      /* inhibit further end of game disclosure */
    int panicking;      /* `panic' is in progress */
# if defined(WIN32)
    int exiting;        /* an exit handler is executing */
# endif
    int in_impossible;
# ifdef PANICLOG
    int in_paniclog;
# endif

    boolean suppress_screen_updates;
    boolean restoring_binary_save;
    boolean in_zero_time_command;

    /*
     * Invariants:
     * * binary_save is uninitialized (and doesn't point to memory that needs
     *   freeing) if binary_save_allocated is false, and the binary save
     *   matching binary_save_location if binary_save_allocated is true;
     * * save_backup_location <= binary_save_location <= gamestate_location;
     * * save_backup_location refers to a save backup (or, if log_sync() has
     *   never been called for this save file, may be 0, but log_sync() is
     *   meant to be called immediately upon loading it);
     * * last_save_backup_location_location is one byte past the first save
     *   backup in the file;
     * * gamestate_location points to the location in the save file that
     *   reflects the start of the line referring to the current gamestate (in
     *   u, level, etc.), except possibly while log_sync() is running.
     * * end_of_gamestate_location is the start of the line immediately after
     *   the one that gamestate_location point to.
     * Note that there are no invariants on target_location. You can call
     * log_sync() to set the binary_save_location as close as possible to
     * target_location, but nothing forces you to do this.
     */
    int logfile;                                      /* file descriptor */
    struct memfile binary_save;
    boolean binary_save_allocated;
    int expected_recovery_count;
    long last_save_backup_location_location; /* bytes from start of file */
    long save_backup_location;               /* bytes from start of file */
    long binary_save_location;               /* bytes from start of file */
    long gamestate_location;                 /* bytes from start of file */
    long end_of_gamestate_location;          /* bytes from start of file */
    long target_location;           /* in units of target_location_units */
    enum target_location_units target_location_units;
    boolean input_was_just_replayed;
    boolean ok_to_diff;
} program_state;

#endif /* DECL_H */

