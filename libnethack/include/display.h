/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-18 */
/* Copyright (c) Dean Luick, with acknowledgements to Kevin Darcy */
/* and Dave Cohrs, 1990.                                          */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DISPLAY_H
# define DISPLAY_H

# include "vision.h"
# include "mondata.h"   /* for mindless() */

# ifndef INVISIBLE_OBJECTS
#  define vobj_at(x,y) (level->objects[x][y])
# endif


# define dbuf_monid(mon, xx, yy, rng) (what_mon(mon->mnum, xx, yy, rng) + 1)
# define dbuf_objid(obj, xx, yy, rng) (what_obj(obj->otyp, xx, yy, rng) + 1)
# define dbuf_effect(type, id) ((type << 16) | (id + 1))
# define dbuf_explosion(etype, id) \
    ((E_EXPLOSION << 16) | (etype * NUMEXPCHARS + id + 1))


/*
 * sensemon()
 *
 * Returns true if the hero can sense the given monster.  This includes
 * monsters that are hiding or mimicing other monsters.
 */
# define tp_sensemon(mon) (     /* The hero can always sense a monster IF:  */\
    (onmap(mon))           &&   /* 1. the monster is actually present  AND  */\
    (mon->dlevel == level) &&   /* 2. the monster is on the same level AND  */\
    (!mindless(mon->data)) &&   /* 3. the monster has a brain to sense AND  */\
    (!u_helpless(hm_unconscious)) && /* 4. the hero is conscious AND        */\
      ((Blind && Blind_telepat) || /* 5a. hero is blind and telepathic OR   */\
                                /* 5b. hero is using a telepathy inducing   */\
                                /*       object and in range                */\
       (Unblind_telepat &&                                              \
        (distu(mon->mx, mon->my) <= (BOLT_LIM * BOLT_LIM))))            \
        )

# define sensemon(mon) (mon->dlevel == level && \
                       (tp_sensemon(mon) || Detect_monsters || \
                        MATCH_WARN_OF_MON(mon)))

/*
 * mon_warning() is used to warn of any dangerous monsters in your
 * vicinity, and a glyph representing the warning level is displayed.
 */

# define mon_warning(mon) (Warning && !(mon)->mpeaceful &&              \
                           (distu((mon)->mx, (mon)->my) < 100) &&       \
                           (((int) ((mon)->m_lev / 4)) >= 1))

/*
 * mon_visible()
 *
 * Returns true if the hero can see the monster.  It is assumed that the
 * hero can physically see the location of the monster.  The function
 * vobj_at() returns a pointer to an object that the hero can see there.
 * Infravision is not taken into account.
 */
# define mon_visible(mon) (             /* The hero can see the monster     */\
                                        /* IF the monster                   */\
    (!mon->minvis || See_invisible) &&  /* 1. is not invisible AND          */\
    (!mon->mundetected) &&              /* 2. not an undetected hider       */\
    (!(mon->mburied || u.uburied))      /* 3. neither you or it is buried   */\
)

/*
 * see_with_infrared()
 *
 * This function is true if the player can see a monster using infravision.
 * The caller must check for invisibility (invisible monsters are also
 * invisible to infravision), because this is usually called from within
 * canseemon() or canspotmon() which already check that.
 */
# define see_with_infrared(mon) (!Blind && Infravision && \
                                 infravisible(mon->data) && \
                                 couldsee(mon->mx, mon->my))


/*
 * canseemon()
 *
 * This is the globally used canseemon().  It is not called within the display
 * routines.  Like mon_visible(), but it checks to see if the hero sees the
 * location instead of assuming it.  (And also considers worms.)
 */
# define canseemon(mon) \
    (onmap(mon) && mon->dlevel == level && \
     (mon->wormno ? worm_known(mon) : \
         (cansee(mon->mx, mon->my) || see_with_infrared(mon))) \
     && mon_visible(mon))


/*
 * canspotmon(mon)
 *
 * This function checks whether you can either see a monster or sense it by
 * telepathy, and is what you usually call for monsters about which nothing is
 * known.
 */
# define canspotmon(mon) (onmap(mon) && (canseemon(mon) || sensemon(mon)))

/* knowninvisible(mon)
 * This one checks to see if you know a monster is both there and invisible.
 * 1) If you can see the monster and have see invisible, it is assumed the
 * monster is transparent, but visible in some manner.
 * 2) If you can't see the monster, but can see its location and you have
 * telepathy that works when you can see, you can tell that there is a
 * creature in an apparently empty spot.
 * Infravision is not relevant; we assume that invisible monsters are also
 * invisible to infravision.
 */
# define knowninvisible(mon) \
    (onmap(mon) && (mon)->minvis && \
        ((cansee(mon->mx, mon->my) && (See_invisible || Detect_monsters)) || \
        (!Blind && (HTelepat & ~INTRINSIC) && \
            distu(mon->mx, mon->my) <= (BOLT_LIM * BOLT_LIM) \
        ) \
        ) \
    )

/*
 * is_safepet(mon)
 *
 * A special case check used in attack() and domove().  Placing the
 * definition here is convenient.
 */
# define is_safepet(mon, uim)                           \
    ((mon) && (mon)->mtame && canspotmon(mon) &&        \
     ((uim) == uim_pacifist || (uim) == uim_standard || \
      (uim) == uim_displace) &&                         \
     !Confusion && !Hallucination && !Stunned)


/*
 * canseeself()
 * senseself()
 *
 * This returns true if the hero can see her/himself.
 *
 * The Engulfed check assumes that you can see yourself even if you are
 * invisible.  If not, then we don't need the check.
 */
# define canseeself()   (Blind || Engulfed || (!Invisible && !u.uundetected))
# define senseself()    (canseeself() || Unblind_telepat || Detect_monsters)

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
 * truthfully. Suitable RNGs include display_rng and rn2; newsym_rng will choose
 * a suitable RNG depending on whether the program is in a zero-time command or
 * not.
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
     level->locations[xx][yy].mem_obj : random_object(rng) : obj)
# define what_mon(mon, xx, yy, rng)  (Hallucination ? random_monster(rng) : mon)
# define what_trap(trp, xx, yy, rng) \
    (Hallucination ? isok(xx, yy) && level->locations[xx][yy].mem_trap ? \
     level->locations[xx][yy].mem_trap : random_trap(rng) : trp)

# define newsym_rng(xx) \
    (program_state.in_zero_time_command ? display_rng(xx) : rn2(xx))

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

#endif /* DISPLAY_H */

