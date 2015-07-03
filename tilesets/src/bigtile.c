/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/*   Copyright (c) NetHack Development Team 1995                    */
/*   NetHack may be freely redistributed.  See license for details. */

#error !AIMAKE_FAIL_SILENTLY!
This is currently unused, and needs updating to the new tile system.

/*
 * Create a set of pseudo 3D tiles by centering monsters/objects/furniture
 * and slanting floors. See Mitsuhiro Itakura's instructions at:
 * http://www.geocities.co.jp/SiliconValley-SanJose/9606/nh/3d/index.html
 */

#include "config.h"
#include "tile.h"
#include "tilesequence.h"

/* Some tiles need slants; other tiles should not be slanted.  In most cases,
   we can distinguish via the tile offset:

   generic branding      slanted
   object                not slanted
   invisible monster     not slanted
   monster               not slanted
   monster branding      not slanted
   warning symbol        not slanted
   explosion             slanted, mid air
   zap                   slanted, mid air
   engulfer              slanted, mid air

   However, some categories (backgrounds, traps, effects) have a mix of slanted
   and nonslanted; these objects should be slanted if they lie along the ground,
   but not if they stand on the ground.  For backgrounds and effects, this can
   be done via a hardcoded list (backgrounds not on the list lie on the ground,
   effects not on the list are in mid air); for traps, this depends on artistic
   choices made when drawing the tile.  This is worked around by using a list of
   traps that happen to stand up from the ground in the 32x32 tileset, but this
   is not a general solution and is rather prone to breaking.  (The "correct"
   fix, of course, is to draw original 3D-effect art for every tile rather than
   upscaling, so that this file is completely unnecessary; however, that would
   take more art skills than I have.) */

static const char *override_no_slant[] = {
    "iron bars",
    "tree",
    "staircase up",
    "staircase down",
    "ladder up",
    "ladder down",
    "long staircase up",
    "long staircase down",
    "altar",
    "opulent throne",
    "sink",
    "fountain",
    "squeaky board",
    "bear trap",
    "land mine",
    "rust trap",
    "fire trap",
    "magic portal",
    "grave",
    "thrown boomerang, open left",
    "thrown boomerang, open right",
    "magic shield 1",
    "magic shield 2",
    "magic shield 3",
    "magic shield 4",
};

struct tile_offset_group {
    int first;
    int past_last;
    int slanted;
    int raised;
};
struct tile_offset_group tile_offset_groups[] = {
    {TILESEQ_CMAP_OFF,    TILESEQ_CMAP_OFF    + TILESEQ_CMAP_SIZE,     1, 0},
    {TILESEQ_GENBRAND_OFF,TILESEQ_GENBRAND_OFF+ TILESEQ_GENBRAND_SIZE, 1, 0},
    {TILESEQ_TRAP_OFF,    TILESEQ_TRAP_OFF    + TILESEQ_TRAP_SIZE,     1, 0},
    {TILESEQ_OBJ_OFF,     TILESEQ_OBJ_OFF     + TILESEQ_OBJ_SIZE,      0, 0},
    {TILESEQ_INVIS_OFF,   TILESEQ_INVIS_OFF   + TILESEQ_INVIS_SIZE,    0, 0},
    {TILESEQ_MON_OFF,     TILESEQ_MON_OFF     + TILESEQ_MON_SIZE,      0, 0},
    {TILESEQ_MONBRAND_OFF,TILESEQ_MONBRAND_OFF+ TILESEQ_MONBRAND_SIZE, 0, 0},
    {TILESEQ_WARN_OFF,    TILESEQ_WARN_OFF    + TILESEQ_WARN_SIZE,     0, 0},
    {TILESEQ_EXPLODE_OFF, TILESEQ_EXPLODE_OFF + TILESEQ_EXPLODE_SIZE,  1, 1},
    {TILESEQ_ZAP_OFF,     TILESEQ_ZAP_OFF     + TILESEQ_ZAP_SIZE,      1, 1},
    {TILESEQ_SWALLOW_OFF, TILESEQ_SWALLOW_OFF + TILESEQ_SWALLOW_SIZE,  1, 1},
    {TILESEQ_EFFECT_OFF,  TILESEQ_EFFECT_OFF  + TILESEQ_EFFECT_SIZE,   1, 1},
};

void
embiggen_tile_in_place(pixel(*pixels)[MAX_TILE_X], const char *name,
                       int *max_x, int *max_y)
{
    int i, j, tileno, slanted = -1, raised = -1;
    pixel bigpixels[MAX_TILE_Y][MAX_TILE_X];
    const pixel default_background = DEFAULT_BACKGROUND;

    if (*max_x * 3 / 2 > MAX_TILE_X || *max_y * 2 > MAX_TILE_Y) {
        fprintf(stderr, "error: embiggening tile makes it too large\n");
        exit(EXIT_FAILURE);
    }

    /* Work out which set of offsets this tile belongs to. */
    tileno = tileno_from_name(name, TILESEQ_INVALID_OFF);
    if (tileno == TILESEQ_INVALID_OFF) {
        fprintf(stderr, "error: cannot embiggen unknown tile '%s'\n", name);
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < SIZE(tile_offset_groups); i++) {
        if (tileno >= tile_offset_groups[i].first &&
            tileno < tile_offset_groups[i].past_last) {
            slanted = tile_offset_groups[i].slanted;
            raised = tile_offset_groups[i].raised;
        }
    }
    for (i = 0; i < SIZE(override_no_slant); i++) {
        if (!strcmp(name, override_no_slant[i])) {
            slanted = 0;
            raised = 0;
        }
    }
    if (slanted == -1 || raised == -1) {
        fprintf(stderr, "internal error: tile number %d has unknown offset\n",
                tileno);
        exit(EXIT_FAILURE);
    }

    if (!slanted) {
        for (j = 0; j < 2 * *max_y; j++)
            for (i = 0; i < 3 * *max_x / 2; i++)
                bigpixels[j][i] = default_background;
        for (j = 0; j < *max_y; j++)
            for (i = 0; i < *max_x; i++)
                bigpixels[j + *max_y / 2][i + *max_x / 4] = pixels[j][i];
    } else {
        for (j = 0; j < 2 * *max_y; j++)
            for (i = 0; i < 3 * *max_x / 2; i++)
                bigpixels[j][i] = default_background;
        for (j = 0; j < *max_y; j++)
            for (i = 0; i < *max_x; i++)
                bigpixels[j + *max_y - (raised ? 14 : 0)]
                    [i + *max_x / 2 - j / 2] = pixels[j][i];
    }

    *max_x = *max_x * 3 / 2;
    *max_y *= 2;
    memcpy(pixels, bigpixels, sizeof bigpixels);
}

/*bigtile.c*/
