/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-12 */
/* Copyright (c) Dean Luick, with acknowledgements to Kevin Darcy */
/* and Dave Cohrs, 1990.                                          */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DISPLAY_H
# define DISPLAY_H

# include "vision.h"

# ifndef INVISIBLE_OBJECTS
#  define vobj_at(x,y) (level->objects[x][y])
# endif


# define dbuf_monid(mon, xx, yy, rng) \
    (what_mon(monsndx(mon->data), xx, yy, rng) + 1)
# define dbuf_objid(obj, xx, yy, rng) (what_obj(obj->otyp, xx, yy, rng) + 1)
# define dbuf_effect(type, id) ((type << 16) | (id + 1))
# define dbuf_explosion(etype, id) \
    ((E_EXPLOSION << 16) | (etype * NUMEXPCHARS + id + 1))

/* The various ways in which a monster/player can sense a monster/player. These
   are the return values for msensem (mondata.c), and the methods listed by
   mon_vision_summary (pager.c).

   These do not take displacement into account (which doesn't prevent a monster
   sensing another, just misleads it as to which square it's on).

   When updating this, update MSENSE_ANYDETECT and MSENSE_ANYVISION if
   necessary, and mon_vision_summary in pager.c. */

/* Normal vision. Requires LOE, that the target is either on a lit square or
   adjacent, and that the target is not invisible, hiding, buried or
   (if the source is not underwater) underwater.*/
# define MSENSE_VISION        0x00000001u

/* See invisible. Requires LOE, and that the target is on a lit square or
   adjacent or (if the source has infravision) infravisible. The target must not
   be hiding, buried, or (if the source is not underwater) underwater. */
# define MSENSE_SEEINVIS      0x00000002u

/* Infravision. Requires LOE, that the target is infravisible, and that the
   source has infravision. The target must not be invisible, hiding, buried, or
   (if the source is not underwater) underwater. */
# define MSENSE_INFRAVISION   0x00000004u

/* Telepathy. Requires that the target is on the same level, and that the source
   is conscious and telepathic. The target must not be mindless. If the source
   is not blind, there's a range limit (of BOLT_LIM, or 0 if the telepathy is
   intrinsic). */
# define MSENSE_TELEPATHY     0x00000008u

/* Astral vision. Requires that the target is either on a lit square or adjacent,
   that the target is not invisible, hiding, or buried, and that the target is
   within the source's astral vision range. */
# define MSENSE_XRAY          0x00000010u

/* Monster detection. Requires that the source has monster detection and that the
   target is on the same level. */
# define MSENSE_MONDETECT     0x00000020u

/* Warning vs. monster class, a property. Requires that the source has the
   property and the target is on the same level and has the chosen monster
   class. (At present, this is only granted by certain artifacts.) */
# define MSENSE_WARNOFMON     0x00000040u

/* Covetous sense. Requires that the source is covetous for an item that the
   target is holding and the target is on the same level. */
# define MSENSE_COVETOUS      0x00000080u

/* Smell of gold. Implemented for xorns sensing the player in 3.4.3; the
   behaviour is preserved and symmetrised for 4.3. */
# define MSENSE_GOLDSMELL     0x00000100u

/* Scent. Works over a radius-root-5 range, and requires that the source and
   target are either both underwater or both out of water. The source must have
   the M3_SCENT flag. TODO: make stinking clouds block this. */
# define MSENSE_SCENT         0x00000200u


# define MSENSE_ANYVISION     (MSENSE_VISION | MSENSE_INFRAVISION |  \
                               MSENSE_SEEINVIS | MSENSE_XRAY)
# define MSENSE_ANYDETECT     (MSENSE_TELEPATHY | MSENSE_MONDETECT | \
                               MSENSE_WARNOFMON | MSENSE_COVETOUS |  \
                               MSENSE_GOLDSMELL | MSENSE_SCENT)

/* Flags that alert us to a monster's existence, but not full details. */

/* Worm segment visible. This allows us to know a long worm is there, without
   knowing its exact location. The rules are the same as for normal vision,
   except that we're iterating over segments. In other words, MSENSE_VISION (or
   infravision, etc.). */
