/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-02 */
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
#include "hacklib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

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

static const char *name_from_tileno_internal(int tileno);

/* Returns TRUE if the given set of substitutions could never occur. */
int
mutually_exclusive_substitutions(unsigned long long substitution)
{
    static unsigned long long impossible_pairings[] = {
        NHCURSES_SUB_LIT | NHCURSES_SUB_UNLIT,
        NHCURSES_SUB_CORPSE | NHCURSES_SUB_STATUE | NHCURSES_SUB_FIGURINE,
        NHCURSES_SUB_LDM(LDM_COUNT) - 1,
        NHCURSES_SUB_LDM(LDM_ROLE_0) -
        NHCURSES_SUB_LDM(LDM_ROLE_0 + LDM_ROLE_COUNT),
        NHCURSES_SUB_LDM(LDM_RACE_0) -
        NHCURSES_SUB_LDM(LDM_RACE_0 + LDM_RACE_COUNT),
        NHCURSES_SUB_LDM(LDM_GENDER_0) -
        NHCURSES_SUB_LDM(LDM_GENDER_0 + LDM_GENDER_COUNT),
    };

    int i;
    for (i = 0; i < SIZE(impossible_pairings); i++) {
        unsigned long long s = substitution & impossible_pairings[i];
        /* We need at most one bit to be set in s. */
        if (popcount(s) > 1)
            return 1;
    }

    return 0;
}

/* Returns a bitmask of substitutions that can reasonably apply to the given
   tile number. This is only used on wildcard matches, and limits what the
   wildcard can match, in order to keep the size of the generated tilesets down,
   and in order to avoid "sub corpse *" overriding the furthest backgrounds. */
unsigned long long
sensible_substitutions(int tileno)
{
    /* The current rule is: corpse/statue/figurine and race/gender apply only to
       monsters; everything else applies to all tiles. */
    if (tileno >= TILESEQ_MON_OFF &&
        tileno < TILESEQ_MON_OFF + TILESEQ_MON_SIZE)
        return (1ULL << (LDM_RACE_0 + 5)) - 1;
    else
        return (1ULL << LDM_GENDER_0) - 1 -
            NHCURSES_SUB_CORPSE - NHCURSES_SUB_STATUE - NHCURSES_SUB_FIGURINE;
}

unsigned long long
substitution_from_name(const char **name)
{
    unsigned long long substitutions = 0;
    int i;
    /* Remove any substitution prefixes. */
    while (!strncmp(*name, "sub ", strlen("sub "))) {
        int found = 0;
        *name += strlen("sub ");
        for (i = 0; nhcurses_sub_names[i]; i++) {
            int len = strlen(nhcurses_sub_names[i]);
            if (strncmp(*name, nhcurses_sub_names[i], len) == 0 &&
                (*name)[len] == ' ') {
                *name += strlen(nhcurses_sub_names[i]) + 1;
                substitutions |= 1ULL << i;
                found = 1;
                break;
            }
        }
        if (!found) {
            /* invalid substitution; just leave the "sub " prefix there so we
               get an invalid name error later; this codepath also implements
               the "sub *" case */
            *name -= strlen("sub ");
            break;
        }
    }
    return substitutions;
}

