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

/* Plugin wrapper for the terminal backend to the uncursed rendering library.
   This notifies uncursed that the tty library is available, merely by being
   loaded. (Thus, it has to be written in C++; in portable C, it's impossible
   for there to be side effects merely upon loading a file. An alternative would
   be to use the GCC-specific __attribute__((constructor)) syntax; but using C++
   and simply creating an actual constructor is just as clear, and more
   portable. Another alternative would be to use common symbols, but that only
   works on UNIXen and would also require editing uncursed.c to add a new common
   symbol whenever a new backend were added. It also does not work correctly
   with shared libraries.) */

/* tty.cxx is always linked statically. */
#define UNCURSED_MAIN_PROGRAM

#include "uncursed_hooks.h"
#include "uncursed_tty.h"

static struct uncursed_hooks tty_uncursed_hooks = {
    tty_hook_init,
    tty_hook_exit,
    tty_hook_beep,
    tty_hook_setcursorsize,
    tty_hook_positioncursor,
    tty_hook_resetpalette16,
    tty_hook_setpalette16,
    tty_hook_update,
    tty_hook_fullredraw,
    tty_hook_flush,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    tty_hook_delay,
    tty_hook_rawsignals,
    tty_hook_activatemouse,
    tty_hook_getkeyorcodepoint,
    tty_hook_signal_getch,
    tty_hook_watch_fd,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    uncursed_hook_type_input,
    "tty",
    80,
    0
};

class tty_uncursed_hook_import {
public:
    tty_uncursed_hook_import() {
        tty_uncursed_hooks.next_hook = uncursed_hook_list;
        uncursed_hook_list = &tty_uncursed_hooks;
    }
};

static tty_uncursed_hook_import importer;
