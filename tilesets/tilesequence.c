/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-23 */
/* Copyright (c) 2013 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

/* Converts between tile numbers and tile names, or between tile numbers and
   API symdef names.

   Most of the information about names is in libnethack/src/drawing.c (which is
   intentionally kept low on dependencies so that it can be linked in safely);
   that file stores the information about what exists in the game (which is
   important to be able to construct a tile sequence for it). It also contains
   the API symdef names, and enough information to be able to construct most of
   the tile names. (The rest of that information is in nethack/src/brandings.c,
   for things that need to be drawn but that libnethack is unaware of.) */

#include "tilesequence.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* One of the various symbol definition arrays in drawing.c. */
struct symdef_array {
    const int offset;
    const struct nh_symdef * const syms;
    const struct nh_symdef * const types; /* can be NULL */
    const int symslen;
    const int typeslen;
};

const struct symdef_array symdef_arrays[] = {
    {TILESEQ_CMAP_OFF,    defsyms,       NULL, TILESEQ_CMAP_SIZE, 0},
    {TILESEQ_TRAP_OFF,    trapsyms,      NULL, TILESEQ_TRAP_SIZE, 0},
    {TILESEQ_WARN_OFF,    warnsyms,      NULL, TILESEQ_WARN_SIZE, 0},
    {TILESEQ_EFFECT_OFF,  effectsyms,    NULL, TILESEQ_EFFECT_SIZE, 0},
    {TILESEQ_SWALLOW_OFF, swallowsyms,   NULL, TILESEQ_SWALLOW_SIZE, 0},
    {TILESEQ_EXPLODE_OFF, explsyms, expltypes, NUMEXPCHARS, EXPL_MAX},
    {TILESEQ_ZAP_OFF,     zapsyms,  zaptypes,  NUMZAPCHARS, NUM_ZAP},
};

/* Find a tile number in an nh_symdef array. */
static int
tileno_name_from_symdef_array(const char *name, const char *type,
                              const struct symdef_array* sa)
{
    int i;
    for (i = 0; i < sa->symslen; i++) {
        if (strcmp(sa->syms[i].symname, name) == 0) {
            if (!sa->types) return i + sa->offset;
            int j;
            for (j = 0; j < sa->typeslen; j++) {
                if (strcmp(sa->types[i].symname, name) == 0)
                    return j * sa->symslen + i + sa->offset;
            }
        }
    }
    return TILESEQ_INVALID_OFF;
}

/* Find a tile number from a monster or object name. (These have the same API
   names and tile names.) */
