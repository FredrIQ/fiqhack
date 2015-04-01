/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-01 */
/* Copyright (c) Daniel Thaler, 2011                              */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef NHCURSES_H
# define NHCURSES_H

# define UNICODE
# define _CRT_SECURE_NO_WARNINGS  /* huge warning spew from MS CRT otherwise */

# include <stdlib.h>
# include <string.h>
# include <stdio.h>
# include <limits.h>

# if !defined(AIMAKE_BUILDOS_MSWin32)    /* UNIX + APPLE */
#  include <unistd.h>
#  include <strings.h>
#  define FILE_OPEN_MASK 0660
# else/* WINDOWS */

#  define boolean windows_h_boolean /* resolve symbol conflict */
#  include <windows.h>
#  undef boolean
#  include <shlobj.h>
#  undef MOUSE_MOVED    /* this definition from windows.h conflicts with a
                           definition from curses */
#  define random rand

#  if defined (_MSC_VER)
#   include <io.h>
#   pragma warning(disable:4996)/* disable warnings about deprecated posix
                                   function names */
/* If we're using the Microsoft compiler, we also get the Microsoft C lib which
 * doesn't have plain snprintf.  Note that this macro is an MSVC-style variadic
 * macro, which is not understood by gcc (it uses a different notation). */
#   define snprintf(buf, len, fmt, ...) \
    _snprintf_s(buf, len, len-1, fmt, __VA_ARGS__)
#   define snwprintf _snwprintf
#   define strncasecmp _strnicmp
#  endif
#  define FILE_OPEN_MASK (_S_IREAD | _S_IWRITE)
# endif

# include "compilers.h"

/* File handling compatibility.
 * On UNIX + APPLE, the normal API can handle multibyte strings, so no special
 * consideration for international charsets is required.  Windows on the other
 * hand has special variants of all api functions that take wchar_t strings.
 * The following abstactions are introduced to handle this:
 *  fnchar: a filename character
 *  fnncat: strncat for fnchars
 *  fnlen:  strlen for fnchars
 *  sys_open: system-dependent open
 */
# if !defined(WIN32)    /* UNIX + APPLE */
typedef char fnchar;

#  define sys_open  open
#  define fnncat(str1, str2, len) strncat(str1, str2, len)
#  define fnlen(str) strlen(str)
#  define FN(x) (x)
#  define FN_FMT "%s"
# else

typedef wchar_t fnchar;

#  define umask(x)
#  define fnncat(str1, str2, len) wcsncat(str1, str2, len)
#  define fnlen(str) wcslen(str)
#  define fopen(name, mode)  _wfopen(name, L ## mode)
#  define sys_open(name, flags, perm)  _wopen(name, flags | _O_BINARY, perm)
#  define unlink _wunlink
#  define FN(x) (L ## x)
#  define FN_FMT "%ls"
# endif


# include "nethack.h"
# include "menulist.h"

# ifdef NETCLIENT
#  define NHNET_TRANSPARENT
#  include "nethack_client.h"
# endif

# include "uncursed.h"

# ifndef max
#  define max(a,b) ((a) > (b) ? (a) : (b))
# endif
# ifndef min
#  define min(x,y) ((x) < (y) ? (x) : (y))
# endif

/* attributes for dialog frames */
# define FRAME_PAIR     6             /* magenta frames for better visibility */
# define NOEDIT_FRAME_PAIR 1          /* dark gray if the dialog doesn't work */

# define MAINFRAME_PAIR 72            /* 8 * 8 + 7 + 1 */

# define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

/* A statically-allocated menulist has a size of 0 and non-null items; that's
   how the menulist management code knows it's statically allocated. */
# define STATIC_MENULIST(x) (&(struct nh_menulist){ \
                .items = (x),                       \
                .icount = ARRAY_SIZE(x),            \
                .size = 0})

/* For curses_menu_callback, chosen to reduce clashes */
# define CURSES_MENU_CANCELLED (INT_MIN + 3)

enum game_dirs {
    CONFIG_DIR,
    SAVE_DIR,
    LOG_DIR,
    DUMP_DIR,
    TILESET_DIR
};

/* A boolean that can be "pinned" at a particular value, or that can be left to
   have its value chosen automatically. 

   I'm /so/ tempted to just use FALSE, TRUE, and ENOENT instead... */
enum autoable_boolean {
    AB_FALSE,
    AB_TRUE,
    AB_AUTO,
};

/* Any extra space on the terminal is used to give advice about controls. This
   enum tracks the context in which a key is being requested, so as to be able
   to give good advice. This is also used to implement the occasional special
   case. */
