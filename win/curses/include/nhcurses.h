/* Copyright (c) Daniel Thaler, 2011				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef NHCURSES_H
#define NHCURSES_H

#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/curses.h>
#include "nethack.h"

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef NETHACKDIR
#define NETHACKDIR "/usr/share/NetHack/"
#endif

#define KEY_ESC 27
#define KEY_BACKDEL 127

#define FRAME_ATTRS  (COLOR_PAIR(6)) /* magenta frames for better visibility */

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

/* I'm not at all sure resizing is a good idea.
 * Resizing is essentially broken, because resizing in ncurses (5.9) is a mess.
 * I got crashes in malloc(?!) in the resizeterm callchain, far away from any
 * of this code. */
// #define ALLOW_RESIZE


enum game_dirs {
    CONFIG_DIR,
    SAVE_DIR,
    LOG_DIR
};


struct interface_flags {
    boolean done_hup;
    boolean ingame;
    boolean draw_frame;
    boolean draw_sidebar;
    boolean status3; /* draw the 3 line status instead of the classic 2 lines */
    boolean color; /* the terminal has color capability */
    boolean unicode; /* ncurses detected a unicode locale */
    int levelmode;
    int playmode;
    int viewheight;
    int msgheight; /* actual height */
};

struct settings {
    char     plname[BUFSZ]; /* standard player name;
                             * size is BUFSZ rather than PL_NSIZ, because the
			     * buffer may be written to directly via curses_getline */
    boolean  end_own;	/* list all own scores */
    int      end_top, end_around;	/* describe desired score list */
    int      graphics;
    int      menu_headings; /* ATR for menu headings */
    int      msgheight; /* requested height of the message win */
    int      msghistory;/* # of historic messages to keep for prevmsg display */
    int      optstyle;	/* option display style */
    boolean  extmenu;	/* extended commands use menu interface */
    boolean  hilite_pet;/* hilight pets */
    boolean  showexp;	/* show experience points */
    boolean  showscore;	/* show score */
    boolean  standout;	/* use standout for --More-- */
    boolean  time;	/* display elapsed 'time' */
    boolean  use_inverse; /* use inverse video for some things   */
    boolean  ignintr;	/* ignore interrupts */
    boolean  unicode;	/* try to display unicode chars */
    boolean  invweight;	/* show item weight in the inventory */
    boolean  blink;	/* show multiple symbols for each location by blinking */
    boolean  sidebar;   /* draw the inventory sidebar */
    boolean  frame;     /* draw a frame around the window sections */
    boolean  status3;	/* draw 3 line status */
};


/* curses_symdef is like nh_symdef, but with extra unicode sauce */
struct curses_symdef {
    char *symname;
    int color;
    wchar_t unichar[CCHARW_MAX+1];
    short ch; /* for non-unicode displays */
};


struct curses_drawing_info {
    /* background layer symbols: nh_dbuf_entry.bg */
    struct curses_symdef *bgelements;
    /* background layer symbols: nh_dbuf_entry.bg */
    struct curses_symdef *traps;
    /* object layer symbols: nh_dbuf_entry.obj */
    struct curses_symdef *objects;
    /* invisible monster symbol: show this if nh_dbuf_entry.invis is true */
    struct curses_symdef *invis;
    /* monster layer symbols: nh_dbuf_entry.mon
     * symbols with id <= num_monsters are actual monsters, followed by warnings */
    struct curses_symdef *monsters;
    struct curses_symdef *warnings;
    /* effect layer symbols: nh_dbuf_entry.effect
     * NH_EFFECT_TYPE */
    struct curses_symdef *explsyms;
    struct curses_symdef *expltypes;
    struct curses_symdef *zapsyms; /* default zap symbols; no color info */
    struct curses_symdef *zaptypes; /* zap beam types + colors. no symbols */
    struct curses_symdef *effects; /* shield, boomerang, digbeam, flashbeam, gascloud */
    struct curses_symdef *swallowsyms; /* no color info: use the color of the swallower */
    int num_bgelements;
    int num_traps;
    int num_objects;
    int num_monsters;
    int num_warnings;
    int num_expltypes;
    int num_zaptypes;
    int num_effects;
};

/*
 * Graphics sets for display symbols
 */
enum nh_text_mode {
    ASCII_GRAPHICS,	/* regular characters: '-', '+', &c */
    IBM_GRAPHICS,	/* PC graphic characters */
    DEC_GRAPHICS,	/* VT100 line drawing characters */
    UNICODE_GRAPHICS	/* uses whatever charecters we want: they're ALL available */
};

typedef boolean (*getlin_hook_proc)(char *, void *);



struct gamewin {
    void (*draw)(struct gamewin *gw);
    void (*resize)(struct gamewin *gw);
    WINDOW *win;
    struct gamewin *next, *prev;
    void *extra[0];
};


