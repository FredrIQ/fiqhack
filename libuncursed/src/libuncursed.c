/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-25 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This library aims for source compatibility with the 'ncurses' library,
   containing many the same function calls, variables, etc. (some calls are left
   unimplemented, either due to being terminal-specific, due to being rarely
   used ncurses extensions, or because they would have added considerable
   complexity for little gain).  It does not aim to produce the same output;
   ncurses aims to adapt output appropriately for the terminal the user is
   using, whereas uncursed has multiple output backends, with the terminal
   backend aiming for a lowest common denominator output, rather than an output
   customized to any specific terminal.  As such, some of the methods in ncurses
   have trivial or no-op implementations.  uncursed also provides a few methods
   of its own.

   Note that there should be no platform-specific code at all in this file.
   That goes in the other files, e.g. tty.c.
*/

#define _ISOC99_SOURCE
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>      /* for vsnprintf, this file does no I/O */
#include <stdbool.h>

#define UNCURSED_MAIN_PROGRAM
#include "uncursed.h"
#include "uncursed_hooks.h"

/* ABI version: upon release, change the first number whenever the ABI or API
   changes in a backwards-incompatible way, the second number whenever the
   functionality changes in a backwards-compatible way, and the third number
   when there is no functionality change (e.g. bugfixes), resetting all numbers
   beyond the number you change to 0.

   There's no need to change the numbers in alpha releases; leave it to actual
   releases, or they'll get way too big way too fast. */
AIMAKE_ABI_VERSION(1.0.2)

#define min(a,b) ((a) < (b) ? (a) : (b))

static WINDOW *nout_win = 0;        /* Window drawn onto by wnoutrefresh */
static WINDOW *disp_win = 0;        /* Window drawn onto by doupdate */

/* uncursed hook handling */
struct uncursed_hooks *uncursed_hook_list = NULL;
static int uncursed_hooks_inited = 0;

/* an intermediate return value; should never escape this file */
#define SCREND 3
static int add_wch_core(WINDOW *win, const cchar_t *ch);

void
initialize_uncursed(int *p_argc, char **argv)
{
    if (uncursed_hooks_inited)
        return;

    /*
     * We need to change the uncursed_hook_list to contain exactly one input set
     * of hooks, and any number of broadcast and recording hooks. (A set of
     * hooks is provided by an uncursed plugin.)
     *
     * When initialize_uncursed is called, we may have a number of hooks in the
     * list already: specifically, any hooks that were statically linked into
     * the program, and any hooks in dynamically linked libraries that are
     * specified as dependencies of the program. We can also add additional
     * hooks via loading dynamically linked libraries in a platform-specific way
     * (e.g. dlopen on POSIXy OSes). Merely loading the libraries will add
     * entries to uncursed_hook_list (which is a reverse export). This mechanism
     * allows the set of available plugins to be changed without changing the
     * executable; thus, an executable and a set of plugins can be in separate
     * packages in a package manager. (The platform-specific plugin loading code
     * is in plugins.c.)
     *
     * We first check the argument list for requested hooks:
     *
     * - If there is an explicit --interface option, we load any plugins
     *   specified there (if they aren't alreay loaded), and error out if we
     *   can't, or if more than one input plugin is specified.
     *
     * - If there is no explicit --interface option, or if it didn't specify an
     *   input plugin, we check the executable's basename; if it ends -foo for
     *   any foo, we attempt to load a plugin named foo, and error out if we
     *   can't. (This leads to weirdness if a non-input plugin is specified as
     *   part of the filename, but we can't do much sensible with that anyway.)
     *
     * - If there still isn't an input plugin, we check the list of hooks for an
     *   input plugin that came prelinked; if we find any, we include the plugin
     *   with the highest priority.
     *
     * - If even that fails to find an input plugin (most likely because there
     *   are no plugins linked into the executable and the user didn't specify
     *   one), we attempt to load the "tty" or "wincon" plugin, depending on
     *   platform, and error out if we can't find it.
     *
     * When we decide to use a set of hooks, we record the fact using the "used"
     * field of the hookset, which always initializes at 0.
     */
    char **p = argv;

    if (*p)
        p++;    /* don't read the filename */

    while (*p) {
        char *plugin_name = NULL;
        char **q;

        /* Look for --interface=tty or --interface tty. If we find it, load the
           specified plugin, then remove the options from argc/argv. */
        if (strncmp(*p, "--interface=", 12) == 0) {
            plugin_name = *p + 12;

            for (q = p; *q; q++)
                q[0] = q[1];

            (*p_argc)--;
        }

        if (p[1] && strcmp(*p, "--interface") == 0) {
            plugin_name = p[1];

            /* Setting q[1] here avoids reading beyond the end of argv. */
            for (q = p; *q; q++)
                q[0] = q[1] = q[2];

            *p_argc -= 2;
        }

        if (plugin_name)
            uncursed_load_plugin_or_error(plugin_name);
        else
            p++;
    }

    int input_plugins = 0;
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            input_plugins++;

    if (input_plugins > 1) {
        fprintf(stderr,
                "Error initializing rendering library: "
                "more than one input plugin specified\n");
        exit(5);
    }

    if (!input_plugins && *argv) {
        char *argv_hyphen = strrchr(*argv, '-');

        if (argv_hyphen && !strchr(argv_hyphen, '/') &&
            !strchr(argv_hyphen, '\\')) {

            char *argv_plugin = malloc(strlen(argv_hyphen));
            strcpy(argv_plugin, argv_hyphen + 1);

            if (strchr(argv_plugin, '.'))
                *(strchr(argv_plugin, '.')) = '\0';
            uncursed_load_plugin_or_error(argv_plugin);
            free(argv_plugin);
        }
    }

    input_plugins = 0;
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            input_plugins++;

    if (!input_plugins) {
        const char *highest_prio_plugin = NULL;
        int highest_prio = -1;

        for (h = uncursed_hook_list; h; h = h->next_hook)
            if (h->hook_type == uncursed_hook_type_input &&
                h->hook_priority > highest_prio) {
                highest_prio_plugin = h->hook_name;
                highest_prio = h->hook_priority;
            }

        if (highest_prio_plugin) {
            uncursed_load_plugin_or_error(highest_prio_plugin);
            input_plugins++;
        } else {
            uncursed_load_default_plugin_or_error();
            input_plugins++;
        }
    }

    uncursed_hooks_inited = 1;
}

static void
uncursed_hook_init(int *r, int *c, char *title)
{
    struct uncursed_hooks *h;

    /* We call this on the input plugin first, so that other plugins can see
       the returned values for the window size. */
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->init(r, c, title);
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type != uncursed_hook_type_input)
            h->init(r, c, title);
}

static void
uncursed_hook_exit(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->exit();
}

static void
uncursed_hook_beep(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->beep();
}

static void
uncursed_hook_setcursorsize(int size)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->setcursorsize(size);
}

static void
uncursed_hook_positioncursor(int y, int x)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->positioncursor(y, x);
}

static void
uncursed_hook_update(int y, int x)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->update(y, x);
}

static void
uncursed_hook_fullredraw(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->fullredraw();
}

static void
uncursed_hook_flush(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            h->flush();
}

static void
uncursed_hook_set_faketerm_font_file(const char *filename)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->set_faketerm_font_file)
            h->set_faketerm_font_file(filename);
}

/* Setting more than one hook to tiles mode at once would be a problem; we'd
   have multiple sets of tile regions floating around and nowhere to store
   them. A 100% solution to this would require some sort of associative array;
   we can make it work in all common configurations by only setting one of the
   hooks (arbitrarily, the first one in the list) to tiles mode.

   If no graphical interfaces are in use, we allocate and free dummy tiles
   regions ourself. */
static void
uncursed_hook_set_tiles_tile_file(const char *filename, int height, int width)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->set_tiles_tile_file) {
            h->set_tiles_tile_file(filename, height, width);
            break;
        }
}

static void
uncursed_hook_get_tile_dimensions(int down, int across, int *height, int *width)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->get_tile_dimensions) {
            h->get_tile_dimensions(down, across, height, width);
            return;
        }

    *height = down;
    *width = across;
}

static void *
uncursed_hook_allocate_tiles_region(int tiles_h, int tiles_w, int char_h,
                                    int char_w, int char_t, int char_l)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->allocate_tiles_region)
            return h->allocate_tiles_region(
                tiles_h, tiles_w, char_h, char_w, char_t, char_l);
    return malloc(1);   /* create a dummy region */
}

static void
uncursed_hook_deallocate_tiles_region(void *region)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->deallocate_tiles_region) {
            h->deallocate_tiles_region(region);
            return;
        }
    free(region);       /* free the dummy region */
}

static void
uncursed_hook_draw_tile_at(int tile, void *region, int y, int x)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->draw_tile_at) {
            h->draw_tile_at(tile, region, y, x);
            return;
        }
}

static void
uncursed_hook_delay(int ms)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->delay(ms);
}

static void
uncursed_hook_rawsignals(int raw)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->rawsignals(raw);
}

static void
uncursed_hook_activatemouse(int active)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->activatemouse(active);
}

static int
uncursed_hook_getkeyorcodepoint(int ms)
{
    struct uncursed_hooks *h;

    /* We initialize kc to a hangup so that we have vaguely sensible behaviour
       in the case that uncursed is incorrectly initialized and there are no
       input hooks. Not that that should be happening anyway. */
    int kc = KEY_HANGUP + KEY_BIAS;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            kc = h->getkeyorcodepoint(ms);
    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type != uncursed_hook_type_input)
            h->recordkeyorcodepoint(kc);
    return kc;
}

static void
uncursed_hook_signal_getch(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->signal_getch();
}

static void
uncursed_hook_watch_fd(int fd, int watch)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_input)
            h->watch_fd(fd, watch);
}

#ifdef __GNUC__
__attribute__ ((unused))
#endif
static void
uncursed_hook_startrecording(char *fn)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->hook_type == uncursed_hook_type_recording)
            h->startrecording(fn);
}

#ifdef __GNUC__
__attribute__ ((unused))
#endif
static void
uncursed_hook_stoprecording(void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used && h->hook_type == uncursed_hook_type_recording)
            h->stoprecording();
}

/* control of graphical interfaces */
void
set_faketerm_font_file(const char *filename)
{
    uncursed_hook_set_faketerm_font_file(filename);
}

static char invalid_region;     /* &invalid_region is used as a sentinel */
void
set_tiles_tile_file(const char *filename, int height, int width)
{
    uncursed_hook_set_tiles_tile_file(filename, height, width);
}

void
get_tile_dimensions(int down, int across, int *height, int *width)
{
    uncursed_hook_get_tile_dimensions(down, across, height, width);
}

