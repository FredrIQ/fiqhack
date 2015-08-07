/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
#ifndef NETHACK_TYPES_H
# define NETHACK_TYPES_H

/*
 * System autodetection: greatly simplified, as we only care about
 * Unix (in all its variations) and Windows
 * CMake just sets UNIX, and only sometimes :)
 * AIMake doesn't set either directly, but does set AIMAKE_BUILDOS_MSWin32
 * on Windows, so we can just use that
 */
# ifdef AIMAKE_BUILDOS_MSWin32
#  undef WIN32
#  define WIN32
# endif
# ifndef UNIX
#  define UNIX
#  ifdef WIN32
#   undef UNIX
#   define STRNCMPI
#   define STRCMPI
#  endif
# endif

# ifndef TRUE   /* defined in some systems' native include files */
#  define FALSE ((nh_bool)0)
#  define TRUE  ((nh_bool)!0)
# endif

/* size of terminal screen is (at least) (ROWNO+3) by (COLNO+1) */
# define COLNO          79
# define ROWNO          21

# define BUFSZ          256     /* for getlin buffers */
# define QBUFSZ         128     /* for building question text */
# define PL_NSIZ        32      /* name of player, ghost, shopkeeper */
# define PLRBUFSZ       16      /* player race/role names */

# define AUTOPICKUP_PATTERNSZ   40
                                

# define FCMASK         0660    /* file creation mask */

# define ROLE_NONE      (-1)
# define ROLE_RANDOM    (-2)

# define A_CHAOTIC      (-1)
# define A_NEUTRAL       0
# define A_LAWFUL        1

/*
 * The color scheme used is tailored for an IBM PC.  It consists of the
 * standard 8 colors, folowed by their bright counterparts.  There are
 * exceptions, these are listed below.  Bright black doesn't mean very
 * much, so it is used as the "default" foreground color of the screen.
 */
# define CLR_BLACK              0
# define CLR_RED                1
# define CLR_GREEN              2
# define CLR_BROWN              3       /* on IBM, low-intensity yellow is
                                           brown */
# define CLR_BLUE               4
# define CLR_MAGENTA            5
# define CLR_CYAN               6
# define CLR_GRAY               7       /* low-intensity white */
# define CLR_DARK_GRAY          8
# define CLR_ORANGE             9
# define CLR_BRIGHT_GREEN       10
# define CLR_YELLOW             11
# define CLR_BRIGHT_BLUE        12
# define CLR_BRIGHT_MAGENTA     13
# define CLR_BRIGHT_CYAN        14
# define CLR_WHITE              15
# define CLR_MAX                16

/*
 * Additional effects.
 */
# define HI_ULINE        0x20

/* flags for displayed monsters */
# define MON_NORMAL   0
# define MON_TAME     (1 << 0)
# define MON_RIDDEN   (1 << 1)
# define MON_DETECTED (1 << 2)
# define MON_WARNING  (1 << 3)  /* this "monster" is actually a warning */
# define MON_PEACEFUL (1 << 4)

/* 
 * level display modes
 * These defines are used by notify_levelchange() to inform the window port 
 * about some characteristic of the new level that might be worth displaying
 * in some nonstandard way (eg the rogue level in tty nethack, or alternate
 * tiles for mines/hell in tiles versions). When you change this list, update
 * nethack/brandings.c.
 */
# define LDM_DEFAULT     0
# define LDM_HELL        1
# define LDM_QUESTHOME   2
# define LDM_MINES       3
# define LDM_SOKOBAN     4
# define LDM_ROGUE       5
# define LDM_KNOX        6
# define LDM_QUESTFILL1  7
# define LDM_QUESTLOCATE 8
# define LDM_QUESTFILL2  9
# define LDM_QUESTGOAL   10
# define LDM_VLAD        11
# define LDM_ASTRAL      12

# define LDM_COUNT       13      /* number of level display modes */

