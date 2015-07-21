/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DECL_H
# define DECL_H

# include "flag.h"
# include "dungeon.h"
# include "monsym.h"
# include "xmalloc.h"
# include "spell.h"
# include "color.h"
# include "obj.h"
# include "role.h"
# include "you.h"
# include "onames.h"
# include "memfile.h"
# ifndef PM_H   /* (pm.h might already have been included via youprop.h) */
#  include "pm.h"
# endif

# define WARNCOUNT 6    /* number of different warning levels */

/* Read-only data */
static const int x_maze_max = (COLNO - 1) & ~1, y_maze_max = (ROWNO - 1) & ~1;

extern const int bases[MAXOCLASSES];

extern const char quitchars[];
extern const char vowels[];
extern const char ynchars[];
extern const char ynqchars[];
extern const char ynaqchars[];

extern const char disclosure_options[];

extern const schar xdir[], ydir[], zdir[];

/* Dungeon topology structure.

   TODO: This should be in a different file, probably. */
struct dgn_topology {    /* special dungeon levels for speed */
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
};


/* This structure is being used as part of the globals purge.

   Current meaning: "globals associated with the player character that aren't
   part of struct you".

   Eventual meaning: "values associated with a player that aren't associated
   with a monster".

   Ideally, we'll continuously deform one into the other. (Also, this is in the
   wrong file; you.h would make more sense.) */
struct youaux {
    int unused;
};


/* The big gamestate structure that holds all saved data. */
extern struct gamestate {

    /* Things that describe the dungeon as a whole. */
    dungeon dungeons[MAXDUNGEON];
    s_level *sp_levchn;
    struct dgn_topology topology;

    /* Things that are conceptually attached to specific levels of a dungeon,
       but for which only one such level exists, and for which the value may be
       needed globally (and thus it makes more sense to store them as part of
       the dungeon). These are currently saved in save_dungeon. */
    char castle_tune[6];
    coord inv_pos; /* TODO: use "vibrating square" trap instead */

    /* Things that describe individual levels of a dungeon. */

    /* Things that describe players within a dungeon.  (Eventually, this will
       just be a single monster chain.  But that's a *long* way off at the
       moment.) */
    struct you us[1];
    struct youaux us_aux[1];

    /* Fruit state. Arguably should be in flags. Currently, this is attached to
       a dungeon as a whole /or/ a bones level, which is weird enough as it is;
       also, it's affected by options, which is even weirder. Anyone have any
       idea how this would be expected to work in multiplayer? */
    struct {
        char curname[PL_FSIZ];
        int current;
        struct fruit *chain;
    } fruits;

    /* Unique IDs. TODO: move flags.ident here. TODO: Is this saved? If not, it
       should probably be a local rather than a global. */
    struct {
        int branch;
    } unique_ids;
} gamestate;

# define find_dungeon(z)        (gamestate.dungeons[(z)->dnum])
# define dunlev_reached(z)      (find_dungeon(z).dunlev_ureached)

# define MAXLINFO (MAXDUNGEON * MAXLEVEL)

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
extern boolean mrg_to_wielded;

extern boolean in_steed_dismounting;

extern const int shield_static[];

extern struct spell spl_book[]; /* sized in decl.c */


extern const char def_oc_syms[MAXOCLASSES];     /* default class symbols */
extern const char def_monsyms[MAXMCLASSES];     /* default class symbols */

extern int lastinvnr;

extern struct obj *invent;
extern struct obj zeroobj;      /* init'd and defined in decl.c */


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

extern unsigned int timer_id;

# define MSGCOUNT 30

extern char toplines[MSGCOUNT][BUFSZ];
extern int toplines_count[MSGCOUNT];
extern int curline;

/* Some game creation options don't have their values stored anywhere else.
   Thus, we store them in a local variable whose type is this structure while
   nh_create_game is running, and pass that variable around. */
struct newgame_options {
    char dogname[PL_PSIZ];
    char catname[PL_PSIZ];
    char horsename[PL_PSIZ];
    char preferred_pet;                  /* '\0', 'c', 'd', 'n' for 'none' */
};

# define add_menuitem(m, i, cap, acc, sel)      \
    add_menu_item((m), i, cap, acc, sel)
# define add_menuheading(m, c)                  \
    add_menu_txt((m), c, MI_HEADING)
# define add_menutext(m, c)                     \
    add_menu_txt((m), c, MI_TEXT)

/* Compatibility macros. */
# define u gamestate.us[0]

# define dungeon_topology       gamestate.topology
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


extern int logfile;

enum target_location_units {
    TLU_BYTES,
    TLU_TURNS,
    TLU_EOF
};

extern struct sinfo {
    int game_running;   /* ok to call nh_do_move */
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

    enum nh_followmode followmode;         /* play/watch/replay */

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
     * * emergency_recover_location is normally 0; if it isn't, panic() and
     *   impossible() should recover to that location rather than trying to
     *   find somewhere to recover to
     * * end_of_gamestate_location is the start of the line immediately after
     *   the one that gamestate_location point to.
     */
    volatile int logfile;                             /* file descriptor */
    void *logfile_watchers;                /* on UNIX, an array of pid_t */
    int logfile_watcher_count;
    struct memfile binary_save;
    boolean binary_save_allocated;
    int expected_recovery_count;
    long last_save_backup_location_location; /* bytes from start of file */
    long save_backup_location;               /* bytes from start of file */
    long binary_save_location;               /* bytes from start of file */
    long gamestate_location;                 /* bytes from start of file */
    long end_of_gamestate_location;          /* bytes from start of file */
    long emergency_recover_location;         /* bytes from start of file */
    boolean input_was_just_replayed;
    boolean ok_to_diff;
} program_state;

#define panic(...) panic_core(__FILE__, __LINE__, __VA_ARGS__)
#define impossible(...) impossible_core(__FILE__, __LINE__,  __VA_ARGS__)

extern const struct cmd_desc cmdlist[];

/* timezones, polyinit are generated in readonly.c */
extern const struct nh_listitem timezone_list[];
extern const struct nh_enum_option timezone_spec;

extern const struct nh_listitem polyinit_list[];
extern const struct nh_enum_option polyinit_spec;

#endif /* DECL_H */