UNCURSED_ANDWINDOWDEF(int,
set_tiles_region,     (int tiles_h, int tiles_w, int tiles_t, int tiles_l,
                       int char_h, int char_w, int char_t, int char_l),
                      (tiles_h, tiles_w, tiles_t, tiles_l, char_h, char_w,
                       char_t, char_l))
{
    if (tiles_h + tiles_t > win->maxy + 1 ||
        tiles_w + tiles_l > win->maxx + 1 ||
        char_h + char_t > win->maxy + 1 ||
        char_w + char_l > win->maxx + 1)
        return ERR;
    if (tiles_h < 1 || tiles_w < 1 || char_h < 1 || char_w < 1 ||
        tiles_t < 0 || tiles_l < 0 || char_t < 0 || char_l < 0)
        return ERR;

    wdelete_tiles_region(win);

    void *region = uncursed_hook_allocate_tiles_region(
        char_h, char_w, tiles_h, tiles_w,
        tiles_t + win->scry, tiles_l + win->scrx);
    if (!region)
        return ERR;

    win->regy = char_t;
    win->regx = char_l;
    win->region = region;

    int j, i;

    for (j = tiles_t; j < tiles_t + tiles_h; j++)
        for (i = tiles_l; i < tiles_l + tiles_w; i++)
            win->regionarray[j * (win->maxx + 1) + i] = region;

    return OK;
}

UNCURSED_ANDWINDOWVDEF(int,
delete_tiles_region)
{
    if (!win->region)
        return OK;

    /* We need to clear the tiles region from all existing structures before
       deallocating it. However, because the only way to move tiles regions
       around is via copywin, and that's banned unless the target is disp_win
       or nout_win, we only have to check those two windows and the target
       itself. We set existing uses in most places to NULL, and existing uses
       in disp_win to the sentinel &invalid_region (that is, we want to draw
       non-tiles at those locations, and currently are drawing garbage). */
    int i;

    for (i = 0; i < (disp_win->maxx + 1) * (disp_win->maxy + 1); i++)
        if (disp_win->regionarray[i] == win->region)
            disp_win->regionarray[i] = &invalid_region;
    for (i = 0; i < (nout_win->maxx + 1) * (nout_win->maxy + 1); i++)
        if (nout_win->regionarray[i] == win->region)
            nout_win->regionarray[i] = NULL;
    for (i = 0; i < (win->maxx + 1) * (win->maxy + 1); i++)
        win->regionarray[i] = NULL;

    uncursed_hook_deallocate_tiles_region(win->region);
    win->region = NULL;

    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
set_tiles_tile, (int tile), (tile))
{
    /* Check to make sure that the window has a tiles region. We don't check
       against regionarray, because that measures the place where the tiles are
       drawn, not the characters they correspond to. (Instead, we let the
       interfaces do the bounds checks on the region.) */
    if (!win->region)
        return ERR;

    uncursed_hook_draw_tile_at(tile, win->region, win->y - win->regy,
                               win->x - win->regx);

    return OK;
}

/* manual page 3ncurses color */
/* Color pairs are kind-of pointless for rendering purposes on modern terminals,
   but are used in the source. They do have a kind-of use for "palette change"
   like activities where the source sets color pairs, and then recolors the
   screen via changing the color pairs. As such, we record color pairs in the
   window content, and change them to colors at the last possible moment. */
int COLORS = 16;                /* externally visible */
int COLOR_PAIRS = 32767;        /* externally visible; must fit into 15 bits */

static uncursed_color (*pair_content_list)[2] = 0;   /* dynamically allocated */
static uncursed_color pair_content_alloc_count = -1;
static bool need_noutwin_recolor = true;

int
start_color(void)
{
    return OK;
}       /* no-op */

/* We use an internal color format for communicating with plugins; we use 5 bits
   for foreground, 5 for background, and 1 for underline (which is treated like
   a color). */
static unsigned short
color_on_screen_for_attr(attr_t a)
{
    int p = PAIR_NUMBER(a);
    uncursed_color f, b;

    pair_content(p, &f, &b);

    /* Many attributes are simulated with color. */

    if (a & A_REVERSE) { int t = f; f = b; b = t; }

    /* For portability, we have bright implies bold, bold implies bright. The
       implementation libraries know this, so we just send the brightness. */
    if (a & A_BOLD)
        f |= 8;

    if (a & A_INVIS)
        f = b;

    if (f == -1)
        f = 16;
    if (b == -1)
        b = 16;

    return f | (b << 5) | (!!(a & A_UNDERLINE) << 10);
}

#define DEFAULT_FOREGROUND COLOR_WHITE
#define DEFAULT_BACKGROUND COLOR_BLACK
int
init_pair(uncursed_color pairnum,
          uncursed_color fgcolor, uncursed_color bgcolor)
{
    if (pairnum <= 0)
        return ERR;

    if (pairnum > pair_content_alloc_count) {
        int default_f, default_b;

        pair_content_list =
            realloc(pair_content_list,
                    (pairnum + 1) * sizeof (*pair_content_list));
        if (!pair_content_list) {
            pair_content_alloc_count = -1;
            return ERR;
        }

        if (pair_content_alloc_count < 0) {
            default_f = DEFAULT_FOREGROUND;
            default_b = DEFAULT_BACKGROUND;
        } else {
            default_f = pair_content_list[0][0];
            default_b = pair_content_list[0][1];
        }

        while (pair_content_alloc_count < pairnum) {
            if (pair_content_alloc_count >= 0) {
                pair_content_list[pair_content_alloc_count][0] = default_f;
                pair_content_list[pair_content_alloc_count][1] = default_b;
            }
            pair_content_alloc_count++;
        }
    }

    pair_content_list[pairnum][0] = fgcolor;
    pair_content_list[pairnum][1] = bgcolor;

    need_noutwin_recolor = true;

    return OK;
}

uncursed_bool
has_colors(void)
{
    return 1;
}

/* We could actually implement this vaguely portably, although it would involve
   refreshing the screen (both to update the colors, and because the relevant
   output would be garbage on some terminals).  The problem comes when you try
   to maintain the color palette (e.g. for people who start watching halfway
   through, or for buggy terminals). */
int
init_color(uncursed_color colornum, short r, short g, short b)
{
    (void)colornum;
    (void)r;
    (void)g;
    (void)b;
    return OK;
}

uncursed_bool
can_change_color(void)
{
    return 0;
}

int
color_content(uncursed_color c, short *r, short *g, short *b)
{
    /* We don't know, but here's a reasonable guess... */
    if (c < 0 || c > 15)
        return ERR;
    else if (c == 7) {
        *r = 750;
        *g = 750;
        *b = 750;
    } else if (c == 8) {
        *r = 500;
        *g = 500;
        *b = 500;
    }
    if (c != 7 && c != 8) {
        *r = (c & 1) ? (c >= 8 ? 1000 : 500) : 0;
        *g = (c & 2) ? (c >= 8 ? 1000 : 500) : 0;
        *b = (c & 4) ? (c >= 8 ? 1000 : 500) : 0;
    }
    return OK;
}

int
pair_content(uncursed_color pairnum, uncursed_color *fgcolor,
             uncursed_color *bgcolor)
{
    if (pairnum < 0) {
        return ERR;
    } else if (pair_content_alloc_count < 0) {
        *fgcolor = DEFAULT_FOREGROUND;
        *bgcolor = DEFAULT_BACKGROUND;
    } else if (pairnum > pair_content_alloc_count) {
        *fgcolor = pair_content_list[0][0];
        *bgcolor = pair_content_list[0][1];
    } else {
        *fgcolor = pair_content_list[pairnum][0];
        *bgcolor = pair_content_list[pairnum][1];
    }
    return OK;
}

static int
add_window_attrs(int attr, int wattr)
{
    if (PAIR_NUMBER(attr))
        return attr | (wattr & ~(COLOR_PAIR(PAIR_NUMBER(wattr))));
    else
        return attr | wattr;
}

/* manual page 3ncurses attr */
UNCURSED_ANDWINDOWDEF(int,
attrset, (attr_t attr), (attr))
{
    win->current_attr = attr;
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
attron, (attr_t attr), (attr))
{
    win->current_attr |= attr;
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
attroff, (attr_t attr), (attr))
{
    win->current_attr &= ~attr;
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
color_set, (uncursed_color pairnum), (pairnum))
{
    win->current_attr &= ~COLOR_PAIR(PAIR_NUMBER(win->current_attr));
    win->current_attr |= COLOR_PAIR(pairnum);
    return OK;
}

UNCURSED_ANDWINDOWVDEF(int,
standout)
{
    wattron(win, A_STANDOUT);
    return OK;
}

UNCURSED_ANDWINDOWVDEF(int,
standend)
{
    wattrset(win, A_NORMAL);
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
attr_get,             (attr_t *attr, uncursed_color *pairnum, void *unused),
                      (attr, pairnum, unused))
{
    (void)unused;
    *attr = win->current_attr;
    *pairnum = PAIR_NUMBER(win->current_attr);
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
attr_off, (attr_t attr, void *unused), (attr, unused))
{
    (void)unused;
    return wattroff(win, attr);
}

UNCURSED_ANDWINDOWDEF(int,
attr_on, (attr_t attr, void *unused), (attr, unused))
{
    (void)unused;
    return wattron(win, attr);
}

UNCURSED_ANDWINDOWDEF(int,
attr_set, (attr_t attr, void *unused), (attr, unused))
{
    (void)unused;
    return wattrset(win, attr);
}

UNCURSED_ANDMVWINDOWDEF(int,
chgat,                  (int len, attr_t attr, uncursed_color pairnum,
                         const void *unused), (len, attr, pairnum, unused))
{
    (void)unused;

    int x = win->x;

    while (len) {
        win->chararray[win->y * win->stride + x].attr =
            attr | COLOR_PAIR(pairnum);
        len--;
        x++;

        if (x > win->maxx)
            break;
    }

    return OK;
}

/* uncursed-specific mouse binding code: works much the same way as the
   attribute code */
UNCURSED_ANDWINDOWDEF(void,
set_mouse_event,      (enum uncursed_mousebutton button, wint_t char_or_keycode,
                       int which), (button, char_or_keycode, which))
{
    int k = -1;
    if (which == OK)
        k = char_or_keycode;
    else if (which == KEY_CODE_YES)
        k = char_or_keycode + KEY_BIAS;
    win->current_bindings[button] = k;
}

void
uncursed_clear_mouse_regions(void)
{
    int y, x;
    enum uncursed_mousebutton b;

    for (y = 0; y <= nout_win->maxy; y++)
        for (x = 0; x <= nout_win->maxx; x++)
            for (b = 0; b < uncursed_mbutton_count; b++)
                nout_win->chararray[y * nout_win->stride + x].bindings[b] = -1;
}