/* Command parameters.
 *
 * A command description specifies a set of parameters that it understands. All
 * the parameters are optional. Because the client does not usually have enough
 * information to explain to the user what an argument means, it should normally
 * omit arguments, in which case the engine will prompt for them if necessary.
 *
 * There are some exceptions to this:
 *
 * * CMD_ARG_LIMIT is never prompted for by the engine (omitting it means "no
 *   limit"); a client should send it if the user specified a limit on the
 *   command (say, using main keyboard numbers in a vikeys-based interface), and
 *   not otherwise. If a command does not have a CMD_ARG_LIMIT tag, a client
 *   should typically interpret what would be a limit as command repeat instead.
 *   (A limit is an integer that specifies some number which should not be
 *   exceeded when running the command, e.g. the maximum number of 
 *
 * * CMD_ARG_OBJ should normally be omitted, but in clients that use a
 *   menu-based interface in combination with get_obj_commands(), they have
 *   enough information to tell the user what the argument means.
 *
 * * CMD_MOVE, movement commands, should always be given a direction.
 *
 * * Some interfaces for other roguelikes work via having an ability to specify
 *   a default direction, which is used for all commands. At the time of writing
 *   there are no NetHack interfaces which work on this principle, but if any
 *   are written, they should feel free to specify CMD_ARG_DIR in every response
 *   where it makes sense. A client should /never/ prompt the user for a
 *   direction except in response to a getpos() call or if CMD_MOVE is set;
 *   CMD_ARG_DIR is sometimes specified on commands (like zap) where a direction
 *   is sometimes relevant but sometimes irrelevant. Thus, it should only be
 *   sent if the server has asked for it, repeating a command when the server
 *   asked for it on a previous repeat, or as a result of an interface like the
 *   one described above.
 */

# define CMD_ARG_DIR   (1 << 0)         /* param can be a direction */
# define CMD_ARG_POS   (1 << 1)         /* param can be a position */
# define CMD_ARG_OBJ   (1 << 2)         /* param can be an inventory letter */
# define CMD_ARG_STR   (1 << 3)         /* param can be a string */
# define CMD_ARG_SPELL (1 << 4)         /* param can be a spell letter */
# define CMD_ARG_LIMIT (1 << 5)         /* param can be a limit */

/* command usage hints */
# define CMD_EXT        (1 << 10)       /* an 'extended' command */
# define CMD_MOVE       (1 << 11)       /* this is a move command */
# define CMD_HELP       (1 << 12)       /* this command should be listed on the 
                                           help menu */
# define CMD_NOTIME     (1 << 13)       /* command does not use any time or
                                           alter the game state in any way.
                                           Marked commands will not be logged */
# define CMD_DEBUG      (1 << 14)       /* a wizmode command */
# define CMD_INTERNAL   (1 << 15)       /* sent automatically by the client;
                                           not meaningful if sent manually */
# define CMD_MAINMENU   (1 << 16)       /* command is related to the character
                                           or dungeon, and not available through
                                           itemactions */

/* note that CMD_INTERNAL commands should be ones that cannot be used to cheat
   if they're nonetheless sent manually anyway */

/* reserved flag for use by ui code that uses struct nh_cmd_desc internally */
# define CMD_UI         (1U << 31)

# define AUTOPICKUP_MAX_RULES 1000      /* this is intended as a rough sanity
                                           check to detect pointers to
                                           autopickup rule structs that instead 
                                           point at random memory */
# define OCLASS_ANY 'a' /* for autopickup */

# define SERVERCANCEL_CHAR '\x1c'

enum nh_direction {
    DIR_SERVERCANCEL = -2,
    DIR_NONE = -1,
    DIR_W = 0,
    DIR_NW,
    DIR_N,
    DIR_NE,
    DIR_E,
    DIR_SE,
    DIR_S,
    DIR_SW,
    DIR_UP,
    DIR_DOWN,
    DIR_SELF
};

/* select_menu() "how" argument types */
enum nh_pick_type {
    PICK_NONE,     /* user picks nothing (display only) */
    PICK_ONE,      /* only pick one */
    PICK_LETTER,   /* can pick any letter */
    PICK_ANY,      /* can pick any amount */
};