static int
tileno_from_obj_mon_name(const char *name, int offset)
{
    /* We can just brute-force it. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_MON_OFF) {
        int i;
        for (i = 0; i < TILESEQ_MON_SIZE; i++) {
            if (!strcmp(make_mon_name(i), name))
                return TILESEQ_MON_OFF + i;
        }
    }
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_OBJ_OFF) {
        int i;
        for (i = 0; i < TILESEQ_OBJ_SIZE; i++) {
            if (!strcmp(make_object_name(i), name))
                return TILESEQ_OBJ_OFF + i;
        }
    }
    return TILESEQ_INVALID_OFF;
}
        
/* Find a tile number from the name used for it by the API.
   Explosions and zaps have a name/type pair; otherwise the type is null.
   The offset specifies what sort of tile it is, if known.

   This cannot find a branding, because those don't have API names. */
int
tileno_from_api_name(const char *name, const char *type, int offset)
{
    int i, tn = TILESEQ_INVALID_OFF;
    /* Straightforward case: hardcoded names. */
    for (i = 0; i < (sizeof symdef_arrays / sizeof *symdef_arrays); i++) {
        if (offset == TILESEQ_INVALID_OFF ||
            offset == symdef_arrays[i].offset)
            tn = tileno_name_from_symdef_array(name, type, symdef_arrays+i);
        if (tn != TILESEQ_INVALID_OFF) return tn;
    }
    /* There's only one invisibile monster tile. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_INVIS_OFF) {
        if (strcmp(name, invismonexplain) == 0) return TILESEQ_INVIS_OFF;
    }
    return tileno_from_obj_mon_name(name, offset);
}


/* Find a tile number from the name used for it by a .txt tile file. */
int
tileno_from_name(const char *name, int offset)
{
    int i, j;
    /* There's only one invisibile monster tile. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_INVIS_OFF) {
        if (strcmp(name, invismonexplain) == 0) return TILESEQ_INVIS_OFF;
    }
    /* We have a hardcoded array for traps. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_TRAP_OFF) {
        for (i = 0; i < TILESEQ_TRAP_SIZE; i++)
            if (!strcmp(trapexplain[i], name))
                return TILESEQ_TRAP_OFF + i;
    }
    /* Backgrounds are similar; however, we have to deal with disambiguation
       (there's a lot of "wall" in the defexplain array, for instance; in the
       tilesets, this is "walls 0" .. "walls 10"). We rely on the fact that
       where disambiguation is required, the repeated descriptions are always
       consecutive. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_CMAP_OFF) {
        int consecutive = 0;
        char buf[80];
        for (i = 0; i < TILESEQ_CMAP_SIZE; i++) {
            if (i > 0 && !strcmp(defexplain[i], defexplain[i-1]))
                consecutive++;
            else
                consecutive = 0;

            sprintf(buf, "%ss %d", defexplain[i], consecutive);

            if (!strcmp(buf, name))
                return TILESEQ_CMAP_OFF + i;
            if (!strcmp(defexplain[i], name))
                return TILESEQ_CMAP_OFF + i;
        }
    }
    /* Warnings use the pattern "warning 0" .. "warning 5" */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_WARN_OFF) {
        if (sscanf(name, "warning %d", &i) == 1 &&
            i >= 0 && i < TILESEQ_WARN_SIZE)
            return TILESEQ_WARN_OFF + i;
    }
    /* Zaps use the pattern "zap 1 2", referring to type and name */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_ZAP_OFF) {
        if (sscanf(name, "zap %d %d", &i, &j) == 2 &&
            i >= 0 && i < NUM_ZAP && j >= 0 && j < NUMZAPCHARS)
            return TILESEQ_ZAP_OFF + i * NUMZAPCHARS + j;
    }
    /* Explosions use the pattern "explosion adjective 1" */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_EXPLODE_OFF) {
        char buf[80];
        if (sscanf(name, "explosion %79s %d", buf, &j) == 2 &&
            j >= 0 && j < NUMEXPCHARS) {
            for (i = 0; i < EXPL_MAX; i++) {
                if (!strcmp(expltypes[i].symname, name))
                    return TILESEQ_EXPLODE_OFF + i * NUMEXPCHARS + j;
            }
        }
    }
    /* Effects, swallowing use custom names from branding.c */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_EFFECT_OFF) {
        for (i = 0; i < TILESEQ_EFFECT_SIZE; i++)
            if (!strcmp(nhcurses_effect_names[i], name))
                return TILESEQ_EFFECT_OFF + i;        
    }
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_SWALLOW_OFF) {
        for (i = 0; i < TILESEQ_SWALLOW_SIZE; i++)
            if (!strcmp(nhcurses_swallow_names[i], name))
                return TILESEQ_SWALLOW_OFF + i;        
    }
    /* Brandings also have custom names, stored in a different way. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_GENBRAND_OFF) {
        for (i = 0; i < TILESEQ_GENBRAND_SIZE; i++)
            if (!strcmp(nhcurses_branding_names[
                            i + nhcurses_genbranding_first], name))
                return TILESEQ_GENBRAND_OFF + i;        
    }
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_MONBRAND_OFF) {
        for (i = 0; i < TILESEQ_MONBRAND_SIZE; i++)
            if (!strcmp(nhcurses_branding_names[
                            i + nhcurses_monbranding_first], name))
                return TILESEQ_MONBRAND_OFF + i;        
    }
    /* Objects and monsters use their API names. */
    return tileno_from_obj_mon_name(name, offset);
}