void
uncursed_enable_mouse(int active)
{
    uncursed_hook_activatemouse(active);
}

/* manual page 3ncurses add_wch */
int TABSIZE = 8;        /* externally visible */
UNCURSED_ANDMVWINDOWDEF(int,
add_wch, (const cchar_t *ch), (ch))
{
    int res = add_wch_core(win, ch);
    if (res == ERR)
        return ERR;
    else
        return OK;
}

static int
add_wch_core(WINDOW * win, const cchar_t *ch)
{
    if (win->x > win->maxx || win->y > win->maxy || win->x < 0 || win->y < 0)
        return ERR;

    if (ch->chars[0] == 8) {    /* backspace */

        if (win->x > 0)
            win->x--;

    } else if (ch->chars[0] == 9) {     /* tab */

        win->x += TABSIZE - (win->x % TABSIZE);
        if (win->x > win->maxx)
            win->x = win->maxx;

    } else if (ch->chars[0] == 10) {
        if (!win->scrollok && win->y == win->maxy)
            return SCREND;

        wclrtoeol(win);
        win->y++;

        if (win->y > win->maxy) {
            scroll(win);
            win->y--;
        }

        win->x = 0;

    } else if (ch->chars[0] < 32 ||     /* nonprintable characters */
               (ch->chars[0] >= 127 && ch->chars[0] < 160)) {
        switch (add_wch_core(win, &(cchar_t){ch->attr, {'^'}})) {
        case ERR:
            return ERR;
        case SCREND:
            return SCREND;
        case OK:
            break;
        }

        return add_wch_core(win, &(cchar_t){ch->attr, {ch->chars[0] + 64}});
    } else {
        /* TODO: Detect whether ch contains only combining and zero-width
           characters, and combine them into the current character rather than
           replacing the current character with them, as well as not moving the
           cursor. (This is curses behaviour, but is a little perverse with
           respect to cursor motion; it'd make more sense to combine into the
           previous character.) */

        int idx = win->y * win->stride + win->x;

        memcpy(win->chararray + idx, ch, sizeof *ch);
        win->chararray[idx].attr =
            add_window_attrs(win->chararray[idx].attr, win->current_attr);
        memcpy(&(win->chararray[idx].bindings), &(win->current_bindings),
               sizeof win->current_bindings);

        win->x++;
        if (win->x > win->maxx) {
            win->x = 0;
            win->y++;
        }

        if (win->y > win->maxy) {
            win->y--;

            if (win->scrollok)
                scroll(win);
            else {
                win->x = win->maxx;
                return SCREND;
            }
        }

    }
    return OK;
}

UNCURSED_ANDWINDOWDEF(int,
echo_wchar, (const cchar_t *ch), (ch))
{
    if (wadd_wch(win, ch) == ERR)
        return ERR;
    return wrefresh(win);
}

static const cchar_t WACS[] = {
    {0, {0x25ae, 0}}, {0, {0x2592, 0}}, {0, {0x2534, 0}}, {0, {0x00b7, 0}},
    {0, {0x2592, 0}}, {0, {0x2193, 0}}, {0, {0x00b0, 0}}, {0, {0x25c6, 0}},
    {0, {0x2265, 0}}, {0, {0x2500, 0}}, {0, {0x2603, 0}}, {0, {0x2190, 0}},
    {0, {0x2264, 0}}, {0, {0x2514, 0}}, {0, {0x2518, 0}}, {0, {0x251c, 0}},
    {0, {0x2260, 0}}, {0, {0x03c0, 0}}, {0, {0x00b1, 0}}, {0, {0x253c, 0}},
    {0, {0x2192, 0}}, {0, {0x2524, 0}}, {0, {0x23ba, 0}}, {0, {0x23bb, 0}},
    {0, {0x23bc, 0}}, {0, {0x23bd, 0}}, {0, {0x00a3, 0}}, {0, {0x252c, 0}},
    {0, {0x2191, 0}}, {0, {0x250c, 0}}, {0, {0x2510, 0}}, {0, {0x2502, 0}}
};

const uncursed_cchar_tp WACS_BLOCK = WACS + 0;        /* ▮ */
const uncursed_cchar_tp WACS_BOARD = WACS + 1;        /* ▒ */
const uncursed_cchar_tp WACS_BTEE = WACS + 2;         /* ┴ */
const uncursed_cchar_tp WACS_BULLET = WACS + 3;       /* · */
const uncursed_cchar_tp WACS_CKBOARD = WACS + 4;      /* ▒ */
const uncursed_cchar_tp WACS_DARROW = WACS + 5;       /* ↓ */
const uncursed_cchar_tp WACS_DEGREE = WACS + 6;       /* ° */
const uncursed_cchar_tp WACS_DIAMOND = WACS + 7;      /* ◆ */
const uncursed_cchar_tp WACS_GEQUAL = WACS + 8;       /* ≥ */
const uncursed_cchar_tp WACS_HLINE = WACS + 9;        /* ─ */
const uncursed_cchar_tp WACS_LANTERN = WACS + 10;     /* ☃ */
const uncursed_cchar_tp WACS_LARROW = WACS + 11;      /* ← */
const uncursed_cchar_tp WACS_LEQUAL = WACS + 12;      /* ≤ */
const uncursed_cchar_tp WACS_LLCORNER = WACS + 13;    /* └ */
const uncursed_cchar_tp WACS_LRCORNER = WACS + 14;    /* ┘ */
const uncursed_cchar_tp WACS_LTEE = WACS + 15;        /* ├ */
const uncursed_cchar_tp WACS_NEQUAL = WACS + 16;      /* ≠ */
const uncursed_cchar_tp WACS_PI = WACS + 17;          /* π */
const uncursed_cchar_tp WACS_PLMINUS = WACS + 18;     /* ± */
const uncursed_cchar_tp WACS_PLUS = WACS + 19;        /* ┼ */
const uncursed_cchar_tp WACS_RARROW = WACS + 20;      /* → */
const uncursed_cchar_tp WACS_RTEE = WACS + 21;        /* ┤ */
const uncursed_cchar_tp WACS_S1 = WACS + 22;          /* ⎺ */
const uncursed_cchar_tp WACS_S3 = WACS + 23;          /* ⎻ */
const uncursed_cchar_tp WACS_S7 = WACS + 24;          /* ⎼ */
const uncursed_cchar_tp WACS_S9 = WACS + 25;          /* ⎽ */
const uncursed_cchar_tp WACS_STERLING = WACS + 26;    /* £ */
const uncursed_cchar_tp WACS_TTEE = WACS + 27;        /* ┬ */
const uncursed_cchar_tp WACS_UARROW = WACS + 28;      /* ↑ */
const uncursed_cchar_tp WACS_ULCORNER = WACS + 29;    /* ┌ */
const uncursed_cchar_tp WACS_URCORNER = WACS + 30;    /* ┐ */
const uncursed_cchar_tp WACS_VLINE = WACS + 31;       /* │ */

static const cchar_t WACS_T[] = {
    {0, {0x250f, 0}}, {0, {0x2517, 0}}, {0, {0x2513, 0}}, {0, {0x251b, 0}},
    {0, {0x2523, 0}}, {0, {0x252b, 0}}, {0, {0x253b, 0}}, {0, {0x2533, 0}},
    {0, {0x2501, 0}}, {0, {0x2503, 0}}, {0, {0x254b, 0}}
};

const uncursed_cchar_tp WACS_T_ULCORNER = WACS_T + 0; /* ┏ */
const uncursed_cchar_tp WACS_T_LLCORNER = WACS_T + 1; /* ┗ */
const uncursed_cchar_tp WACS_T_URCORNER = WACS_T + 2; /* ┓ */
const uncursed_cchar_tp WACS_T_LRCORNER = WACS_T + 3; /* ┛ */
const uncursed_cchar_tp WACS_T_LTEE = WACS_T + 4;     /* ┣ */
const uncursed_cchar_tp WACS_T_RTEE = WACS_T + 5;     /* ┫ */
const uncursed_cchar_tp WACS_T_BTEE = WACS_T + 6;     /* ┻ */
const uncursed_cchar_tp WACS_T_TTEE = WACS_T + 7;     /* ┳ */
const uncursed_cchar_tp WACS_T_HLINE = WACS_T + 8;    /* ━ */
const uncursed_cchar_tp WACS_T_VLINE = WACS_T + 9;    /* ┃ */
const uncursed_cchar_tp WACS_T_PLUS = WACS_T + 10;    /* ╋ */

static const cchar_t WACS_D[] = {
    {0, {0x2554, 0}}, {0, {0x255a, 0}}, {0, {0x2557, 0}}, {0, {0x255d, 0}},
    {0, {0x2560, 0}}, {0, {0x2563, 0}}, {0, {0x2569, 0}}, {0, {0x2566, 0}},
    {0, {0x2550, 0}}, {0, {0x2551, 0}}, {0, {0x256c, 0}}
};

const uncursed_cchar_tp WACS_D_ULCORNER = WACS_D + 0; /* ╔ */
const uncursed_cchar_tp WACS_D_LLCORNER = WACS_D + 1; /* ╚ */
const uncursed_cchar_tp WACS_D_URCORNER = WACS_D + 2; /* ╗ */
const uncursed_cchar_tp WACS_D_LRCORNER = WACS_D + 3; /* ╝ */
const uncursed_cchar_tp WACS_D_LTEE = WACS_D + 4;     /* ╠ */
const uncursed_cchar_tp WACS_D_RTEE = WACS_D + 5;     /* ╣ */
const uncursed_cchar_tp WACS_D_BTEE = WACS_D + 6;     /* ╩ */
const uncursed_cchar_tp WACS_D_TTEE = WACS_D + 7;     /* ╦ */
const uncursed_cchar_tp WACS_D_HLINE = WACS_D + 8;    /* ═ */
const uncursed_cchar_tp WACS_D_VLINE = WACS_D + 9;    /* ║ */
const uncursed_cchar_tp WACS_D_PLUS = WACS_D + 10;    /* ╬ */

/* manual page 3ncurses add_wchstr */
UNCURSED_ANDMVWINDOWDEF(int,
add_wchstr, (const cchar_t *charray), (charray))
{
    int n = 0;

    while (charray[n].chars[0] != 0 && n != CCHARW_MAX)
        n++;

    return wadd_wchnstr(win, charray, n);
}

UNCURSED_ANDMVWINDOWDEF(int,
add_wchnstr, (const cchar_t *charray, int n), (charray, n))
{
    if (n > win->maxx - win->x + 1)
        n = win->maxx - win->x + 1;

    memcpy(win->chararray + win->y * win->stride + win->x, charray,
           n * sizeof *charray);

    return OK;
}

