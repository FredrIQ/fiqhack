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

/* Plugin wrapper for the Windows console backend to the uncursed rendering
   library. See tty.cxx for details; this is pretty much an exact copy with
   s/tty/wincon/g, and the priority reduced (although this depends only on
   libuncursed and libraries that ship with Windows, it is also very slow
   compared to most other backends). */

/* wincon.cxx is always linked statically. */
#define UNCURSED_MAIN_PROGRAM

#include "uncursed_hooks.h"
#include "uncursed_wincon.h"

static struct uncursed_hooks wincon_uncursed_hooks = {
    wincon_hook_init,
    wincon_hook_exit,
    wincon_hook_beep,
    wincon_hook_setcursorsize,
    wincon_hook_positioncursor,
    NULL,
    NULL,
    wincon_hook_update,
    wincon_hook_fullredraw,
    wincon_hook_flush,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    wincon_hook_delay,
    wincon_hook_rawsignals,
    wincon_hook_activatemouse,
    wincon_hook_getkeyorcodepoint,
    wincon_hook_signal_getch,
    wincon_hook_watch_fd,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    uncursed_hook_type_input,
    "wincon",
    30,
    0
};

class wincon_uncursed_hook_import {
public:
    wincon_uncursed_hook_import() {
        wincon_uncursed_hooks.next_hook = uncursed_hook_list;
        uncursed_hook_list = &wincon_uncursed_hooks;
    }
};

static wincon_uncursed_hook_import importer;