/* Always use full pathnames for file names, rather than assuming that they're
   all in the current directory. This provides all the subclasses that seem
   reasonable. An array listing a path for each prefix must be passed to
   nh_lib_init(). (You can also use the value $OMIT in some cases, to turn off
   processing for files that would be placed in that directory; this feature is
   used by testsuite and might be useful elsewhere.) */
enum nh_path_prefix {
    BONESPREFIX = 0,  /* read/write; $OMIT: disable bones */
    DATAPREFIX,       /* read-only;  $OMIT: illegal */
    SCOREPREFIX,      /* read/write; $OMIT: illegal */
    LOCKPREFIX,       /* currently unused */
    TROUBLEPREFIX,    /* write-only; $OMIT: disable panic logging */
    DUMPPREFIX,       /* write-only; $OMIT: disable dumplogs */
    PREFIX_COUNT
};

enum nh_game_modes {
    MODE_NORMAL,
    MODE_EXPLORE,
    MODE_WIZARD,
    MODE_SETSEED
};

enum nh_followmode {
    FM_PLAY,
    /*
     * Playing the game.
     *
     * - request_command: all commands are valid and interpreted as normal;
     *   if commands already exist, they are replayed immediately
     * - windowprocs: input is accepted, although input from the save file is
     *   favoured if it exists
     * - server cancels: recheck the save file, if there's nothing there, then
     *   re-ask the user
     */

    FM_WATCH,
    /*
     * Watching a game.
     *
     * - request_command: zero-time commands are accepted; all others produce
     *   error messages; if commands already exist (or arrive), they are
     *   replayed immediately
     * - windowprocs: input is not accepted; all input is interpreted as a
     *   server cancel
     * - server cancels: recheck the save file, if there's nothing there, go
     *   back to what you were doing
     */

    FM_REPLAY,
    /*
     * Replaying a game.
     *
     * - request_command: zero-time commands are accepted; movement commands
     *   seek in the save file; other commands produce an error message;
     *   pre-existing and arriving commands are left there unread until the
     *   player seeks past them
     * - windowprocs: input is read from the save file if available; otherwise
     *   a "this is the end of the replay" box is displayed, and the turn
     *   restarts if it is dismissed (rather than detached)
     * - server cancels: are ignored
     */

    FM_RECOVERQUIT,
    /*
     * Redoing the endgame sequence.
     *
     * - request_command: pre-existing commands are replayed; if there is no
     *   pre-existing command, raise an error
     * - windowprocs: input is read from the save file if available; otherwise
     *   yn_function() returns 'n' (to skip disclose), other functions error out
     * - server cancels: cannot occur (because we never prompt the client)
     */
};

enum nh_pause_reason {
    P_MESSAGE,
    P_MAP
};

enum nh_opttype {
    OPTTYPE_BOOL,
    OPTTYPE_INT,
    OPTTYPE_ENUM,
    OPTTYPE_STRING,
    OPTTYPE_AUTOPICKUP_RULES    /* so this is a special case... I considered
                                   creating a general purpose mechanism, but I
                                   came to the conclusion that YAGNI applies */
};

enum nh_bucstatus {
    B_UNKNOWN,
    B_BLESSED,
    B_UNCURSED,
    B_CURSED,
    B_DONT_CARE /* objects never have this; it's used for autopickup matching */
};

enum nh_menuitem_role {
    MI_TEXT = 0,
    MI_NORMAL,
    MI_HEADING
};

enum nh_command_status {
    COMMAND_DEBUG_ONLY,
    COMMAND_UNKNOWN,
    COMMAND_BAD_ARG,
    COMMAND_OK,
    COMMAND_ZERO_TIME
};

enum nh_effect_types {
    E_EXPLOSION,
    E_SWALLOW,
    E_ZAP,
    E_MISC
};

enum nh_exit_types {
    EXIT_SAVE,
    EXIT_QUIT,
    EXIT_PANIC,
    EXIT_RESTART,
};

