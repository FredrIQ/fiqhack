#ifndef NETHACK_TYPES_H
#define NETHACK_TYPES_H

/*
 * System autodetection: greatly simplified, as we only care about
 * Unix (in all its variations) and Windows */
#define UNIX
#ifdef WIN32
# undef UNIX
# define STRNCMPI
# define STRCMPI
#endif

#ifndef TRUE	/* defined in some systems' native include files */
#define FALSE	((boolean)0)
#define TRUE	((boolean)!0)
#endif

/* size of terminal screen is (at least) (ROWNO+3) by COLNO */
#define COLNO		80
#define ROWNO		21

#define BUFSZ		256	/* for getlin buffers */
#define QBUFSZ		128	/* for building question text */
#define TBUFSZ		300	/* toplines[] buffer max msg: 3 81char names */
				/* plus longest prefix plus a few extra words */
#define PL_NSIZ		32	/* name of player, ghost, shopkeeper */
#define PLRBUFSZ	16	/* player race/role names */

#define FCMASK		0660	/* file creation mask */
#define msleep(k) usleep((k)*1000)

#define ROLE_NONE	(-1)
#define ROLE_RANDOM	(-2)

#define A_CHAOTIC	(-1)
#define A_NEUTRAL	 0
#define A_LAWFUL	 1

/*
 * The color scheme used is tailored for an IBM PC.  It consists of the
 * standard 8 colors, folowed by their bright counterparts.  There are
 * exceptions, these are listed below.	Bright black doesn't mean very
 * much, so it is used as the "default" foreground color of the screen.
 */
#define CLR_BLACK		0
#define CLR_RED			1
#define CLR_GREEN		2
#define CLR_BROWN		3 /* on IBM, low-intensity yellow is brown */
#define CLR_BLUE		4
#define CLR_MAGENTA		5
#define CLR_CYAN		6
#define CLR_GRAY		7 /* low-intensity white */
#define NO_COLOR		8
#define CLR_ORANGE		9
#define CLR_BRIGHT_GREEN	10
#define CLR_YELLOW		11
#define CLR_BRIGHT_BLUE		12
#define CLR_BRIGHT_MAGENTA	13
#define CLR_BRIGHT_CYAN		14
#define CLR_WHITE		15
#define CLR_MAX			16

/* flags for displayed monsters */
#define MON_NORMAL   0
#define MON_TAME     (1 << 0)
#define MON_RIDDEN   (1 << 1)
#define MON_DETECTED (1 << 2)
#define MON_WARNING  (1 << 3) /* this "monster" is actually a warning */

/* 
 * level display modes
 * These defines are used by notify_levelchange() to inform the window port 
 * about some characteristic of the new level that might be worth displaying
 * in some nonstandard way (eg the rogue level in tty nethack, or alternate
 * tiles for mines/hell in tiles versions)
 */
#define LDM_DEFAULT	0
#define LDM_HELL	1
#define LDM_QUEST	2
#define LDM_MINES	3
#define LDM_SOKOBAN	4
#define LDM_ROGUE	5

/* command param type specification */
#define CMD_ARG_NONE (1 << 1)  /* param can be empty */
#define CMD_ARG_DIR  (1 << 2)  /* param can be a direction */
#define CMD_ARG_POS  (1 << 3)  /* param can be a position */
#define CMD_ARG_FLAGS (CMD_ARG_DIR | CMD_ARG_POS)

/* command usage hints */
#define CMD_EXT        (1 << 10) /* an 'extended' command */
#define CMD_MOVE       (1 << 11) /* this is a move command */
#define CMD_OBJ        (1 << 12) /* command manipulates items */
#define CMD_NOTIME     (1 << 13) /* command will not use up any game time */
#define CMD_DEBUG      (1 << 14) /* a wizmode command */

/* reserved flag for use by ui code that uses struct nh_cmd_desc internally */
#define CMD_UI         (1 << 31) 

#define NH_ARG_NONE	(1<<0)
#define NH_ARG_DIR	(1<<1)

enum nh_direction {
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
    PICK_NONE,	/* user picks nothing (display only) */
    PICK_ONE,	/* only pick one */
    PICK_ANY,	/* can pick any amount */
};

/* Always use full pathnames for file names,
 * rather than assuming that they're all in the current directory.  This
 * provides all the subclasses that seem reasonable.
 * An array listing a path for each prefix must be passed to nh_init().
 */
enum nh_path_prefix {
    BONESPREFIX = 0,
    DATAPREFIX,
    SCOREPREFIX,
    LOCKPREFIX,
    TROUBLEPREFIX,
    PREFIX_COUNT
};

enum nh_game_modes {
    MODE_NORMAL,
    MODE_EXPLORE,
    MODE_WIZARD
};

