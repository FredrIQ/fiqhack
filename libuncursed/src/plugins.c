/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This file contains the plugin loading for the uncursed rendering library.
   It was split into a different file because the plugins have to be loaded in a
   platform-specific maner.
*/

#include "uncursed_hooks.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Loads a plugin by name, and returns 1 on success, 0 on failure.  Note that
   loading a plugin is not the same thing as initializing it; it simply
   dynamically links the plugin into the executable (if necessary), then updates
   uncursed_hook_list (adding the plugin's hookset to the list if it isn't
   already there, and setting its "used" field to 1). */
int uncursed_load_plugin(const char *plugin_name) {
    /* The easy case: is the plugin linked into the executable already? */
    struct uncursed_hooks *h;
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (strcmp(plugin_name, h->hook_name) == 0) {
            h->used = 1;
            return 1;
        }
    /* The difficult case: is the plugin available in a shared library
       somewhere? */
    return 0; /* TODO */
}

void uncursed_load_plugin_or_error(const char *plugin_name) {
    if (uncursed_load_plugin(plugin_name)) return;
    fprintf(stderr, "Error initializing rendering library: "
            "could not find plugin '%s'\n", plugin_name);
    /* Leaving a prompt on the screen is necessary, because we might have been
       called from a GUI with no console; normally the plugins handle
       abstracting that sort of thing, but when the issue is a failure to load
       plugins... */
    fprintf(stderr, "Press <return> to end.\n");
    getchar();
    exit(5);
}

void uncursed_load_default_plugin_or_error(void) {
#ifdef AIMAKE_BUILDOS_MSWin32
    uncursed_load_plugin_or_error("wincon");
#else
    uncursed_load_plugin_or_error("tty");
#endif
}