enum keyreq_context {
    krc_getdir,
    krc_getlin,
    krc_yn,
    krc_ynq,
    krc_yn_generic,
    krc_count,
    krc_get_command,
    krc_interrupt_long_action,
    krc_get_movecmd_direction,
    krc_getpos,
    krc_menu,
    krc_objmenu,
    krc_more,
    krc_pause_map,
    krc_notification,
    krc_keybinding,

    /* Keep these in the same order as in nethack_types.h */
    krc_query_key_inventory,
    krc_query_key_inventory_nullable,
    krc_query_key_inventory_or_floor,
    krc_query_key_symbol,
    krc_query_key_letter_reassignment,
};


struct interface_flags {
    int done_hup;
    nh_bool initialized;  /* false before init_curses_ui() */
    nh_bool ingame;
    nh_bool connected_to_server;
    enum nh_followmode current_followmode;
    enum nh_followmode available_followmode;
    nh_bool in_zero_time_command;
    const char *gameload_message;                 /* string literal or NULL */
    int queued_server_cancels;

    /* Window layout dimensions.

       These dimensions are the amount of space actually allocated on screen, in
       characters; settings, COLNO, etc. hold the amount of space we'd actually
       like to have.  For instance, if we want to fit the map into 90
       characters, and are using an ASCII interface where the map only takes up
       80 characters of space, mapwidth is nontheless 90, and map_padding is
       5. */
    nh_bool draw_horizontal_frame_lines;
    nh_bool draw_vertical_frame_lines;
    nh_bool draw_outer_frame_lines;
    int statusheight; /* usually 3 or 2; can be smaller in emergencies */
    int mapheight;    /* height of the content area of the map */
    int msgheight;    /* amount of height the message area occupies */
    int extraheight;  /* amount of height the extra window occupies */
    int mapwidth;     /* width of the content area of the map */
    int sidebarwidth; /* width of the sidebar */
    int map_padding;  /* padding on each side of the map */

    nh_bool asciiborders;

    nh_bool extrawin_populated;

    int levelmode;
    int playmode;

    int connection_only;  /* connect to localhost, don't play normally */
    int no_stop;          /* do not allow the process to suspend */

    /* These values are -1, -1 while the mouse cursor is not over the map, or an
       x, y pair while it is. */
    int maphoverx;
    int maphovery;

    nh_bool want_cursor;       /* delayed-action cursor setting */

    char username[BUFSZ];      /* username being used in connection-only mode */
};

enum nh_palette {
    PALETTE_NONE,      /*  Keep default terminal palette */
    PALETTE_DEFAULT,   /*  The libuncursed default palette */
    PALETTE_ALT1,      /*  Alternative palette 1, stef70 */
    PALETTE_ALT2,      /*  Alternative palette 2, stef70 */
    PALETTE_ALT3,      /*  Alternative palette 3, stef70 */
    PALETTE_SATURATED, /*  Saturated Colors, jonadab */
    PALETTE_TERTIARY,  /*  Tertiary Colors, jonadab */
    PALETTE_TTERMINAL, /*  Defaults from a typical X11 terminal */
    PALETTE_LCONSOLE,  /*  Konsole claims these are the Linux vt values */
    PALETTE_EQUILUM,   /*  Equiluminous, from paxed's wintermcolors.php */
    PALETTE_DKPASTELS, /*  Dark Pastels, from Konsole via paxed */
    PALETTE_REDMOND,   /*  Windows console default colors */
};

enum nh_animation {
    ANIM_INSTANT,         /* no animation */
    ANIM_INTERRUPTIBLE,   /* animate only interruptible events */
    ANIM_ALL,             /* animate all events */
    ANIM_SLOW,            /* animate all events, slowly */
};

enum nh_motd_setting {
    MOTD_TRUE,
    MOTD_FALSE,
    MOTD_ASK,
};

enum nh_menupaging {
    MP_LINES,
    MP_PAGES,
};

enum nh_frame {
    FRAME_ALL,
    FRAME_MENUS,
    FRAME_NONE,
};

struct settings {
    nh_bool end_own;    /* list all own scores */
    int end_top, end_around;    /* describe desired score list */
    int menu_headings;  /* ATR for menu headings */
    int msgheight;      /* requested height of the message win */
    int msghistory;     /* # of historic messages to keep for prevmsg display */
    int optstyle;       /* option display style */

    enum autoable_boolean sidebar;   /* whether to draw the inventory sidebar */
    enum nh_animation animation;     /* when to delay */
    enum nh_motd_setting show_motd;
    enum nh_menupaging menupaging;
    enum nh_frame whichframes;
    enum nh_palette palette;         /* palette to use for text */