const char *
name_from_substitution(unsigned long long substitution)
{
    static char *buffer = NULL;
    static int buflen = 0;
    int bufptr = 0;
    int i;

    if (!substitution)
        return "";

    if (buflen)
        *buffer = '\0';

    for (i = 0; nhcurses_sub_names[i]; i++) {
        if (substitution & (1ULL << i)) {
            int newlen = bufptr + (sizeof "sub  ") +
                strlen(nhcurses_sub_names[i]);
            if (newlen > buflen) {
                buffer = realloc(buffer, newlen);
                if (!buflen)
                    *buffer = '\0';
                buflen = newlen;
            }
            strcat(buffer, "sub ");
            strcat(buffer, nhcurses_sub_names[i]);
            strcat(buffer, " ");
            bufptr = newlen - 1;
        }
    }

    return buffer;
}

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
                if (strcmp(sa->types[j].symname, type) == 0)
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
        if (tn != TILESEQ_INVALID_OFF)
            return tn;
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

    /* There's only one invisible monster tile. */
    if (offset == TILESEQ_INVALID_OFF || offset == TILESEQ_INVIS_OFF) {
        if (strcmp(name, invismonexplain) == 0)
            return TILESEQ_INVIS_OFF;
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

            snprintf(buf, sizeof(buf), "%ss %d", defexplain[i], consecutive);

            /* Should we be using the disambiguated or original name?
               Disambiguate if there's the same name either immediately before
               (already calculated in 'consecutive') or immediately after. */
            if ((i < TILESEQ_CMAP_SIZE - 1 &&
                 !strcmp(defexplain[i], defexplain[i+1])) || consecutive) {
                if (!strcmp(buf, name))
                    return TILESEQ_CMAP_OFF + i;
            } else {
                if (!strcmp(defexplain[i], name))
                    return TILESEQ_CMAP_OFF + i;
            }
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
                if (!strcmp(expltypes[i].symname, buf))
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

/* Look up the symdef rendering for a given tile, from drawing.c.

   This may be a pointer to a static buffer in some cases. */
static const struct nh_symdef *
symdef_from_tileno(int tileno)
{
    int i;
    static struct nh_symdef temp_symdef;

    /* cmap, trap, warn, effect, swallow, explode, zap */
    for (i = 0; i < (sizeof symdef_arrays / sizeof *symdef_arrays); i++) {
        int sym_offset = tileno - symdef_arrays[i].offset;
        int symslen = symdef_arrays[i].symslen;
        if (!symdef_arrays[i].types) {
            if (sym_offset >= 0 && sym_offset < symslen)
                return symdef_arrays[i].syms + sym_offset;
        } else {
            int typeslen = symdef_arrays[i].typeslen;
            if (sym_offset >= 0 && sym_offset < symslen * typeslen) {
                int type_offset = sym_offset / symslen;
                sym_offset %= symslen;
                temp_symdef.ch = symdef_arrays[i].syms[sym_offset].ch;
                temp_symdef.color = symdef_arrays[i].types[type_offset].color;
                temp_symdef.symname = NULL;
                return &temp_symdef;
            }
        }
    }

    /* monster */
    i = tileno - TILESEQ_MON_OFF;
    if (i >= 0 && i < TILESEQ_MON_SIZE) {
        populate_mon_symdef(i, &temp_symdef);
        return &temp_symdef;
    }

    /* object */
    i = tileno - TILESEQ_OBJ_OFF;
    if (i >= 0 && i < TILESEQ_OBJ_SIZE) {
        populate_obj_symdef(i, &temp_symdef);
        return &temp_symdef;
    }

    /* This leaves three possibilities, invis, genbrand and monbrand. The
       details of genbrand and monbrand are considered to not be libnethack's
       problem, so it doesn't suggest renderings (or even know they exist). The
       invisible monster symbol is known by core, but it's not present in any of
       the symdef arrays (it's historically been special-cased), and as it's
       only the one symbol, that's left to the client too.

       In all these cases, we return NULL, meaning that the tile won't be added
       to a generated cchar tileset. Those tiles will need to be given
       renderings manually. */
    return NULL;
}

unsigned long
cchar_from_tileno(int tileno)
{
    const struct nh_symdef *symdef = symdef_from_tileno(tileno);
    if (!symdef)
        return ULONG_MAX;

    /* Pack into a cchar as follows:

       .ch: bottom 7 bits (ASCII matches Unicode there)

       color & 15: bits 21 and up (we use nethack_types' color codes)
       HI_ULINE: penultimate bit (30)

       We set "copy underlining" (bit 30 clear and 31 set) if outputting
       anything other than a monster or cmap, because only monsters and cmaps
       have underlining rules of their own. (This allows underlining to be used
       for a branding, if a tileset happens to want that.) This is overridden by
       ascii_overrides.txt in the case of furthest backgrounds (which can't copy
       anything).

       Background color is copied, so we set bits 26 to 29 to have a value of
       8. Bits 7-20 (Unicode) and 25 (copy foreground) are clear. */

    unsigned long rv = 8UL << 26;
    rv |= (unsigned char)symdef->ch;
    rv |= (((int)symdef->color) & 15) << 21;

    if ((tileno < TILESEQ_MON_OFF ||
         tileno >= TILESEQ_MON_OFF + TILESEQ_MON_SIZE) &&
        (tileno < TILESEQ_CMAP_OFF ||
         tileno >= TILESEQ_CMAP_OFF + TILESEQ_CMAP_SIZE))
        rv |= 2UL << 30;
    else if (symdef->color & HI_ULINE)
        rv |= 1UL << 30;

    return rv;
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
    char rvcopy[strlen(rv) + 1];
    strcpy(rvcopy, rv);
    int tileno2 = tileno_from_name(rv, TILESEQ_INVALID_OFF);
    if (tileno != tileno2) {
        fprintf(stderr, "Tile roundtrip failure: %d -> \"%s\" -> %d\n",
                tileno, rvcopy, tileno2);
        abort();
    }
    return name_from_tileno_internal(tileno);
}