enum nh_pause_reason {
    P_MESSAGE,
    P_MAP
};

enum nh_opttype {
    OPTTYPE_BOOL,
    OPTTYPE_INT,
    OPTTYPE_ENUM,
    OPTTYPE_STRING
};

enum nh_option_list {
    CURRENT_BIRTH_OPTIONS,
    ACTIVE_BIRTH_OPTIONS,
    GAME_OPTIONS
};

enum nh_bucstatus {
    B_UNKNOWN,
    B_BLESSED,
    B_UNCURSED,
    B_CURSED
};

enum nh_menuitem_role {
    MI_TEXT = 0,
    MI_NORMAL,
    MI_HEADING
};

enum nh_command_status {
    COMMAND_NOT_GIVEN,
    COMMAND_UNKNOWN,
    COMMAND_BAD_ARG,
    COMMAND_OK
};

/* return values for nh_do_move */
enum nh_input_status {
    READY_FOR_INPUT,
    MULTI_IN_PROGRESS,
    OCCUPATION_IN_PROGRESS,
    POST_ACTION_DELAY,
    ERR_NO_INPUT_ALLOWED,
    
    /* for a status >= GAME_OVER exit the command loop */
    GAME_OVER, /* i.e. player died */
    GAME_SAVED,
    GAME_PANICKED, /* something went wrong in libnethack and panic() was called */
    ERR_GAME_NOT_RUNNING /* possibilities: the init sequence was incorrect and
                                           the game is not running YET
                                        or some api call other than nh_do_move    
                                           caused a panic, which killed the game
                                        or an attempt was made to call nh_do_move
                                           after some other final status (>= GAME_OVER)
                                           was returned
                          */
};

enum nh_effect_types {
    E_EXPLOSION,
    E_SWALLOW,
    E_ZAP,
    E_MISC
};

enum nh_exit_types {
    EXIT_REQUEST_SAVE,
    EXIT_FORCE_SAVE,
    EXIT_REQUEST_QUIT,
    EXIT_FORCE_QUIT,
    EXIT_PANIC
};

enum nh_restore_status {
    GAME_RESTORED,
    ERR_BAD_ARGS,
    ERR_BAD_FILE, /* file isn't a saved game */
    ERR_GAME_OVER, /* this is the log of a completed game, play cannot be resumed */
    ERR_RESTORE_FAILED, /* restoring the saved game state did not succeed (try replaying the log instead) */
    ERR_REPLAY_FAILED /* replaying the action log did not succeed */
};


enum nh_log_status {
    LS_INVALID = -1,
    LS_SAVED,
    LS_DONE, /* quit, died, ascended, etc */
    LS_IN_PROGRESS
};



typedef signed char	boolean;		/* 0 or 1 */


struct nh_listitem {
    int id;
    char *caption;
};