/* manual page 3ncurses addch */
static wchar_t cp437[] = {      /* codepage 437 character table */

/* First 128 chars are the same as ASCII */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
    0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,

/* Next 128 chars are IBM extended */
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
};

UNCURSED_ANDMVWINDOWDEF(int,
addch, (const chtype ch), (ch))
{
    cchar_t cchar = {
        win->current_attr | (ch & ~(A_CHARTEXT)),
        {cp437[ch & A_CHARTEXT], 0}
    };

    return wadd_wch(win, &cchar);
}

UNCURSED_ANDWINDOWDEF(int,
echochar, (const chtype ch), (ch))
{
    cchar_t cchar = {
        win->current_attr | (ch & ~(A_CHARTEXT)),
        {cp437[ch & A_CHARTEXT], 0}
    };

    return wecho_wchar(win, &cchar);
}

/* manual page 3ncurses addchstr */
UNCURSED_ANDMVWINDOWDEF(int,
addchstr, (const chtype *charray), (charray))
{
    int n = 0;

    while (charray[n])
        n++;
    return waddchnstr(win, charray, n);
}

UNCURSED_ANDMVWINDOWDEF(int,
addchnstr, (const chtype *charray, int n), (charray, n))
{
    cchar_t *p = win->chararray + win->y * win->stride + win->x;
    int i;

    if (n > win->maxx - win->x + 1)
        n = win->maxx - win->x + 1;

    for (i = 0; i < n; i++) {
        p->attr = add_window_attrs(charray[i] & ~(A_CHARTEXT),
                                   win->current_attr);
        p->chars[0] = cp437[charray[i] & A_CHARTEXT];
        p->chars[1] = 0;
        memcpy(&(p->bindings), &(win->current_bindings),
               sizeof win->current_bindings);
        p++;
    }
    return OK;
}

/* manual page 3ncurses addstr */
UNCURSED_ANDMVWINDOWDEF(int,
addstr, (const char *s), (s))
{
    while (*s)
        switch (add_wch_core(win, &(cchar_t){0, {*s++}})) {
        case ERR:
            return ERR;
        case SCREND:
            return OK;
        case OK:
            break;
        }

    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
addnstr, (const char *s, int n), (s, n))
{
    while (*s && n-- != 0) {
        switch (add_wch_core(win, &(cchar_t){0, {*s++}})) {
        case ERR:
            return ERR;
        case SCREND:
            return OK;
        case OK:
            break;
        }

        /* Negative n means write until the end of the line. */
        /* BUG: this does not handle control characters, which become two
         * characters, correctly */
        if (n < 0 && win->x == 0)
            return OK;
    }
    return OK;
}

/* manual page 3ncurses addwstr */
UNCURSED_ANDMVWINDOWDEF(int,
addwstr, (const wchar_t * s), (s))
{
    while (*s) {
        cchar_t c = { 0, {*s++, 0} };

        if (wadd_wch(win, &c) == ERR)
            return ERR;
    }
    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
addwnstr, (const wchar_t * s, int n), (s, n))
{
    while (*s && n-- != 0) {
        cchar_t c = { 0, {*s++, 0} };

        if (wadd_wch(win, &c) == ERR)
            return ERR;

        /* Negative n means write until the end of the line. */
        if (n < 0 && win->x == 0)
            return OK;
    }
    return OK;
}

/* manual page 3ncurses default_colors */
int
use_default_colors(void)
{
    assume_default_colors(-1, -1);
    return OK;
}

/* No, I don't know why these are ints either. */
int
assume_default_colors(int fgcolor, int bgcolor)
{
    init_pair(0, fgcolor, bgcolor);
    return OK;
}

/* manual page 3ncurses beep */
int
beep(void)
{
    uncursed_hook_beep();
    return OK;
}

int
flash(void)
{
    /* Invert colors on the entire screen. */
    int i;

    for (i = 0; i < pair_content_alloc_count; i++) {
        uncursed_color t = pair_content_list[i][0];

        pair_content_list[i][0] = pair_content_list[i][1];
        pair_content_list[i][1] = t;
    }

    /* Redraw the entire screen. */
    touchwin(nout_win);
    doupdate();
    uncursed_hook_delay(500);

    /* Now put it back to the way it was. */
    for (i = 0; i < pair_content_alloc_count; i++) {
        uncursed_color t = pair_content_list[i][0];

        pair_content_list[i][0] = pair_content_list[i][1];
        pair_content_list[i][1] = t;
    }

    touchwin(nout_win);
    doupdate();
    return OK;
}

/* manual page 3ncurses border */
UNCURSED_ANDWINDOWDEF(int,
border,               (chtype ls, chtype rs, chtype ts, chtype bs,
                       chtype tl, chtype tr, chtype bl, chtype br),
                      (ls, rs, ts, bs, tl, tr, bl, br))
{
    if (ls == 0)
        ls = ACS_VLINE;
    if (rs == 0)
        rs = ACS_VLINE;
    if (ts == 0)
        ts = ACS_HLINE;
    if (bs == 0)
        bs = ACS_HLINE;
    if (tl == 0)
        tl = ACS_ULCORNER;
    if (tr == 0)
        tr = ACS_URCORNER;
    if (bl == 0)
        bl = ACS_LLCORNER;
    if (br == 0)
        br = ACS_LRCORNER;

    int sx = win->x;
    int sy = win->y;
    int i;

    for (i = 1; i < win->maxx; i++) {
        mvwaddchnstr(win, 0, i, &ts, 1);
        mvwaddchnstr(win, win->maxy, i, &bs, 1);
    }
    for (i = 1; i < win->maxy; i++) {
        mvwaddchnstr(win, i, 0, &ls, 1);
        mvwaddchnstr(win, i, win->maxx, &rs, 1);
    }

    mvwaddchnstr(win, 0, 0, &tl, 1);
    mvwaddchnstr(win, 0, win->maxx, &tr, 1);
    mvwaddchnstr(win, win->maxy, 0, &bl, 1);
    mvwaddchnstr(win, win->maxy, win->maxx, &br, 1);

    win->x = sx;
    win->y = sy;
    return OK;
}

int
box(WINDOW *win, chtype verch, chtype horch)
{
    return wborder(win, verch, verch, horch, horch, 0, 0, 0, 0);
}

UNCURSED_ANDMVWINDOWDEF(int,
hline, (chtype ch, int n), (ch, n))
{
    /* We'd go into an infinite loop if someone tried to draw a line of cursor
       motion commands... */
    if (ch == 8 || ch == 9 || ch == 10)
        return ERR;

    int sx = win->x;

    while (n > 0 && win->x <= win->maxx) {
        mvwaddchnstr(win, win->y, win->x, &ch, 1);
        n--;
        win->x++;
    }

    win->x = sx;
    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
vline, (chtype ch, int n), (ch, n))
{
    if (ch == 8 || ch == 9 || ch == 10)
        return ERR;

    int sy = win->y;

    while (n > 0 && win->y <= win->maxy) {
        mvwaddchnstr(win, win->y, win->x, &ch, 1);
        n--;
        win->y++;
    }

    win->y = sy;
    return OK;
}

/* manual page 3ncurses border_set */
UNCURSED_ANDWINDOWDEF(int,
border_set,           (const cchar_t *ls, const cchar_t *rs, const cchar_t *ts,
                       const cchar_t *bs, const cchar_t *tl, const cchar_t *tr,
                       const cchar_t *bl, const cchar_t *br),
                      (ls, rs, ts, bs, tl, tr, bl, br))
{
    if (ls == 0)
        ls = WACS_VLINE;
    if (rs == 0)
        rs = WACS_VLINE;
    if (ts == 0)
        ts = WACS_HLINE;
    if (bs == 0)
        bs = WACS_HLINE;
    if (tl == 0)
        tl = WACS_ULCORNER;
    if (tr == 0)
        tr = WACS_URCORNER;
    if (bl == 0)
        bl = WACS_LLCORNER;
    if (br == 0)
        br = WACS_LRCORNER;

    int sx = win->x;
    int sy = win->y;
    int i;

    for (i = 1; i < win->maxx; i++) {
        mvwadd_wch(win, 0, i, ts);
        mvwadd_wch(win, win->maxy, i, bs);
    }
    for (i = 1; i < win->maxy; i++) {
        mvwadd_wch(win, i, 0, ls);
        mvwadd_wch(win, i, win->maxx, rs);
    }

    mvwadd_wch(win, 0, 0, tl);
    mvwadd_wch(win, 0, win->maxx, tr);
    mvwadd_wch(win, win->maxy, 0, bl);
    mvwadd_wch(win, win->maxy, win->maxx, br);

    win->x = sx;
    win->y = sy;
    return OK;
}

int
box_set(WINDOW *win, const cchar_t *verch, const cchar_t *horch)
{
    return wborder_set(win, verch, verch, horch, horch, 0, 0, 0, 0);
}

UNCURSED_ANDMVWINDOWDEF(int,
hline_set, (const cchar_t *ch, int n), (ch, n))
{
    /* We'd go into an infinite loop if someone tried to draw a line of cursor
       motion commands... */
    if (ch->chars[0] == 8 || ch->chars[0] == 9 || ch->chars[0] == 10)
        return ERR;

    int sx = win->x;
    int sy = win->y;

    while (n > 0) {
        if (win->x == win->maxx)
            n = 1;
        wadd_wch(win, ch);
        n--;
    }

    win->x = sx;
    win->y = sy;
    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
vline_set, (const cchar_t *ch, int n), (ch, n))
{
    if (ch->chars[0] == 8 || ch->chars[0] == 9 || ch->chars[0] == 10)
        return ERR;

    int sx = win->x;
    int sy = win->y;

    while (n > 0) {
        int oy = win->y;
        if (win->y == win->maxy)
            n = 1;
        wadd_wch(win, ch);
        wmove(win, oy + 1, sx);
        n--;
    }

    win->x = sx;
    win->y = sy;
    return OK;
}

/* manual page 3ncurses inopts */
int
cbreak(void)
{
    return noraw();
}

int
nocbreak(void)
{
    timeout(-1);
    return OK;
}

int
noecho(void)
{
    return OK;
}

int
halfdelay(int d)
{
    timeout(d * 100);
    return OK;
}

int
intrflush(WINDOW *win, uncursed_bool b)
{
    (void)win;
    (void)b;
    return OK;
}

int
keypad(WINDOW *win, uncursed_bool b)
{
    (void)win;
    (void)b;
    return OK;
}

int
meta(WINDOW *win, uncursed_bool b)
{
    (void)win;
    (void)b;
    return OK;
}