# define MSENSE_WORM          0x00100000u

/* Warning. This lets us know the location, but not the exact monster. */
# define MSENSE_WARNING       0x00200000u

/* Item/furniture mimic visible. The target can be sensed, but doesn't look like
   a monster. (This replaces the normal "vision" senses.) */
# define MSENSE_ITEMMIMIC     0x00400000u

/* Other flags calculated at the same time as visibility. Be careful only to set
   these when the target is sensed via some means, so that the result from
   msensem can be interpreted as a boolean. */

/* The target is sensed, and known to be invisible (because normal vision can
   see the square, but not the monster at that location). */
# define MSENSEF_KNOWNINVIS   0x10000000u

/* Various msensem() wrappers. */

/* seen specifically by telepathy; you usually don't need this */
# define tp_sensemon(mon)     !!(msensem(&youmonst, (mon)) & MSENSE_TELEPATHY)
/* seen by any non-vision means (possibly also via vision) */
# define sensemon(mon)        !!(msensem(&youmonst, (mon)) & MSENSE_ANYDETECT)
/* monster /head/ seen via vision: attacking monsters, monsters using items */
# define mon_visible(mon)     !!(msensem(&youmonst, (mon)) & MSENSE_ANYVISION)
/* /any part of the monster/ seen via vision; general purpose; not mimics */
# define canseemon(mon)       !!(msensem(&youmonst, (mon)) & \
                                 (MSENSE_ANYVISION | MSENSE_WORM))
/* monster seen via vision, including mimicking mimics */
# define canseemonoritem(mon) !!(msensem(&youmonst, (mon)) & \
                                 (MSENSE_ANYVISION | MSENSE_WORM | \
                                  MSENSE_ITEMMIMIC))
/* monster seen or sensed via any means; mimicking mimics are included if sensed
   via telepathy/monster detection/etc., but not if sensed via vision */
# define canspotmon(mon)      !!(msensem(&youmonst, (mon)) & \
                                 (MSENSE_ANYDETECT | MSENSE_ANYVISION))
/* monster sensed or seen via any means, including mimicking mimics */
# define canspotmonoritem(mon)!!(msensem(&youmonst, (mon)) & \
                                 (MSENSE_ANYDETECT | MSENSE_ANYVISION | \
                                  MSENSE_ITEMMIMIC))
/* player has at least a minimal idea of monster species; used when naming
   monsters (in cases where we're naming an item-form mimic, either it's in the
   process of waking up or the player just discovered what it was); when
   hallucinating, returns whether the player would know what it was if they
   weren't hallucinating */
# define canclassifymon(mon)  !!(msensem(&youmonst, (mon)) &            \
                                 (MSENSE_ANYDETECT | MSENSE_ANYVISION | \
                                  MSENSE_ITEMMIMIC | MSENSE_WORM))
/* player has at least a minimal idea of monster existence; used to determine
   whether the player is aware of the presence of monster creation magic; this
   doesn't detect item-form mimics (because those don't look like monsters);
   also used to determine whether the player is aware that a monster has died or
   left the square */
# define cansuspectmon(mon)   !!(msensem(&youmonst, (mon)) &            \
                                 (MSENSE_ANYDETECT | MSENSE_ANYVISION | \
                                  MSENSE_WARNING | MSENSE_WORM))


/* the player can see that the monster is invisible (either seeing it via see
   invis, or because the player can see the square it's on, and can sense the
   monster via an ANYDETECT method, but can't see the monster via ANYVISION) */
# define knowninvisible(mon)  !!(msensem(&youmonst, (mon)) & MSENSEF_KNOWNINVIS)

# define m_canseeu(mon)       !!(msensem((mon), &youmonst) & MSENSE_ANYVISION)
# define m_cansenseu(mon)     !!(msensem((mon), &youmonst))

/* not quite the same as the above, but it can do with centralizing; only use
   on the current level; assumes that long worms aren't infravisible */
