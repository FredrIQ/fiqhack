/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-02 */
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

    nhcurses_branding_count,
    nhcurses_no_branding = nhcurses_branding_count
};

# define NHCURSES_GENBRANDING_COUNT \
    ((int)nhcurses_genbranding_last - (int)nhcurses_genbranding_first + 1)

# define NHCURSES_MONBRANDING_COUNT \
    ((int)nhcurses_monbranding_last - (int)nhcurses_monbranding_first + 1)

extern const char *const nhcurses_branding_names[(int)nhcurses_branding_count];

/* Substitution tiles are sort-of like brandings, but are separately drawn
   images rather than alpha-blended, and might not exist in any given tileset.
   These are a bitmask, because it is possible to have a substitution tile for,
   say, female orc wizards ("sub female sub orc wizard" in the .txt file file).
   These brandings are listed in precedence order; although it is unwise for a
   tilset to rely on this, because "orc" is listed later than "female", a female
   orc wizard will be drawn as an orc wizard rather than a female wizard if both
   of those tiles exist, but no female orc wizard tile exists.

   Substitution tiles are not present in situations where they'd match the base
   tile. For instance, many existing tilesets have a human-looking tile for
   their non-race-specific character tiles. Thus, they would not have a separate
   human tile for the same character.

   When changing these, change the list in brandings.c, and
   sensible_substitutions in tilesequence.c. */

/* All LDMs define substitutions. */
#define NHCURSES_SUB_LDM(ldm) (1ULL << (ldm))

/* Lit and unlit statuses are substitution tiles. */
#define NHCURSES_SUB_LIT      (1ULL << (LDM_COUNT + 0))
#define NHCURSES_SUB_UNLIT    (1ULL << (LDM_COUNT + 1))

/* Corpses/statues/figurines are substitutions of the matching monster. */
#define NHCURSES_SUB_CORPSE   (1ULL << (LDM_COUNT + 2))
#define NHCURSES_SUB_STATUE   (1ULL << (LDM_COUNT + 3))
#define NHCURSES_SUB_FIGURINE (1ULL << (LDM_COUNT + 4))

#define LDM_ROLE_0            (LDM_COUNT + 5)
#define LDM_ROLE_COUNT        13
#define LDM_GENDER_0          (LDM_ROLE_0 + LDM_ROLE_COUNT)
#define LDM_GENDER_COUNT      2
#define LDM_RACE_0            (LDM_GENDER_0 + LDM_GENDER_COUNT)
#define LDM_RACE_COUNT        5

/* A NULL-terminated list of substitution names. */
extern const char *const nhcurses_sub_names[];

/* The remaining substitutions are role, race and gender names (the role names
   to specify which Quest is being used, the race and gender names to change the
   sprites of player-monsters and monsters generally, respectively). */

/* Effect symbols don't have names in libnethack (because they can't be
   farlooked), but tilesequence needs names for them. Likewise, engulfing
   uses the name of the monster, but the tiles look different. */
extern const char *const nhcurses_effect_names[E_COUNT];
extern const char *const nhcurses_swallow_names[NUMSWALLOWCHARS];

#endif