#define MAXCOLS 16
struct win_menu {
    WINDOW *content;
    struct nh_menuitem *items;
    char *selected;
    const char *title;
    int icount, how, offset;
    int height, frameheight, innerheight;
    int width, innerwidth, colpos[MAXCOLS], maxcol;
    int x1, y1, x2, y2;
};

struct win_objmenu {
    WINDOW *content;
    struct nh_objitem *items;
    int *selected;
    const char *title;
    int icount, how, offset, selcount;
    int height, frameheight, innerheight;
    int width, innerwidth;
};

struct win_getline {
    char *buf;
    const char *query;
    int pos;
};

/*----------------------------------------------------------------------------*/

extern struct settings settings;
extern struct interface_flags ui_flags;
extern boolean interrupt_multi, game_is_running;
extern const char quitchars[];
extern struct nh_window_procs curses_windowprocs;
extern WINDOW *mapwin, *msgwin, *statuswin, *sidebar;
extern struct curses_drawing_info *default_drawing, *cur_drawing;
extern struct nh_player_info player;
extern int initrole, initrace, initgend, initalign;
extern boolean random_player;

/*----------------------------------------------------------------------------*/

/* dialog.c */
extern WINDOW *newdialog(int height, int width);
extern enum nh_direction curses_getdir(const char *query, boolean restricted);
extern char curses_yn_function(const char *query, const char *resp, char def);
extern char curses_query_key(const char *query, int *count);
extern int curses_msgwin(const char *msg);

/* gameover.c */
extern void curses_outrip(struct nh_menuitem *items, int icount, boolean tombstone,
		          char *plname, long gold, char *killbuf, int year);


/* getline.c */
extern void draw_getline(struct gamewin *gw);
extern boolean curses_get_ext_cmd(char *cmd_out, const char **namelist,
			   const char **desclist, int listlen);
extern void curses_getline(const char *query, char *buffer);

/* keymap.c */
extern const char *get_command(int *count, struct nh_cmd_arg *arg);
extern void load_keymap(void);
extern void free_keymap(void);
extern void show_keymap_menu(boolean readonly);
extern enum nh_direction key_to_dir(int key);

/* map.c */
extern void init_nhcolors(void);
extern void curses_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO]);
extern void curses_clear_map(void);
extern int curses_getpos(int *x, int *y, boolean force, const char *goal);
extern void draw_map(int frame);

/* menu.c */
extern void draw_menu(struct gamewin *gw);
extern int curses_display_menu(struct nh_menuitem *items, int icount,
			       const char *title, int how, int *results);
extern int curses_display_menu_core(struct nh_menuitem *items, int icount,
			     const char *title, int how, int *results,
			     int x1, int y1, int x2, int y2,
			     boolean (*changefn)(struct win_menu*, int));
extern int curses_display_objects(struct nh_objitem *items, int icount,
		  const char *title, int how, struct nh_objresult *pick_list);
extern void draw_objlist(WINDOW *win, int icount, struct nh_objitem *items,
		  int *selected, int how);

/* messages.c */
extern void alloc_hist_array(void);
extern void curses_print_message(int turn, const char *msg);
extern void newturn(void);
extern void draw_msgwin(void);
extern void pause_messages(void);
extern void doprev_message(void);
extern void cleanup_messages(void);

/* options.c */
extern void display_options(boolean change_birth_opt);
extern void init_options(void);
extern void read_nh_config(void);
extern void write_config(void);

/* outchars.c */
extern void init_displaychars(void);
extern void free_displaychars(void);
extern int mapglyph(struct nh_dbuf_entry *dbe, struct curses_symdef *syms);
extern void set_rogue_level(boolean enable);
extern void switch_graphics(enum nh_text_mode mode);
extern int curses_color_attr(int nh_color);
extern void print_sym(WINDOW *win, struct curses_symdef *sym, int extra_attrs);
extern void curses_notify_level_changed(int dmode);

/* playerselect.c */
extern boolean player_selection(int *out_role, int *out_race, int *out_gend,
				int *out_align, int randomall);

/* rungame.c */
extern boolean get_gamedir(enum game_dirs dirtype, char *buf);
extern void rungame(void);
extern boolean loadgame(void);

/* sidebar.c */
extern void draw_sidebar(void);
extern boolean curses_list_items(struct nh_objitem *items, int icount, boolean invent);
extern void cleanup_sidebar(boolean dealloc);

/* status.c */
extern void curses_update_status(struct nh_player_info *pi);

/* topten.c */
extern void show_topten(char *player, int top, int around, boolean own);

/* windows.c */
extern void init_curses_ui(void);
extern void exit_curses_ui(void);
extern void create_game_windows(void);
extern void destroy_game_windows(void);
extern void redraw_game_windows(void);
extern void rebuild_ui(void);
extern int nh_wgetch(WINDOW *win);
extern struct gamewin *alloc_gamewin(int extra);
extern void delete_gamewin(struct gamewin *win);
extern void curses_pause(enum nh_pause_reason reason);
extern void curses_display_buffer(char *buf, boolean trymove);
extern void curses_raw_print(const char *str);
extern void curses_delay_output(void);

#endif
