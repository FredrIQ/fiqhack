/* Copyright (c) David Cohrs, 1991,1992				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINTTY_H
#define WINTTY_H

#ifndef NETHACKDIR
#define NETHACKDIR "/usr/share/NetHack/"
#endif

/* generic parameter - must not be any larger than a pointer */
typedef union any {
    void * a_void;
    struct obj *a_obj;
    int  a_int;
    char a_char;
    schar a_schar;
    /* add types as needed */
} anything;
#define ANY_P union any /* avoid typedef in prototypes */

/* menu return list */
typedef struct mi {
    anything item;		/* identifier */
    long count;			/* count */
} menu_item;
#define MENU_ITEM_P struct mi

#ifndef WINDOW_STRUCTS
#define WINDOW_STRUCTS

/* menu structure */
typedef struct tty_mi {
    struct tty_mi *next;
    anything identifier;	/* user identifier */
    long count;			/* user count */
    char *str;			/* description string (including accelerator) */
    int attr;			/* string attribute */
    boolean selected;		/* TRUE if selected by user */
    char selector;		/* keyboard accelerator */
    char gselector;		/* group accelerator */
} tty_menu_item;

/* descriptor for tty-based windows */
struct WinDesc {
    int flags;			/* window flags */
    xchar type;			/* type of window */
    boolean active;		/* true if window is active */
    uchar offx, offy;		/* offset from topleft of display */
    short rows, cols;		/* dimensions */
    short curx, cury;		/* current cursor position */
    short maxrow, maxcol;	/* the maximum size used -- for MENU wins */
				/* maxcol is also used by WIN_MESSAGE for */
				/* tracking the ^P command */
    short *datlen;		/* allocation size for *data */
    char **data;		/* window data [row][column] */
    char *morestr;		/* string to display instead of default */
    tty_menu_item *mlist;	/* menu information (MENU) */
    tty_menu_item **plist;	/* menu page pointers (MENU) */
    short plist_size;		/* size of allocated plist (MENU) */
    short npages;		/* number of pages in menu (MENU) */
    short nitems;		/* total number of items (MENU) */
    short how;			/* menu mode - pick 1 or N (MENU) */
    char menu_ch;		/* menu char (MENU) */
};

/* window flags */
#define WIN_CANCELLED 1
#define WIN_STOP 1		/* for NHW_MESSAGE; stops output */

/* descriptor for tty-based displays -- all the per-display data */
struct DisplayDesc {
    uchar rows, cols;		/* width and height of tty display */
    uchar curx, cury;		/* current cursor position on the screen */
    int color;			/* current color */
    int attrs;			/* attributes in effect */
    int toplin;			/* flag for topl stuff */
    int rawprint;		/* number of raw_printed lines since synch */
    int inmore;			/* non-zero if more() is active */
    int inread;			/* non-zero if reading a character */
    int intr;			/* non-zero if inread was interrupted */
    winid lastwin;		/* last window used for I/O */
    char dismiss_more;		/* extra character accepted at --More-- */
};

#endif /* WINDOW_STRUCTS */

struct interface_flags {
	boolean  cbreak;	/* in cbreak mode, rogue format */    
	boolean  showexp;	/* show experience points */
	boolean  showscore;	/* show score */
    	boolean  standout;	/* use standout for --More-- */
	boolean  time;		/* display elapsed 'time' */
	boolean  use_inverse;	/* use inverse video for some things   */
	int menu_headings;	/* ATR for menu headings */
};

extern struct interface_flags ui_flags;

#define MAXWIN 20		/* maximum number of windows, cop-out */

/* tty dependent window types */
#ifdef NHW_BASE
#undef NHW_BASE
#endif
#define NHW_BASE    6

extern char *hackdir;
extern char *var_playground;
extern struct window_procs tty_procs;
extern int hackpid;

/* port specific variable declarations */
extern winid BASE_WINDOW;

extern struct WinDesc *wins[MAXWIN];

extern struct DisplayDesc *ttyDisplay;	/* the tty display descriptor */

extern char morc;		/* last character typed to xwaitforspace */
extern char defmorestr[];	/* default --more-- prompt */

/* port specific external function references */

/* ### getline.c ### */
extern void xwaitforspace(const char *);

/* ### termcap.c, video.c ### */

extern void tty_startup(int*, int*);
#ifndef NO_TERMS
extern void tty_shutdown(void);
#endif
extern void xputc(char);
extern void xputs(const char *);
#if defined(SCREEN_VGA) || defined(SCREEN_8514)
extern void xputg(int, int, unsigned);
#endif
extern void cl_end(void);
extern void clear_screen(void);
extern void home(void);
extern void standoutbeg(void);
extern void standoutend(void);

extern void backsp(void);
extern void graph_on(void);
extern void graph_off(void);
extern void cl_eos(void);

