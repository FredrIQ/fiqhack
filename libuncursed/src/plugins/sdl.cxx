/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c++;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-12-29 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */

/* Plugin wrapper for the SDL 2 backend to the uncursed rendering library.
   Based on tty.cxx. */

/* Don't define UNCURSED_MAIN_PROGRAM; this goes into a separate library. */
#include "uncursed_hooks.h"
#include "uncursed_sdl.h"

static struct uncursed_hooks sdl_uncursed_hooks = {
    sdl_hook_init,
    sdl_hook_exit,
    sdl_hook_beep,
    sdl_hook_setcursorsize,
    sdl_hook_positioncursor,
    sdl_hook_resetpalette16,
    sdl_hook_setpalette16,
    sdl_hook_update,
    sdl_hook_fullredraw,
    sdl_hook_flush,
    sdl_hook_set_faketerm_font_file,
    sdl_hook_set_tiles_tile_file,
    sdl_hook_get_tile_dimensions,
    sdl_hook_allocate_tiles_region,
    sdl_hook_deallocate_tiles_region,
    sdl_hook_draw_tile_at,
    sdl_hook_delay,
    sdl_hook_rawsignals,
    sdl_hook_activatemouse,
    sdl_hook_getkeyorcodepoint,
    sdl_hook_signal_getch,
    sdl_hook_watch_fd,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    uncursed_hook_type_input,
    "sdl",
    70,
    0
};

class sdl_uncursed_hook_import {
public:
    sdl_uncursed_hook_import() {
        sdl_uncursed_hooks.next_hook = uncursed_hook_list;
        uncursed_hook_list = &sdl_uncursed_hooks;
    }
};

static sdl_uncursed_hook_import importer;
