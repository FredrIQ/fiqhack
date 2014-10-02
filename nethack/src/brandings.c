/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-02 */
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
const char *const nhcurses_branding_names[(int)nhcurses_branding_count] = {
    [nhcurses_genbranding_stepped] = "remembered as stepped on",
    [nhcurses_genbranding_locked] = "remembered as locked",
    [nhcurses_genbranding_unlocked] = "remembered as unlocked",
    [nhcurses_genbranding_trapped] = "remembered as trapped",

    [nhcurses_monbranding_tame] = "monster is tame",
    [nhcurses_monbranding_peaceful] = "monster is peaceful",
    [nhcurses_monbranding_detected] = "monster is detected",
    [nhcurses_monbranding_ridden] = "monster is ridden",
};

/* The names of substitutions, as seen in a .txt tile file. The names in the
   .txt file are compiled to bitflags in the .tiledesc file, and this is the
   map between them. */
const char *const nhcurses_sub_names[] = {

    /* Names of level display modes. */
    [LDM_DEFAULT] = "default",
    [LDM_HELL] = "gehennom",
    [LDM_QUESTHOME] = "questhome",
    [LDM_MINES] = "mine", /* /not/ "mines"; existing tilesets use "mine" */
    [LDM_SOKOBAN] = "sokoban",
    [LDM_ROGUE] = "rogue",
    [LDM_KNOX] = "knox",
    [LDM_QUESTFILL1] = "questfill1",
    [LDM_QUESTLOCATE] = "questlocate",
    [LDM_QUESTFILL2] = "questfill2",
    [LDM_QUESTGOAL] = "questgoal",

    /* These two are special cases. */
    [LDM_COUNT +  0] = "lit",
    [LDM_COUNT +  1] = "unlit",

    /* These represent /quests/, and so affect terrain. This gives some
       awkwardness with gender-specific player-monster names, so we truncate to
       the first three characters (which works in English,
       *cav*eman / *cav*ewoman, *pri*est / *pri*estess). */
    [LDM_COUNT +  2] = "arc",
    [LDM_COUNT +  3] = "bar",
    [LDM_COUNT +  4] = "cav",
    [LDM_COUNT +  5] = "hea",
    [LDM_COUNT +  6] = "kni",
    [LDM_COUNT +  7] = "mon",
    [LDM_COUNT +  8] = "pri",
    [LDM_COUNT +  9] = "ran",
    [LDM_COUNT + 10] = "rog",
    [LDM_COUNT + 11] = "sam",
    [LDM_COUNT + 12] = "tou",
    [LDM_COUNT + 13] = "val",
    [LDM_COUNT + 14] = "wiz",

    /* Whereas these represent personal attributes, and so affect monsters.
       Currently this is only implemented for the player, but, e.g. "sub male
       mountain nymph" is certainly meaningful and may be implemented in the
       future.

       There is no particular need for these to be in the same order as in
       libnethack; the libnethack internals should be invisible to the tiles
       system. */
    [LDM_COUNT + 15] = "female",
    [LDM_COUNT + 16] = "male",

    [LDM_COUNT + 17] = "human",
    [LDM_COUNT + 18] = "gnome",
    [LDM_COUNT + 19] = "elf",
    [LDM_COUNT + 20] = "orc",
    [LDM_COUNT + 21] = "dwarf",

    [LDM_COUNT + 22] = NULL /* fencepost */
};

static_assert((sizeof nhcurses_sub_names) / (sizeof *nhcurses_sub_names) < 64,
              "overflow in substitution tiles bitfield");

/* Some things can't be farlooked, or give the name of something else when
   farlooked, and so aren't named in drawing.c. Give them names here. */
const char *const nhcurses_effect_names[E_COUNT] = {
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
const char *const nhcurses_swallow_names[NUMSWALLOWCHARS] = {
    "swallow top left",
    "swallow top center",
    "swallow top right",
    "swallow middle left",
    "swallow middle right",
    "swallow bottom left",
    "swallow bottom center",
    "swallow bottom right"
};