/* Returns a pointer to a static buffer. */
static const char *
name_from_tileno_internal(int tileno)
{
    static char rv[80];
    int i;
    i = tileno - TILESEQ_INVIS_OFF;
    if (i >= 0 && i < TILESEQ_INVIS_SIZE) return invismonexplain;
    i = tileno - TILESEQ_TRAP_OFF;
    if (i >= 0 && i < TILESEQ_TRAP_SIZE) return trapexplain[i];
    i = tileno - TILESEQ_CMAP_OFF;
    if (i >= 0 && i < TILESEQ_CMAP_SIZE) {
        if ((i == 0 || strcmp(defexplain[i], defexplain[i-1]) != 0) &&
            (i == TILESEQ_CMAP_SIZE - 1 ||
             strcmp(defexplain[i], defexplain[i+1]) != 0))
            return defexplain[i];
        int j = 0;
        while (i > 0 && !strcmp(defexplain[i], defexplain[i-1])) {
            i--;
            j++;
        }
        snprintf(rv, 79, "%ss %d", defexplain[i], j);
        return rv;
    }
    i = tileno - TILESEQ_WARN_OFF;
    if (i >= 0 && i < TILESEQ_WARN_SIZE) {
        snprintf(rv, 79, "warning %d", i);
        return rv;
    }
    i = tileno - TILESEQ_ZAP_OFF;
    if (i >= 0 && i < TILESEQ_ZAP_SIZE) {
        snprintf(rv, 79, "zap %d %d", i / NUMZAPCHARS, i % NUMZAPCHARS);
        return rv;
    }
    i = tileno - TILESEQ_EXPLODE_OFF;
    if (i >= 0 && i < TILESEQ_EXPLODE_SIZE) {
        snprintf(rv, 79, "explosion %s %d",
                 expltypes[i / NUMEXPCHARS].symname, i % NUMEXPCHARS);
        return rv;
    }
    i = tileno - TILESEQ_EFFECT_OFF;
    if (i >= 0 && i < TILESEQ_EFFECT_SIZE) return nhcurses_effect_names[i];
    i = tileno - TILESEQ_SWALLOW_OFF;
    if (i >= 0 && i < TILESEQ_SWALLOW_SIZE) return nhcurses_swallow_names[i];
    i = tileno - TILESEQ_GENBRAND_OFF;
    if (i >= 0 && i < TILESEQ_GENBRAND_SIZE)
        return nhcurses_branding_names[i + nhcurses_genbranding_first];
    i = tileno - TILESEQ_MONBRAND_OFF;
    if (i >= 0 && i < TILESEQ_MONBRAND_SIZE)
        return nhcurses_branding_names[i + nhcurses_monbranding_first];
    i = tileno - TILESEQ_OBJ_OFF;
    if (i >= 0 && i < TILESEQ_OBJ_SIZE)
        return make_object_name(i);
    i = tileno - TILESEQ_MON_OFF;
    if (i >= 0 && i < TILESEQ_MON_SIZE)
        return make_mon_name(i);
    return NULL;
}

/* Test that the above routines work; tilemap.c calls name_from_tileno on
   every integer in range, make it noisly error out if it doesn't round-trip
   correctly. This routine isn't called from elsewhere, so the test won't
   crash a running program. */
const char *
name_from_tileno(int tileno) {
    const char *rv = name_from_tileno_internal(tileno);
    if (!rv) return rv;
    int tileno2 = tileno_from_name(rv, TILESEQ_INVALID_OFF);
    if (tileno != tileno2) {
        fprintf(stderr, "Tile roundtrip failure: %d -> \"%s\" -> %d\n",
                tileno, rv, tileno2);
        abort();
    }
    return rv;
}