enum nh_play_status {
    /* The game loaded */
    GAME_DETACHED,      /* the game loaded, then unloaded, it still exists */
    GAME_OVER,          /* the game loaded, but now it's over */
    GAME_ALREADY_OVER,  /* the game loaded, some other process ended it */
    RESTART_PLAY,       /* the server needed to longjmp out of nh_play_game; try
                           giving the same call again */
    CLIENT_RESTART,     /* ditto, except it was the client that needed to */
    REPLAY_FINISHED,    /* the game loaded in replay mode, the player tried to
                           step off the end of the replay; the game itself is
                           still ongoing, though */

    /* The game didn't load */
    ERR_BAD_ARGS,       /* game ID does not exist, or fd is out of range */
    ERR_BAD_FILE,       /* file isn't a saved game */
    ERR_IN_PROGRESS,    /* locking issues attaching to the game */
    ERR_RESTORE_FAILED, /* game needs manual recovery */
    ERR_RECOVER_REFUSED,/* automatic recovery was declined */

    /* Special statuses, used internally only */
    ERR_NETWORK_ERROR,  /* generated by client lib if something goes wrong */
    ERR_CREATE_FAILED,  /* the game did not start because it wasn't created */
    GAME_CREATED,       /* end of the newgame sequence was reached */
};

enum nh_log_status {
    LS_CRASHED = -2,    /* the game crashed (or was "kill -9"ed) */
    LS_INVALID = -1,    /* not a nethack log/savegame */
    LS_SAVED,           /* an ordinary save */
    LS_DONE,            /* quit, died, ascended, etc */
    LS_IN_PROGRESS      /* locking issues trying to obtain save information */
};

enum autopickup_action {
    AP_GRAB,
    AP_LEAVE
};

enum replay_control {
    REPLAY_FORWARD,
    REPLAY_BACKWARD,
    REPLAY_GOTO
};

enum placement_hint {
    PLHINT_ANYWHERE,
    PLHINT_LEFT,
    PLHINT_RIGHT,
    PLHINT_URGENT,
    PLHINT_INFO,
    PLHINT_ONELINER,
    PLHINT_CONTAINER,
    PLHINT_INVENTORY
};

/* When doing a create over the network, instead of an OK, we will get back the
 * game number. Thus negative values are used for errors, so they are errors in
 * both the local and network versions of the call.
 */
enum nh_create_response {
    NHCREATE_OK,
    NHCREATE_INVALID = -1,
    NHCREATE_FAIL = -2
};

enum nh_client_response {
    NHCR_ACCEPTED,
    NHCR_CLIENT_CANCEL,
    NHCR_CONTINUE, 
    NHCR_MOREINFO,
    NHCR_MOREINFO_CONTINUE,
    NHCR_SERVER_CANCEL,
};

/* Note: if you change these, also change nhcurses.h */
enum nh_query_key_flags {
    NQKF_INVENTORY_ITEM,
    NQKF_INVENTORY_ITEM_NULLABLE,
    NQKF_INVENTORY_OR_FLOOR,
    NQKF_SYMBOL,
    NQKF_LETTER_REASSIGNMENT,
};

/* the name "boolean" is too common to use here */
typedef signed char nh_bool;    /* 0 or 1 */


struct nh_listitem {
    int id;
    const char *caption;
};

struct nh_int_option {
    int max;
    int min;
};

struct nh_enum_option {
    const struct nh_listitem *choices;
    int numchoices;
};

struct nh_string_option {
    int maxlen;
};

struct nh_autopick_option {
    const struct nh_listitem *classes;
    int numclasses;
};

struct nh_autopickup_rule {
    char pattern[AUTOPICKUP_PATTERNSZ];
    int oclass; /* valid values are those given in the a.classes list */
    enum nh_bucstatus buc;
    enum autopickup_action action;
};

struct nh_autopickup_rules {
    struct nh_autopickup_rule *rules;
    int num_rules;      /* < AUTOPICKUP_MAX_RULES */
};

union nh_optvalue {
    char *s;
    nh_bool b;
    int i;
    int e;
    struct nh_autopickup_rules *ar;
};

enum nh_optbirth {
    nh_birth_ingame  = 0,  /* not a birth option; changable in game */
    nh_birth_lasting = 1,  /* birth option; has effects in game */
    nh_birth_creation = 2, /* game creation option; no effect from turn 1 on */
};

