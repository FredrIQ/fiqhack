/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-24 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */

#ifdef __cplusplus
extern "C" {
#endif

    extern void wincon_hook_init(int *, int *, const char *);
    extern void wincon_hook_exit(void);
    extern void wincon_hook_beep(void);
    extern void wincon_hook_setcursorsize(int);
    extern void wincon_hook_positioncursor(int, int);
    extern void wincon_hook_update(int, int);
    extern void wincon_hook_fullredraw(void);
    extern void wincon_hook_flush(void);
    extern void wincon_hook_delay(int);
    extern void wincon_hook_rawsignals(int);
    extern void wincon_hook_activatemouse(int);
    extern int wincon_hook_getkeyorcodepoint(int);
    extern void wincon_hook_signal_getch(void);
    extern void wincon_hook_watch_fd(int, int);

#ifdef __cplusplus
}
#endif
