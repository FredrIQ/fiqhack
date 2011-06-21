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

#define FCMASK		0660	/* file creation mask */
#define HLOCK		"perm"	/* an empty file used for locking purposes */
#define msleep(k) usleep((k)*1000)

/* default glyph value for all menus that don't need glyphs */
#define NO_GLYPH	(-1)

/* Special returns from mapglyph() */
#define MG_CORPSE	0x01
#define MG_INVIS	0x02
#define MG_DETECT	0x04
#define MG_PET		0x08
#define MG_RIDDEN	0x10

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


/* command param type specification */
#define CMD_ARG_NONE (1 << 1)  /* param can be empty */
#define CMD_ARG_DIR  (1 << 2)  /* param can be a direction */

/* command usage hints */
#define CMD_EXT        (1 << 10) /* an 'extended' command */
#define CMD_MOVE       (1 << 11) /* this is a move command */
#define CMD_OBJ        (1 << 12) /* command manipulates items */
#define CMD_NOTIME     (1 << 13) /* command will not use up any game time */
#define CMD_DEBUG      (1 << 14) /* a wizmode command */


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

/* window types */
enum nh_window_type {
    NHW_MESSAGE,
    NHW_STATUS,
    NHW_MAP,
    /* the following 2 only have meaning inside the tty port. remove when possible */
    NHW_MENU,
    NHW_TEXT
};

/* nh_poskey() modifier types */
enum nh_click_type {
    CLICK_1,
    CLICK_2
};

/*
 * Graphics sets for display symbols
 */
enum nh_text_mode {
    ASCII_GRAPHICS,	/* regular characters: '-', '+', &c */
    IBM_GRAPHICS,	/* PC graphic characters */
    DEC_GRAPHICS	/* VT100 line drawing characters */
};

/* Always use full pathnames for file names,
 * rather than assuming that they're all in the current directory.  This
 * provides all the subclasses that seem reasonable.
 * An array listing a path for each prefix must be passed to nh_init().
 */
enum nh_path_prefix {
    HACKPREFIX,
    LEVELPREFIX,
    SAVEPREFIX,
    BONESPREFIX,
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

enum nh_opttype {
    OPTTYPE_BOOL,
    OPTTYPE_INT,
    OPTTYPE_ENUM,
    OPTTYPE_STRING
};

enum nh_bucstatus {
    BUC_UNKNOWN,
    BUC_BLESSED,
    BUC_UNCURSED,
    BUC_CURSED
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

enum nh_input_status {
    READY_FOR_INPUT,
    MULTI_IN_PROGRESS,
    OCCUPATION_IN_PROGRESS,
    ERR_GAME_NOT_RUNNING,
    ERR_NO_INPUT_ALLOWED
};


typedef signed char	schar;
typedef unsigned char	uchar;

/*
 * type xchar: small integers in the range 0 - 127, usually coordinates
 * although they are nonnegative they must not be declared unsigned
 * since otherwise comparisons with signed quantities are done incorrectly
 */
typedef schar	xchar;
typedef xchar	boolean;		/* 0 or 1 */


struct nh_listitem {
    int id;
    char *caption;
};

struct nh_boolopt_map {
    const char *optname;
    boolean	*addr;
};

struct nh_int_option {
    int max;
    int min;
};

struct nh_enum_option {
    struct nh_listitem *choices;
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
    char caption[COLNO];
    char accel;
    char group_accel;
    boolean selected;
};

struct nh_objitem {
    char caption[COLNO];
    int id;
    int count;
    int glyph;
    char accel;
    char group_accel;
    int otype;
    int oclass;
    enum nh_bucstatus buc;
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
};


struct nh_cmd_desc {
	const char *name;
	const char *desc;
	char defkey, altkey;
	unsigned flags;
};

struct nh_cmd_arg {
	unsigned argtype;
	union {
	    enum nh_direction d;
	};
};


struct window_procs {
    void (*win_player_selection)(int,int,int,int,int);
    void (*win_exit_nhwindows)(const char *);
    void (*win_create_game_windows)(void);
    void (*win_destroy_game_windows)(void);
    void (*win_clear_nhwindow)(int);
    void (*win_display_nhwindow)(int, boolean);
    void (*win_display_buffer)(char *,boolean);
    void (*win_update_status)(void);
    void (*win_print_message)(const char *);
    
    int (*win_display_menu)(struct nh_menuitem*, int, const char*, int, int*);
    int (*win_display_objects)(struct nh_objitem*, int, const char*, int, struct nh_objresult*);
    
    char (*win_message_menu)(char,int,const char *);
    void (*win_update_inventory)(void);
    void (*win_mark_synch)(void);
    void (*win_wait_synch)(void);
    void (*win_print_glyph)(xchar,xchar,int);
    void (*win_raw_print)(const char *);
    void (*win_raw_print_bold)(const char *);
    int (*win_nhgetch)(void);
    int (*win_getpos)(int *, int *, boolean, const char*);
    enum nh_direction (*win_getdir)(const char *, boolean);
    void (*win_nhbell)(void);
    char (*win_yn_function)(const char *, const char *, char, long*);
    void (*win_getlin)(const char *,char *);
    void (*win_delay_output)(void);

    void (*win_outrip)(struct nh_menuitem*,int, int, char *, long, char *, int);
};

struct instance_flags2 {
    boolean  DECgraphics;	/* use DEC VT-xxx extended character set */
    boolean  IBMgraphics;	/* use IBM extended character set */
    boolean  news;		/* print news */
    boolean  window_inited; /* true if init_nhwindows() completed */
};

#endif
