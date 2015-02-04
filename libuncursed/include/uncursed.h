/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-03 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */

#include <stddef.h>     /* for wchar_t */
#include <stdarg.h>
#include <wchar.h>

#ifdef UNCURSED_MAIN_PROGRAM
# define EI(x) AIMAKE_EXPORT(x)
#else
# define EI(x) AIMAKE_IMPORT(x)
#endif

#define UNCURSED_ANDWINDOW(t, f, ...)                    \
    extern t EI(f)(__VA_ARGS__);                         \
    extern t EI(w##f)(WINDOW *, __VA_ARGS__)
#define UNCURSED_ANDWINDOWV(t, f)               \
    extern t EI(f)(void);                       \
    extern t EI(w##f)(WINDOW *)
#define UNCURSED_ANDMVWINDOW(t, f, ...)                      \
    extern t EI(f)(__VA_ARGS__);                             \
    extern t EI(w##f)(WINDOW *, __VA_ARGS__);                \
    extern t EI(mv##f)(int, int, __VA_ARGS__);               \
    extern t EI(mvw##f)(WINDOW *, int, int, __VA_ARGS__)
#define UNCURSED_ANDMVWINDOWV(t, f)                          \
    extern t EI(f)(void);                                    \
    extern t EI(w##f)(WINDOW *);                             \
    extern t EI(mv##f)(int, int);                            \
    extern t EI(mvw##f)(WINDOW *, int, int)

#define UNCURSED_DEPAREN(...) __VA_ARGS__

#define UNCURSED_ANDWINDOWDEF(t, f, proto, args)                \
    t f proto {return w##f(stdscr, UNCURSED_DEPAREN args);}     \
    t w##f (WINDOW *win, UNCURSED_DEPAREN proto)
#define UNCURSED_ANDWINDOWVDEF(t, f)                            \
    t f(void) {return w##f(stdscr);}                            \
    t w##f(WINDOW *win)

#define UNCURSED_ANDMVWINDOWDEF(t, f, proto, args)              \
    t f proto {return w##f(stdscr, UNCURSED_DEPAREN args);}     \
    t mv##f(int y, int x, UNCURSED_DEPAREN proto) {             \
        return mvw##f(stdscr, y, x, UNCURSED_DEPAREN args);     \
    }                                                           \
    t mvw##f(WINDOW *win, int y, int x, UNCURSED_DEPAREN proto) \
    {                                                           \
        if (wmove(win, y, x) == ERR) return ERR;                \
        return w##f(win, UNCURSED_DEPAREN args);                \
    }                                                           \
    t w##f(WINDOW *win, UNCURSED_DEPAREN proto)
#define UNCURSED_ANDMVWINDOWVDEF(t, f)                          \
    t f(void) {return w##f(stdscr);}                            \
    t mv##f(int y, int x) {                                     \
        return mvw##f(stdscr, y, x);                            \
    }                                                           \
    t mvw##f(WINDOW *win, int y, int x) {                       \
        if (wmove(win, y, x) == ERR) return ERR;                \
        return w##f(win);                                       \
    }                                                           \
    t w##f(WINDOW *win)

#define ERR        (-1)
#define OK           0
#define KEY_CODE_YES 2

typedef int uncursed_bool;
typedef unsigned long attr_t;
typedef unsigned long chtype;   /* attr_t | char */

enum uncursed_mousebutton {
    uncursed_mbutton_left,
    uncursed_mbutton_middle,
    uncursed_mbutton_right,
    uncursed_mbutton_wheelup,
    uncursed_mbutton_wheeldown,
    uncursed_mbutton_hover,

    uncursed_mbutton_count
};

typedef int uncursed_mouse_bindings[uncursed_mbutton_count];

#define CCHARW_MAX 5
typedef struct {
    attr_t attr;
    wchar_t chars[CCHARW_MAX];
    unsigned short color_on_screen; /* used only for disp_win, nout_win */
    uncursed_mouse_bindings bindings;
} cchar_t;


typedef struct WINDOW {
    attr_t current_attr;
    uncursed_mouse_bindings current_bindings;
    int y, x;
    int maxy, maxx;     /* maximum legal coordinate, e.g. maxy = 1 means height 
                           2 */
    int stride; /* (y,x) is at chararray[x+y*stride] */
    int scry, scrx;     /* top, right on screen */
    int regy, regx;     /* top, left of character region */
    void *region;       /* tile region */
    struct WINDOW *parent;      /* so parents aren't freed while children exist 
                                 */
    struct WINDOW *child;       /* for resizing */
    struct WINDOW *sibling;     /* ditto */
    cchar_t *chararray;
    void **regionarray; /* like chararray, but for tile regions */
    int timeout;
    uncursed_bool clear_on_refresh;
    uncursed_bool scrollok;
} WINDOW, *uncursed_WINDOW_p;

typedef char *uncursed_char_p;
typedef int *uncursed_int_p;
typedef wchar_t *uncursed_wchar_tp;

extern uncursed_WINDOW_p EI(stdscr);

/* uncursed-specific */
extern void EI(initialize_uncursed) (int *, char **);
extern void EI(uncursed_set_title) (const char *);

extern void EI(set_faketerm_font_file) (const char *);

extern void EI(set_tiles_tile_file) (const char *, int, int);
extern void EI(get_tile_dimensions) (int, int, int *, int *);
UNCURSED_ANDWINDOW(int, set_tiles_region,
                   int, int, int, int, int, int, int, int);
UNCURSED_ANDWINDOWV(int, delete_tiles_region);
UNCURSED_ANDMVWINDOW(int, set_tiles_tile, int);

typedef struct uncursed_palette16 {
    unsigned char color[16][3];
} uncursed_palette16;

extern void EI(uncursed_reset_palette16) (void);
extern void EI(uncursed_set_palette16) (const uncursed_palette16 *p);

#define UNCURSED_DEFAULT_COLORS {                                      \
{0x00,0x00,0x00}, {0xaf,0x00,0x00}, {0x00,0x87,0x00}, {0xaf,0x5f,0x00},\
{0x00,0x00,0xaf}, {0x87,0x00,0x87}, {0x00,0xaf,0x87}, {0xaf,0xaf,0xaf},\
{0x5f,0x5f,0x5f}, {0xff,0x5f,0x00}, {0x00,0xff,0x00}, {0xff,0xff,0x00},\
{0x87,0x5f,0xff}, {0xff,0x5f,0xaf}, {0x00,0xd7,0xff}, {0xff,0xff,0xff} \
}

/* manual page 3ncurses color */
extern int EI(COLORS);
extern int EI(COLOR_PAIRS);

/* This must be a signed type; and for compatibility with curses, must be short.
   Keep it synched with COLOR_PAIRS in uncursed.c. */
typedef short uncursed_color;

extern int EI(start_color) (void);
extern int EI(init_pair) (uncursed_color, uncursed_color, uncursed_color);
extern int EI(init_color) (uncursed_color, short, short, short);
extern uncursed_bool EI(has_colors) (void);
extern uncursed_bool EI(can_change_color) (void);
extern int EI(color_content) (uncursed_color, short *, short *, short *);
extern int EI(pair_content) (uncursed_color, uncursed_color *,
                             uncursed_color *);

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

/* manual page 3ncurses attr */
/* ncurses uses "int" for the next three, but that breaks on 16-bit systems */
UNCURSED_ANDWINDOW(int, attroff, attr_t);
UNCURSED_ANDWINDOW(int, attron, attr_t);
UNCURSED_ANDWINDOW(int, attrset, attr_t);
UNCURSED_ANDWINDOW(int, color_set, uncursed_color);
UNCURSED_ANDWINDOWV(int, standend);
UNCURSED_ANDWINDOWV(int, standout);
UNCURSED_ANDWINDOW(int, attr_get, attr_t *, uncursed_color *, void *);
UNCURSED_ANDWINDOW(int, attr_off, attr_t, void *);
UNCURSED_ANDWINDOW(int, attr_on, attr_t, void *);
UNCURSED_ANDWINDOW(int, attr_set, attr_t, void *);
UNCURSED_ANDMVWINDOW(int, chgat, int, attr_t, short, const void *);

/* These constants are designed to be OR'd with a char. As such, they must be
   0 in the bottom 8 bits (we interpret a non-wide 'char' as cp437), and must
   fit into an attr_t (which is at least 32 bits wide, with the top bit being
   iffy due to signedness issues). */
#define A_NORMAL       0
#define A_UNDERLINE    (1<<8)
#define A_REVERSE      (1<<9)   /* translated to an fg/bg swap later on */
#define A_BLINK        0        /* unimplemented */
#define A_DIM          0        /* unimplemented */
#define A_BOLD         (1<<10)  /* converted to color later */
#define A_PROTECT      0        /* unimplemented */
#define A_INVIS        (1<<11)  /* converted to color later */
#define A_ALTCHARSET   0        /* unimplemented */
#define A_STANDOUT     A_UNDERLINE | A_BOLD | A_REVERSE

#define A_CHARTEXT     ((1<<8)-1)
#define COLOR_PAIR(n)  ((attr_t)(n)<<13)        /* converted to color later */
#define PAIR_NUMBER(n) ((attr_t)(n)>>13)        /* inverse of COLOR_PAIR */

/* manual page 3ncurses add_wch */
UNCURSED_ANDMVWINDOW(int, add_wch, const cchar_t *);
UNCURSED_ANDWINDOW(int, echo_wchar, const cchar_t *);

extern int EI(TABSIZE);

typedef const cchar_t *uncursed_cchar_tp;
extern const uncursed_cchar_tp EI(WACS_BLOCK), EI(WACS_BOARD), EI(WACS_BTEE),
EI(WACS_BULLET), EI(WACS_CKBOARD), EI(WACS_DARROW), EI(WACS_DEGREE),
EI(WACS_DIAMOND), EI(WACS_GEQUAL), EI(WACS_HLINE), EI(WACS_LANTERN),
EI(WACS_LARROW), EI(WACS_LEQUAL), EI(WACS_LLCORNER), EI(WACS_LRCORNER),
EI(WACS_LTEE), EI(WACS_NEQUAL), EI(WACS_PI), EI(WACS_PLMINUS), EI(WACS_PLUS),
EI(WACS_RARROW), EI(WACS_RTEE), EI(WACS_S1), EI(WACS_S3), EI(WACS_S7),
EI(WACS_S9), EI(WACS_STERLING), EI(WACS_TTEE), EI(WACS_UARROW),
EI(WACS_ULCORNER), EI(WACS_URCORNER), EI(WACS_VLINE), EI(WACS_T_ULCORNER),
EI(WACS_T_LLCORNER), EI(WACS_T_URCORNER), EI(WACS_T_LRCORNER), EI(WACS_T_LTEE),
EI(WACS_T_RTEE), EI(WACS_T_BTEE), EI(WACS_T_TTEE), EI(WACS_T_HLINE),
EI(WACS_T_VLINE), EI(WACS_T_PLUS), EI(WACS_D_ULCORNER), EI(WACS_D_LLCORNER),
EI(WACS_D_URCORNER), EI(WACS_D_LRCORNER), EI(WACS_D_LTEE), EI(WACS_D_RTEE),
EI(WACS_D_BTEE), EI(WACS_D_TTEE), EI(WACS_D_HLINE), EI(WACS_D_VLINE),
EI(WACS_D_PLUS);

/* manual page 3ncurses add_wchstr */
UNCURSED_ANDMVWINDOW(int, add_wchstr, const cchar_t *);
UNCURSED_ANDMVWINDOW(int, add_wchnstr, const cchar_t *, int n);

/* manual page 3ncurses addch */
UNCURSED_ANDMVWINDOW(int, addch, const chtype);
UNCURSED_ANDWINDOW(int, echochar, const chtype);

#define ACS_BLOCK    0xfe
#define ACS_BOARD    0xd8
#define ACS_BTEE     0xc1
#define ACS_BULLET   0xf9
#define ACS_CKBOARD  0xb1
#define ACS_DARROW   0xdc
#define ACS_DEGREE   0xf8
#define ACS_DIAMOND  0xf9
#define ACS_GEQUAL   0xf2
#define ACS_HLINE    0xc4
#define ACS_LANTERN  0xe9
#define ACS_LARROW   0xdd
#define ACS_LEQUAL   0xf3
#define ACS_LLCORNER 0xc0
#define ACS_LRCORNER 0xd9
#define ACS_LTEE     0xc3
#define ACS_NEQUAL   0x21
#define ACS_PI       0xe3
#define ACS_PLMINUS  0xf1
#define ACS_PLUS     0xc5
#define ACS_RARROW   0xde
#define ACS_RTEE     0xb4
#define ACS_S1       0xc4
#define ACS_S3       0xc4
#define ACS_S7       0xc4
#define ACS_S9       0xc4
#define ACS_STERLING 0x9c
#define ACS_TTEE     0xc2
#define ACS_UARROW   0xdf
#define ACS_ULCORNER 0xda
#define ACS_URCORNER 0xbf
#define ACS_VLINE    0xb3

/* manual page 3ncurses addchstr */
UNCURSED_ANDMVWINDOW(int, addchstr, const chtype *);
UNCURSED_ANDMVWINDOW(int, addchnstr, const chtype *, int);

/* manual page 3ncurses addstr */
UNCURSED_ANDMVWINDOW(int, addstr, const char *);
UNCURSED_ANDMVWINDOW(int, addnstr, const char *, int);

/* manual page 3ncurses addwstr */
UNCURSED_ANDMVWINDOW(int, addwstr, const wchar_t *);
UNCURSED_ANDMVWINDOW(int, addwnstr, const wchar_t *, int);

/* manual page 3ncurses default_colors */
extern int EI(use_default_colors) (void);
extern int EI(assume_default_colors) (int, int);

/* manual page 3ncurses beep */
extern int EI(beep) (void);
extern int EI(flash) (void);

/* manual page 3ncurses border */
UNCURSED_ANDWINDOW(int, border, chtype, chtype, chtype, chtype, chtype, chtype,
                   chtype, chtype);
extern int EI(box) (WINDOW *win, chtype, chtype);
UNCURSED_ANDMVWINDOW(int, hline, chtype, int);
UNCURSED_ANDMVWINDOW(int, vline, chtype, int);

/* manual page 3ncurses border_set */
UNCURSED_ANDWINDOW(int, border_set, const cchar_t *, const cchar_t *,
                   const cchar_t *, const cchar_t *, const cchar_t *,
                   const cchar_t *, const cchar_t *, const cchar_t *);
extern int EI(box_set) (WINDOW *win, const cchar_t *, const cchar_t *);
UNCURSED_ANDMVWINDOW(int, hline_set, const cchar_t *, int);
UNCURSED_ANDMVWINDOW(int, vline_set, const cchar_t *, int);

/* manual page 3ncurses inopts */
/* The semantics here are a little different from ncurses; we never use line-
   at-a-time input, and most of these just change how long we wait for input
   before returning ERR. The exception is raw, whose purpose is to change the
   meaning of control-C from "send SIGINT" to "return 3". */
extern int EI(cbreak) (void);   /* calls noraw, otherwise ignored */
extern int EI(nocbreak) (void); /* cancels halfdelay, otherwise ignored */

/* echo intentionally unimplemented */
extern int EI(noecho) (void);                       /* ignored */
extern int EI(halfdelay) (int);
extern int EI(intrflush) (WINDOW *, uncursed_bool); /* ignored, always false */
extern int EI(keypad) (WINDOW *, uncursed_bool);    /* ignored, always true */
extern int EI(meta) (WINDOW *, uncursed_bool);      /* ignored, always false */
extern int EI(nodelay) (WINDOW *, uncursed_bool);
extern int EI(raw) (void);
extern int EI(noraw) (void);
extern int EI(qiflush) (void);                      /* ignored */
extern int EI(noqiflush) (void);                    /* ignored */

/* Note that notimeout and timeout have nothing to do with each other...
   notimeout mode turns off the use of a timer to distinguish between
   ESC [ A and the up arrow; timeout changes how long reads wait for
   input. */
extern int EI(notimeout) (WINDOW *, uncursed_bool);     /* ignored */
extern void EI(timeout) (int);  /* can't use ANDWINDOW, it returns void */
extern void EI(wtimeout) (WINDOW *, int);
extern int EI(typeahead) (int); /* ignored */

/* manual page 3ncurses overlay */
extern int EI(overlay) (const WINDOW *, const WINDOW *);
extern int EI(overwrite) (const WINDOW *, const WINDOW *);
extern int EI(copywin) (const WINDOW *, const WINDOW *,
                        int, int, int, int, int, int, int);

/* manual page 3ncurses clear */
UNCURSED_ANDWINDOWV(int, erase);
UNCURSED_ANDWINDOWV(int, clear);
UNCURSED_ANDWINDOWV(int, clrtobot);
UNCURSED_ANDWINDOWV(int, clrtoeol);

/* manual page 3ncurses outopts: only clearok implemented */
extern int EI(clearok) (WINDOW *, uncursed_bool);
extern int EI(scrollok) (WINDOW *, uncursed_bool);
extern int EI(nonl) (void);                             /* ignored */
extern int EI(leaveok) (WINDOW *, uncursed_bool);       /* ignored */

/* manual page 3ncurses kernel: only curs_set is implemented */
extern int EI(curs_set) (int visibility);

/* manual page 3ncurses util */
extern uncursed_char_p EI(unctrl) (char);
extern uncursed_wchar_tp EI(wunctrl) (wchar_t);
extern uncursed_char_p EI(keyname) (int);
extern uncursed_char_p EI(key_name) (wint_t);
extern int EI(delay_output) (int);

/* unimplemented: getwin, putwin, use_env, filter, nofilter, flushinp */

/* and something extra of our own */
extern uncursed_char_p EI(friendly_keyname) (int);

#define KEY_SHIFT     2048
#define KEY_ALT       4096
#define KEY_CTRL      8192
#define KEY_NONDEC    (1024 | 256)
#define KEY_KEYPAD    (512 | 256)
#define KEY_FUNCTION  256

/* This is basically a rundown of all the codes supported by various
   terminals... */

/* Main keyboard keys */
#define KEY_HOME      (KEY_FUNCTION | 1)        /* Home */
#define KEY_IC        (KEY_FUNCTION | 2)        /* Insert */
#define KEY_DC        (KEY_FUNCTION | 3)        /* Delete */
#define KEY_END       (KEY_FUNCTION | 4)        /* End */
#define KEY_PPAGE     (KEY_FUNCTION | 5)        /* Page Up */
#define KEY_NPAGE     (KEY_FUNCTION | 6)        /* Page Down */

#define KEY_UP        (KEY_KEYPAD | 'A')
#define KEY_DOWN      (KEY_KEYPAD | 'B')
#define KEY_RIGHT     (KEY_KEYPAD | 'C')
#define KEY_LEFT      (KEY_KEYPAD | 'D')
/* KEY_KEYPAD | 'E', 'F', 'G', 'H' (which correspond to num5, end, num5,
   home) are all translated to other codes for terminal compatibility. */

#define KEY_BREAK     (KEY_NONDEC | 'P')        /* Pause/Break */
#define KEY_BTAB      (KEY_KEYPAD | 'Z')        /* Backtab */

/* Next problem: The VT100 has no function keys F1-F5 (at least, that are
   accessible via a program), but it does have keys PF1-PF4, just above the
   numpad. Some terminals translate the keys just above the numpad to
   PF1-PF4; other terminals translate F1-F4 to PF1-PF4. Our response is to
   add extra keycodes KEY_PF1..KEY_PF4 for these, and reserve KEY_F1 to
   KEY_F4 for the physical F1 to F4 on a keyboard. */
#define KEY_F1        (KEY_NONDEC | 'A')
#define KEY_F2        (KEY_NONDEC | 'B')
#define KEY_F3        (KEY_NONDEC | 'C')
#define KEY_F4        (KEY_NONDEC | 'D')
#define KEY_F5        (KEY_NONDEC | 'E')
/* Note that this is not an arithmetic progression.
   The numbers written here are what actual terminals send. */
#define KEY_F6        (KEY_FUNCTION | 17)
#define KEY_F7        (KEY_FUNCTION | 18)
#define KEY_F8        (KEY_FUNCTION | 19)
#define KEY_F9        (KEY_FUNCTION | 20)
#define KEY_F10       (KEY_FUNCTION | 21)
#define KEY_F11       (KEY_FUNCTION | 23)
#define KEY_F12       (KEY_FUNCTION | 24)
/* Some terminals can simulate F-keys above 12 using modifiers. */
#define KEY_F13       (KEY_FUNCTION | 25)
#define KEY_F14       (KEY_FUNCTION | 26)
#define KEY_F15       (KEY_FUNCTION | 28)
#define KEY_F16       (KEY_FUNCTION | 29)
#define KEY_F17       (KEY_FUNCTION | 31)
#define KEY_F18       (KEY_FUNCTION | 32)
#define KEY_F19       (KEY_FUNCTION | 33)
#define KEY_F20       (KEY_FUNCTION | 34)

#define KEY_LAST_FUNCTION (KEY_FUNCTION | 34)

/* Numeric keypad keys */
#define KEY_ENTER     (KEY_KEYPAD | 'M')        /* Numeric keypad Enter, not
                                                   return */

/* These may well be F1-F4 (say, on xterm) rather than PF1-PF4
   (NumLock, Num/, Num*, Num-). */
#define KEY_PF1       (KEY_KEYPAD | 'P')
#define KEY_PF2       (KEY_KEYPAD | 'Q')
#define KEY_PF3       (KEY_KEYPAD | 'R')
#define KEY_PF4       (KEY_KEYPAD | 'S')

/* There's no KEY_NUMLEFT, etc., in curses normally. There /are/ names
   available for numpad home/end/ins/del/5, though, based on their
   positions on the keyboard. So we generalize those into, say, KEY_A2
   for the numpad 'up'. */
#define KEY_A1        (KEY_KEYPAD | 'w')        /* Num Home */
#define KEY_A2        (KEY_KEYPAD | 'x')        /* Num Up */
#define KEY_A3        (KEY_KEYPAD | 'y')        /* Num PgUp */
#define KEY_B1        (KEY_KEYPAD | 't')        /* Num Left */
#define KEY_B2        (KEY_KEYPAD | 'u')        /* Num 5 */
#define KEY_B3        (KEY_KEYPAD | 'v')        /* Num Right */
#define KEY_C1        (KEY_KEYPAD | 'q')        /* Num End */
#define KEY_C2        (KEY_KEYPAD | 'r')        /* Num Down */
#define KEY_C3        (KEY_KEYPAD | 's')        /* Num PgDn */
#define KEY_D1        (KEY_KEYPAD | 'p')        /* Num Ins */
#define KEY_D3        (KEY_KEYPAD | 'n')        /* Num Del */
#define KEY_NUMPLUS   (KEY_KEYPAD | 'l')        /* Num + */
#define KEY_NUMMINUS  (KEY_KEYPAD | 'm')        /* Num - */
#define KEY_NUMTIMES  (KEY_KEYPAD | 'j')        /* Num * */
#define KEY_NUMDIVIDE (KEY_KEYPAD | 'o')        /* Num / */

/* There isn't a KEY_TAB or KEY_RETURN, probably because terminals send them as
   control-I and control-M. We do have two special keys that are aliases to
   control-something, though... */
#define KEY_BACKSPACE (KEY_KEYPAD | 127)        /* backspace = control-? */
#define KEY_ESCAPE    (KEY_KEYPAD | 27)         /* escape = control-] */

/* Keys that can't be generated via a terminal, but might be available via
   other input methods. */
#define KEY_UNHOVER   (KEY_NONDEC | 1)
#define KEY_RESIZE    (KEY_NONDEC | 2)
/* KEY_SHIFT, KEY_CTRL, KEY_ALT are already defined, and we don't want to
   trigger on them because they're used as modifier keys. */

/* PrtSc is an actual key on a typical keyboard. There doesn't seem to be any
   way to get terminals to send it, though. (Although, control-PrtSc appears to
   be equivalent to control-\.) */
#define KEY_PRINT     (KEY_NONDEC | 3)

/* Returned by uncursed_hook_getkeyorcodepoint if the user presses no keys
   before timeout. (As such, it's always used as KEY_SILENCE + KEY_BIAS.) */
#define KEY_SILENCE   (KEY_NONDEC | 4)

/* What we get if the terminal sends us a malformed key code. */
#define KEY_INVALID   (KEY_NONDEC | 5)

/* We're being asked to exit in a hurry (SIGHUP, SIGTERM). */
#define KEY_HANGUP    (KEY_NONDEC | 6)

/* Input that isn't handled by uncursed, but needs to break it out of get_wch */
#define KEY_SIGNAL    (KEY_NONDEC | 7)
#define KEY_OTHERFD   (KEY_NONDEC | 8)

#define KEY_MAX       (KEY_NONDEC | KEY_CTRL | KEY_ALT | KEY_SHIFT | 255)

/* manual page 3ncurses delch */
UNCURSED_ANDMVWINDOWV(int, delch);

/* manual page 3ncurses deleteln */
UNCURSED_ANDWINDOWV(int, deleteln);
UNCURSED_ANDWINDOWV(int, insertln);
UNCURSED_ANDWINDOW(int, insdelln, int);

/* manual page 3ncurses initscr */
extern uncursed_WINDOW_p EI(initscr) (void);
extern int EI(endwin) (void);
extern uncursed_bool EI(isendwin) (void);
extern int EI(LINES);
extern int EI(COLS);

/* manual page 3ncurses window */
extern uncursed_WINDOW_p EI(newwin) (int, int, int, int);
extern uncursed_WINDOW_p EI(subwin) (WINDOW *, int, int, int, int);
extern uncursed_WINDOW_p EI(derwin) (WINDOW *, int, int, int, int);

/* TODO: dupwin not implemented for now */
extern int EI(delwin) (WINDOW *);
extern int EI(mvwin) (WINDOW *, int, int);
extern int EI(mvderwin) (WINDOW *, int, int);
extern void EI(wsyncup) (WINDOW *);
extern int EI(syncok) (WINDOW *, uncursed_bool);
extern void EI(wcursyncup) (WINDOW *);
extern void EI(wsyncdown) (WINDOW *);

/* manual page 3ncurses refresh */
UNCURSED_ANDWINDOWV(int, refresh);
extern int EI(wnoutrefresh) (WINDOW *win);
extern int EI(doupdate) (void);
extern int EI(redrawwin) (WINDOW *win);
extern int EI(wredrawln) (WINDOW *win, int, int);

/* manual page 3ncurses get_wch */
UNCURSED_ANDMVWINDOW(int, get_wch, wint_t *);
extern int EI(unget_wch) (wchar_t);
/* and some uncursed-specific functions: */
extern int EI(timeout_get_wch) (int, wint_t *);
extern void EI(uncursed_signal_getch) (void);
extern void EI(uncursed_watch_fd) (int);
extern void EI(uncursed_unwatch_fd) (int);

/* uncursed mouse handling works differently from ncurses; the ncurses API is
   badly designed in that it can only wait on mouse actions in one window at a
   time; this API is less general than it could be (e.g. it doesn't handle
   drags, shift-clicks, etc.), but that helps to limit programs to a subset of
   mouse actions that will actually be supported by terminals in practice */
extern void EI(uncursed_enable_mouse) (int);
extern void EI(uncursed_clear_mouse_regions) (void);
UNCURSED_ANDWINDOW(void, set_mouse_event, enum uncursed_mousebutton,
                   wint_t, int);

/* manual page 3ncurses getyx, legacy; these are all macros */
#define getyx(win, yy, xx) \
    do {(yy) = (win)->y; (xx) = (win)->x;} while(0)
#define getcury(win) ((win)->y)
#define getcurx(win) ((win)->x)
/* getparyx unimplemented */
#define getbegyx(win, yy, xx) \
    do {(yy) = (win)->scry; (xx) = (win)->scrx;} while(0)
#define getbegy(win) ((win)->scry)
#define getbegx(win) ((win)->scrx)
#define getmaxyx(win, yy, xx) \
    do {(yy) = (win)->maxy+1; (xx) = (win)->maxx+1;} while(0)
#define getmaxy(win) ((win)->maxy+1)
#define getmaxx(win) ((win)->maxx+1)

/* manual page 3ncurses getcchar */
extern int EI(getcchar) (const cchar_t *, wchar_t *, attr_t *, short *, void *);
extern int EI(setcchar) (cchar_t *, const wchar_t *, attr_t, short, void *);

/* manual page 3ncurses getch */
UNCURSED_ANDMVWINDOWV(int, getch);
extern int EI(ungetch) (int);

/* has_key not implemented, for multiple reasons */

/* manual page 3ncurses move */
UNCURSED_ANDWINDOW(int, move, int, int);

/* manual page 3ncurses touch */
/* These are all no-ops for now, and probably forever unless we turn out to
   have insurmountable performance problems. */
extern int EI(touchwin) (WINDOW *);
extern int EI(touchline) (WINDOW *, int, int);
extern int EI(untouchwin) (WINDOW *);
extern int EI(wtouchln) (WINDOW *, int, int, int);

/* is_linetouched, is_wintouched unimplemented */

/* manual page 3ncurses printw */
/* We can't use the helper macros because this is varargs. */
extern int EI(printw) (const char *, ...);
extern int EI(wprintw) (WINDOW *, const char *, ...);
extern int EI(mvprintw) (int, int, const char *, ...);
extern int EI(mvwprintw) (WINDOW *, int, int, const char *, ...);
extern int EI(vw_printw) (WINDOW *, const char *, va_list);

/* manual page 3ncurses scroll */
extern int EI(scroll) (WINDOW *);
UNCURSED_ANDWINDOW(int, scrl, int);

/* manual page 3ncurses wresize */
extern int EI(wresize) (WINDOW *, int, int);

#undef EI

/* manual pages 3ncurses bkgd, bkgrnd, define_key, extensions, get_wstr, getstr,
   in{,_w}{ch,str}, legacy_coding, pad, scanw, scr_dump, slk, termattrs,
   termcap, terminfo: left unimplemented */
/* manual page 3ncurses mouse implemented differently (and incompatibly) */
/* manual pages 3ncurses ins*, resizeterm not implemented for now: TODO */
