/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
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

/* Calls from uncursed.c into an implementation library */
extern void uncursed_hook_beep(void);
extern void uncursed_hook_delay(int); /* milliseconds */

#define KEY_BIAS (0x10ff00) /* add this to a key to show it isn't a codepoint */
extern void uncursed_hook_rawsignals(int);          /* 0 or 1 */
extern wint_t uncursed_hook_getkeyorcodepoint(int); /* timeout in ms */

extern void uncursed_hook_setcursorsize(int);       /* 0 to 2 */
extern void uncursed_hook_positioncursor(int, int); /* y, x */

/* Note: _init can be called multiple times, with _exit in between; also
   _init is allowed to exit the program in case of disaster */
extern void uncursed_hook_init(int *, int *); /* rows, columns */
extern void uncursed_hook_exit(void);

/* The way updates work is that uncursed.c calls uncursed_hook_update on some
   changed character, then the implementation library requests state using
   the uncursed_rhook_..._at functions in order to update at least that (and
   possibly other) characters, then calls uncursed_rhook_updated on each
   character it actually updated. (So for instance, if it decides to redraw
   the entire screen, the information it needs is available.) */
extern void uncursed_hook_update(int, int);  /* y, x to update */
extern void uncursed_hook_fullredraw(void);  /* redraw from scratch */
extern void uncursed_hook_flush(void);       /* called at the end of updates */

/* Calls from implementation libraries into uncursed.c */
extern char uncursed_rhook_cp437_at(int, int); /* y, x */
extern char *uncursed_rhook_utf8_at(int, int); /* y, x; static buffer */
/* This returns fg|bg<<5|ul<<10; i.e. color and underlining information.
   Default color is given as 16, not as -1. */
extern int uncursed_rhook_color_at(int, int);  /* y, x */
extern int uncursed_rhook_needsupdate(int, int); /* y, x */

extern void uncursed_rhook_updated(int, int);  /* y, x */
extern void uncursed_rhook_setsize(int, int);  /* rows, columns */