/* This structure and all its contents should always be dynamically allocated
   so that they can safely be freed with nhlib_free_optlist. */
struct nh_option_desc {
    const char *name;
    const char *helptxt;
    enum nh_optbirth birth_option;
    enum nh_opttype type;
    union nh_optvalue value;
    union {
        struct nh_int_option i;
        struct nh_enum_option e;
        struct nh_string_option s;
        struct nh_autopick_option a;
    };
};

struct nh_menuitem {
    int id;
    enum nh_menuitem_role role;
    char caption[BUFSZ];
    char accel;
    char group_accel;
    nh_bool selected;
};

/* Used by the menu functions, and by the memory allocation functions.

   This can either be used in an automatically memory-managed way (either items
   and size are both NULL/0, or items points to malloc'ed memory and size is the
   number of elements allocated); or in a manually memory-managed way (size is 0
   and items points to statically allocated memory). */
struct nh_menulist {
    struct nh_menuitem *items;
    int size;
    int icount;
};

struct nh_objitem {
    char caption[BUFSZ];
    int id;
    enum nh_menuitem_role role;
    int count;
    int otype;
    int oclass;
    int weight; /* w < 0 == weight unknown */
    enum nh_bucstatus buc;
    char accel;
    char group_accel;
    nh_bool worn;
};

/* Works like nh_menulist, but for objitems. */
struct nh_objlist {
    struct nh_objitem *items;
    int size;
    int icount;
};

struct nh_objresult {
    int id;
    int count;
};

# define ITEMLEN 12
# define STATUSITEMS_MAX 24
struct nh_player_info {
    char plname[PL_NSIZ];
    int x, y, z;
    char rank[PL_NSIZ];
    char rolename[PL_NSIZ];
    char gendername[PL_NSIZ];
    char racename[PL_NSIZ];
    char level_desc[COLNO];
    char statusitems[STATUSITEMS_MAX][ITEMLEN];
    int score, xp, gold, moves;
    int max_rank_sz;
    int st, st_extra, dx, co, in, wi, ch;
    int align, nr_items;
    int hp, hpmax, en, enmax, ac, level;
    char coinsym;
    int monnum, cur_monnum;
    nh_bool can_enhance;
};


/* info about saved games as provided by nh_get_savegame_status */
struct nh_game_info {
    enum nh_game_modes playmode;
    char name[PL_NSIZ];
    char plrole[PLRBUFSZ];
    char plrace[PLRBUFSZ];
    char plgend[PLRBUFSZ];
    char plalign[PLRBUFSZ];

    /* A freeform text description of the status of the game */
    char game_state[COLNO];
};


struct nh_roles_info {
    int num_roles, num_races, num_genders, num_aligns;
    const char **rolenames_m;
    const char **rolenames_f;
    const char **racenames;
    const char **gendnames;
    const char **alignnames;

    /* race/role/gend/align compatibility matrix size = num_role * num_races *
       num_genders * num_aligns */
    const nh_bool *matrix;
};

/* generate an index in the compat matrix */
# define nh_cm_idx(_ri, _rolenum, _racenum, _gendnum, _alignnum) \
    ((((_rolenum) * (_ri).num_races + (_racenum)) * \
    (_ri).num_genders + (_gendnum)) * (_ri).num_aligns + (_alignnum))


struct nh_replay_info {
    char nextcmd[64];
    int actions, max_actions;
    int moves, max_moves;
};


struct nh_cmd_desc {
    char name[20];
    char desc[80];
    unsigned char defkey, altkey;
    unsigned flags;
};

struct nh_cmdarg_pos {
    short x, y;
};

struct nh_cmd_arg {
    unsigned argtype; /* which of the other fields are present */

    enum nh_direction dir;     /* CMD_ARG_DIR */
    struct nh_cmdarg_pos pos;  /* CMD_ARG_POS */
    char invlet;               /* CMD_ARG_OBJ */
    const char *str;           /* CMD_ARG_STR */
    char spelllet;             /* CMD_ARG_SPELL */
    int limit;                 /* CMD_ARG_LIMIT */
};

