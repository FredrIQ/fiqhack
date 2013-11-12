/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-12 */
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

    extern void sdl_hook_init(int *, int *, char *);
    extern void sdl_hook_exit(void);
    extern void sdl_hook_beep(void);
    extern void sdl_hook_setcursorsize(int);
    extern void sdl_hook_positioncursor(int, int);
    extern void sdl_hook_update(int, int);
    extern void sdl_hook_fullredraw(void);
    extern void sdl_hook_flush(void);
    extern void sdl_hook_set_faketerm_font_file(char *);
    extern void sdl_hook_set_tiles_tile_file(char *, int, int);
    extern void *sdl_hook_allocate_tiles_region(int, int, int, int, int, int);
    extern void sdl_hook_deallocate_tiles_region(void *);
    extern void sdl_hook_draw_tile_at(int, void *, int, int);
    extern void sdl_hook_delay(int);
    extern void sdl_hook_rawsignals(int);
    extern int sdl_hook_getkeyorcodepoint(int);

#ifdef __cplusplus
}
#endif