int
nodelay(WINDOW *win, uncursed_bool b)
{
    wtimeout(win, b ? 0 : -1);
    return OK;
}

int
raw(void)
{
    uncursed_hook_rawsignals(1);
    return OK;
}

int
noraw(void)
{
    uncursed_hook_rawsignals(0);
    return OK;
}

int
qiflush(void)
{
    return OK;
}

int
noqiflush(void)
{
    return OK;
}

int
notimeout(WINDOW *win, uncursed_bool b)
{
    (void)win;
    (void)b;
    return OK;
}

void
timeout(int t)
{
    wtimeout(stdscr, t);
}

void
wtimeout(WINDOW *win, int t)
{
    win->timeout = t;
}

int
typeahead(int fd)
{
    (void)fd;
    return OK;
}

/* manual page 3ncurses overlay */
int
overlay(const WINDOW *from, const WINDOW *to)
{
    return copywin(from, to, 0, 0, 0, 0, min(from->maxy, to->maxy),
                   min(from->maxx, to->maxx), 1);
}

int
overwrite(const WINDOW *from, const WINDOW *to)
{
    return copywin(from, to, 0, 0, 0, 0, min(from->maxy, to->maxy),
                   min(from->maxx, to->maxx), 0);
}

int
copywin(const WINDOW *from, const WINDOW *to, int from_miny, int from_minx,
        int to_miny, int to_minx, int to_maxy, int to_maxx, int skip_blanks)
{
    if (to != disp_win && to != nout_win)
        if (from->region || to->region)
            return ERR;

    int i, j;

    const int xoffset = to_minx - from_minx;
    const int yoffset = to_miny - from_miny;

    int imin = from_minx;
    if (imin < 0)
        imin = 0;
    if (imin < -xoffset)
        imin = -xoffset;

    int imax = to_maxx - xoffset;
    if (imax > from->maxx)
        imax = from->maxx;
    if (imax > to->maxx - xoffset) /* note: to->maxx might not be to_maxx */
        imax = to->maxx - xoffset;

    int irange = imax + 1 - imin;

    int jmin = from_miny;
    if (jmin < 0)
        jmin = 0;
    if (jmin < -yoffset)
        jmin = -yoffset;

    int jmax = to_maxy - yoffset;
    if (jmax > from->maxy)
        jmax = from->maxy;
    if (jmax > to->maxy - yoffset) /* note: to->maxy might not be to_maxy */
        jmax = to->maxy - yoffset;

    for (j = jmin; j <= jmax; j++) {

        /* Note: C doesn't allow pointers to go more than one byte past the
           end of an array. This makes it hard to dead-reckon these pointers
           with respect to j, because we can't be an entire stride past the
           start or the end. The easiest solution is to just calculate them
           anew on each loop iteration (especially because this isn't the
           inner loop; the loop on i is the inner loop). */

        cchar_t *fc = from->chararray + j * from->stride;
        cchar_t *tc = to->chararray + xoffset + ((j + yoffset) * to->stride);
        void **fr = from->regionarray + j * (from->maxx + 1);
        void **tr = to->regionarray + xoffset +
            ((j + yoffset) * (to->maxx + 1));

        if (skip_blanks) {
            /* Conceptually "for (i = imin; i <= imax; i++)", but we don't use
               the value of i, so make the loop as optimizable as possible. */
            for (i = 0; i < irange; i++) {
                if (fc->chars[0] != 32) {
                    *tc = *fc;
                    *tr = *fr;

                    if (to == nout_win)
                        tc->color_on_screen =
                            color_on_screen_for_attr(tc->attr);
                }

                /* Dead-reckon these pointers for performance. This function is
                   often the inner loop of libuncursed (depending on what the
                   application using us wants). */
                tc++;
                fc++;
                tr++;
                fr++;
            }
        } else {
            /* We could use a dead-reckoned loop like the above. However,
               there's no particular reason to copy one cchar/void * at a time
               when we could copy as opaque objects (which is even faster). */
            memcpy(tc, fc, irange * sizeof *tc);
            memcpy(tr, fr, irange * sizeof *tr);

            if (to == nout_win)
                for (i = 0; i < irange; i++) {
                    tc->color_on_screen =
                        color_on_screen_for_attr(tc->attr);
                    tc++;
                }
        }
    }
    return OK;
}


/* manual page 3ncurses clear */
UNCURSED_ANDWINDOWVDEF(int,
erase)
{
    wmove(win, 0, 0);
    return wclrtobot(win);
}

UNCURSED_ANDWINDOWVDEF(int,
clear)
{
    werase(win);
    return clearok(win, 1);
}

UNCURSED_ANDWINDOWVDEF(int,
clrtobot)
{
    int j, i;
    unsigned short cosfa = color_on_screen_for_attr(win->current_attr);

    wclrtoeol(win);

    for (j = win->y + 1; j <= win->maxy; j++) {
        for (i = 0; i <= win->maxx; i++) {
            cchar_t *ccp = win->chararray + i + j * win->stride;
            ccp->attr = win->current_attr;
            ccp->chars[0] = 32;
            ccp->chars[1] = 0;
            ccp->color_on_screen = cosfa;

            /* Note that it's possible to erase a window in "mouse binding
               color" to set bindings. This might be useful to have default
               bindings for the wheel, for instance. */
            memcpy(&(ccp->bindings), &(win->current_bindings),
                   sizeof win->current_bindings);
        }
    }

    return OK;
}

UNCURSED_ANDWINDOWVDEF(int,
clrtoeol)
{
    int maxpos = win->maxx + win->y * win->stride;
    int curpos = win->x + win->y * win->stride;
    unsigned short cosfa = color_on_screen_for_attr(win->current_attr);

    while (curpos <= maxpos) {
        win->chararray[curpos].attr = win->current_attr;
        win->chararray[curpos].chars[0] = 32;
        win->chararray[curpos].chars[1] = 0;
        win->chararray[curpos].color_on_screen = cosfa;
        memcpy(&(win->chararray[curpos].bindings), &(win->current_bindings),
               sizeof win->current_bindings);
        curpos++;
    }

    return OK;
}

/* manual page 3ncurses outopts */
int
clearok(WINDOW *win, uncursed_bool clear_on_refresh)
{
    win->clear_on_refresh = clear_on_refresh;
    return OK;
}

int
scrollok(WINDOW *win, uncursed_bool scrollok)
{
    win->scrollok = scrollok;
    return OK;
}

int
nonl(void)
{
    return OK;
}

int
leaveok(WINDOW *win, uncursed_bool dont_restore_cursor)
{
    (void)win;
    (void)dont_restore_cursor;
    return OK;
}

/* manual page 3ncurses kernel */
int
curs_set(int vis)
{
    uncursed_hook_setcursorsize(vis);
    return OK;
}

/* manual page 3ncurses util */
char *
unctrl(char d)
{
    int c = d;

    if (c < 0)
        c += 256;
    static char s[5] = { 'M', '-' };
    char *r = s + 2;

    if (c > 127) {
        c -= 128;
        r = s;
    }

    if (c == 127) {
        c = '?';
        r = s;
    }

    if (c < 32) {
        s[2] = '^';
        s[3] = c + 64;
        s[4] = 0;
    } else {
        s[2] = c;
        s[3] = 0;
    }

    return r;
}

wchar_t *
wunctrl(wchar_t c)
{
    static wchar_t s[5] = { (wchar_t) 'M', (wchar_t) '-' };
    wchar_t *r = s + 2;

    if (c >= 128 && c <= 255) {
        c -= 128;
        r = s;
    }

    if (c == 127) {
        c = '?';
        r = s;
    }

    if (c < 32) {
        s[2] = '^';
        s[3] = c + 64;
        s[4] = 0;
    } else {
        s[2] = c;
        s[3] = 0;
    }

    return r;
}

char *
keyname(int c)
{
    if (c < 256)
        return unctrl(c);

    /*
     * We have three types of special keys:
     *
     * - Cursor motion / numeric keypad: ESC [ letter or ESC O letter
     *   (Modified: ESC [ 1 ; modifier letter or ESC O 1 ; modifier letter)
     * - General function keys: ESC [ number ~
     *   (Modified: ESC [ number ; modifier ~)
     * - F1-F5 can send other codes, such as ESC [ [ letter
     *
     * The letters can be both uppercase and lowercase. (Lowercase letters
     * are used for the numeric keypad by some terminals.)
     *
     * We use the integer as a bitfield:
     * 256     always true (to make the code >= 256)
     * 512     true for cursor motion/numpad
     * 1024    true for Linux console F1-F5
     * 2048 up the modifier seen minus 1 (0 for no modifier)
     * 1       the number or letter seen
     *
     * Based on the codes normally sent, a modifier of shift sets the 2048s bit,
     * of alt sets the 4096s bit, of control sets the 8192s it. Some codes won't
     * be sent by certain terminals, and some will overlap. One problem we have
     * is that there are some keys that are the same but have different codes on
     * different terminals (e.g. home/end), and some keys that are conflated by
     * some terminals but not others (e.g. numpad home versus main keyboard
     * home). In order to give stable KEY_x definitions, we translate ^[OH and
     * ^[OF (home/end on xterm) to ^[[1~ and ^[[4~ (home/end on Linux console),
     * ^[[15~ (F5 on xterm) to ^[[[E (F5 on Linux console), and ^[[E (num5 on
     * xterm) and ^[[G (num6 on Linux console outside application mode) to ^[Ou
     * (num5 on Linux console inside application mode). There's also one clash
     * between ESC [ P (Pause/Break), and ESC O P (PF1); we resolve this by
     * translating ESC [ P as ESC [ [ P.
     *
     * keyname's job is to undo all this, and return a sensible name for the
     * key that's pressed. Unlike curses keyname, it will construct a name for
     * any keypress.
     */

    static char keybuf[80];

    if (c > KEY_MAX) {
        snprintf(keybuf, sizeof(keybuf), "KEY_MAX + %d", c - KEY_MAX);
        return keybuf;
    }

    strcpy(keybuf, "KEY_");
    if (c & KEY_CTRL)
        strcat(keybuf, "CTRL_");
    if (c & KEY_ALT)
        strcat(keybuf, "ALT_");
    if (c & KEY_SHIFT)
        strcat(keybuf, "SHIFT_");

    c &= ~(KEY_CTRL | KEY_ALT | KEY_SHIFT);

    switch (c) {
#define KEYCHECK(x) case KEY_##x: strcat(keybuf, #x); break
        KEYCHECK(HOME); KEYCHECK(IC); KEYCHECK(DC); KEYCHECK(END);
        KEYCHECK(PPAGE); KEYCHECK(NPAGE); KEYCHECK(BREAK); KEYCHECK(BTAB);
        KEYCHECK(UP); KEYCHECK(DOWN); KEYCHECK(RIGHT); KEYCHECK(LEFT);
        KEYCHECK(F1); KEYCHECK(F2); KEYCHECK(F3); KEYCHECK(F4); KEYCHECK(F5);
        KEYCHECK(F6); KEYCHECK(F7); KEYCHECK(F8); KEYCHECK(F9); KEYCHECK(F10);
        KEYCHECK(F11); KEYCHECK(F12); KEYCHECK(F13); KEYCHECK(F14);
        KEYCHECK(F15); KEYCHECK(F16); KEYCHECK(F17); KEYCHECK(F18);
        KEYCHECK(F19); KEYCHECK(F20);
        KEYCHECK(PF1); KEYCHECK(PF2); KEYCHECK(PF3); KEYCHECK(PF4);
        KEYCHECK(A1); KEYCHECK(B1); KEYCHECK(C1); KEYCHECK(D1);
        KEYCHECK(A2); KEYCHECK(B2); KEYCHECK(C2);
        KEYCHECK(A3); KEYCHECK(B3); KEYCHECK(C3); KEYCHECK(D3);
        KEYCHECK(NUMDIVIDE); KEYCHECK(NUMPLUS);
        KEYCHECK(NUMMINUS); KEYCHECK(NUMTIMES);
        KEYCHECK(BACKSPACE); KEYCHECK(ESCAPE); KEYCHECK(ENTER);
        KEYCHECK(RESIZE); KEYCHECK(PRINT);
        KEYCHECK(INVALID); KEYCHECK(HANGUP);
        KEYCHECK(OTHERFD); KEYCHECK(SIGNAL); KEYCHECK(UNHOVER);
#undef KEYCHECK

    default:
        if (c < 256) {
            /* It might be an alt-combination. */
            if (c >= 32 && c <= 126 && !strcmp(keybuf, "KEY_ALT_"))
                sprintf(keybuf + strlen(keybuf) - 1, "|'%c'", c & 255);
            else
                sprintf(keybuf + strlen(keybuf) - 1, "|%d", c & 255);
        } else if ((c & KEY_KEYPAD) == KEY_KEYPAD) {
            /* Unknown key; synthesize a name. We use an ASCII representation
               because that's the format that KEY_KEYPAD uses. */
            sprintf(keybuf + strlen(keybuf), "KEYPAD|'%c'", c & 255);
        } else {
            /* Unknown key; synthesize a name. KEY_FUNCTION sends arbitrary
               numbers, so write it as a decimal integer. */
            sprintf(keybuf + strlen(keybuf), "FUNCTION|%d", c & 255);
        }
    }

    return keybuf;
}