struct nh_cmd_and_arg {
    const char *cmd;
    struct nh_cmd_arg arg;
};

/* various extra information that the character knows, and the 
   windowport might want to display */
# define NH_BRANDING_STEPPED   0x0001
# define NH_BRANDING_LOCKED    0x0002
# define NH_BRANDING_UNLOCKED  0x0004
# define NH_BRANDING_TRAPPED   0x0008   /* for door traps and the like */
# define NH_BRANDING_UNTRAPPED 0x0010   /* probably not worth drawing */
# define NH_BRANDING_SEEN      0x0020   /* a square that is seen */
# define NH_BRANDING_LIT       0x0040   /* a square that is permanently lit */
# define NH_BRANDING_TEMP_LIT  0x0080   /* a square that is temporarily lit */
/* monster attitude could go here, but is in monflags instead as
   that's a more appropriate place */

/* a single position in the display buffer passed by win_update_screen */
struct nh_dbuf_entry {
    int effect; /* to decode type and id see macros below */
    short bg;
    short trap;
    short obj;
    short obj_mn;
    short mon;
    short monflags;
    short branding;
    nh_bool invis;
    nh_bool visible;    /* can the hero see this location? */
};

# define NH_EFFECT_TYPE(e) ((enum nh_effect_types)((e) >> 16))
# define NH_EFFECT_ID(e) (((e) - 1) & 0xffff)


struct nh_symdef {
    char ch;
    const char *symname;
    int color;
};

/* 
 * all information necessary to interpret and display the values supplied
 * in an nh_dbuf_entry */
struct nh_drawing_info {
    /* background layer symbols: nh_dbuf_entry.bg */
    const struct nh_symdef *bgelements;
    /* background layer symbols: nh_dbuf_entry.bg */
    const struct nh_symdef *traps;
    /* object layer symbols: nh_dbuf_entry.obj */
    const struct nh_symdef *objects;
    /* invisible monster symbol: show this if nh_dbuf_entry.invis is true */
    const struct nh_symdef *invis;
    /* monster layer symbols: nh_dbuf_entry.mon symbols with id <= num_monsters 
       are actual monsters, followed by warnings */
    const struct nh_symdef *monsters;
    const struct nh_symdef *warnings;
    /* effect layer symbols: nh_dbuf_entry.effect NH_EFFECT_TYPE */
    const struct nh_symdef *explsyms;
    const struct nh_symdef *expltypes;
    const struct nh_symdef *zapsyms;  /* default zap symbols; no color info */
    const struct nh_symdef *zaptypes; /* zap beam types + colors. no symbols */
    const struct nh_symdef *effects;  /* shield, boomerang, digbeam, flashbeam,
                                         gascloud */
    const struct nh_symdef *swallowsyms;  /* no color info: use the color of the
                                             swallower */
    int num_bgelements;
    int num_traps;
    int num_objects;
    int num_monsters;
    int num_warnings;
    int num_expltypes;
    int num_zaptypes;
    int num_effects;

    /* bg contains boring elements (floor, walls, stone) and interesting ones
       (dungeon features like stairs, altars, etc). In some situations it is
       useful to know which is which: all elements with ids < bg_feature_offset
       are boring. */
    int bg_feature_offset;
};

# define NUMEXPCHARS 9  /* explosions fill a 3x3 grid */
# define NUMZAPCHARS 4  /* beam directions: vert., horiz., left diag., right
                           diag */
# define NUMSWALLOWCHARS 8      /* like explosions, but without the center */


/* 
 * output buffers for nh_describe_pos()
 * there is one buffer per display layer (see nh_dbuf_entry)
 */
struct nh_desc_buf {
    char bgdesc[BUFSZ];
    char trapdesc[BUFSZ];
    char objdesc[BUFSZ];
    char mondesc[BUFSZ];
    char invisdesc[BUFSZ];
    char effectdesc[BUFSZ];     /* can only describe the swallow effect */
    int objcount;       /* number of (visible) objects or -1 if the location is 
                           not visible */
    nh_bool feature_described;  /* bgdesc is redundant to another field */
};