    char *tileset;                   /* tileset file name */

    nh_bool alt_is_esc; /* parse Alt-letter as ESC letter */
    /* use bolded black instead of dark blue for CLR_BLACK */
    nh_bool darkgray;
    nh_bool dungeoncolor;       /* respect level display modes */
    nh_bool visible_rock;       /* render solid rock unlike unexplored */
    nh_bool extmenu;    /* extended commands use menu interface */
    nh_bool hilite_pet; /* hilight pets */
    nh_bool showexp;    /* show experience points */
    nh_bool use_inverse;        /* use inverse video for some things */
    nh_bool invweight;  /* show item weight in the inventory */
    nh_bool bgbranding; /* show hidden traps/stairs with background */
    nh_bool floorcolor; /* draw stepped-on information for the floor */
    nh_bool status3;    /* draw 3 line status */
    nh_bool mouse;      /* accept mouse input */
    nh_bool prompt_inline; /* draw prompts in the message window */
};


/* curses_symdef is like nh_symdef, but with extra unicode sauce */
struct curses_symdef {
    const char *symname;
    int color;
    wchar_t unichar[CCHARW_MAX + 1];
    short ch;   /* for non-unicode displays */
    nh_bool custom;     /* true if this is a custom value that was explicitly
                           changed by the user */
};


struct curses_drawing_info {
    /* background layer symbols: nh_dbuf_entry.bg */
    struct curses_symdef *bgelements;
    /* trap layer symbols: nh_dbuf_entry.trap */
    struct curses_symdef *traps;
    /* object layer symbols: nh_dbuf_entry.obj */
    struct curses_symdef *objects;
    /* invisible monster symbol: show this if nh_dbuf_entry.invis is true */
    struct curses_symdef *invis;
    /* monster layer symbols: nh_dbuf_entry.mon symbols with id <= num_monsters 
       are actual monsters, followed by warnings */
    struct curses_symdef *monsters;
    struct curses_symdef *warnings;
    /* effect layer symbols: nh_dbuf_entry.effect NH_EFFECT_TYPE */
    struct curses_symdef *explsyms;
    struct curses_symdef *expltypes;
    struct curses_symdef *zapsyms;     /* default zap symbols; no color info */
    struct curses_symdef *zaptypes;    /* zap beam types + colors. no symbols */
    struct curses_symdef *effects;     /* shield, boomerang, digbeam,
                                          flashbeam, gascloud */
    struct curses_symdef *swallowsyms; /* no color info: use the color of the
                                          swallower */
    int num_bgelements;
    int num_traps;
    int num_objects;
    int num_monsters;
    int num_warnings;
    int num_expltypes;
    int num_zaptypes;
    int num_effects;

    int bg_feature_offset;
};

struct gamewin {
    void (*draw) (struct gamewin * gw);
    void (*resize) (struct gamewin * gw);
    WINDOW *win, *win2;
    struct gamewin *next, *prev;
    char **dyndata;
    void *extra[];
};


# define MAXCOLS 16
struct win_menu {
    struct nh_menuitem *items;
    char *selected;
    const char *title;
    int icount, how, offset, placement_hint;
    int height, frameheight, innerheight;
    int width, innerwidth, colpos[MAXCOLS], maxcol;
    int x1, y1, x2, y2;
    int dismissable;
};

struct win_objmenu {
    struct nh_objitem *items;
    int *selected;
    const char *title;
    int icount, how, offset, selcount, placement_hint;
    int height, frameheight, innerheight;
    int width, innerwidth;
};

struct win_getline {
    char *buf;
    size_t buf_alloclen;
    const char *query;
    int pos;
};

struct win_msgwin {
    const char *msg;
    int layout_width;
    int layout_height;
    enum keyreq_context context;
};

typedef nh_bool(*getlin_hook_proc) (struct win_getline *, void *);

/*----------------------------------------------------------------------------*/

extern struct settings settings;
extern struct interface_flags ui_flags;
extern nh_bool interrupt_multi, game_is_running;
extern const char quit_chars[];
extern struct nh_window_procs curses_windowprocs;
extern WINDOW *basewin, *mapwin, *msgwin, *statuswin, *sidebar, *extrawin;
extern struct curses_drawing_info *default_drawing;
extern int curses_level_display_mode;
extern struct nh_player_info player;
extern int cmdline_role, cmdline_race, cmdline_gend, cmdline_align;
extern char cmdline_name[];
extern nh_bool random_player;
extern struct gamewin *firstgw, *lastgw;
extern struct nh_cmd_desc *keymap[KEY_MAX + 1];
extern const char *nhlogo_small[11], *nhlogo_large[14];
extern char *override_hackdir, *override_userdir;
extern int repeats_remaining;
extern char *tiletable;
extern int tiletable_len;
extern nh_bool tiletable_is_cchar;

