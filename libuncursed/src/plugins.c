/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-05-22 */
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

#define UNCURSED_MAIN_PROGRAM
#include "uncursed_hooks.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef AIMAKE_BUILDOS_MSWin32
# include <dlfcn.h>
#else
# include <windows.h>
#endif

#if defined(AIMAKE_BUILDOS_linux) || defined(AIMAKE_BUILDOS_freebsd)
# define DLLEXT ".so"
#else
# ifdef AIMAKE_BUILDOS_MSWin32
#  define DLLEXT ".dll"
# else
#  ifdef AIMAKE_BUILDOS_darwin
#   define DLLEXT ".dylib"
#  else
#   error plugins.c requires dynamic library loading code for your OS.
#  endif
# endif
#endif


#define MAX_PLUGINS 10

static int plugins_dynamically_loaded = 0;
static void *plugin_handles[MAX_PLUGINS];

static void
unload_handle(void *handle)
{
#if defined(AIMAKE_BUILDOS_linux) || defined(AIMAKE_BUILDOS_darwin) || \
    defined(AIMAKE_BUILDOS_freebsd)
    dlclose(handle);
#else
# ifdef AIMAKE_BUILDOS_MSWin32
    FreeLibrary(*(HMODULE *) handle);
    free(handle);
# else
#  error plugins.c requires dynamic library loading code for your OS.
# endif
#endif
}

static void
unload_dynamic_plugins(void)
{
    while (plugins_dynamically_loaded--)
        unload_handle(plugin_handles[plugins_dynamically_loaded]);
    plugins_dynamically_loaded = 0;
}

/* Loads a plugin by name, and returns 1 on success, 0 on failure.  Note that
   loading a plugin is not the same thing as initializing it; it simply
   dynamically links the plugin into the executable (if necessary), then updates
   uncursed_hook_list (adding the plugin's hookset to the list if it isn't
   already there, and setting its "used" field to 1). */
int
uncursed_load_plugin(const char *plugin_name)
{
    /* The easy case: is the plugin linked into the executable already? */
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (strcmp(plugin_name, h->hook_name) == 0) {
            h->used = 1;
            return 1;
        }

    /* The difficult case: is the plugin available in a shared library
       somewhere? */
    if (plugins_dynamically_loaded == MAX_PLUGINS)
        return 0;

    void *handle = NULL;

    char fname[strlen("libuncursed_") + strlen(plugin_name) +
               strlen(DLLEXT) + 1];

    strcpy(fname, "libuncursed_");
    strcat(fname, plugin_name);
    strcat(fname, DLLEXT);

#if defined(AIMAKE_BUILDOS_linux) || defined(AIMAKE_BUILDOS_darwin) || \
    defined(AIMAKE_BUILDOS_freebsd)

    /* dlopen() uses much the same search path rules as ld.so, so this will
       look for the plugins in the rpath, which includes the libdir where they
       should have been installed. */
    handle = dlopen(fname, RTLD_NOW);
    if (!handle)
        fprintf(stderr, "Warning: could not load %s: %s\n", fname, dlerror());

#else
# ifdef AIMAKE_BUILDOS_MSWin32

    handle = malloc(sizeof (HMODULE));
    if (handle) {
        *(HMODULE *) handle = LoadLibraryA(fname);
        if (!*(HMODULE *) handle) {
            free(handle);
            handle = 0;
        }
    }

# else
#  error plugins.c requires dynamic library loading code for your OS.
# endif
#endif

    if (!handle)
        return 0;

    plugin_handles[plugins_dynamically_loaded++] = handle;

    /* Now check again to see if it's loaded. */
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (strcmp(plugin_name, h->hook_name) == 0) {
            h->used = 1;

            if (plugins_dynamically_loaded == 1)
                atexit(unload_dynamic_plugins);

            return 1;
        }

    /* If it had the wrong name, unload it again. */
    plugins_dynamically_loaded--;
    unload_handle(handle);

    return 0;
}

void
uncursed_load_plugin_or_error(const char *plugin_name)
{
    if (uncursed_load_plugin(plugin_name))
        return;

    fprintf(stderr,
            "Error initializing rendering library: "
            "could not find plugin '%s'\n", plugin_name);

    /* Leaving a prompt on the screen is necessary, because we might have been
       called from a GUI with no console; normally the plugins handle
       abstracting that sort of thing, but when the issue is a failure to load
       plugins... */
    fprintf(stderr, "Press <return> to end.\n");
    getchar();
    exit(5);
}

void
uncursed_load_default_plugin_or_error(void)
{
#ifdef AIMAKE_BUILDOS_MSWin32
    uncursed_load_plugin_or_error("wincon");
#else
    uncursed_load_plugin_or_error("tty");
#endif
}
