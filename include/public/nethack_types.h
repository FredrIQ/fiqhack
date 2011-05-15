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

typedef signed char	schar;
typedef unsigned char	uchar;

/*
 * type xchar: small integers in the range 0 - 127, usually coordinates
 * although they are nonnegative they must not be declared unsigned
 * since otherwise comparisons with signed quantities are done incorrectly
 */
typedef schar	xchar;
typedef xchar	boolean;		/* 0 or 1 */

typedef schar	aligntyp;	/* basic alignment type */

#ifndef TRUE		/* defined in some systems' native include files */
#define FALSE	((boolean)0)
#define TRUE	((boolean)!0)
#endif

/* size of terminal screen is (at least) (ROWNO+3) by COLNO */
#define COLNO	80
#define ROWNO	21

#define BUFSZ		256	/* for getlin buffers */
#define QBUFSZ		128	/* for building question text */
#define TBUFSZ		300	/* toplines[] buffer max msg: 3 81char names */
				/* plus longest prefix plus a few extra words */
#define PL_NSIZ		32	/* name of player, ghost, shopkeeper */

#define FCMASK	0660	/* file creation mask */

#define HLOCK	"perm"	/* an empty file used for locking purposes */

#define msleep(k) usleep((k)*1000)

#include "wintype.h"
#include "coord.h"

struct nh_menuitem;
struct nh_objitem;
struct nh_objresult;
struct nh_status_info;

struct window_procs {
    void (*win_player_selection)(int,int,int,int,int);
    void (*win_get_nh_event)(void);
    void (*win_exit_nhwindows)(const char *);
    void (*win_suspend_nhwindows)(const char *);
    void (*win_resume_nhwindows)(void);
    void (*win_create_game_windows)(void);
    void (*win_destroy_game_windows)(void);
    void (*win_clear_nhwindow)(int);
    void (*win_display_nhwindow)(int, boolean);
    void (*win_curs)(int,int);
    void (*win_display_buffer)(char *,boolean);
    void (*win_update_status)(struct nh_status_info *);
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
    int (*win_nh_poskey)(int *, int *, int *);
    void (*win_nhbell)(void);
    int (*win_doprev_message)(void);
    char (*win_yn_function)(const char *, const char *, char);
    void (*win_getlin)(const char *,char *);
    int (*win_get_ext_cmd)(const char**, const char**, int);
    void (*win_number_pad)(int);
    void (*win_delay_output)(void);

    /* other defs that really should go away (they're tty specific) */
    void (*win_start_screen)(void);
    void (*win_end_screen)(void);

    void (*win_outrip)(struct nh_menuitem*,int, int, char *, long, char *, int);
};

struct instance_flags2 {
    boolean  DECgraphics;	/* use DEC VT-xxx extended character set */
    boolean  IBMgraphics;	/* use IBM extended character set */
    boolean  num_pad;	/* use numbers for movement commands */
    boolean  news;		/* print news */
    boolean  window_inited; /* true if init_nhwindows() completed */
};

struct sinfo {
	int game_started;
	int gameover;		/* self explanatory? */
	int stopprint;		/* inhibit further end of game disclosure */
#if defined(UNIX) || defined(WIN32)
	int done_hup;		/* SIGHUP or moral equivalent received
				 * -- no more screen output */
#endif
	int something_worth_saving;	/* in case of panic */
	int panicking;		/* `panic' is in progress */
#if defined(WIN32)
	int exiting;		/* an exit handler is executing */
#endif
	int in_impossible;
#ifdef PANICLOG
	int in_paniclog;
#endif
};

#define ON		1
#define OFF		0

/* attribute IDs; window port code should only need A_MAX */
#define A_STR	0
#define A_INT	1
#define A_WIS	2
#define A_DEX	3
#define A_CON	4
#define A_CHA	5

#define A_MAX	6	/* used in rn2() selection of attrib */


/* Special returns from mapglyph() */
#define MG_CORPSE	0x01
#define MG_INVIS	0x02
#define MG_DETECT	0x04
#define MG_PET		0x08
#define MG_RIDDEN	0x10

/* default glyph value for all menus that don't need glyphs */
#define NO_GLYPH	(-1)


#define MENU_SELECTED	TRUE
#define MENU_UNSELECTED FALSE


/*
 * Graphics sets for display symbols
 */
#define ASCII_GRAPHICS	0	/* regular characters: '-', '+', &c */
#define IBM_GRAPHICS	1	/* PC graphic characters */
#define DEC_GRAPHICS	2	/* VT100 line drawing characters */

/* Some systems want to use full pathnames for some subsets of file names,
 * rather than assuming that they're all in the current directory.  This
 * provides all the subclasses that seem reasonable, and sets up for all
 * prefixes being null.  Port code can set those that it wants.
 */
#define HACKPREFIX	0
#define LEVELPREFIX	1
#define SAVEPREFIX	2
#define BONESPREFIX	3
#define DATAPREFIX	4	/* this one must match hardcoded value in dlb.c */
#define SCOREPREFIX	5
#define LOCKPREFIX	6
#define TROUBLEPREFIX	7
#define PREFIX_COUNT	8

#define ROLE_NONE	(-1)
#define ROLE_RANDOM	(-2)

#define A_CHAOTIC	(-1)
#define A_NEUTRAL	 0
#define A_LAWFUL	 1

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

struct nh_listitem {
    int id;
    char *caption;
};

struct nh_boolopt_map {
	const char *optname;
	boolean	*addr;
} ;

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

enum nh_menuitem_role {
	MI_TEXT = 0,
	MI_NORMAL,
	MI_HEADING
};

struct nh_menuitem {
	int id;
	enum nh_menuitem_role role;
	char caption[COLNO];
	char accel;
	char group_accel;
	boolean selected;
};

enum nh_bucstatus {
    BUC_UNKNOWN,
    BUC_BLESSED,
    BUC_UNCURSED,
    BUC_CURSED
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

struct nh_status_info {
    char plname[PL_NSIZ];
    char rank[PL_NSIZ];
    char level_desc[COLNO];
    char items[12][ITEMLEN];
    long score, xp, gold, moves;
    int mrank_sz, st, st_extra, dx, co, in, wi, ch, align, nr_items;
    int hp, hpmax, en, enmax, ac, level;
    char coinsym;
    boolean polyd;

};

#endif

