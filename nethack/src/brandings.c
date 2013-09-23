/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-23 */
/* Copyright (c) 2013 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

/* Lists names used by tiles ports for things that need to be drawn (and thus
   need names so that the tiles system can refer to them), but for which there
   are no textual names used by the rest of the code. Things like the little
   heart symbol that hilite_pet uses to show that a monster is tame. (Versions
   before NetHack 4 used horrible special-casing to deal with that
   situation.) */

#include "brandings.h"

#include <stdlib.h>

/* NetHack 4 is a modern variant, so I'm going to use C99 if it's clearer.
   Which it is. It's 2013, compilers should be able to handle a 14-year-old
   language by now. */
const char *nhcurses_branding_names[(int)nhcurses_branding_count] = {
    [nhcurses_genbranding_stepped] = "remembered as stepped on",
    [nhcurses_genbranding_locked] = "remembered as locked",
    [nhcurses_genbranding_unlocked] = "remembered as unlocked",
    [nhcurses_genbranding_trapped] = "remembered as trapped",

    [nhcurses_monbranding_tame] = "monster is tame",
    [nhcurses_monbranding_peaceful] = "monster is peaceful",
    [nhcurses_monbranding_detected] = "monster is detected",
    [nhcurses_monbranding_ridden] = "monster is ridden",
};

/* The names which level display modes are given in the tilesets. Some of these
   are arbitrary, but most of them are taken from substitutions in existing
   tilesets for NetHack 3 or Slash'EM. Changing these names will break tilemap
   compatibility; please don't do that. Adding new names is OK, though. */
const char *nhcurses_ldm_names[LDM_COUNT] = {
    [LDM_DEFAULT] = "default",
    [LDM_HELL] = "gehennom",
    [LDM_QUEST] = "quest",
    [LDM_MINES] = "mine", /* /not/ "mines"; existing tilesets use "mine" */
    [LDM_SOKOBAN] = "sokoban",
    [LDM_ROGUE] = "rogue",
    [LDM_KNOX] = "knox",
};

/* Some things can't be farlooked, or give the name of something else when
   farlooked, and so aren't named in drawing.c. Give them names here. */
const char *nhcurses_effect_names[E_COUNT] = {
    "dig beam",
    "flash beam",
    "thrown boomerang, open left",
    "thrown boomerang, open right",
    "magic shield 1",
    "magic shield 2",
    "magic shield 3",
    "magic shield 4",
    "gas cloud",
};
const char *nhcurses_swallow_names[NUMSWALLOWCHARS] = {
    "swallow top left",
    "swallow top center",
    "swallow top right",
    "swallow middle left",
    "swallow middle right",
    "swallow bottom left",
    "swallow bottom center",
    "swallow bottom right"
};
