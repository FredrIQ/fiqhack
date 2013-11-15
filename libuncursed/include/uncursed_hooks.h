/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-13 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */

#include <stddef.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

enum uncursed_hook_type {
    uncursed_hook_type_input,       /* sends input, receives output */
    uncursed_hook_type_broadcast,   /* receives output and input */
    uncursed_hook_type_recording,   /* like broadcast, but only on request */
};

struct uncursed_hooks;
struct uncursed_hooks {
/***** Calls from uncursed.c into an implementation library *****/
/*** Output: called on all hooks (should be ignored by recording hooks when
     not recording) */

    /* Note: init can be called multiple times, with exit in between; also
       init is allowed to exit the program in case of disaster */
    void (*init) (int *, int *, char *);    /* rows, columns, title */
    void (*exit) (void);

    void (*beep) (void);

    void (*setcursorsize) (int);            /* 0 to 2 */
    void (*positioncursor) (int, int);      /* y, x */

    /* The way updates work is that uncursed.c calls update on some changed
       character, then the implementation library requests state using the
       uncursed_rhook_..._at functions in order to update at least that (and
       possibly other) characters, then calls uncursed_rhook_updated on each
       character it actually updated. (So for instance, if it decides to
       redraw the entire screen, the information it needs is available.) */
    void (*update) (int, int);      /* y, x to update */
    void (*fullredraw) (void);      /* redraw from scratch */
    void (*flush) (void);   /* called at the end of updates */

    /* Only graphical interfaces care about these; the others can ignore
       them via setting them to NULL */
    void (*set_faketerm_font_file) (char *);
    void (*set_tiles_tile_file) (char *, int, int);
    void *(*allocate_tiles_region) (int, int, int, int, int, int);
    void (*deallocate_tiles_region) (void *);
    void (*draw_tile_at) (int, void *, int, int);

/*** Receiving input: called on input hooks only ***/
    void (*delay) (int);            /* milliseconds */
    void (*rawsignals) (int);       /* 0 or 1 */
#define KEY_BIAS (0x10ff00) /* add this to a key to show it isn't a codepoint */
    int (*getkeyorcodepoint) (int); /* timeout in ms */
    void (*signal_getch) (void);
    void (*watch_fd) (int, int);    /* fd, then 1 to watch or 0 to unwatch */

/*** Input notification: called on broadcast and recording hooks (should
     be ignored by recording hooks when not recording) ***/
    void (*recordkeyorcodepoint) (int);
    void (*resized) (int, int);

/*** Recording notification: called on recording hooks ***/
    void (*startrecording) (char *);        /* filename, no extension */
    void (*stoprecording) (void);

/***** Information about the hook *****/
    struct uncursed_hooks *next_hook;
    enum uncursed_hook_type hook_type;
    const char *hook_name;
    int hook_priority;
    int used;
};

#ifdef UNCURSED_MAIN_PROGRAM
# define HOOK_EI(x) AIMAKE_EXPORT(x)
#else
# define HOOK_EI(x) AIMAKE_IMPORT(x)
#endif

typedef struct uncursed_hooks *uncursed_hooks_p;
typedef char *uncursed_hook_char_p;
typedef void *uncursed_hook_void_p;

/***** Calls from implementation plugins into uncursed.c *****/
extern uncursed_hooks_p HOOK_EI(uncursed_hook_list);

extern char HOOK_EI(uncursed_rhook_cp437_at) (int, int);    /* y, x */
extern uncursed_hook_char_p HOOK_EI(uncursed_rhook_utf8_at) (int, int); /* y, x */
extern unsigned short HOOK_EI(uncursed_rhook_ucs2_at) (int, int); /* y, x */
/* This returns fg|bg<<5|ul<<10; i.e. color and underlining information.
   Default color is given as 16, not as -1. */
extern int HOOK_EI(uncursed_rhook_color_at) (int, int);     /* y, x */
extern uncursed_hook_void_p HOOK_EI(uncursed_rhook_region_at) (int, int); /* y, x */
extern int HOOK_EI(uncursed_rhook_needsupdate) (int, int);  /* y, x */

extern void HOOK_EI(uncursed_rhook_updated) (int, int);     /* y, x */
extern void HOOK_EI(uncursed_rhook_setsize) (int, int);     /* rows, columns */

/***** Calls from uncursed.c into plugins.c *****/
extern int uncursed_load_plugin(const char *);  /* plugin name; 1 on success */
extern void uncursed_load_plugin_or_error(const char *);    /* fatal version */
extern void uncursed_load_default_plugin_or_error(void);

#undef HOOK_EI

#ifdef __cplusplus
}
#endif