char *
key_name(wint_t c)
{
    if (c < 256)
        return 0;       /* it's not a key code */

    return keyname(c);
}

char *
friendly_keyname(int c)
{
    /* Output the name of a key in the way that will be most familiar to the
       user. Assumes ASCII (although a lot more would break otherwise...) */

    static char keybuf[80];

    if (c > KEY_MAX) {
        snprintf(keybuf, sizeof(keybuf), "User%d", c - KEY_MAX);
        return keybuf;
    }

    *keybuf = 0;

    if (c & KEY_ALT) {
        c &= ~KEY_ALT;
        strcat(keybuf, "Alt-");
    }

    if (c >= 128 && c <= 256) {
        c -= 128;
        strcat(keybuf, "Meta-");
    }

    if (c & KEY_SHIFT) {
        c &= ~KEY_SHIFT;
        strcat(keybuf, "Shift-");
    }
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
        strcat(keybuf, "Shift-");
    }

    if (c & KEY_CTRL) {
        c &= ~KEY_CTRL;
        strcat(keybuf, "Ctrl-");
    }
    if (c < 32) {
        c += 'A' - 1;
        strcat(keybuf, "Ctrl-");
    }

    if (c >= 33 && c <= 126) {  /* printable and visible */
        sprintf(keybuf + strlen(keybuf), "%c", c);
        return keybuf;
    }

    switch (c) {
#define KEYNAME(k,n) case KEY_##k: strcat(keybuf, #n); break;
        KEYNAME(HOME, Home); KEYNAME(IC, Ins); KEYNAME(DC, Del);
        KEYNAME(END, End); KEYNAME(PPAGE, PgUp); KEYNAME(NPAGE, PgDn);
        KEYNAME(UP, Up); KEYNAME(DOWN, Down);
        KEYNAME(RIGHT, Right); KEYNAME(LEFT, Left);
        KEYNAME(BREAK, Break); KEYNAME(BTAB, Shift-Tab);
        KEYNAME(F1, F1); KEYNAME(F2, F2); KEYNAME(F3, F3); KEYNAME(F4, F4);
        KEYNAME(F5, F5); KEYNAME(F6, F6); KEYNAME(F7, F7); KEYNAME(F8, F8);
        KEYNAME(F9, F9); KEYNAME(F10, F10); KEYNAME(F11, F11);
        KEYNAME(F12, F12); KEYNAME(F13, F13); KEYNAME(F14, F14);
        KEYNAME(F15, F15); KEYNAME(F16, F16); KEYNAME(F17, F17);
        KEYNAME(F18, F18); KEYNAME(F19, F19); KEYNAME(F20, F20);
        KEYNAME(PF1, F1/NumLock); KEYNAME(PF2, F2/NumDivide);
        KEYNAME(PF3, F3/NumTimes); KEYNAME(PF4, F4/NumMinus);
        KEYNAME(A1, NumHome); KEYNAME(A2, NumUp); KEYNAME(A3, NumPgUp);
        KEYNAME(B1, NumLeft); KEYNAME(B2, Num5); KEYNAME(B3, NumRight);
        KEYNAME(C1, NumEnd); KEYNAME(C2, NumDown); KEYNAME(C3, NumPgDn);
        KEYNAME(D1, NumIns); KEYNAME(D3, NumDel); KEYNAME(ENTER, Enter);
        KEYNAME(NUMPLUS, NumPlus); KEYNAME(NUMMINUS, NumMinus);
        KEYNAME(NUMTIMES, NumTimes); KEYNAME(NUMDIVIDE, NumDivide);
        KEYNAME(BACKSPACE, BkSp); KEYNAME(ESCAPE, Escape);
        KEYNAME(RESIZE, Resize);
        KEYNAME(PRINT, PrtSc); KEYNAME(INVALID, Invalid);
        KEYNAME(HANGUP, Hangup); KEYNAME(UNHOVER, Unhover);
        KEYNAME(OTHERFD, OtherFD); KEYNAME(SIGNAL, Signal);
#undef KEYAME

    case ' ':
        strcat(keybuf, "Space");
        break;

    default:
        /* Synthesize a key name out of the codes we were actually sent. */

        if ((c & KEY_KEYPAD) == KEY_KEYPAD) {
            sprintf(keybuf + strlen(keybuf), "Unknown%d1",
                    (int)(unsigned char)c);
        } else {
            sprintf(keybuf + strlen(keybuf), "Unknown%d2",
                    (int)(unsigned char)c);
        }
    }

    return keybuf;
}

int
delay_output(int ms)
{
    uncursed_hook_delay(ms);
    return OK;
}

/* manual page 3ncurses delch */
UNCURSED_ANDMVWINDOWVDEF(int,
delch)
{
    memmove(win->chararray + win->x + win->y * win->stride,
            win->chararray + win->x + win->y * win->stride + 1,
            (win->maxx - win->x) * sizeof *(win->chararray));

    cchar_t *lastchar = win->chararray + win->maxx + win->y * win->stride;

    lastchar->attr = win->current_attr;
    lastchar->chars[0] = 32;
    lastchar->chars[1] = 0;
    memcpy(&(lastchar->bindings), &(win->current_bindings),
           sizeof win->current_bindings);

    return OK;
}

/* manual page 3ncurses deleteln */
UNCURSED_ANDWINDOWVDEF(int,
deleteln)
{
    return winsdelln(win, -1);
}

UNCURSED_ANDWINDOWVDEF(int,
insertln)
{
    return winsdelln(win, 1);
}

UNCURSED_ANDWINDOWDEF(int,
insdelln, (int n), (n))
{
    uncursed_bool inserting = 1;

    if (n < 0)
        inserting = 0;
    else if (n == 0)
        return OK;      /* avoid memcpy(x,x), it's undefined behaviour */

    int j, i;

    for (j = (inserting ? win->maxy : win->y);
         j >= win->y && j <= win->maxy;
         j += (inserting ? -1 : 1)) {

        if (j - n >= win->y && j - n <= win->maxy)
            memcpy(win->chararray + j * win->stride,
                   win->chararray + (j - n) * win->stride,
                   win->maxx * sizeof *(win->chararray));
        else
            for (i = 0; i <= win->maxx; i++) {
                cchar_t *newchar = win->chararray + i + j * win->stride;
                newchar->attr = win->current_attr;
                newchar->chars[0] = 32;
                newchar->chars[1] = 0;
                memcpy(&(newchar->bindings), &(win->current_bindings),
                       sizeof win->current_bindings);
            }
    }
    return OK;
}

/* manual page 3ncurses initscr */
WINDOW *stdscr = 0;                   /* externally visible */
static WINDOW *save_stdscr = 0;
static char title[120] = "Uncursed";
int LINES, COLS;                      /* externally visible */

void
uncursed_set_title(const char *t)
{
    strncpy(title, t, sizeof (title) - 1);
    title[sizeof (title) - 1] = '\0';
}

WINDOW *
initscr(void)
{
    if (!uncursed_hooks_inited) {
        /* UB, and very bad UB at that. Also UB that's easy to do by mistake,
           because an unmodified curses program would do that. Take the
           opportunity to crash the program with a helpful message. */

        fprintf(stderr,
                "Do not call initscr() (or any other uncursed function)\n"
                "without first calling initialize_uncursed().\n");
        abort();
    }

    if (save_stdscr || stdscr)
        return 0;

    uncursed_hook_init(&LINES, &COLS, title);
    nout_win = newwin(0, 0, 0, 0);
    disp_win = newwin(0, 0, 0, 0);
    stdscr = newwin(0, 0, 0, 0);

    if (!nout_win || !disp_win || !stdscr) {
        fprintf(stderr, "uncursed: could not allocate memory!\n");
        exit(5);
    }

    return stdscr;
}

int
endwin(void)
{
    save_stdscr = stdscr;
    stdscr = 0;
    uncursed_hook_exit();

    return touchwin(save_stdscr);
}

uncursed_bool
isendwin(void)
{
    return !stdscr;
}