struct nh_boolopt_map {
    const char *optname;
    boolean *addr;
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

union nh_optvalue {
    char *s; /* largest element first for static initialisation */
    boolean b;
    int i;
    int e;
};

struct nh_option_desc {
    const char *name;
    const char *helptxt;
    enum nh_opttype type;
    union nh_optvalue value;
    union {
	/* only the first element of a union can be initialized at compile
	 * time (without C99), so boolean args go first, there are more of those ...*/
	struct nh_int_option i;
	struct nh_enum_option e;
	struct nh_string_option s;
    };
};

struct nh_menuitem {
    int id;
    enum nh_menuitem_role role;
    char caption[BUFSZ];
    char accel;
    char group_accel;
    boolean selected;
};

struct nh_objitem {
    char caption[BUFSZ];
    int id;
    enum nh_menuitem_role role;
    int count;
    int otype;
    int oclass;
    int weight; /* w < 0  == weight unknown */
    enum nh_bucstatus buc;
    char accel;
    char group_accel;
    boolean worn;
};

struct nh_objresult {
    int id;
    int count;
};

#define ITEMLEN 12
struct nh_player_info {
    char plname[PL_NSIZ];
    int x, y, z;
    char rank[PL_NSIZ];
    char level_desc[COLNO];
    char statusitems[12][ITEMLEN];
    long score, xp, gold, moves;
    int max_rank_sz;
    int st, st_extra, dx, co, in, wi, ch;
    int align, nr_items;
    int hp, hpmax, en, enmax, ac, level;
    char coinsym;
    int monnum, cur_monnum;
    boolean enhance_possible;
};


/* info about saved games as provided by nh_get_savegame_status */
struct nh_save_info {
    enum nh_game_modes playmode;
    char name[PL_NSIZ];
    char plrole[PLRBUFSZ];
    char plrace[PLRBUFSZ];
    char plgend[PLRBUFSZ];
    char plalign[PLRBUFSZ];
    /* the following fields are only valid if the status is LS_SAVED
     * retrieving the values for LS_IN_PROGRESS would require reconstructing
     * the full game. You can force that by doing:
     *   if (nh_restore_game(fd, NULL, TRUE) != GAME_RESTORED)
     *       handle_error(...)
     *   nh_exit(EXIT_FORCE_SAVE);
     * Now a call to nh_get_savegame_status will return LS_SAVED.
     */
    char level_desc[COLNO];
    int moves, depth;
    boolean has_amulet;
    /* most of nh_player_info is possible, but what makes sense? */
};


struct nh_cmd_desc {
    const char *name;
    const char *desc;
    unsigned char defkey, altkey;
    unsigned flags;
};

struct nh_cmdarg_pos {
    short x, y;
};

struct nh_cmd_arg {
    unsigned argtype;
    union {
	enum nh_direction d;
	struct nh_cmdarg_pos pos;
    };
};


/* a single position in the display buffer passed by win_update_screen */
struct nh_dbuf_entry {
    int effect; /* to decode type and id see macros below */
    short bg;
    short trap;
    short obj;
    short obj_mn;
    short mon;
    short monflags;
    boolean invis;
};

#define NH_EFFECT_TYPE(e) ((enum nh_effect_types)((e) >> 16))
#define NH_EFFECT_ID(e) (((e) - 1) & 0xffff)


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
    struct nh_symdef *bgelements;
    /* background layer symbols: nh_dbuf_entry.bg */
    struct nh_symdef *traps;
    /* object layer symbols: nh_dbuf_entry.obj */
    struct nh_symdef *objects;
    /* invisible monster symbol: show this if nh_dbuf_entry.invis is true */
    struct nh_symdef *invis;
    /* monster layer symbols: nh_dbuf_entry.mon
     * symbols with id <= num_monsters are actual monsters, followed by warnings */
    struct nh_symdef *monsters;
    struct nh_symdef *warnings;
    /* effect layer symbols: nh_dbuf_entry.effect
     * NH_EFFECT_TYPE */
    struct nh_symdef *explsyms;
    struct nh_symdef *expltypes;
    struct nh_symdef *zapsyms; /* default zap symbols; no color info */
    struct nh_symdef *zaptypes; /* zap beam types + colors. no symbols */
    struct nh_symdef *effects; /* shield, boomerang, digbeam, flashbeam, gascloud */
    struct nh_symdef *swallowsyms; /* no color info: use the color of the swallower */
    int num_bgelements;
    int num_traps;
    int num_objects;
    int num_monsters;
    int num_warnings;
    int num_expltypes;
    int num_zaptypes;
    int num_effects;
};

#define NUMEXPCHARS 9 /* explosions fill a 3x3 grid */
#define NUMZAPCHARS 4 /* beam directions: vert., horiz., left diag., right diag */
#define NUMSWALLOWCHARS 8 /* like explosions, but without the center */


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
    char effectdesc[BUFSZ]; /* can only describe the swallow effect */
    int objcount; /* number of (visible) objects or -1 if the location is not visible */
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
    int deathdate, birthdate; /* decimal representation, ex: 20101231 for 31 Dec 2010 */
    char plrole[PLRBUFSZ];
    char plrace[PLRBUFSZ];
    char plgend[PLRBUFSZ];
    char plalign[PLRBUFSZ];
    char name[PL_NSIZ];
    char death[BUFSZ];
    char entrytxt[BUFSZ];
    boolean highlight;
};

struct nh_window_procs {
    void (*win_clear_map)(void);
    void (*win_pause)(enum nh_pause_reason reason);
    void (*win_display_buffer)(char *,boolean);
    void (*win_update_status)(struct nh_player_info *pi);
    void (*win_print_message)(int turn, const char *msg);
    int (*win_display_menu)(struct nh_menuitem*, int, const char*, int, int*);
    int (*win_display_objects)(struct nh_objitem*, int, const char*, int, struct nh_objresult*);
    boolean (*win_list_items)(struct nh_objitem *items, int icount, boolean invent);
    void (*win_update_screen)(struct nh_dbuf_entry dbuf[ROWNO][COLNO]);
    void (*win_raw_print)(const char *str);
    char (*win_query_key)(const char *query, int *count);
    int (*win_getpos)(int *, int *, boolean, const char*);
    enum nh_direction (*win_getdir)(const char *, boolean);
    char (*win_yn_function)(const char *query, const char *rset, char defchoice);
    void (*win_getlin)(const char *,char *);
    void (*win_delay_output)(void);
    void (*win_level_changed)(int displaymode);
    void (*win_outrip)(struct nh_menuitem*,int, boolean, char *, long, char *, int);
};

#endif
