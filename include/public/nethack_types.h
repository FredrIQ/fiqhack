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
    void (*win_init_nhwindows)(int *, char **);
    void (*win_player_selection)(int,int,int,int,int);
    void (*win_askname)(void);
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
    void (*win_display_file)(const char *, boolean);
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
    int (*win_get_ext_cmd)(void);
    void (*win_number_pad)(int);
    void (*win_delay_output)(void);
#ifdef CHANGE_COLOR
    void (*win_change_color)(int,long,int);
    char * (*win_get_color_string)(void);
#endif

    /* other defs that really should go away (they're tty specific) */
    void (*win_start_screen)(void);
    void (*win_end_screen)(void);

    void (*win_outrip)(winid,int);
    void (*win_preference_update)(const char *);
};


/*
 * Flags that are set each time the game is started.
 * These are not saved with the game.
 *
 */
struct instance_flags {
	boolean  cbreak;	/* in cbreak mode, rogue format */
	boolean  DECgraphics;	/* use DEC VT-xxx extended character set */
	boolean  echo;		/* 1 to echo characters */
	boolean  IBMgraphics;	/* use IBM extended character set */
	unsigned msg_history;	/* hint: # of top lines to save */
	boolean  num_pad;	/* use numbers for movement commands */
	boolean  news;		/* print news */
	boolean  window_inited; /* true if init_nhwindows() completed */
	boolean  vision_inited; /* true if vision is ready */
	boolean  menu_tab_sep;	/* Use tabs to separate option menu fields */
	boolean  menu_requested; /* Flag for overloaded use of 'm' prefix
				  * on some non-move commands */
	uchar num_pad_mode;
	int	menu_headings;	/* ATR for menu headings */
	int      purge_monsters;	/* # of dead monsters still on fmon list */
	int *opt_booldup;	/* for duplication of boolean opts in config file */
	int *opt_compdup;	/* for duplication of compound opts in config file */
	uchar	bouldersym;	/* symbol for boulder display */
	boolean travel1;	/* first travel step */
	coord	travelcc;	/* coordinates for travel_cache */
	boolean  sanity_check;	/* run sanity checks */
	boolean  mon_polycontrol;	/* debug: control monster polymorphs */
#ifdef TTY_GRAPHICS
	char prevmsg_window;	/* type of old message window to use */
	boolean  extmenu;	/* extended commands use menu interface */
#endif
#if defined(WIN32)
	boolean  rawio;		/* whether can use rawio (IOCTL call) */
	boolean hassound;	/* has a sound card */
	boolean usesound;	/* use the sound card */
	boolean usepcspeaker;	/* use the pc speaker */
	boolean tile_view;
	boolean over_view;
	boolean traditional_view;
#endif
/*
 * Window capability support.
 */
	boolean wc_color;		/* use color graphics                  */
	boolean wc_hilite_pet;		/* hilight pets                        */
	boolean wc_ascii_map;		/* show map using traditional ascii    */
	boolean wc_tiled_map;		/* show map using tiles                */
	boolean wc_preload_tiles;	/* preload tiles into memory           */
	int	wc_tile_width;		/* tile width                          */
	int	wc_tile_height;		/* tile height                         */
	char	*wc_tile_file;		/* name of tile file;overrides default */
	boolean wc_inverse;		/* use inverse video for some things   */
	int	wc_align_status;	/*  status win at top|bot|right|left   */
	int	wc_align_message;	/* message win at top|bot|right|left   */
	int     wc_vary_msgcount;	/* show more old messages at a time    */
	char    *wc_foregrnd_menu;	/* points to foregrnd color name for menu win   */
	char    *wc_backgrnd_menu;	/* points to backgrnd color name for menu win   */
	char    *wc_foregrnd_message;	/* points to foregrnd color name for msg win    */
	char    *wc_backgrnd_message;	/* points to backgrnd color name for msg win    */
	char    *wc_foregrnd_status;	/* points to foregrnd color name for status win */
	char    *wc_backgrnd_status;	/* points to backgrnd color name for status win */
	char    *wc_foregrnd_text;	/* points to foregrnd color name for text win   */
	char    *wc_backgrnd_text;	/* points to backgrnd color name for text win   */
	char    *wc_font_map;		/* points to font name for the map win */
	char    *wc_font_message;	/* points to font name for message win */
	char    *wc_font_status;	/* points to font name for status win  */
	char    *wc_font_menu;		/* points to font name for menu win    */
	char    *wc_font_text;		/* points to font name for text win    */
	int     wc_fontsiz_map;		/* font size for the map win           */
	int     wc_fontsiz_message;	/* font size for the message window    */
	int     wc_fontsiz_status;	/* font size for the status window     */
	int     wc_fontsiz_menu;	/* font size for the menu window       */
	int     wc_fontsiz_text;	/* font size for text windows          */
	int	wc_scroll_amount;	/* scroll this amount at scroll_margin */
	int	wc_scroll_margin;	/* scroll map when this far from
						the edge */
	int	wc_map_mode;		/* specify map viewing options, mostly
						for backward compatibility */
	int	wc_player_selection;	/* method of choosing character */
	boolean	wc_splash_screen;	/* display an opening splash screen or not */
	boolean	wc_popup_dialog;	/* put queries in pop up dialogs instead of
				   		in the message window */
	boolean wc_eight_bit_input;	/* allow eight bit input               */
	boolean wc_mouse_support;	/* allow mouse support */
	boolean wc2_fullscreen;		/* run fullscreen */
	boolean wc2_softkeyboard;	/* use software keyboard */
	boolean wc2_wraptext;		/* wrap text */

	boolean  cmdassist;	/* provide detailed assistance for some commands */
	boolean	 obsolete;	/* obsolete options can point at this, it isn't used */
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
#ifdef WIN32CON
#define MAX_ALTKEYHANDLER 25
	char	 altkeyhandler[MAX_ALTKEYHANDLER];
#endif
};

struct sinfo {
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
#define CONFIGPREFIX	7
#define TROUBLEPREFIX	8
#define PREFIX_COUNT	9

#endif