/* should only be called immediately prior to returning KEY_RESIZE */
void
uncursed_rhook_setsize(int h, int w)
{
    LINES = h;
    COLS = w;
    wresize(stdscr, h, w);
    wresize(nout_win, h, w);
    wresize(disp_win, h, w);
    redrawwin(stdscr); /* we need to touch every character */

    struct uncursed_hooks *hook;

    for (hook = uncursed_hook_list; hook; hook = hook->next_hook)
        if (hook->used && hook->hook_type != uncursed_hook_type_input)
            hook->resized(h, w);
}

/* manual page 3ncurses window */
WINDOW *
newwin(int h, int w, int t, int l)
{
    WINDOW *win = malloc(sizeof (WINDOW));
    int i;

    if (!win || h < 0 || w < 0)
        return 0;
    if (h == 0)
        h = LINES;
    if (w == 0)
        w = COLS;

    win->chararray = malloc(w * h * sizeof *(win->chararray));
    if (!win->chararray) {
        free(win);
        return 0;
    }

    win->regionarray = malloc(w * h * sizeof *(win->regionarray));
    if (!win->regionarray) {
        free(win->chararray);
        free(win);
        return 0;
    }

    for (i = 0; i < w * h; i++)
        win->regionarray[i] = NULL;

    win->current_attr = 0;
    for (i = 0; i < uncursed_mbutton_count; i++)
        win->current_bindings[i] = -1;

    win->y = win->x = 0;
    win->maxx = w - 1;
    win->maxy = h - 1;
    win->stride = w;    /* no reason to use any other packing scheme */
    win->scry = t;
    win->scrx = l;
    win->regy = 0;
    win->regx = 0;
    win->region = 0;
    win->parent = 0;
    win->child = 0;
    win->sibling = 0;
    win->timeout = -1;  /* input in this WINDOW is initially blocking */
    win->clear_on_refresh = 0;
    win->scrollok = 0;

    werase(win);

    return win;
}

WINDOW *
subwin(WINDOW *parent, int h, int w, int t, int l)
{
    WINDOW *win = malloc(sizeof (WINDOW));
    int i;

    if (!win)
        return 0;

    win->regionarray = malloc(w * h * sizeof *(win->regionarray));
    if (!win->regionarray) {
        free(win);
        return 0;
    }

    for (i = 0; i < w * h; i++)
        win->regionarray[i] = NULL;
    win->parent = parent;
    win->child = 0;
    win->sibling = win->parent->child;
    win->parent->child = win;
    win->chararray = parent->chararray;

    win->current_attr = 0;
    for (i = 0; i < uncursed_mbutton_count; i++)
        win->current_bindings[i] = -1;

    win->y = win->x = 0;
    win->maxx = w - 1;
    win->maxy = h - 1;
    win->stride = parent->stride;
    win->scry = t;
    win->scrx = l;
    win->regy = 0;
    win->regx = 0;
    win->region = 0;
    win->timeout = -1;
    win->clear_on_refresh = 0;
    win->scrollok = 0;

    return win;
}

WINDOW *
derwin(WINDOW *parent, int h, int w, int t, int l)
{
    WINDOW *rv = subwin(parent, h, w, t + parent->scry, l + parent->scrx);

    if (rv)
        mvderwin(rv, t, l);
    return rv;
}

int
delwin(WINDOW *win)
{
    if (!win)
        return ERR;
    if (win->child)
        return ERR;

    if (win->parent) {
        /* Remove this window from its family of children. */
        WINDOW *prevsibling = win->parent->child;

        while (prevsibling->sibling != win && prevsibling != win) {
            prevsibling = prevsibling->sibling;
        }
        if (prevsibling != win)
            prevsibling->sibling = win->sibling;
        if (win->parent->child == win)
            win->parent->child = win->sibling;
    } else
        free(win->chararray);

    wdelete_tiles_region(win);
    free(win->regionarray);
    free(win);

    return OK;
}

int
mvwin(WINDOW *win, int y, int x)
{
    if (win->maxy + y > LINES || y < 0)
        return ERR;
    if (win->maxx + x > COLS || x < 0)
        return ERR;

    win->scry = y;
    win->scrx = x;

    wdelete_tiles_region(win);

    return OK;
}

int
mvderwin(WINDOW *win, int y, int x)
{
    win->chararray = win->parent->chararray + x + y * (win->parent->stride);
    return OK;
}

/* Synch routines are mostly no-ops because touchwin is also a no-op */
void
wsyncup(WINDOW *win)
{
    (void)win;
}

void
wsyncdown(WINDOW *win)
{
    (void)win;
}

int
syncok(WINDOW *win, uncursed_bool sync)
{
    (void)win;
    (void)sync;
    return OK;
}

/* but this one isn't */
void
wcursyncup(WINDOW *win)
{
    if (!win->parent)
        return;

    win->parent->x =
        win->x + (win->chararray -
                  win->parent->chararray) % (win->parent->stride);
    win->parent->y =
        win->y + (win->chararray -
                  win->parent->chararray) / (win->parent->stride);

    wcursyncup(win->parent);
}

/* manual page 3ncurses refresh */
UNCURSED_ANDWINDOWVDEF(int,
refresh)
{
    if (wnoutrefresh(win) == ERR)
        return ERR;
    return doupdate();
}

int
redrawwin(WINDOW *win)
{
    return wredrawln(win, 0, win->maxy+1);
}

int
wredrawln(WINDOW *win, int first, int num)
{
    int j, i;
    for (j = win->scry + first;
         j < win->scry + first + num && j <= disp_win->maxy; j++) {
        if (j >= 0)
            for (i = win->scrx;
                 i <= win->scrx + win->maxx && i <= disp_win->maxx; i++) {
                if (i >= 0) {
                    disp_win->chararray[i + j * win->stride].attr = -1;
                    disp_win->chararray[i + j * win->stride].color_on_screen =
                        (unsigned short)-1;
                }
            }
    }
    return touchline(win, first, num);
}

int
wnoutrefresh(WINDOW *win)
{
    if (!win) {

        if (stdscr || !save_stdscr)
            return ERR;

        win = stdscr = save_stdscr;
        save_stdscr = 0;
        redrawwin(stdscr);
        win->clear_on_refresh = 1;
        uncursed_hook_init(&LINES, &COLS, title);
        uncursed_rhook_setsize(LINES, COLS);
    }

    /* Don't redraw a window that's offscreen. */
    if (win->scry + win->maxy > nout_win->maxy ||
        win->scrx + win->maxx > nout_win->maxx ||
        win->scry < 0 || win->scrx < 0)
        return ERR;

    if (win->clear_on_refresh)
        nout_win->clear_on_refresh = 1;

    win->clear_on_refresh = 0;
    copywin(win, nout_win, 0, 0, win->scry, win->scrx, win->scry + win->maxy,
            win->scrx + win->maxx, 0);

    return wmove(nout_win, win->scry + win->y, win->scrx + win->x);
}

int
doupdate(void)
{
    int i, j;

    if (nout_win->clear_on_refresh) {
        redrawwin(stdscr);
        uncursed_hook_fullredraw();
    }
    nout_win->clear_on_refresh = 0;

    cchar_t *p = nout_win->chararray;
    cchar_t *q = disp_win->chararray;
    void **rp = nout_win->regionarray;
    void **rq = disp_win->regionarray;

    for (j = 0; j <= nout_win->maxy; j++) {
        for (i = 0; i <= nout_win->maxx; i++) {
            int k;

            if (need_noutwin_recolor)
                p->color_on_screen = color_on_screen_for_attr(p->attr);

            if (p->color_on_screen != q->color_on_screen ||
                *rp != *rq || *rq == &invalid_region)
                uncursed_hook_update(j, i);

            for (k = 0; k < CCHARW_MAX; k++) {
                if (p->chars[k] != q->chars[k])
                    uncursed_hook_update(j, i);
                if (p->chars[k] == 0)
                    break;
            }

            p++; q++; rp++; rq++;
        }
    }

    uncursed_hook_positioncursor(nout_win->y, nout_win->x);
    uncursed_hook_flush();
    need_noutwin_recolor = false;
    return OK;
}

void
uncursed_rhook_updated(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return;

    disp_win->chararray[x + y * disp_win->stride] =
        nout_win->chararray[x + y * nout_win->stride];
    disp_win->regionarray[x + y * (disp_win->maxx + 1)] =
        nout_win->regionarray[x + y * (nout_win->maxx + 1)];
}

int
uncursed_rhook_needsupdate(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return 0;

    cchar_t *p = nout_win->chararray + x + y * nout_win->stride;
    cchar_t *q = disp_win->chararray + x + y * disp_win->stride;

    if (p->color_on_screen != q->color_on_screen)
        return 1;

    if (disp_win->regionarray[x + y * (disp_win->maxx + 1)] ==
        &invalid_region)
        return 1;

    if (nout_win->regionarray[x + y * (nout_win->maxx + 1)] !=
        disp_win->regionarray[x + y * (disp_win->maxx + 1)])
        return 1;

    int k;

    for (k = 0; k < CCHARW_MAX; k++) {
        if (p->chars[k] != q->chars[k])
            return 1;
        if (p->chars[k] == 0)
            return 0;
    }

    return 0;
}

void *
uncursed_rhook_region_at(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return 0;

    return nout_win->regionarray[x + y * (nout_win->maxx + 1)];
}

int
uncursed_rhook_color_at(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return 0;

    return nout_win->chararray[x + y * nout_win->stride].color_on_screen;
}

int
uncursed_rhook_mousekey_from_pos(int y, int x, int b)
{
    if (y < 0 || x < 0 || y > nout_win->maxy || x > nout_win->maxx)
        return 0;

    return nout_win->chararray[x + y * nout_win->stride].bindings[b];
}

char
uncursed_rhook_cp437_at(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return 0xa8;

    wchar_t wc = nout_win->chararray[x + y * nout_win->stride].chars[0];
    int i;

    for (i = 0; i < 256; i++)
        if (cp437[i] == wc)
            return i;

    return 0xa8;        /* an upside-down question mark */
}

#ifdef __GNUC__
/* We want to check for Unicode characters outside the UCS-2 range. If the
   system's using UCS-2 for wchar_t, then the compiler will complain that
   we're checking for something impossible. In general, the check is very
   useful, so we disable it just for the next two functions. */
# pragma GCC diagnostic push "-Wtype-limits"
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

unsigned short
uncursed_rhook_ucs2_at(int y, int x)
{
    if (y > nout_win->maxy || x > nout_win->maxx)
        return 0xfffdu;
    wchar_t wc = nout_win->chararray[x + y * nout_win->stride].chars[0];

    if (wc < 65536)
        return (unsigned short)wc;
    return 0xfffdu;     /* REPLACEMENT CHARACTER */
}