# define knownwormtail(x, y)  (cansee((x), (y)) &&                 \
                               MON_AT(level, (x), (y)) &&          \
                               (m_at(level, (x), (y))->mx != x ||  \
                                m_at(level, (x), (y))->my != y) && \
                               (!m_at(level, (x), (y))->minvis ||  \
                                See_invisible))

/*
 * canseeself()
 * senseself()
 *
 * This returns true if the hero can see her/himself.
 *
 * The Engulfed check assumes that you can see yourself even if you are
 * invisible.  If not, then we don't need the check.
 */
# define canseeself()   (Engulfed || msensem(&youmonst, &youmonst) & \
                         MSENSE_ANYVISION)
# define senseself()    (Engulfed || msensem(&youmonst, &youmonst) & \
                         (MSENSE_ANYVISION | MSENSE_ANYDETECT))

/*
 * random_monster()
 * random_object()
 * random_trap()
 *
 * Respectively return a random monster, object, or trap number.
 */
# define random_monster(rng) (rng(NUMMONS))
# define random_object(rng)  (rng(NUM_OBJECTS-1) + 1)
# define random_trap(rng)    (rng(TRAPNUM-1) + 1)

/*
 * what_obj()
 * what_mon()
 * what_trap()
 *
 * If hallucinating, choose an object/monster/trap based on character memory if
 * possible, or the given RNG otherwise; otherwise, just return the value
 * truthfully. Suitable RNGs include rn2_on_display_rng and rn2; newsym_rng will
 * choose a suitable RNG depending on whether the program is in a zero-time
 * command or not.
 *
 * The given coordinates can be set out of range to force a random result when
 * hallucinating. This should only be done if the random result has no effect
 * but on messages.
 *
 * These functions generally only work on the current level, unless the
 * coordinates are out of bounds, in which case they work on the current level
 * and also during level creation.
 */
# define what_obj(obj, xx, yy, rng)  \
    (Hallucination ? isok(xx, yy) && level->locations[xx][yy].mem_obj ? \
     level->locations[xx][yy].mem_obj - 1 : random_object(rng) : obj)
# define what_mon(mon, xx, yy, rng)  (Hallucination ? random_monster(rng) : mon)
# define what_trap(trp, xx, yy, rng) \
    (Hallucination ? isok(xx, yy) && level->locations[xx][yy].mem_trap ? \
     level->locations[xx][yy].mem_trap : random_trap(rng) : trp)

# define newsym_rng(xx) \
    (program_state.in_zero_time_command ? rn2_on_display_rng(xx) : rn2(xx))

/*
 * covers_objects()
 * covers_traps()
 *
 * These routines are true if what is really at the given location will
 * "cover" any objects or traps that might be there.
 */
# define covers_objects(lev,xx,yy) \
    ((is_pool(lev,xx,yy) && !Underwater) || \
     (lev->locations[xx][yy].typ == LAVAPOOL))

# define covers_traps(lev,xx,yy)        covers_objects(lev,xx,yy)

/*
 * tmp_at() display styles
 */
# define DISP_BEAM    (-1)     /* Keep all symbols showing & clean up at end. */
# define DISP_FLASH   (-2)     /* Clean up each symbol before displaying new
                                  one. */
# define DISP_ALWAYS  (-3)     /* Like flash, but still displayed if not
                                  visible. */
# define DISP_OBJECT  (-4)     /* Like flash, but shows an object instead of
                                  an effect symbol */

/* Macros for explosion types */
/* Moved from hack.h, because the tileset code needs access to it */
# define EXPL_DARK        0
# define EXPL_NOXIOUS     1
# define EXPL_MUDDY       2
# define EXPL_WET         3
# define EXPL_MAGICAL     4
# define EXPL_FIERY       5
# define EXPL_FROSTY      6
# define EXPL_MAX         7

/* Total number of cmap indices in the shield_static[] array. */
# define SHIELD_COUNT 21
# define NUM_ZAP 8      /* number of zap beam types */

/* Used for temporary display symbols. */
struct tmp_sym;

extern const unsigned char seenv_matrix[3][3];

#endif /* DISPLAY_H */

