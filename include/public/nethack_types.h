#ifndef NETHACK_TYPES_H
#define NETHACK_TYPES_H

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


#include "wintype.h"
#include "coord.h"

struct window_procs {
    const char *name;
    unsigned long wincap;	/* window port capability options supported */
    unsigned long wincap2;	/* additional window port capability options supported */
    void (*win_player_selection)(int,int,int,int,int);
    void (*win_get_nh_event)(void);
    void (*win_exit_nhwindows)(const char *);
    void (*win_suspend_nhwindows)(const char *);
    void (*win_resume_nhwindows)(void);
    winid (*win_create_nhwindow)(int);
    void (*win_clear_nhwindow)(winid);
    void (*win_display_nhwindow)(winid, boolean);
    void (*win_destroy_nhwindow)(winid);
    void (*win_curs)(winid,int,int);
    void (*win_putstr)(winid, int, const char *);
    void (*win_display_buffer)(char *,boolean);
    void (*win_start_menu)(winid);
    void (*win_add_menu)(winid,int,const ANY_P *,
		char,char,int,const char *, boolean);
    void (*win_end_menu)(winid, const char *);
    int (*win_select_menu)(winid, int, MENU_ITEM_P **);
    char (*win_message_menu)(char,int,const char *);
    void (*win_update_inventory)(void);
    void (*win_mark_synch)(void);
    void (*win_wait_synch)(void);
    void (*win_print_glyph)(winid,xchar,xchar,int);
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

    void (*win_outrip)(winid,int);
};


/*
 * Flags that are set each time the game is started.
 * These are not saved with the game.
 *
 */
struct instance_flags {
	boolean  DECgraphics;	/* use DEC VT-xxx extended character set */
	boolean  IBMgraphics;	/* use IBM extended character set */
	unsigned msg_history;	/* hint: # of top lines to save */
	boolean  num_pad;	/* use numbers for movement commands */
	boolean  news;		/* print news */
	boolean  window_inited; /* true if init_nhwindows() completed */
	boolean  vision_inited; /* true if vision is ready */
	boolean  menu_tab_sep;	/* Use tabs to separate option menu fields */
	boolean  menu_requested; /* Flag for overloaded use of 'm' prefix
				  * on some non-move commands */
	int	menu_headings;	/* ATR for menu headings */
	int      purge_monsters;	/* # of dead monsters still on fmon list */
	uchar	bouldersym;	/* symbol for boulder display */
	boolean travel1;	/* first travel step */
	coord	travelcc;	/* coordinates for travel_cache */
	boolean  sanity_check;	/* run sanity checks */
	boolean  mon_polycontrol;	/* debug: control monster polymorphs */

	char prevmsg_window;	/* type of old message window to use */
	boolean  extmenu;	/* extended commands use menu interface */

/*
 * Window capability support.
 */
	boolean wc_hilite_pet;		/* hilight pets                        */
	boolean wc_inverse;		/* use inverse video for some things   */
	boolean wc_eight_bit_input;	/* allow eight bit input               */

	boolean  cmdassist;	/* provide detailed assistance for some commands */
	/* Items which belong in flags, but are here to allow save compatibility */
	boolean  lootabc;	/* use "a/b/c" rather than "o/i/b" when looting */
	boolean  showrace;	/* show hero glyph by race rather than by role */
	boolean  travelcmd;	/* allow travel command */
	int	 runmode;	/* update screen display during run moves */
#ifdef AUTOPICKUP_EXCEPTIONS
	struct autopickup_exception *autopickup_exceptions[2];
#define AP_LEAVE 0
#define AP_GRAB	 1
#endif
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

struct nh_option_desc
{
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


#endif

