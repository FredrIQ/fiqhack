/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-23 */
/* Copyright (c) 2013 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef BRANDINGS_H
# define BRANDINGS_H

# include "nethack_types.h"
# include "global.h"
# include "align.h"
# include "timeout.h"
# include "monsym.h"
# include "objclass.h"
# include "dungeon.h"
# include "decl.h"
# include "mkroom.h"
# include "rm.h"

/* Brandings are extra images shown on tiles to give specific information that
   the player might care about, e.g. "this monster is peaceful" or "this square
   has been stepped on". Unlike most tiles, which correspond to something that
   exists, with a name, in libnethack, brandings are passed as a bitfield rather
   than an index into a list of names. Brandings are just shown using solid
   backgrounds in the tty ports (or on occasion, different foregrounds), but for
   tiles, we might have to superimpose lots of brandings on top of each
   other. Thus, they all need numbers (in this file), and names (in
   brandings.c); the tiles handling code needs to know both, whereas the tiles
   rendering code only needs to know about the numbers (because the names aren't
   known to libnethack).

   Note that the list is /not/ the same as the lists in nethack_types.h; that
   list lists what the character can know, this list lists what is shown to the
   player. So it's a different sort of presentation. (For instance, warning
   symbols have a branding-like flag to show that they're not monsters; this is
   useful information to the code, and the character knows that they're not
   monsters, but nonetheless we don't have a display branding for them: the tile
   contains all the required information already, so an extra branding would be
   pointless.) */

enum nhcurses_brandings {
    /* general brandings */
    nhcurses_genbranding_first,
    nhcurses_genbranding_stepped = nhcurses_genbranding_first,
    nhcurses_genbranding_locked,
    nhcurses_genbranding_unlocked,
    nhcurses_genbranding_last,
    nhcurses_genbranding_trapped = nhcurses_genbranding_last,
    /* monster brandings */
    nhcurses_monbranding_first,
    nhcurses_monbranding_tame = nhcurses_monbranding_first,
    nhcurses_monbranding_peaceful,
    nhcurses_monbranding_detected,
    nhcurses_monbranding_last,
    nhcurses_monbranding_ridden = nhcurses_monbranding_last,

    nhcurses_branding_count
};

# define NHCURSES_GENBRANDING_COUNT \
    ((int)nhcurses_genbranding_last - (int)nhcurses_genbranding_first + 1)

# define NHCURSES_MONBRANDING_COUNT \
    ((int)nhcurses_monbranding_last - (int)nhcurses_monbranding_first + 1)

extern const char *nhcurses_branding_names[(int)nhcurses_branding_count];

/* Tiles ports also care about the level display modes, which substitute some
   tiles. These also have names, which need to be accessible to the tiles
   port. */
extern const char *nhcurses_ldm_names[LDM_COUNT];

/* Effect symbols don't have names in libnethack (because they can't be
   farlooked), but tilesequence needs names for them. Likewise, engulfing
   uses the name of the monster, but the tiles look different. */
extern const char *nhcurses_effect_names[E_COUNT];
extern const char *nhcurses_swallow_names[NUMSWALLOWCHARS];

#endif