/* 
 * return type for nh_get_topten()
 */
struct nh_topten_entry {
    int rank;
    int points;
    int maxlvl; /* maximum depth level reached */
    int hp, maxhp;
    int deaths;
    int ver_major, ver_minor, patchlevel;
    int deathdate, birthdate;   /* decimal representation, ex: 20101231 for 31
                                   Dec 2010 */
    int moves, end_how;
    char plrole[PLRBUFSZ];
    char plrace[PLRBUFSZ];
    char plgend[PLRBUFSZ];
    char plalign[PLRBUFSZ];
    char name[PL_NSIZ];
    char death[BUFSZ];
    char entrytxt[BUFSZ];
    nh_bool highlight;
};

/*
 * return type for win_query_key()
 */
struct nh_query_key_result {
    int key; /* ASCII only */
    int count;
};

/*
 * return type for win_getpos()
 */
struct nh_getpos_result {
    enum nh_client_response howclosed;
    short x;
    short y;
};

/* Any nh_menulist *, nh_objlist * have their "items" elements freed by the
   callee, unless they're marked as statically allocated (items non-NULL, size
   0), or are empty lists and so were never allocated in the first place (items
   NULL).

   Any variably-sized that are transmitted from the callee to the caller are
   transmitted by means of a callback function, in order to avoid memory
   allocation issues. */
struct nh_window_procs {
    void (*win_pause) (enum nh_pause_reason reason);
    void (*win_display_buffer) (const char *buf, nh_bool trymove);
    void (*win_update_status) (struct nh_player_info *pi);
    void (*win_print_message) (int turn, const char *msg);
    void (*win_request_command) (nh_bool debug, nh_bool completed,
                                 nh_bool interrupted, void *callbackarg,
                                 void (*callback)(
                                     const struct nh_cmd_and_arg *cmd,
                                     void *callbackarg));
    void (*win_display_menu) (
        struct nh_menulist *menulist, const char *title, int how,
        int placement_hint, void *callbackarg,
        void (*callback)(const int *results, int nresults, void *callbackarg));
    void (*win_display_objects) (
        struct nh_objlist *menulist, const char *title, int how,
        int placement_hint, void *callback_arg,
        void (*callback)(const struct nh_objresult *results, int nresults,
                         void *callbackarg));
    void (*win_list_items) (struct nh_objlist *itemlist, nh_bool invent);
    void (*win_update_screen) (struct nh_dbuf_entry dbuf[ROWNO][COLNO],
                               int ux, int uy);
    void (*win_raw_print) (const char *str);
    struct nh_query_key_result (*win_query_key) (
        const char *query, enum nh_query_key_flags flags,
        nh_bool count_allowed);
    struct nh_getpos_result (*win_getpos) (int origx, int origy,
                                           nh_bool force, const char *goal);
    enum nh_direction (*win_getdir) (const char *, nh_bool);
    char (*win_yn_function) (const char *query, const char *rset,
                             char defchoice);
    void (*win_getlin) (const char *query, void *callbackarg,
                        void (*callback)(const char *lin, void *callbackarg));
    void (*win_delay) (void);
    void (*win_load_progress) (int progress);
    void (*win_level_changed) (int displaymode);
    void (*win_outrip) (struct nh_menulist *menulist,
                        nh_bool tombstone, const char *name, int gold,
                        const char *killbuf, int end_how, int year);
    void (*win_print_message_nonblocking) (int turn, const char *msg);
    void (*win_server_cancel) (void);
};

/* typedefs for import/export */
typedef char *char_p;
typedef const char *const_char_p;
typedef const char *const *const_char_p_const_p;
typedef struct nh_cmd_desc *nh_cmd_desc_p;
typedef struct nh_drawing_info *nh_drawing_info_p;
typedef struct nh_option_desc *nh_option_desc_p;
typedef struct nh_roles_info *nh_roles_info_p;
typedef struct nh_topten_entry *nh_topten_entry_p;

typedef struct nhnet_game *nhnet_game_p;

#endif