/*
 * termcap.c (or facsimiles in other ports) is the right place for doing
 * strange and arcane things such as outputting escape sequences to select
 * a color or whatever.  wintty.c should concern itself with WHERE to put
 * stuff in a window.
 */
extern void term_start_attr(int attr);
extern void term_end_attr(int attr);
extern void term_start_raw_bold(void);
extern void term_end_raw_bold(void);

extern void term_end_color(void);
extern void term_start_color(int color);
extern int has_color(int color);


/* ### topl.c ### */

extern void addtopl(const char *);
extern void more(void);
extern void update_topl(const char *);
extern void putsyms(const char*);

/* ### wintty.c ### */
extern void docorner(int, int);
extern void end_glyphout(void);
extern void g_putch(int);
extern void win_tty_init(void);

/* external declarations */
extern void tty_init_nhwindows(void);
extern void tty_player_selection(int,int,int,int,int);
extern void tty_askname(char*);
extern void tty_get_nh_event(void);
extern void tty_exit_nhwindows(const char *);
extern void tty_suspend_nhwindows(const char *);
extern void tty_resume_nhwindows(void);
extern winid tty_create_nhwindow(int);
extern void tty_clear_nhwindow(winid);
extern void tty_display_nhwindow(winid, boolean);
extern void tty_dismiss_nhwindow(winid);
extern void tty_destroy_nhwindow(winid);
extern void tty_curs(winid,int,int);
extern void tty_putstr(winid, int, const char *);
extern void tty_display_buffer(char *,boolean);
extern void tty_update_status(struct nh_status_info *status);
extern void tty_print_message(const char *);
extern void tty_start_menu(winid);
extern void tty_add_menu(winid,int,const ANY_P *,
		char,char,int,const char *, boolean);
extern void tty_end_menu(winid, const char *);
extern int tty_select_menu(winid, int, MENU_ITEM_P **);
extern int tty_display_menu(struct nh_menuitem*, int, const char*, int, int*);
extern int tty_display_objects(struct nh_objitem*, int, const char*, int, struct nh_objresult*);
extern char tty_message_menu(char,int,const char *);
extern void tty_update_inventory(void);
extern void tty_mark_synch(void);
extern void tty_wait_synch(void);
extern void tty_print_glyph(winid,xchar,xchar,int);
extern void tty_raw_print(const char *);
extern void tty_raw_print_bold(const char *);
extern int tty_nhgetch(void);
extern int tty_nh_poskey(int *, int *, int *);
extern void tty_nhbell(void);
extern int tty_doprev_message(void);
extern char tty_yn_function(const char *, const char *, char);
extern void tty_getlin(const char *,char *);
extern int tty_get_ext_cmd(const char **namelist, const char **desclist, int listlen);
extern void tty_number_pad(int);
extern void tty_delay_output(void);
extern void tty_outrip(struct nh_menuitem *items,int icount, int how,
		       char *plname, long gold, char *killbuf, int year);

extern void gettty(void);
extern void settty(const char *);
extern void setftty(void);
extern void error(const char *,...);
#if defined(UNIX)
extern void intron(void);
extern void introff(void);
#endif /* UNIX */

extern int base_nhgetch(void);

/* other defs that really should go away (they're tty specific) */
extern void tty_start_screen(void);
extern void tty_end_screen(void);

/* optwin.c */
extern void tty_init_options(void);
extern void display_options(boolean);
extern void read_config(void);
extern void write_config(void);
extern EXPORT void add_menu_cmd_alias(char, char);
extern EXPORT char map_menu_cmd(char);

extern char mapped_menu_cmds[];

extern int locknum;
extern const char quitchars[];
extern const char ynchars[];
extern void getlock(void);

/* ioctl.c */
#ifdef UNIX
extern void getioctls(void);
extern void setioctls(void);
#endif


#ifdef NO_TERMS
# if defined(WIN32CON)
#   undef putchar
#   undef putc
#   undef puts
#   define putchar(x) xputc(x)	/* these are in video.c, nttty.c */
#   define putc(x) xputc(x)
#   define puts(x) xputs(x)
# endif
#endif/*NO_TERMS*/

/* menu window keyboard commands (may be mapped) */
#define MENU_FIRST_PAGE		'^'
#define MENU_LAST_PAGE		'|'
#define MENU_NEXT_PAGE		'>'
#define MENU_PREVIOUS_PAGE	'<'
#define MENU_SELECT_ALL		'.'
#define MENU_UNSELECT_ALL	'-'
#define MENU_INVERT_ALL		'@'
#define MENU_SELECT_PAGE	','
#define MENU_UNSELECT_PAGE	'\\'
#define MENU_INVERT_PAGE	'~'
#define MENU_SEARCH		':'


/* system terminal functions */
extern int tgetent(char *,const char *);
extern void tputs(const char *,int,int (*)(void));
extern int tgetnum(const char *);
extern int tgetflag(const char *);
extern char *tgetstr(const char *,char **);
extern char *tgoto(const char *,int,int);


#endif /* WINTTY_H */