static const wchar_t replacement_char_array[CCHARW_MAX] = { 0xfffdu, };

char *
uncursed_rhook_utf8_at(int y, int x)
{
    const wchar_t *c = replacement_char_array;

    if (y <= nout_win->maxy && x <= nout_win->maxx)
        c = nout_win->chararray[x + y * nout_win->stride].chars;

    /* The maximum number of UTF-8 bytes for one codepoint is 4. */
    static char utf8[CCHARW_MAX * 4 + 1];
    char *r = utf8;
    int itercount = 0;

    while (*c != 0) {

        if (*c < 0x80)
            *r++ = *c;
        else if (*c < 0x800) {
            *r++ = 0xc0 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else if (*c < 0x10000) {
            *r++ = 0xe0 | ((*c >> 12) & 0x3f);
            *r++ = 0x80 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else if (*c < 0x110000) {
            *r++ = 0xf0 | ((*c >> 18) & 0x3f);
            *r++ = 0x80 | ((*c >> 12) & 0x3f);
            *r++ = 0x80 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else {
            /* out of Unicode range; send a REPLACEMENT CHARACTER. */
            *r++ = 0xef;
            *r++ = 0xbf;
            *r++ = 0xbd;
        }

        itercount++;
        c++;

        if (itercount == CCHARW_MAX)
            break;
    }

    *r = 0;
    return utf8;
}

#ifdef __GNUC__
# pragma GCC diagnostic pop "-Wtype-limits"
#endif

/* manual page 3ncurses get_wch */
static int pushback_w = 0x110000;
int
unget_wch(wchar_t c)
{
    if (pushback_w < 0x110000)
        return ERR;
    pushback_w = c;
    return OK;
}

int
timeout_get_wch(int timeout, wint_t * rv)
{
    int r;

    if (pushback_w < 0x110000) {
        *rv = pushback_w;
        pushback_w = 0x110000;
        return OK;
    }

    r = uncursed_hook_getkeyorcodepoint(timeout);

    /* When we have multiple possible key codes for certain keys, pick one and
       merge them together. */
    if (r >= 0x110000) {

        r -= KEY_BIAS;

        if (r == KEY_SILENCE)
            return ERR;

        int mods = r & (KEY_SHIFT | KEY_ALT | KEY_CTRL);
        r &= ~mods;

        /* We can actually distinguish numpad Home from main keyboard Home in
           some terminals that don't distinguish numpad PgUp from main keyboard
           PgUp. This is, however, not particularly useful, so we just merge
           them. */
        if (r == (KEY_KEYPAD | 'H'))
            r = KEY_HOME;
        if (r == (KEY_KEYPAD | 'F'))
            r = KEY_END;
        if (r == (KEY_FUNCTION | 15))
            r = KEY_F5;
        if (r == (KEY_KEYPAD | 'E'))
            r = KEY_B2;
        if (r == (KEY_KEYPAD | 'G'))
            r = KEY_B2;
        if (r == (KEY_KEYPAD | 'k'))
            r = KEY_NUMPLUS;

        r |= mods;

        *rv = r;
        return KEY_CODE_YES;
    }

    *rv = r;
    return OK;
}

UNCURSED_ANDMVWINDOWDEF(int,
get_wch, (wint_t * rv), (rv))
{
    return timeout_get_wch(win->timeout, rv);
}

void
uncursed_signal_getch(void)
{
    uncursed_hook_signal_getch();
}

void
uncursed_watch_fd(int fd)
{
    uncursed_hook_watch_fd(fd, 1);
}

void
uncursed_unwatch_fd(int fd)
{
    uncursed_hook_watch_fd(fd, 0);
}

/* manual page 3ncurses getcchar */
int
getcchar(const cchar_t *c, wchar_t * s, attr_t *attr, short *pairnum,
         void *unused)
{
    (void)unused;

    int ccount = 0;
    while (c->chars[ccount] != 0 && ccount != CCHARW_MAX)
        ccount++;

    if (!s)
        return ccount;

    s[ccount] = 0;
    while (ccount--)
        s[ccount] = c->chars[ccount];

    *attr = c->attr;
    *pairnum = PAIR_NUMBER(c->attr);

    return OK;
}

int
setcchar(cchar_t *c, const wchar_t * s, attr_t attr, short pairnum,
         void *unused)
{
    (void)unused;

    int ccount = 0;
    while (s[ccount] != 0 && ccount != CCHARW_MAX) {
        c->chars[ccount] = s[ccount];
        ccount++;
    }

    if (ccount < CCHARW_MAX)
        c->chars[ccount] = 0;

    c->attr = attr & ~(COLOR_PAIR(PAIR_NUMBER(attr)));
    c->attr |= COLOR_PAIR(pairnum);

    return OK;
}

/* manual page 3ncurses getch */
UNCURSED_ANDMVWINDOWVDEF(int,
getch)
{
    wint_t w;

    wrefresh(win);
    int r = wget_wch(win, &w);

    if (r == ERR)
        return ERR;

    if (r == KEY_CODE_YES)
        return (int)w;  /* keypress */

    /* Interpret the input as Latin-1, not CP437. This is to handle the well
       known "meta = add 128 to the character code" rule. (This works both with
       terminals that add 128 in Latin-1/CP437, and terminals that add 128 in
       Unicode for some reason, because the platform-specific code assumes
       Latin-1 on input if it gets unexpected input with the high bit set.) */
    if (w < 256)
        return w;

    return 0x94;        /* CANCEL CHARACTER */
}

int
ungetch(int k)
{
    return unget_wch(k);
}


/* manual page 3ncurses move */
UNCURSED_ANDWINDOWDEF(int,
move, (int y, int x), (y, x))
{
    if (y > win->maxy || x > win->maxx || y < 0 || x < 0)
        return ERR;

    win->y = y;
    win->x = x;

    return OK;
}

/* manual page 3ncurses touch */
int
touchwin(WINDOW *win)
{
    (void)win;
    return OK;
}

int
untouchwin(WINDOW *win)
{
    (void)win;
    return OK;
}

int
touchline(WINDOW *win, int first, int count)
{
    (void)win;
    (void)first;
    (void)count;
    return OK;
}

int
wtouchln(WINDOW *win, int first, int count, int touched)
{
    (void)win;
    (void)first;
    (void)count;
    (void)touched;
    return OK;
}

/* manual page 3ncurses printw */
/* These don't use the UNCURSED_AND*WINDOWDEF helper functions because
   they're varargs. */

int
printw(const char *fmt, ...)
{
    int rv;
    va_list vl;

    va_start(vl, fmt);
    rv = vw_printw(stdscr, fmt, vl);
    va_end(vl);

    return rv;
}

int
wprintw(WINDOW *win, const char *fmt, ...)
{
    int rv;
    va_list vl;

    va_start(vl, fmt);
    rv = vw_printw(win, fmt, vl);
    va_end(vl);

    return rv;
}

int
mvprintw(int y, int x, const char *fmt, ...)
{
    int rv;
    va_list vl;

    va_start(vl, fmt);
    move(y, x);
    rv = vw_printw(stdscr, fmt, vl);
    va_end(vl);

    return rv;
}

int
mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    int rv;
    va_list vl;

    va_start(vl, fmt);
    wmove(win, y, x);
    rv = vw_printw(win, fmt, vl);
    va_end(vl);

    return rv;
}

int
vw_printw(WINDOW *win, const char *fmt, va_list vl)
{
    va_list vl2;
    char *bf = malloc(1);

    if (!bf)
        return ERR;

    /* Count the length of the string. vsnprintf is supposed to return the
       length if it doesn't fit in the buffer, but on some OSes/libcs, it
       doesn't. This loop allows for a vsnprintf that returns negative/0 on an
       overlong string, or one that returns the buffer size on an overlong
       string. */
    int ccount = 1;
    int oldccount = 0;

    while (ccount != oldccount) {
        oldccount = ccount;

        va_copy(vl2, vl);
        *bf = 1;
        ccount = vsnprintf(bf, ccount, fmt, vl2);
        va_end(vl2);

        if (ccount < 0 || (ccount == 0 && *bf))
            ccount = oldccount * 2 + 1;
        else
            ccount = ccount * 2 + 1;

        if (ccount > LINES * COLS) {
            free(bf);
            return ERR; /* sanity */
        }

        bf = realloc(bf, ccount + 1);
        if (!bf)
            return ERR;
    }

    char *r = bf;
    while (*r)
        waddch(win, *r++);

    free(bf);

    return OK;
}

/* manual page 3ncurses scroll */
int
scroll(WINDOW *win)
{
    return wscrl(win, 1);
}

UNCURSED_ANDWINDOWDEF(int,
scrl, (int n), (n))
{
    if (!win->scrollok)
        return OK;

    int y = win->y;

    win->y = 0;
    winsdelln(win, -n);

    win->y = y;

    return OK;
}

/* manual page 3ncurses wresize */
int
wresize(WINDOW *win, int newh, int neww)
{
    if (newh < 1 || neww < 1)
        return ERR;

    /* If a window is resized such that a child is outside the bounds of a
       parent, we permit this, but drawing to the window will cause memory
       corruption until the application rectifies this.  At the moment, we
       merely document the limitation; ideally, we should set some sort of flag
       that drawing commands check and error out on, or even do some sort of
       clipping. */
    if (win->parent) {
        win->maxy = newh - 1;
        win->maxx = neww - 1;
        return 0;
    }

    WINDOW *temp = newwin(newh, neww, win->scry, win->scrx);
    if (!temp)
        return ERR;

    wdelete_tiles_region(win);
    overwrite(win, temp);
    free(win->regionarray);

    cchar_t *old_chararray = win->chararray;
    WINDOW *oldchild = win->child;

    *win = *temp;
    win->child = oldchild;
    free(temp);

    if (win->child) {
        WINDOW *w;

        /* Make the shared memory inside the derived windows point at the main
           window. */
        for (w = win->child; w; w = w->sibling) {
            int offset = w->chararray - old_chararray;

            offset = (offset % w->stride) + (offset / w->stride) * win->stride;
            w->chararray = win->chararray + offset;
            w->stride = win->stride;
        }
    }

    free(old_chararray);
    return OK;
}


void uncursed_reset_palette16 (void)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            if ( h->resetpalette16!=NULL )
                h->resetpalette16();
}

void uncursed_set_palette16 (const uncursed_palette16 *p)
{
    struct uncursed_hooks *h;

    for (h = uncursed_hook_list; h; h = h->next_hook)
        if (h->used)
            if ( h->setpalette16!=NULL )
                h->setpalette16(p);
}