/*----------------------------------------------------------------------------*/

/* color.c */
extern void init_nhcolors(void);
extern int curses_color_attr(int nh_color, int bg_color);
extern void set_darkgray(void);

/* dialog.c */
extern WINDOW *newdialog(int height, int width, int dismissable, WINDOW *old);
extern enum nh_direction curses_getdir(const char *query, nh_bool restricted);
extern char curses_yn_function_game(const char *query, const char *resp,
                                    char def);
extern char curses_yn_function_internal(const char *query, const char *resp,
                                        char def);
extern struct nh_query_key_result curses_query_key_game(
    const char *query, enum nh_query_key_flags flags, nh_bool count_allowed);
extern struct nh_query_key_result curses_query_key_internal(
    const char *query, enum nh_query_key_flags flags, nh_bool count_allowed);
extern int curses_msgwin(const char *msg, enum keyreq_context context);

/* extrawin.c */
extern int classify_key(int);
extern void draw_extrawin(enum keyreq_context context);
extern void clear_extrawin(void);

/* gameover.c */
extern void curses_outrip(
    struct nh_menulist *ml, nh_bool tombstone, const char *name, int gold,
    const char *killbuf, int how, int year);


/* getline.c */
extern void draw_getline(struct gamewin *gw);
extern void curses_get_ext_cmd(
    const char **namelist, const char **desclist, int listlen,
    void *callbackarg, void (*callback)(const char *, void *));
extern void curses_getline(const char *query, void *callbackarg,
                           void (*callback)(const char *, void *));
extern void curses_getline_pw(const char *query, void *callbackarg,
                              void (*callback)(const char *, void *));

/* keymap.c */
extern void handle_internal_cmd(struct nh_cmd_desc **cmd,
                                struct nh_cmd_arg *arg,
                                nh_bool include_debug);
extern void get_command(void *callbackarg,
                        void (*callback)(const struct nh_cmd_and_arg *, void *),
                        nh_bool include_debug);
extern void set_next_command(const char *cmd, struct nh_cmd_arg *arg);
extern void load_keymap(void);
extern void free_keymap(void);
extern void show_keymap_menu(nh_bool readonly);
extern void handle_nested_key(int key);
extern enum nh_direction key_to_dir(int key, int* range);

/* main.c */
extern void curses_impossible(const char *msg);

/* map.c */
extern int get_map_key(nh_bool place_cursor, nh_bool report_clicks,
                       enum keyreq_context context);
extern void curses_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO],
                                 int ux, int uy);
extern struct nh_getpos_result curses_getpos(int x, int y, nh_bool force,
                                             const char *goal);
extern void draw_map(int cx, int cy);
extern void mark_mapwin_for_full_refresh(void);

extern nh_bool branding_is_at(short branding, int x, int y);
extern nh_bool monflag_is_at(short monflag, int x, int y);
extern nh_bool apikey_is_at(const char *apikey, int x, int y);

/* menu.c */
extern void draw_menu(struct gamewin *gw);
extern void curses_menu_callback(const int *results, int nresults, void *arg);
extern void curses_display_menu(
    struct nh_menulist *ml, const char *title, int how, int placement_hint,
    void *callbackarg, void (*callback)(const int *, int, void *));
extern void curses_display_menu_core(
    struct nh_menulist *ml, const char *title, int how,
    void *callbackarg, void (*callback)(const int *, int, void *),
    int x1, int y1, int x2, int y2, nh_bool bottom,
    nh_bool(*changefn)(struct win_menu *, int), nh_bool);
extern void curses_display_objects(
    struct nh_objlist *objlist, const char *title, int how, int placement_hint,
    void *callbackarg, void (*callback)(const struct nh_objresult *,
                                        int, void *));
extern void draw_objlist(WINDOW * win, struct nh_objlist *objlist,
                         int *selected, int how);
extern nh_bool do_item_actions(const struct nh_objitem *);

/* messages.c */
extern void alloc_hist_array(void);
extern void setup_showlines(void);
extern void redo_showlines(void);
extern void curses_print_message(int turn, const char *msg);
extern void curses_print_message_nonblocking(int turn, const char *inmsg);
extern void curses_temp_message(const char *msg);
extern void curses_clear_temp_messages(void);
extern void draw_msgwin(void);
extern void mark_showlines_seen(void);
extern void fresh_message_line(nh_bool blocking);
extern void pause_messages(void);
extern void doprev_message(void);
extern void cleanup_messages(void);
extern void new_action(void);
extern void wrap_text(int width, const char *input, int *output_count,
                      char ***output);
extern void free_wrap(char **wrap_output);

/* motd.c */
extern int network_motd(void);

/* options.c */
extern struct nh_option_desc *curses_get_nh_opts(void);
extern void curses_free_nh_opts(struct nh_option_desc *opts);
extern nh_bool curses_set_option(const char *name, union nh_optvalue value);
extern void display_options(nh_bool change_birth_opt);
extern void print_options(void);
extern void init_options(void);
extern nh_bool read_nh_config(void);
extern void read_ui_config(void);
extern void write_nh_config(void);
extern void write_ui_config(void);


/* outchars.c */
extern void init_displaychars(void);
extern void free_displaychars(void);
extern unsigned long long dbe_substitution(struct nh_dbuf_entry *dbe);
extern void print_tile(WINDOW *win, struct curses_symdef *api_name,
                       struct curses_symdef *api_type, int offset,
                       unsigned long long substitution);
extern void print_background_tile(WINDOW *win, struct nh_dbuf_entry *dbe);
extern void print_low_priority_brandings(WINDOW *win,
                                         struct nh_dbuf_entry *dbe);
extern void print_high_priority_brandings(WINDOW *win,
                                          struct nh_dbuf_entry *dbe);
extern void print_cchar(WINDOW *win);
extern void init_cchar(char c);
extern void curses_notify_level_changed(int dmode);

/* playerselect.c */
extern nh_bool player_selection(int *out_role, int *out_race, int *out_gend,
                                int *out_align, int randomall);

/* replay.c */
extern void replay(void);

/* rungame.c */
extern nh_bool get_gamedir(enum game_dirs dirtype, fnchar *buf);
extern nh_bool get_gamedirA(enum game_dirs dirtype, char *buf);
extern void curses_request_command(nh_bool debug, nh_bool completed,
                                   nh_bool interrupted, void *callbackarg,
                                   void (*)(const struct nh_cmd_and_arg *cmd,
                                            void *callbackarg));
extern void describe_game(char *buf, enum nh_log_status status,
                          struct nh_game_info *gi);
extern void rungame(nh_bool net);
extern nh_bool loadgame(void);
extern void game_ended(int status, fnchar *filename, nh_bool net);
extern fnchar **list_gamefiles(fnchar *dir, int *count);
extern enum nh_play_status playgame(int fd_or_gameno, enum nh_followmode);

/* sidebar.c */
extern void draw_sidebar(void);
extern void curses_list_items(struct nh_objlist *objlist, nh_bool invent);
extern void curses_list_items_nonblocking(struct nh_objlist *objlist,
                                          nh_bool invent);
extern void cleanup_sidebar(nh_bool dealloc);
extern void item_actions_from_sidebar(char accel);

/* status.c */
extern void curses_update_status(struct nh_player_info *pi);
extern void curses_update_status_silent(struct nh_player_info *pi);

/* topten.c */
extern void show_topten(char *player, int top, int around, nh_bool own);

/* windows.c */
extern void init_curses_ui(const char *dataprefix);
extern void exit_curses_ui(void);
extern void set_font_file(const char *);
extern void set_tile_file(const char *);
extern int nh_curs_set(int);
extern void nh_mvwvline(WINDOW *, int, int, int);
extern void nh_mvwhline(WINDOW *, int, int, int);
extern void nh_window_border(WINDOW *, int);
extern WINDOW *newwin_onscreen(int, int, int, int);
extern void create_game_windows(void);
extern void destroy_game_windows(void);
extern void redraw_popup_windows(void);
extern void redraw_game_windows(void);
extern void handle_resize(void);
extern void rebuild_ui(void);
extern int nh_wgetch(WINDOW * win, enum keyreq_context context);
extern struct gamewin *alloc_gamewin(int extra);
extern void delete_gamewin(struct gamewin *win);
extern void delete_all_gamewins(void);
extern void curses_pause(enum nh_pause_reason reason);
extern void curses_display_buffer(const char *buf, nh_bool trymove);
extern void curses_raw_print(const char *str);
extern void curses_delay_output(void);
extern void curses_load_progress(int progress);
extern void curses_server_cancel(void);
extern void setup_palette(void);

# if defined(NETCLIENT)
/* netgame.c */
extern void netgame(void);

/* netplay.c */
extern void net_loadgame(void);
extern void net_replay(void);
# endif

#endif
