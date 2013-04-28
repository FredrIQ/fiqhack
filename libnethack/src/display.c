/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Dean Luick, with acknowledgements to Kevin Darcy */
/* and Dave Cohrs, 1990.                                          */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *                      THE DISPLAY CODE
 *
 * The old display code has been broken up into three parts: vision, display,
 * and drawing.  Vision decides what locations can and cannot be physically
 * seen by the hero.  Display decides _what_ is displayed at a given location.
 * Drawing decides _how_ to draw a monster, fountain, sword, etc.
 *
 * The display system uses information from the vision system to decide
 * what to draw at a given location.  The routines for the vision system
 * can be found in vision.c and vision.h.  The routines for display can
 * be found in this file (display.c) and display.h.  The drawing routines
 * are part of the window port.  See doc/window.doc for the drawing
 * interface.
 *
 * What is seen on the screen is a combination of what the hero remembers
 * and what the hero currently sees.  Objects and dungeon features (walls
 * doors, etc) are remembered when out of sight.  Monsters and temporary
 * effects are not remembered.
 * Specifically, the hero memory for each location has a mem_<foo>
 * entry for background, traps, objects and invisible moster markers
 *
 * In most circumstances the entire hero memory is copied into the display
 * buffer (dbuf) together with monsters and effects and passed to the window
 * port for display. Exceptions are: {object,monster} detection, underwater
 * and swallowed display.
 *
 * Here is a list of the major routines in this file to be used externally:
 *
 * newsym
 *
 * Possibly update the screen location (x,y).  This is the workhorse routine.
 * It is always correct --- where correct means following the in-sight/out-
 * of-sight rules.  **Most of the code should use this routine.**  This
 * routine updates the map and displays monsters.
 *
 *
 * map_background
 * map_object
 * map_trap
 * map_invisible
 * unmap_object
 *
 * If you absolutely must override the in-sight/out-of-sight rules, there
 * are two possibilities.  First, you can mess with vision to force the
 * location in sight then use newsym(), or you can  use the map_* routines.
 * The first has not been tried [no need] and the second is used in the
 * detect routines --- detect object, magic mapping, etc.  The map_*
 * routines *change* what the hero remembers.  All changes made by these
 * routines will be sticky --- they will survive screen redraws.  Do *not*
 * use these for things that only temporarily change the screen.  These
 * routines are also used directly by newsym().  unmap_object is used to
 * clear a remembered object when/if detection reveals it isn't there.
 *
 *
 * dbuf_set
 * dbuf_set_<foo>
 *
 * This is direct (no processing in between) buffered access to the screen.
 * Temporary screen effects are run through this and its companion,
 * flush_screen().  There is yet a lower level routine, print_glyph(),
 * but this is unbuffered and graphic dependent (i.e. it must be surrounded
 * by graphic set-up and tear-down routines).  Do not use print_glyph().
 *
 *
 * see_monsters
 * see_objects
 * see_traps
 *
 * These are only used when something affects all of the monsters or
 * objects or traps.  For objects and traps, the only thing is hallucination.
 * For monsters, there are hallucination and changing from/to blindness, etc.
 *
 *
 * tmpsym_init
 * tmpsym_change
 * tmpsym_at
 * tmpsym_end
 *
 * This is a useful interface for displaying temporary items on the screen.
 * Its interface is different than previously, so look at it carefully.
 *
 *
 *
 * Parts of the rm structure that are used:
 *
 *      typ     - What (dungeon feature) is really there.
 *      mem_*   - What the hero remembers.
 *      lit     - True if the position is lit.  An optimization for
 *                lit/unlit rooms.
 *      waslit  - True if the position was *remembered* as lit.
 *      seenv   - A vector of bits representing the directions from which the
 *                hero has seen this position.  The vector's primary use is
 *                determining how walls are seen.  E.g. a wall sometimes looks
 *                like stone on one side, but is seen as a wall from the other.
 *                Other uses are for unmapping detected objects and felt
 *                locations, where we need to know if the hero has ever
 *                seen the location.
 *      flags   - Additional information for the typ field.  Different for
 *                each typ.
 *      horizontal - Indicates whether the wall or door is horizontal or
 *                   vertical.
 */
#include "hack.h"
#include "region.h"

static void display_monster(xchar, xchar, struct monst *, int, xchar);
static int swallow_to_effect(int, int);
static void display_warning(struct monst *);

static int check_pos(struct level *lev, int, int, int);
static int set_twall(struct level *lev, int, int, int, int, int, int, int, int);
static int set_wall(struct level *lev, int, int, int);
static int set_corn(struct level *lev, int, int, int, int, int, int, int, int);
static int set_crosswall(struct level *lev, int, int);
static void set_seenv(struct rm *, int, int, int, int);
static void t_warn(struct rm *);
static int wall_angle(struct rm *);
static void dbuf_set_object(int x, int y, int oid, int omn);
static void dbuf_set_loc(int x, int y);

static boolean delay_flushing;

#ifdef INVISIBLE_OBJECTS
/*
 * vobj_at()
 *
 * Returns a pointer to an object if the hero can see an object at the
 * given location.  This takes care of invisible objects.  NOTE, this
 * assumes that the hero is not blind and on top of the object pile.
 * It does NOT take into account that the location is out of sight, or,
 * say, one can see blessed, etc.
 */
struct obj *
vobj_at(xchar x, xchar y)
{
    struct obj *obj = level->objects[x][y];

    while (obj) {
        if (!obj->oinvis || See_invisible)
            return obj;
        obj = obj->nexthere;
    }
    return NULL;
}
#endif /* else vobj_at() is defined in display.h */

/*
 * magic_map_background()
 *
 * This function is similar to map_background (see below) except we pay
 * attention to and correct unexplored, lit ROOM and CORR spots.
 */
void
magic_map_background(xchar x, xchar y, int show)
{
    int cmap = back_to_cmap(level, x, y);       /* assumes hero can see x,y */
    struct rm *loc = &level->locations[x][y];
    struct rm tmp_location;

    if (!level->flags.hero_memory) {
        tmp_location = *loc;
        loc = &tmp_location;
    }

    /* 
     * Correct for out of sight lit corridors and rooms that the hero
     * doesn't remember as lit.
     */
    if (!cansee(x, y) && !loc->waslit) {
        /* Floor spaces are dark if unlit.  Corridors are dark if unlit. */
        if (loc->typ == ROOM && cmap == S_room)
            cmap = S_darkroom;
        else if (loc->typ == CORR && cmap == S_litcorr)
            cmap = S_corr;
    }
    loc->mem_bg = cmap;
    if (cmap == S_vodoor || cmap == S_hodoor || cmap == S_vcdoor ||
        cmap == S_hcdoor) {
        loc->mem_door_l = 1;
        loc->mem_door_t = 1;
    } else {
        /* mem_door_l, mem_door_t must be 0 for non-doors */
        loc->mem_door_l = 0;
        loc->mem_door_t = 0;
    }

    if (show)
        dbuf_set(x, y, cmap, 0, 0, 0, 0, 0, 0, 0, dbuf_branding(loc));
}

/* FIXME: some of these use xchars for x and y, and some use ints.  Make
 * this consistent.
 */

/*
 * map_background()
 *
 * Make the real background part of our map.  This routine assumes that
 * the hero can physically see the location.  Update the screen if directed.
 */
void
map_background(xchar x, xchar y, int show)
{
    int cmap = back_to_cmap(level, x, y);
    struct rm *loc = &level->locations[x][y];
    struct rm tmp_location;

    if (!level->flags.hero_memory) {
        tmp_location = *loc;
        loc = &tmp_location;
    }

    loc->mem_bg = cmap;
    if (cmap == S_vodoor || cmap == S_hodoor || cmap == S_vcdoor ||
        cmap == S_hcdoor) {
        /* leave memory alone, it'll be 0 if this wasn't remembered as a door */
    } else {
        /* mem_door_l, mem_door_t must be 0 for non-doors */
        loc->mem_door_l = 0;
        loc->mem_door_t = 0;
    }

    if (show)
        dbuf_set(x, y, cmap, 0, 0, 0, 0, 0, 0, 0, dbuf_branding(loc));

}

/*
 * map_trap()
 *
 * Map the trap and print it out if directed.  This routine assumes that the
 * hero can physically see the location.
 */
void
map_trap(struct trap *trap, int show)
{
    int x = trap->tx, y = trap->ty;
    int trapid = what_trap(trap->ttyp);
    struct rm *loc = &level->locations[x][y];
    struct rm tmp_location;

    if (!level->flags.hero_memory) {
        tmp_location = *loc;
        loc = &tmp_location;
    }

    loc->mem_trap = trapid;
    if (show)
        dbuf_set(x, y, loc->mem_bg, loc->mem_trap, 0, 0, 0, 0, 0, 0,
                 dbuf_branding(loc));
}

/*
 * map_object()
 *
 * Map the given object.  This routine assumes that the hero can physically
 * see the location of the object.  Update the screen if directed.
 */
void
map_object(struct obj *obj, int show)
{
    int x = obj->ox, y = obj->oy;
    int objtyp = what_obj(obj->otyp);
    int monnum = 0;
    struct rm *loc = &level->locations[x][y];
    struct rm tmp_location;

    if (!level->flags.hero_memory) {
        tmp_location = *loc;
        loc = &tmp_location;
    }

    if (objtyp == CORPSE || objtyp == STATUE || objtyp == FIGURINE) {
        if (Hallucination)
            monnum = random_monster();
        else
            monnum = obj->corpsenm;
    }

    loc->mem_obj = objtyp + 1;
    loc->mem_obj_mn = monnum + 1;

    if (show)
        dbuf_set(x, y, loc->mem_bg, loc->mem_trap, loc->mem_obj,
                 loc->mem_obj_mn, 0, 0, 0, 0, dbuf_branding(loc));
}

/*
 * map_invisible()
 *
 * Make the hero remember that a square contains an invisible monster.
 * This is a special case in that the square will continue to be displayed
 * this way even when the hero is close enough to see it.  To get rid of
 * this and display the square's actual contents, use unmap_object() followed
 * by newsym() if necessary.
 */
void
map_invisible(xchar x, xchar y)
{
    if (x != u.ux || y != u.uy) {       /* don't display I at hero's location */
        if (level->flags.hero_memory)
            level->locations[x][y].mem_invis = 1;
        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 1, 0, 0, 0,
                 dbuf_branding(&level->locations[x][y]));
    }
}

/*
 * unmap_object()
 *
 * Remove something from the map when the hero realizes it's not there any
 * more.  Replace it with background or known trap, but not with any other
 * If this is used for detection, a full screen update is imminent anyway;
 * if this is used to get rid of an invisible monster notation, we might have
 * to call newsym().
 */
void
unmap_object(int x, int y)
{
    if (!level->flags.hero_memory)
        return;

    level->locations[x][y].mem_invis = 0;
    level->locations[x][y].mem_obj = 0;
    level->locations[x][y].mem_obj_mn = 0;
}


/*
 * map_location()
 *
 * Make whatever at this location show up.  This is only for non-living
 * things.  This will not handle feeling invisible objects correctly.
 * Trapped doors and chests are not covered either
 */
void
map_location(int x, int y, int show)
{
    struct obj *obj;
    struct trap *trap;

    if (level->flags.hero_memory) {
        if ((obj = vobj_at(x, y)) && !covers_objects(level, x, y))
            map_object(obj, FALSE);
        else
            level->locations[x][y].mem_obj_mn = level->locations[x][y].mem_obj =
                0;
        if ((trap = t_at(level, x, y)) && trap->tseen &&
            !covers_traps(level, x, y))
            map_trap(trap, FALSE);
        else
            level->locations[x][y].mem_trap = 0;
        map_background(x, y, FALSE);
        if (show)
            dbuf_set_loc(x, y);
    } else if ((obj = vobj_at(x, y)) && !covers_objects(level, x, y))
        map_object(obj, show);
    else if ((trap = t_at(level, x, y)) && trap->tseen &&
             !covers_traps(level, x, y))
        map_trap(trap, show);
    else
        map_background(x, y, show);
}


void
clear_memory_glyph(schar x, schar y, int to)
{
    level->locations[x][y].mem_bg = to;
    level->locations[x][y].mem_trap = 0;
    level->locations[x][y].mem_obj = 0;
    level->locations[x][y].mem_obj_mn = 0;
    level->locations[x][y].mem_invis = 0;
    level->locations[x][y].mem_stepped = 0;
    level->locations[x][y].mem_door_l = 0;
    level->locations[x][y].mem_door_t = 0;
}


#define DETECTED          2
#define PHYSICALLY_SEEN   1
#define is_worm_tail(mon) ((mon) && ((x != (mon)->mx)  || (y != (mon)->my)))

/*
 * display_monster()
 *
 * Note that this is *not* a map_XXXX() function!  Monsters sort of float
 * above everything.
 *
 */
static void
display_monster(xchar x, xchar y,       /* display position */
                struct monst *mon,      /* monster to display */
                int sightflags, /* 1 if the monster is physically seen */
                /* 2 if detected using Detect_monsters */
                xchar worm_tail)
{       /* mon is actually a worm tail */
    boolean mon_mimic = (mon->m_ap_type != M_AP_NOTHING);
    int sensed = mon_mimic && (Protection_from_shape_changers || sensemon(mon));

    /* 
     * We must do the mimic check first.  If the mimic is mimicing something,
     * and the location is in sight, we have to change the hero's memory
     * so that when the position is out of sight, the hero remembers what
     * the mimic was mimicing.
     */
    if (mon_mimic && (sightflags == PHYSICALLY_SEEN)) {
        switch (mon->m_ap_type) {
        default:
            impossible("display_monster:  bad m_ap_type value [ = %d ]",
                       (int)mon->m_ap_type);

        case M_AP_FURNITURE:{
                /* 
                 * This is a poor man's version of map_background().  I can't
                 * use map_background() because we are overriding what is in
                 * the 'typ' field.
                 *
                 * mappearance is currently set to an S_ index value in
                 * makemon.c.
                 */
                level->locations[x][y].mem_bg = mon->mappearance;
                /* cannot correctly remember a mimic's locked/trapped status */
                level->locations[x][y].mem_door_l = 0;
                level->locations[x][y].mem_door_t = 0;
                if (!sensed)
                    dbuf_set_loc(x, y);
                break;
            }

        case M_AP_OBJECT:{
                struct obj obj; /* Make a fake object to send */

                /* to map_object().  */
                obj.ox = x;
                obj.oy = y;
                obj.otyp = mon->mappearance;
                obj.corpsenm = PM_TENGU;        /* if mimicing a corpse */
                map_object(&obj, !sensed);
                break;
            }

        case M_AP_MONSTER:
            dbuf_set(x, y, level->locations[x][y].mem_bg,
                     level->locations[x][y].mem_trap,
                     level->locations[x][y].mem_obj,
                     level->locations[x][y].mem_obj_mn, 0,
                     what_mon((int)mon->mappearance) + 1,
                     mon->mtame ? MON_TAME : mon->mpeaceful ? MON_PEACEFUL : 0,
                     0, dbuf_branding(&level->locations[x][y]));
            break;
        }

    }

    /* If the mimic is unsucessfully mimicing something, display the monster */
    if (!mon_mimic || sensed) {
        int monnum;
        int mflag = 0;

        if (worm_tail)
            monnum = PM_LONG_WORM_TAIL;
        else
            monnum = monsndx(mon->data);

        if (sightflags == DETECTED)
            mflag |= MON_DETECTED;

        if (mon->mtame && !Hallucination)
            mflag |= MON_TAME;
        else if (mon->mpeaceful && !Hallucination)
            mflag |= MON_PEACEFUL;

        /* If the hero can see a monster at this location, then no longer
           remember the 'invisible' state. */
        level->locations[x][y].mem_invis = 0;

        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 0, what_mon(monnum) + 1,
                 mflag, 0, dbuf_branding(&level->locations[x][y]));
    }
}

/*
 * display_warning()
 *
 * This is also *not* a map_XXXX() function!  Monster warnings float
 * above everything just like monsters do, but only if the monster
 * is not showing.
 *
 * Do not call for worm tails.
 */
static void
display_warning(struct monst *mon)
{
    int x = mon->mx, y = mon->my;
    int wl = (int)(mon->m_lev / 4);
    int monnum, mflag;

    if (mon_warning(mon)) {
        if (wl > WARNCOUNT - 1)
            wl = WARNCOUNT - 1;
        /* 3.4.1: this really ought to be rn2(WARNCOUNT), but value "0" isn't
           handled correctly by the what_is routine so avoid it */
        if (Hallucination)
            wl = rn1(WARNCOUNT - 1, 1);
        monnum = 1 + NUMMONS + wl;
        mflag = MON_WARNING;
    } else if (MATCH_WARN_OF_MON(mon)) {
        monnum = dbuf_monid(mon);
        mflag = 0;
    } else {
        impossible("display_warning did not match warning type?");
        return;
    }

    dbuf_set(x, y, level->locations[x][y].mem_bg,
             level->locations[x][y].mem_trap, level->locations[x][y].mem_obj,
             level->locations[x][y].mem_obj_mn, 0, monnum, mflag, 0,
             dbuf_branding(&level->locations[x][y]));
}

/*
 * feel_location()
 *
 * Feel the given location.  This assumes that the hero is blind and that
 * the given position is either the hero's or one of the eight squares
 * adjacent to the hero (except for a boulder push).
 * If an invisible monster has gone away, that will be discovered.  If an
 * invisible monster has appeared, this will _not_ be discovered since
 * searching only finds one monster per turn so we must check that separately.
 */
void
feel_location(xchar x, xchar y)
{
    struct rm *loc = &(level->locations[x][y]);
    struct obj *boulder;
    struct monst *mon;

    /* If the hero's memory of an invisible monster is accurate, we want to
       keep him from detecting the same monster over and over again on each
       turn. We must return (so we don't erase the monster).  (We must also, in 
       the search function, be sure to skip over previously detected 'I's.) */
    if (level->locations[x][y].mem_invis && m_at(level, x, y))
        return;

    /* The hero can't feel non pool locations while under water. */
    if (Underwater && !Is_waterlevel(&u.uz) && !is_pool(level, x, y))
        return;

    /* If it passed the above check, then there should not be an invisible
       monster marker at this location, so clear it now. */
    level->locations[x][y].mem_invis = 0;

    /* Set the seen vector as if the hero had seen it.  It doesn't matter */
    /* if the hero is levitating or not.  */
    set_seenv(loc, u.ux, u.uy, x, y);

    if (Levitation && !Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz)) {
        /* 
         * Levitation Rules.  It is assumed that the hero can feel the state
         * of the walls around herself and can tell if she is in a corridor,
         * room, or doorway.  Boulders are felt because they are large enough.
         * Anything else is unknown because the hero can't reach the ground.
         * This makes things difficult.
         *
         * Check (and display) in order:
         *
         *      + Stone, walls, and closed doors.
         *      + Boulders.  [see a boulder before a doorway]
         *      + Doors.
         *      + Room/water positions
         *      + Everything else (hallways!)
         */
        if (IS_ROCK(loc->typ) ||
            (IS_DOOR(loc->typ) && (loc->doormask & (D_LOCKED | D_CLOSED)))) {
            map_background(x, y, 1);
        } else if ((boulder = sobj_at(BOULDER, level, x, y)) != 0) {
            map_object(boulder, 1);
        } else if (IS_DOOR(loc->typ)) {
            map_background(x, y, 1);
        } else if (IS_ROOM(loc->typ) || IS_POOL(loc->typ)) {
            /* 
             * An open room or water location.  Normally we wouldn't touch
             * this, but we have to get rid of remembered boulder symbols.
             * This will only occur in rare occations when the hero goes
             * blind and doesn't find a boulder where expected (something
             * came along and picked it up).  We know that there is not a
             * boulder at this location.  Show fountains, pools, etc.
             * underneath if already seen.  Otherwise, show the appropriate
             * floor symbol.
             *
             * Similarly, if the hero digs a hole in a wall or feels a location
             * that used to contain an unseen monster.  In these cases,
             * there's no reason to assume anything was underneath, so
             * just show the appropriate floor symbol.  If something was
             * embedded in the wall, the glyph will probably already
             * reflect that.  Don't change the symbol in this case.
             *
             * This isn't quite correct.  If the boulder was on top of some
             * other objects they should be seen once the boulder is removed.
             * However, we have no way of knowing that what is there now
             * was there then.  So we let the hero have a lapse of memory.
             * We could also just display what is currently on the top of the
             * object stack (if anything).
             */
            if (loc->mem_obj - 1 == BOULDER) {
                if (loc->typ != ROOM && loc->seenv) {
                    map_background(x, y, 1);
                } else {
                    loc->mem_bg = loc->waslit ? S_room : S_darkroom;
                    loc->mem_door_l = 0;
                    loc->mem_door_t = 0;
                    dbuf_set_loc(x, y);
                }
            } else if (loc->mem_bg < S_room || loc->mem_invis) {
                loc->mem_bg = loc->waslit ? S_room : S_darkroom;
                loc->mem_door_l = 0;
                loc->mem_door_t = 0;
                dbuf_set_loc(x, y);
            }
        } else {
            /* We feel it (I think hallways are the only things left). */
            map_background(x, y, 1);
            /* Corridors are never felt as lit (unless remembered that way) */
            /* (lit_corridor only).  */
            if (loc->typ == CORR && loc->mem_bg == S_litcorr && !loc->waslit) {
                loc->mem_bg = S_corr;
                loc->mem_door_l = 0;
                loc->mem_door_t = 0;
                dbuf_set_loc(x, y);
            }
        }
    } else {
        map_location(x, y, 1);

        if (Punished) {
            /* 
             * A ball or chain is only felt if it is first on the object
             * location list.  Otherwise, we need to clear the felt bit ---
             * something has been dropped on the ball/chain.  If the bit is
             * not cleared, then when the ball/chain is moved it will drop
             * the wrong glyph.
             */
            if (uchain->ox == x && uchain->oy == y) {
                if (level->objects[x][y] == uchain)
                    u.bc_felt |= BC_CHAIN;
                else
                    u.bc_felt &= ~BC_CHAIN;     /* do not feel the chain */
            }
            if (!carried(uball) && uball->ox == x && uball->oy == y) {
                if (level->objects[x][y] == uball)
                    u.bc_felt |= BC_BALL;
                else
                    u.bc_felt &= ~BC_BALL;      /* do not feel the ball */
            }
        }

        /* Floor spaces are dark if unlit.  Corridors are dark if unlit. */
        if (loc->typ == ROOM && loc->mem_bg == S_room && !loc->waslit) {
            loc->mem_bg = S_darkroom;
            loc->mem_door_l = 0;
            loc->mem_door_t = 0;
            dbuf_set_loc(x, y);
        } else if (loc->typ == CORR && loc->mem_bg == S_litcorr && !loc->waslit) {
            loc->mem_bg = S_corr;
            loc->mem_door_l = 0;
            loc->mem_door_t = 0;
            dbuf_set_loc(x, y);
        }
    }
    /* draw monster on top if we can sense it */
    if ((x != u.ux || y != u.uy) && (mon = m_at(level, x, y)) && sensemon(mon))
        display_monster(x, y, mon,
                        (tp_sensemon(mon) ||
                         MATCH_WARN_OF_MON(mon)) ? PHYSICALLY_SEEN : DETECTED,
                        is_worm_tail(mon));
}

/*
 * newsym()
 *
 * Possibly put a new glyph at the given location.
 */
void
newsym(int x, int y)
{
    struct monst *mon;
    struct rm *loc = &(level->locations[x][y]);
    int see_it;
    xchar worm_tail;

    if (in_mklev)
        return;

    /* only permit updating the hero when swallowed */
    if (u.uswallow) {
        if (x == u.ux && y == u.uy)
            display_self();
        return;
    }
    if (Underwater && !Is_waterlevel(&u.uz)) {
        /* don't do anything unless (x,y) is an adjacent underwater position */
        int dx, dy;

        if (!is_pool(level, x, y))
            return;
        dx = x - u.ux;
        if (dx < 0)
            dx = -dx;
        dy = y - u.uy;
        if (dy < 0)
            dy = -dy;
        if (dx > 1 || dy > 1)
            return;
    }

    /* Can physically see the location. */
    if (cansee(x, y)) {
        struct region *reg = visible_region_at(level, x, y);

        /* 
         * Don't use templit here:  E.g.
         *
         *      loc->waslit = !!(loc->lit || templit(x,y));
         *
         * Otherwise we have the "light pool" problem, where non-permanently
         * lit areas just out of sight stay remembered as lit.  They should
         * re-darken.
         *
         * Perhaps ALL areas should revert to their "unlit" look when
         * out of sight.
         */
        loc->waslit = (loc->lit != 0);  /* remember lit condition */

        if (x == u.ux && y == u.uy) {
            if (senseself()) {
                map_location(x, y, 0);  /* map *under* self */
                display_self();
            } else
                /* we can see what is there */
                map_location(x, y, 1);
        } else {
            mon = m_at(level, x, y);
            worm_tail = is_worm_tail(mon);
            see_it = mon && (worm_tail ? (!mon->minvis || See_invisible)
                             : (mon_visible(mon)) || tp_sensemon(mon) ||
                             MATCH_WARN_OF_MON(mon));
            if (mon && (see_it || (!worm_tail && Detect_monsters))) {
                if (mon->mtrapped) {
                    struct trap *trap = t_at(level, x, y);
                    int tt = trap ? trap->ttyp : NO_TRAP;

                    /* if monster is in a physical trap, you see the trap too */
                    if (tt == BEAR_TRAP || tt == PIT || tt == SPIKED_PIT ||
                        tt == WEB) {
                        trap->tseen = TRUE;
                    }
                }
                map_location(x, y, 0);  /* map under the monster */
                /* also gets rid of any invisibility glyph */
                display_monster(x, y, mon, see_it ? PHYSICALLY_SEEN : DETECTED,
                                worm_tail);
            } else if (mon && mon_warning(mon) && !is_worm_tail(mon))
                display_warning(mon);
            else if (level->locations[x][y].mem_invis)
                map_invisible(x, y);
            else
                map_location(x, y, 1);  /* map the location */
        }

        if (reg != NULL && ACCESSIBLE(loc->typ))
            dbuf_set_effect(x, y, reg->effect_id);
    }

    /* Can't see the location. */
    else {
        if (x == u.ux && y == u.uy) {
            feel_location(u.ux, u.uy);  /* forces an update */

            if (senseself())
                display_self();
        } else if ((mon = m_at(level, x, y))
                   && ((see_it = (tp_sensemon(mon) || MATCH_WARN_OF_MON(mon)
                                  || (see_with_infrared(mon) &&
                                      mon_visible(mon))))
                       || Detect_monsters)
                   && !is_worm_tail(mon)) {
            /* Monsters are printed every time. */
            /* This also gets rid of any invisibility glyph */
            display_monster(x, y, mon, see_it ? 0 : DETECTED, 0);
        } else if ((mon = m_at(level, x, y)) && mon_warning(mon) &&
                   !is_worm_tail(mon)) {
            display_warning(mon);
        }

        /* 
         * If the location is remembered as being both dark (waslit is false)
         * and lit (glyph is a lit room or lit corridor) then it was either:
         *
         *      (1) A dark location that the hero could see through night
         *          vision.
         *
         *      (2) Darkened while out of the hero's sight.  This can happen
         *          when cursed scroll of light is read.
         *
         * In either case, we have to manually correct the hero's memory to
         * match waslit.  Deciding when to change waslit is non-trivial.
         *
         *  Note:  If flags.lit_corridor is set, then corridors act like room
         *         squares.  That is, they light up if in night vision range.
         *         If flags.lit_corridor is not set, then corridors will
         *         remain dark unless lit by a light spell and may darken
         *         again, as discussed above.
         *
         * These checks and changes must be here and not in back_to_glyph().
         * They are dependent on the position being out of sight.
         */
        else if (!loc->waslit) {
            if (loc->mem_bg == S_litcorr && loc->typ == CORR) {
                loc->mem_bg = S_corr;
                loc->mem_door_l = 0;
                loc->mem_door_t = 0;
                dbuf_set_loc(x, y);
            } else if (loc->mem_bg == S_room && loc->typ == ROOM) {
                loc->mem_bg = S_darkroom;
                loc->mem_door_l = 0;
                loc->mem_door_t = 0;
                dbuf_set_loc(x, y);
            } else
                goto show_mem;
        } else {
        show_mem:
            dbuf_set_loc(x, y);
        }
    }
}

#undef is_worm_tail

/*
 * shieldeff()
 *
 * Put magic shield pyrotechnics at the given location.  This *could* be
 * pulled into a platform dependent routine for fancier graphics if desired.
 */
void
shieldeff(xchar x, xchar y)
{
    int i;

    if (!flags.sparkle)
        return;

    if (cansee(x, y)) { /* Don't see anything if can't see the location */
        for (i = 0; i < SHIELD_COUNT; i++) {
            dbuf_set_effect(x, y, dbuf_effect(E_MISC, shield_static[i]));
            flush_screen();     /* make sure the effect shows up */
            win_delay_output();
        }

        dbuf_set_effect(x, y, 0);
    }
}


struct tmp_sym {
    coord saved[COLNO]; /* previously updated positions */
    int sidx;   /* index of next unused slot in saved[] */
    int style;  /* DISP_BEAM, DISP_FLASH, DISP_ALWAYS or DISP_OBJECT */
    int sym;    /* symbol to use when printing */
    int extra;  /* extra data (used for obj_mn for objects) */
    /* Doubly linked list to allow freeing at end of game. */
    struct tmp_sym *prev;
    struct tmp_sym *next;
};
static struct tmp_sym *tsym_head;

static struct tmp_sym *
tmpsym_initimpl(int style, int sym, int extra)
{
    /* FIXME: Figure out how to deallocate this when the game ends. */
    struct tmp_sym *tsym = malloc(sizeof (struct tmp_sym));

    tsym->sidx = 0;
    tsym->style = style;
    tsym->sym = sym;
    tsym->extra = extra;
    flush_screen(); /* flush buffered glyphs */
    
    tsym->prev = NULL;
    if (tsym_head) {
        tsym_head->prev = tsym;
        tsym->next = tsym_head;
    } else
        tsym->next = NULL;
    tsym_head = tsym;
    return tsym;
}

/*
 * tmpsym_init()
 *
 * Set up structure for temporarily placing glyphs on the screen. Do not
 * call delay_output(); it is up to the caller to decide if it wants to wait. 
 *
 * The display styles are as follows:
 *
 * DISP_BEAM  - Display the given sym at each location, but do not erase
 *              any until the close call.
 * DISP_FLASH - Display the given sym at each location, but erase the
 *              previous location's sym.
 * DISP_ALWAYS- Like DISP_FLASH, but vision is not taken into account.
 * DISP_OBJECT- Like flash, but shows an object instead of an effect symbol
 *
 * Use tmpsym_initobj for DISP_OBJECT.
 */
struct tmp_sym *
tmpsym_init(int style, int sym)
{
    if (style == DISP_OBJECT)
        panic("Use tmpsym_initobj for DISP_OBJECT!");

    return tmpsym_initimpl(style, sym, 0);
}

/*
 * tmpsym_init()
 *
 * Used for initializing a temporary object symbol.
 */
struct tmp_sym *
tmpsym_initobj(struct obj *obj)
{
    return tmpsym_initimpl(DISP_OBJECT, dbuf_objid(obj),
                           what_mon(obj->corpsenm) + 1);
}


/*
 * tmpsym_change()
 *
 * Change the symbol used for a tmpsym structure.
 */
void
tmpsym_change(struct tmp_sym *tsym, int sym)
{
    /* Since we won't necessarily update extra correctly. */
    if (tsym->style == DISP_OBJECT)
        panic("Can't change symbol of DISP_OBJECT tmpsym!");

    tsym->sym = sym;
}

/*
 * tmpsym_end()
 *
 * Clean up a tmpsym struct: remove its glyphs and free it. It becomes invalid
 * after this.
 */
void
tmpsym_end(struct tmp_sym *tsym)
{
    if (tsym->style == DISP_BEAM) {
        int i;

        /* Erase (reset) from source to end */
        for (i = 0; i < tsym->sidx; i++)
            newsym(tsym->saved[i].x, tsym->saved[i].y);
    } else {        /* DISP_FLASH or DISP_ALWAYS */
        if (tsym->sidx)     /* been called at least once */
            newsym(tsym->saved[0].x, tsym->saved[0].y);
    }
    /* tsym->sidx = 0; -- about to be freed, so not necessary */

    if (tsym->prev)
        tsym->prev->next = tsym->next;
    if (tsym->next)
        tsym->next->prev = tsym->prev;
    if (tsym == tsym_head)
        tsym_head = tsym->next;

    free(tsym);
}

/*
 * tmpsym_at()
 *
 * Display a temporary symbol at a given location.
 */
void
tmpsym_at(struct tmp_sym *tsym, int x, int y)
{
    if (tsym->style == DISP_BEAM) {
        if (!cansee(x, y))
            return;
        /* save pos for later erasing */
        tsym->saved[tsym->sidx].x = x;
        tsym->saved[tsym->sidx].y = y;
        tsym->sidx += 1;
    } else {        /* DISP_FLASH/ALWAYS */
        if (tsym->sidx) {   /* not first call, so reset previous pos */
            newsym(tsym->saved[0].x, tsym->saved[0].y);
            tsym->sidx = 0; /* display is presently up to date */
        }
        if (!cansee(x, y) && tsym->style != DISP_ALWAYS)
            return;
        tsym->saved[0].x = x;
        tsym->saved[0].y = y;
        tsym->sidx = 1;
    }

    if (tsym->style == DISP_OBJECT)
        dbuf_set_object(x, y, tsym->sym, tsym->extra);
    else
        dbuf_set_effect(x, y, tsym->sym);   /* show it */
    flush_screen(); /* make sure it shows up */
}

/*
 * tmpsym_freeall()
 *
 * Free all still-extant tmp_sym data structures. Used for when the game ends
 * while temporary symbols are still active.
 */
void tmpsym_freeall(void)
{
    struct tmp_sym *tsym;
    while (tsym_head) {
        tsym = tsym_head->next;
        free(tsym_head);
        tsym_head = tsym;
    }
}

/*
 * swallowed()
 *
 * The hero is swallowed.  Show a special graphics sequence for this.  This
 * bypasses all of the display routines and messes with buffered screen
 * directly.  This method works because both vision and display check for
 * being swallowed.
 */
void
swallowed(int first)
{
    static xchar lastx, lasty;  /* last swallowed position */
    int swallower, left_ok, rght_ok;

    if (first)
        cls();
    else {
        int x, y;

        /* Clear old location */
        for (y = lasty - 1; y <= lasty + 1; y++)
            for (x = lastx - 1; x <= lastx + 1; x++)
                dbuf_set_effect(x, y, 0);
        dbuf_set(lastx, lasty, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        /* remove hero symbol */
    }

    swallower = monsndx(u.ustuck->data);
    /* assume isok(u.ux,u.uy) */
    left_ok = isok(u.ux - 1, u.uy);
    rght_ok = isok(u.ux + 1, u.uy);
    /* 
     *  Display the hero surrounded by the monster's stomach.
     */
    if (isok(u.ux, u.uy - 1)) {
        if (left_ok)
            dbuf_set_effect(u.ux - 1, u.uy - 1,
                            swallow_to_effect(swallower, S_sw_tl));
        dbuf_set_effect(u.ux, u.uy - 1, swallow_to_effect(swallower, S_sw_tc));
        if (rght_ok)
            dbuf_set_effect(u.ux + 1, u.uy - 1,
                            swallow_to_effect(swallower, S_sw_tr));
    }

    if (left_ok)
        dbuf_set_effect(u.ux - 1, u.uy, swallow_to_effect(swallower, S_sw_ml));
    display_self();
    if (rght_ok)
        dbuf_set_effect(u.ux + 1, u.uy, swallow_to_effect(swallower, S_sw_mr));

    if (isok(u.ux, u.uy + 1)) {
        if (left_ok)
            dbuf_set_effect(u.ux - 1, u.uy + 1,
                            swallow_to_effect(swallower, S_sw_bl));
        dbuf_set_effect(u.ux, u.uy + 1, swallow_to_effect(swallower, S_sw_bc));
        if (rght_ok)
            dbuf_set_effect(u.ux + 1, u.uy + 1,
                            swallow_to_effect(swallower, S_sw_br));
    }

    /* Update the swallowed position. */
    lastx = u.ux;
    lasty = u.uy;
}

/*
 * under_water()
 *
 * Similar to swallowed() in operation.  Shows hero when underwater
 * except when in water level.  Special routines exist for that.
 */
void
under_water(int mode)
{
    static xchar lastx, lasty;
    static boolean dela;
    int x, y;

    /* swallowing has a higher precedence than under water */
    if (Is_waterlevel(&u.uz) || u.uswallow)
        return;

    /* full update */
    if (mode == 1 || dela) {
        cls();
        dela = FALSE;
    }
    /* delayed full update */
    else if (mode == 2) {
        dela = TRUE;
        return;
    }
    /* limited update */
    else {
        for (y = lasty - 1; y <= lasty + 1; y++)
            for (x = lastx - 1; x <= lastx + 1; x++)
                if (isok(x, y))
                    dbuf_set(x, y, S_unexplored, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    for (x = u.ux - 1; x <= u.ux + 1; x++)
        for (y = u.uy - 1; y <= u.uy + 1; y++)
            if (isok(x, y) && is_pool(level, x, y)) {
                if (Blind && !(x == u.ux && y == u.uy))
                    dbuf_set(x, y, S_unexplored, 0, 0, 0, 0, 0, 0, 0, 0);
                else
                    newsym(x, y);
            }
    lastx = u.ux;
    lasty = u.uy;
}

/*
 * under_ground()
 *
 * Very restricted display.  You can only see yourself.
 */
void
under_ground(int mode)
{
    static boolean dela;

    /* swallowing has a higher precedence than under ground */
    if (u.uswallow)
        return;

    /* full update */
    if (mode == 1 || dela) {
        cls();
        dela = FALSE;
    }
    /* delayed full update */
    else if (mode == 2) {
        dela = TRUE;
        return;
    }
    /* limited update */
    else
        newsym(u.ux, u.uy);
}


/* ========================================================================= */

/*
 * Loop through all of the monsters and update them.  Called when:
 *      + going blind & telepathic
 *      + regaining sight & telepathic
 *      + getting and losing infravision 
 *      + hallucinating
 *      + doing a full screen redraw
 *      + see invisible times out or a ring of see invisible is taken off
 *      + when a potion of see invisible is quaffed or a ring of see
 *        invisible is put on
 *      + gaining telepathy when blind [givit() in eat.c, pleased() in pray.c]
 *      + losing telepathy while blind [xkilled() in mon.c, attrcurse() in
 *        sit.c]
 */
void
see_monsters(void)
{
    struct monst *mon;

    if (!level)
        return; /* can be called during startup, before any level exists */

    for (mon = level->monlist; mon; mon = mon->nmon) {
        if (DEADMONSTER(mon))
            continue;
        newsym(mon->mx, mon->my);
        if (mon->wormno)
            see_wsegs(mon);
    }
    /* when mounted, hero's location gets caught by monster loop */
    if (!u.usteed)
        newsym(u.ux, u.uy);
}

/*
 * Block/unblock light depending on what a mimic is mimicing and if it's
 * invisible or not.  Should be called only when the state of See_invisible
 * changes.
 */
void
set_mimic_blocking(void)
{
    struct monst *mon;

    for (mon = level->monlist; mon; mon = mon->nmon) {
        if (DEADMONSTER(mon))
            continue;
        if (mon->minvis &&
            ((mon->m_ap_type == M_AP_FURNITURE &&
              (mon->mappearance == S_vcdoor || mon->mappearance == S_hcdoor)) ||
             (mon->m_ap_type == M_AP_OBJECT && mon->mappearance == BOULDER))) {
            if (See_invisible)
                block_point(mon->mx, mon->my);
            else
                unblock_point(mon->mx, mon->my);
        }
    }
}

/*
 * Loop through all of the object *locations* and update them.  Called when
 *      + hallucinating.
 */
void
see_objects(void)
{
    struct obj *obj;

    for (obj = level->objlist; obj; obj = obj->nobj)
        if (vobj_at(obj->ox, obj->oy) == obj)
            newsym(obj->ox, obj->oy);
}

/*
 * Update hallucinated traps.
 */
void
see_traps(void)
{
    struct trap *trap;

    for (trap = level->lev_traps; trap; trap = trap->ntrap)
        if (level->locations[trap->tx][trap->ty].mem_trap)
            newsym(trap->tx, trap->ty);
}

/*
 * display_self()
 *
 * Display the hero.  It is assumed that all checks necessary to determine
 * _if_ the hero can be seen have already been done.
 */
void
display_self(void)
{
    int x = u.ux, y = u.uy;

    if (u.usteed && mon_visible(u.usteed)) {
        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 0,
                 what_mon(u.usteed->mnum) + 1, MON_RIDDEN, 0,
                 dbuf_branding(&level->locations[x][y]));
    } else if (youmonst.m_ap_type == M_AP_NOTHING) {
        int monnum = (Upolyd || !iflags.showrace) ? u.umonnum :
            (flags.female && urace.femalenum != NON_PM) ?
            urace.femalenum : urace.malenum;
        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 0, monnum + 1, 0, 0,
                 dbuf_branding(&level->locations[x][y]));
    } else if (youmonst.m_ap_type == M_AP_FURNITURE) {
        dbuf_set(x, y, youmonst.mappearance, 0, 0, 0, 0, 0, 0, 0,
                 dbuf_branding(&level->locations[x][y]));
    } else if (youmonst.m_ap_type == M_AP_OBJECT) {
        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap, youmonst.mappearance + 1, 0,
                 0, 0, 0, 0, dbuf_branding(&level->locations[x][y]));
    } else      /* M_AP_MONSTER */
        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 0, youmonst.mappearance + 1,
                 0, 0, dbuf_branding(&level->locations[x][y]));
}


int
doredraw(void)
{
    int x, y;

    if (!u.ux)
        return 0;       /* display isn't ready yet */

    if (u.uswallow) {
        swallowed(1);
        return 0;
    }
    if (Underwater && !Is_waterlevel(&u.uz)) {
        under_water(1);
        return 0;
    }
    if (u.uburied) {
        under_ground(1);
        return 0;
    }

    /* shut down vision */
    vision_recalc(2);

    /* display memory */
    for (x = 1; x < COLNO; x++) {
        for (y = 0; y < ROWNO; y++)
            dbuf_set_loc(x, y);
    }

    /* see what is to be seen */
    vision_recalc(0);

    /* overlay with monsters */
    see_monsters();

    iflags.botl = 1;    /* force a redraw of the bottom line */
    return 0;
}


/* ========================================================================= */
/* Display Buffering (3rd screen) ========================================== */
static struct nh_dbuf_entry dbuf[ROWNO][COLNO];


/* 
 * object ids need to be obfuscated for non-identified types to prevent
 * visual identification in tile ports:
 * if a worthless piece of red glass and a ruby have different tile images
 * even when the glass rock is unidentified, describing both as "red gem" is
 * useless.
 * Additionally, it is better if descriptions match up with tiles for potions etc.
 * so that a potion described as "milky" will actually be ablt to look milky.
 * 
 * also used in pickup.c and invent.c
 */
int
obfuscate_object(int otyp)
{
    if (!otyp)
        return 0;

    /* object ids are shifted by 1 for display, so that 0 can mean "no object" */
    otyp -= 1;

    if (!objects[otyp].oc_name_known) {
        switch (otyp) {
        case SACK:
        case OILSKIN_SACK:
        case BAG_OF_TRICKS:
        case BAG_OF_HOLDING:
            otyp = SACK;
            break;

        case LOADSTONE:
        case LUCKSTONE:
        case FLINT:
        case TOUCHSTONE:
            otyp = FLINT;
            break;

        case OIL_LAMP:
        case MAGIC_LAMP:
            otyp = OIL_LAMP;
            break;

        case TIN_WHISTLE:
        case MAGIC_WHISTLE:
            otyp = TIN_WHISTLE;
            break;

            /* all gems initially look like pieces of glass */
        case DILITHIUM_CRYSTAL:
        case DIAMOND:
        case RUBY:
        case JACINTH:
        case SAPPHIRE:
        case BLACK_OPAL:
        case EMERALD:
        case TURQUOISE:
        case CITRINE:
        case AQUAMARINE:
        case AMBER:
        case TOPAZ:
        case JET:
        case OPAL:
        case CHRYSOBERYL:
        case GARNET:
        case AMETHYST:
        case JASPER:
        case FLUORITE:
        case OBSIDIAN:
        case AGATE:
        case JADE:
            switch (objects[otyp].oc_color) {
            case CLR_WHITE:
                otyp = WORTHLESS_PIECE_OF_WHITE_GLASS;
                break;
            case CLR_BLUE:
                otyp = WORTHLESS_PIECE_OF_BLUE_GLASS;
                break;
            case CLR_RED:
                otyp = WORTHLESS_PIECE_OF_RED_GLASS;
                break;
            case CLR_BROWN:
                otyp = WORTHLESS_PIECE_OF_YELLOWISH_BROWN_GLASS;
                break;
            case CLR_ORANGE:
                otyp = WORTHLESS_PIECE_OF_ORANGE_GLASS;
                break;
            case CLR_YELLOW:
                otyp = WORTHLESS_PIECE_OF_YELLOW_GLASS;
                break;
            case CLR_BLACK:
                otyp = WORTHLESS_PIECE_OF_BLACK_GLASS;
                break;
            case CLR_GREEN:
                otyp = WORTHLESS_PIECE_OF_GREEN_GLASS;
                break;
            case CLR_MAGENTA:
                otyp = WORTHLESS_PIECE_OF_VIOLET_GLASS;
                break;
            }
            break;
        }
    }

    /* finally, account for shuffled descriptions */
    return objects[otyp].oc_descr_idx + 1;
}


void
dbuf_set_effect(int x, int y, int eglyph)
{
    if (!isok(x, y))
        return;

    dbuf[y][x].effect = eglyph;
}

static void
dbuf_set_object(int x, int y, int oid, int omn)
{
    if (!isok(x, y))
        return;

    dbuf[y][x].obj = obfuscate_object(oid);
    dbuf[y][x].obj_mn = omn;
}

/*
 * copy player memory for a location into the display buffer
 */
static void
dbuf_set_loc(int x, int y)
{
    dbuf_set(x, y, level->locations[x][y].mem_bg,
             level->locations[x][y].mem_trap, level->locations[x][y].mem_obj,
             level->locations[x][y].mem_obj_mn,
             level->locations[x][y].mem_invis, 0, 0, 0,
             dbuf_branding(&level->locations[x][y]));
}


/*
 * Calculate the branding information for a location.
 */
short
dbuf_branding(struct rm *loc)
{
    short b = 0;

    if (loc->mem_stepped)
        b |= NH_BRANDING_STEPPED;
    if (loc->mem_door_l && (loc->flags & D_LOCKED))
        b |= NH_BRANDING_LOCKED;
    else if (loc->mem_door_l)
        b |= NH_BRANDING_UNLOCKED;
    if (loc->mem_door_t && (loc->flags & D_TRAPPED))
        b |= NH_BRANDING_TRAPPED;
    else if (loc->mem_door_t)
        b |= NH_BRANDING_UNTRAPPED;
    return b;
}

/*
 * Store display information for later flushing.
 */
void
dbuf_set(int x, int y, int bg, int trap, int obj, int obj_mn, boolean invis,
         int mon, int monflags, int effect, int branding)
{
    if (!isok(x, y))
        return;

    dbuf[y][x].bg = bg;
    dbuf[y][x].trap = trap;
    dbuf[y][x].obj = obfuscate_object(obj);
    dbuf[y][x].obj_mn = obj_mn;
    dbuf[y][x].invis = invis;
    dbuf[y][x].mon = mon;
    dbuf[y][x].monflags = monflags;
    dbuf[y][x].effect = effect;
    dbuf[y][x].visible = cansee(x, y);
    dbuf[y][x].branding = branding;
}


int
dbuf_get_mon(int x, int y)
{
    if (!isok(x, y))
        return 0;

    return dbuf[y][x].mon;
}


/* warning_at: return TRUE if there is a warning symbol at the given position
 *
 * attack_checks() needs to know and there is no other way to get this info
 */
boolean
warning_at(int x, int y)
{
    return (dbuf[y][x].mon > NUMMONS) && (dbuf[y][x].monflags & MON_WARNING);
}


void
cls(void)
{
    memset(dbuf, 0, sizeof (struct nh_dbuf_entry) * ROWNO * COLNO);
}


void
flush_screen_disable(void)
{
    delay_flushing = TRUE;
}


void
flush_screen_enable(void)
{
    delay_flushing = FALSE;
}


/*
 * Send the display buffer to the window port.
 */
void
flush_screen(void)
{
    if (delay_flushing)
        return;

    update_screen(dbuf, u.ux, u.uy);

    if (iflags.botl)
        bot();
}


/* for remote level viewing from the overview menu: display the level, but
 * send a player position of (-1, -1) to indicate that the player's location
 * shouldn't be highlighted */
void
flush_screen_nopos(void)
{
    update_screen(dbuf, -1, -1);
}

/* ========================================================================= */

/*
 * back_to_cmap()
 *
 * Use the information in the rm structure at the given position to create
 * a glyph of a background.
 *
 * I had to add a field in the rm structure (horizontal) so that we knew
 * if open doors and secret doors were horizontal or vertical.  Previously,
 * the screen symbol had the horizontal/vertical information set at
 * level generation time.
 *
 * I used the 'ladder' field (really doormask) for deciding if stairwells
 * were up or down.  I didn't want to check the upstairs and dnstairs
 * variables.
 */
int
back_to_cmap(struct level *lev, xchar x, xchar y)
{
    int idx;
    struct rm *ptr = &(lev->locations[x][y]);

    switch (ptr->typ) {
    case SCORR:
    case STONE:
        idx = lev->flags.arboreal ? S_tree : S_stone;
        break;
    case ROOM:
        idx = (!cansee(x, y) && !ptr->waslit) ? S_darkroom : S_room;
        break;
    case CORR:
        idx = (ptr->waslit || flags.lit_corridor) ? S_litcorr : S_corr;
        break;
    case HWALL:
    case VWALL:
    case TLCORNER:
    case TRCORNER:
    case BLCORNER:
    case BRCORNER:
    case CROSSWALL:
    case TUWALL:
    case TDWALL:
    case TLWALL:
    case TRWALL:
    case SDOOR:
        idx = ptr->seenv ? wall_angle(ptr) : S_stone;
        break;
    case DOOR:
        if (ptr->doormask) {
            if (ptr->doormask & D_BROKEN)
                idx = S_ndoor;
            else if (ptr->doormask & D_ISOPEN)
                idx = (ptr->horizontal) ? S_hodoor : S_vodoor;
            else        /* else is closed */
                idx = (ptr->horizontal) ? S_hcdoor : S_vcdoor;
        } else
            idx = S_ndoor;
        break;
    case IRONBARS:
        idx = S_bars;
        break;
    case TREE:
        idx = S_tree;
        break;
    case POOL:
    case MOAT:
        idx = S_pool;
        break;
    case STAIRS:
        if (lev->sstairs.sx == x && lev->sstairs.sy == y)
            idx = (ptr->ladder & LA_DOWN) ? S_dnsstair : S_upsstair;
        else
            idx = (ptr->ladder & LA_DOWN) ? S_dnstair : S_upstair;
        break;
    case LADDER:
        idx = (ptr->ladder & LA_DOWN) ? S_dnladder : S_upladder;
        break;
    case FOUNTAIN:
        idx = S_fountain;
        break;
    case SINK:
        idx = S_sink;
        break;
    case ALTAR:
        idx = S_altar;
        break;
    case GRAVE:
        idx = S_grave;
        break;
    case THRONE:
        idx = S_throne;
        break;
    case LAVAPOOL:
        idx = S_lava;
        break;
    case ICE:
        idx = S_ice;
        break;
    case AIR:
        idx = S_air;
        break;
    case CLOUD:
        idx = S_cloud;
        break;
    case WATER:
        idx = S_water;
        break;
    case DBWALL:
        idx = (ptr->horizontal) ? S_hcdbridge : S_vcdbridge;
        break;
    case DRAWBRIDGE_UP:
        switch (ptr->drawbridgemask & DB_UNDER) {
        case DB_MOAT:
            idx = S_pool;
            break;
        case DB_LAVA:
            idx = S_lava;
            break;
        case DB_ICE:
            idx = S_ice;
            break;
        case DB_FLOOR:
            idx = (!cansee(x, y) && !ptr->waslit) ? S_darkroom : S_room;
            break;
        default:
            impossible("Strange db-under: %d", ptr->drawbridgemask & DB_UNDER);
            idx = S_room;       /* something is better than nothing */
            break;
        }
        break;
    case DRAWBRIDGE_DOWN:
        idx = (ptr->horizontal) ? S_hodbridge : S_vodbridge;
        break;
    default:
        impossible("back_to_cmap:  unknown location type [ = %d ]", ptr->typ);
        idx = S_room;
        break;
    }

    return idx;
}


/*
 * swallow_to_effect()
 *
 * Convert a monster number and a swallow location into the correct glyph.
 * If you don't want a patchwork monster while hallucinating, decide on
 * a random monster in swallowed() and don't use what_mon() here.
 */
static int
swallow_to_effect(int mnum, int loc)
{
    if (loc < S_sw_tl || S_sw_br < loc) {
        impossible("swallow_to_effect: bad swallow location");
        loc = S_sw_br;
    }
    return ((E_SWALLOW << 16) | (what_mon(mnum) << 3) | loc) + 1;
}



/*
 * zapdir_to_effect()
 *
 * Change the given zap direction and beam type into a glyph.  Each beam
 * type has four glyphs, one for each of the symbols below.  The order of
 * the zap symbols [0-3] as defined in rm.h are:
 *
 *      |  S_vbeam      ( 0, 1) or ( 0,-1) -> dx = 0
 *      -  S_hbeam      ( 1, 0) or (-1, 0) -> dx = 1
 *      \  S_lslant     ( 1, 1) or (-1,-1) -> dx = 2
 *      /  S_rslant     (-1, 1) or ( 1,-1) -> dx = 3
 */
int
zapdir_to_effect(int dx, int dy, int beam_type)
{
    if (beam_type >= NUM_ZAP) {
        impossible("zapdir_to_effect:  illegal beam type");
        beam_type = 0;
    }
    dx = (dx == dy) ? 2 : (dx && dy) ? 3 : dx ? 1 : 0;

    return (((E_ZAP << 16) | (beam_type << 2) | dx)) + 1;
}


/* Dump a rough ascii representation of the screen to the given dumpfp
 * This may or may not look anything like what the player sees: they might be
 * using tiles or a unicode charset. */
void
dump_screen(FILE * dumpfp)
{
    int x, y;
    char scrline[COLNO + 1];
    const struct nh_drawing_info *di = nh_get_drawing_info();
    const struct nh_dbuf_entry *dbe;

    for (y = 0; y < ROWNO; y++) {
        for (x = 0; x < COLNO; x++) {
            dbe = &dbuf[y][x];
            scrline[x] = di->bgelements[dbe->bg].ch;
            if (dbe->trap)
                scrline[x] = di->traps[dbe->trap - 1].ch;
            if (dbe->obj)
                scrline[x] = di->objects[dbe->obj - 1].ch;
            if (dbe->invis)
                scrline[x] = di->invis[0].ch;
            else if (dbe->mon) {
                if (dbe->mon > di->num_monsters &&
                    (dbe->monflags & MON_WARNING))
                    scrline[x] =
                        di->warnings[dbe->mon - 1 - di->num_monsters].ch;
                else
                    scrline[x] = di->monsters[dbe->mon - 1].ch;
            }
        }

        scrline[COLNO] = '\0';
        fprintf(dumpfp, "%s\n", scrline);
    }
}

/* ------------------------------------------------------------------------- */
/* Wall Angle -------------------------------------------------------------- */

/*
 * Return 'which' if position is implies an unfinshed exterior.  Return
 * zero otherwise.  Unfinished implies outer area is rock or a corridor.
 *
 * Things that are ambigious: lava
 */
static int
check_pos(struct level *lev, int x, int y, int which)
{
    int type;

    if (!isok(x, y))
        return which;
    type = lev->locations[x][y].typ;
    if (IS_ROCK(type) || type == CORR || type == SCORR)
        return which;
    return 0;
}

/* Return TRUE if more than one is non-zero. */
#define more_than_one(a, b, c) \
    (((a) && ((b)|(c))) || ((b) && ((a)|(c))) || ((c) && ((a)|(b))))

/* Return the wall mode for a T wall. */
static int
set_twall(struct level *lev, int x0, int y0, int x1, int y1, int x2, int y2,
          int x3, int y3)
{
    int wmode, is_1, is_2, is_3;

    is_1 = check_pos(lev, x1, y1, WM_T_LONG);
    is_2 = check_pos(lev, x2, y2, WM_T_BL);
    is_3 = check_pos(lev, x3, y3, WM_T_BR);
    if (more_than_one(is_1, is_2, is_3)) {
        wmode = 0;
    } else {
        wmode = is_1 + is_2 + is_3;
    }
    return wmode;
}

/* Return wall mode for a horizontal or vertical wall. */
static int
set_wall(struct level *lev, int x, int y, int horiz)
{
    int wmode, is_1, is_2;

    if (horiz) {
        is_1 = check_pos(lev, x, y - 1, WM_W_TOP);
        is_2 = check_pos(lev, x, y + 1, WM_W_BOTTOM);
    } else {
        is_1 = check_pos(lev, x - 1, y, WM_W_LEFT);
        is_2 = check_pos(lev, x + 1, y, WM_W_RIGHT);
    }
    if (more_than_one(is_1, is_2, 0)) {
        wmode = 0;
    } else {
        wmode = is_1 + is_2;
    }
    return wmode;
}


/* Return a wall mode for a corner wall. (x4,y4) is the "inner" position. */
static int
set_corn(struct level *lev, int x1, int y1, int x2, int y2, int x3, int y3,
         int x4, int y4)
{
    int wmode, is_1, is_2, is_3, is_4;

    is_1 = check_pos(lev, x1, y1, 1);
    is_2 = check_pos(lev, x2, y2, 1);
    is_3 = check_pos(lev, x3, y3, 1);
    is_4 = check_pos(lev, x4, y4, 1);   /* inner location */

    /* 
     * All 4 should not be true.  So if the inner location is rock,
     * use it.  If all of the outer 3 are true, use outer.  We currently
     * can't cover the case where only part of the outer is rock, so
     * we just say that all the walls are finished (if not overridden
     * by the inner section).
     */
    if (is_4) {
        wmode = WM_C_INNER;
    } else if (is_1 && is_2 && is_3)
        wmode = WM_C_OUTER;
    else
        wmode = 0;      /* finished walls on all sides */

    return wmode;
}

/* Return mode for a crosswall. */
static int
set_crosswall(struct level *lev, int x, int y)
{
    int wmode, is_1, is_2, is_3, is_4;

    is_1 = check_pos(lev, x - 1, y - 1, 1);
    is_2 = check_pos(lev, x + 1, y - 1, 1);
    is_3 = check_pos(lev, x + 1, y + 1, 1);
    is_4 = check_pos(lev, x - 1, y + 1, 1);

    wmode = is_1 + is_2 + is_3 + is_4;
    if (wmode > 1) {
        if (is_1 && is_3 && (is_2 + is_4 == 0)) {
            wmode = WM_X_TLBR;
        } else if (is_2 && is_4 && (is_1 + is_3 == 0)) {
            wmode = WM_X_BLTR;
        } else {
            wmode = 0;
        }
    } else if (is_1)
        wmode = WM_X_TL;
    else if (is_2)
        wmode = WM_X_TR;
    else if (is_3)
        wmode = WM_X_BR;
    else if (is_4)
        wmode = WM_X_BL;

    return wmode;
}

/* Called from mklev.  Scan the level and set the wall modes. */
void
set_wall_state(struct level *lev)
{
    int x, y;
    int wmode;
    struct rm *loc;

    for (x = 0; x < COLNO; x++)
        for (loc = &lev->locations[x][0], y = 0; y < ROWNO; y++, loc++) {
            switch (loc->typ) {
            case SDOOR:
                wmode = set_wall(lev, x, y, (int)loc->horizontal);
                break;
            case VWALL:
                wmode = set_wall(lev, x, y, 0);
                break;
            case HWALL:
                wmode = set_wall(lev, x, y, 1);
                break;
            case TDWALL:
                wmode =
                    set_twall(lev, x, y, x, y - 1, x - 1, y + 1, x + 1, y + 1);
                break;
            case TUWALL:
                wmode =
                    set_twall(lev, x, y, x, y + 1, x + 1, y - 1, x - 1, y - 1);
                break;
            case TLWALL:
                wmode =
                    set_twall(lev, x, y, x + 1, y, x - 1, y - 1, x - 1, y + 1);
                break;
            case TRWALL:
                wmode =
                    set_twall(lev, x, y, x - 1, y, x + 1, y + 1, x + 1, y - 1);
                break;
            case TLCORNER:
                wmode =
                    set_corn(lev, x - 1, y - 1, x, y - 1, x - 1, y, x + 1,
                             y + 1);
                break;
            case TRCORNER:
                wmode =
                    set_corn(lev, x, y - 1, x + 1, y - 1, x + 1, y, x - 1,
                             y + 1);
                break;
            case BLCORNER:
                wmode =
                    set_corn(lev, x, y + 1, x - 1, y + 1, x - 1, y, x + 1,
                             y - 1);
                break;
            case BRCORNER:
                wmode =
                    set_corn(lev, x + 1, y, x + 1, y + 1, x, y + 1, x - 1,
                             y - 1);
                break;
            case CROSSWALL:
                wmode = set_crosswall(lev, x, y);
                break;

            default:
                wmode = -1;     /* don't set wall info */
                break;
            }

            if (wmode >= 0)
                loc->wall_info = (loc->wall_info & ~WM_MASK) | wmode;
        }
}

/* ------------------------------------------------------------------------- */
/* This matrix is used here and in vision.c. */
unsigned const char seenv_matrix[3][3] = { {SV2, SV1, SV0},
{SV3, SVALL, SV7},
{SV4, SV5, SV6}
};

#define sign(z) ((z) < 0 ? -1 : ((z) > 0 ? 1 : 0))

/* Set the seen vector of lev as if seen from (x0,y0) to (x,y). */
static void
set_seenv(struct rm *loc, int x0, int y0, int x, int y)
{
    int dx = x - x0, dy = y0 - y;

    loc->seenv |= seenv_matrix[sign(dy) + 1][sign(dx) + 1];
}

/* ------------------------------------------------------------------------- */

/* T wall types, one for each row in wall_matrix[][]. */
#define T_d 0
#define T_l 1
#define T_u 2
#define T_r 3

/*
 * These are the column names of wall_matrix[][].  They are the "results"
 * of a tdwall pattern match.  All T walls are rotated so they become
 * a tdwall.  Then we do a single pattern match, but return the
 * correct result for the original wall by using different rows for
 * each of the wall types.
 */
#define T_stone  0
#define T_tlcorn 1
#define T_trcorn 2
#define T_hwall  3
#define T_tdwall 4

static const int wall_matrix[4][5] = {
    {S_unexplored, S_tlcorn, S_trcorn, S_hwall, S_tdwall},      /* tdwall */
    {S_unexplored, S_trcorn, S_brcorn, S_vwall, S_tlwall},      /* tlwall */
    {S_unexplored, S_brcorn, S_blcorn, S_hwall, S_tuwall},      /* tuwall */
    {S_unexplored, S_blcorn, S_tlcorn, S_vwall, S_trwall},      /* trwall */
};


/* Cross wall types, one for each "solid" quarter.  Rows of cross_matrix[][]. */
#define C_bl 0
#define C_tl 1
#define C_tr 2
#define C_br 3

/*
 * These are the column names for cross_matrix[][].  They express results
 * in C_br (bottom right) terms.  All crosswalls with a single solid
 * quarter are rotated so the solid section is at the bottom right.
 * We pattern match on that, but return the correct result depending
 * on which row we'ere looking at.
 */
#define C_trcorn 0
#define C_brcorn 1
#define C_blcorn 2
#define C_tlwall 3
#define C_tuwall 4
#define C_crwall 5

static const int cross_matrix[4][6] = {
    {S_brcorn, S_blcorn, S_tlcorn, S_tuwall, S_trwall, S_crwall},
    {S_blcorn, S_tlcorn, S_trcorn, S_trwall, S_tdwall, S_crwall},
    {S_tlcorn, S_trcorn, S_brcorn, S_tdwall, S_tlwall, S_crwall},
    {S_trcorn, S_brcorn, S_blcorn, S_tlwall, S_tuwall, S_crwall},
};


/* Print out a T wall warning and all interesting info. */
static void
t_warn(struct rm *loc)
{
    static const char warn_str[] = "wall_angle: %s: case %d: seenv = 0x%x";
    const char *wname;

    if (loc->typ == TUWALL)
        wname = "tuwall";
    else if (loc->typ == TLWALL)
        wname = "tlwall";
    else if (loc->typ == TRWALL)
        wname = "trwall";
    else if (loc->typ == TDWALL)
        wname = "tdwall";
    else
        wname = "unknown";
    impossible(warn_str, wname, loc->wall_info & WM_MASK,
               (unsigned int)loc->seenv);
}


/*
 * Return the correct graphics character index using wall type, wall mode,
 * and the seen vector.  It is expected that seenv is non zero.
 *
 * All T-wall vectors are rotated to be TDWALL.  All single crosswall
 * blocks are rotated to bottom right.  All double crosswall are rotated
 * to W_X_BLTR.  All results are converted back.
 *
 * The only way to understand this is to take out pen and paper and
 * draw diagrams.  See rm.h for more details on the wall modes and
 * seen vector (SV).
 */
static int
wall_angle(struct rm *loc)
{
    unsigned int seenv = loc->seenv & 0xff;
    const int *row;
    int col, idx;

#define only(sv, bits)  (((sv) & (bits)) && ! ((sv) & ~(bits)))
    switch (loc->typ) {
    case TUWALL:
        row = wall_matrix[T_u];
        seenv = (seenv >> 4 | seenv << 4) & 0xff;       /* rotate to tdwall */
        goto do_twall;
    case TLWALL:
        row = wall_matrix[T_l];
        seenv = (seenv >> 2 | seenv << 6) & 0xff;       /* rotate to tdwall */
        goto do_twall;
    case TRWALL:
        row = wall_matrix[T_r];
        seenv = (seenv >> 6 | seenv << 2) & 0xff;       /* rotate to tdwall */
        goto do_twall;
    case TDWALL:
        row = wall_matrix[T_d];
    do_twall:
        switch (loc->wall_info & WM_MASK) {
        case 0:
            if (seenv == SV4) {
                col = T_tlcorn;
            } else if (seenv == SV6) {
                col = T_trcorn;
            } else if (seenv & (SV3 | SV5 | SV7) ||
                       ((seenv & SV4) && (seenv & SV6))) {
                col = T_tdwall;
            } else if (seenv & (SV0 | SV1 | SV2)) {
                col = (seenv & (SV4 | SV6) ? T_tdwall : T_hwall);
            } else {
                t_warn(loc);
                col = T_stone;
            }
            break;
        case WM_T_LONG:
            if (seenv & (SV3 | SV4) && !(seenv & (SV5 | SV6 | SV7))) {
                col = T_tlcorn;
            } else if (seenv & (SV6 | SV7) && !(seenv & (SV3 | SV4 | SV5))) {
                col = T_trcorn;
            } else if ((seenv & SV5) ||
                       ((seenv & (SV3 | SV4)) && (seenv & (SV6 | SV7)))) {
                col = T_tdwall;
            } else {
                /* only SV0|SV1|SV2 */
                if (!only(seenv, SV0 | SV1 | SV2))
                    t_warn(loc);
                col = T_stone;
            }
            break;
        case WM_T_BL:
            if (only(seenv, SV4 | SV5))
                col = T_tlcorn;
            else if ((seenv & (SV0 | SV1 | SV2 | SV7)) &&
                     !(seenv & (SV3 | SV4 | SV5)))
                col = T_hwall;
            else if (only(seenv, SV6))
                col = T_stone;
            else
                col = T_tdwall;
            break;
        case WM_T_BR:
            if (only(seenv, SV5 | SV6))
                col = T_trcorn;
            else if ((seenv & (SV0 | SV1 | SV2 | SV3)) &&
                     !(seenv & (SV5 | SV6 | SV7)))
                col = T_hwall;
            else if (only(seenv, SV4))
                col = T_stone;
            else
                col = T_tdwall;

            break;
        default:
            impossible("wall_angle: unknown T wall mode %d",
                       loc->wall_info & WM_MASK);
            col = T_stone;
            break;
        }
        idx = row[col];
        break;

    case SDOOR:
        if (loc->horizontal)
            goto horiz;
        /* fall through */
    case VWALL:
        switch (loc->wall_info & WM_MASK) {
        case 0:
            idx = seenv ? S_vwall : S_stone;
            break;
        case 1:
            idx = seenv & (SV1 | SV2 | SV3 | SV4 | SV5) ? S_vwall : S_stone;
            break;
        case 2:
            idx = seenv & (SV0 | SV1 | SV5 | SV6 | SV7) ? S_vwall : S_stone;
            break;
        default:
            impossible("wall_angle: unknown vwall mode %d",
                       loc->wall_info & WM_MASK);
            idx = S_stone;
            break;
        }
        break;

    case HWALL:
    horiz:
        switch (loc->wall_info & WM_MASK) {
        case 0:
            idx = seenv ? S_hwall : S_stone;
            break;
        case 1:
            idx = seenv & (SV3 | SV4 | SV5 | SV6 | SV7) ? S_hwall : S_stone;
            break;
        case 2:
            idx = seenv & (SV0 | SV1 | SV2 | SV3 | SV7) ? S_hwall : S_stone;
            break;
        default:
            impossible("wall_angle: unknown hwall mode %d",
                       loc->wall_info & WM_MASK);
            idx = S_stone;
            break;
        }
        break;

#define set_corner(idx, loc, which, outer, inner, name) \
    switch ((loc)->wall_info & WM_MASK) {                                   \
        case 0:          idx = which; break;                                \
        case WM_C_OUTER: idx = seenv &  (outer) ? which : S_stone; break;   \
        case WM_C_INNER: idx = seenv & ~(inner) ? which : S_stone; break;   \
        default:                                                            \
            impossible("wall_angle: unknown %s mode %d", name,              \
                (loc)->wall_info & WM_MASK);                                \
            idx = S_stone;                                                  \
            break;                                                          \
    }

    case TLCORNER:
        set_corner(idx, loc, S_tlcorn, (SV3 | SV4 | SV5), SV4, "tlcorn");
        break;
    case TRCORNER:
        set_corner(idx, loc, S_trcorn, (SV5 | SV6 | SV7), SV6, "trcorn");
        break;
    case BLCORNER:
        set_corner(idx, loc, S_blcorn, (SV1 | SV2 | SV3), SV2, "blcorn");
        break;
    case BRCORNER:
        set_corner(idx, loc, S_brcorn, (SV7 | SV0 | SV1), SV0, "brcorn");
        break;


    case CROSSWALL:
        switch (loc->wall_info & WM_MASK) {
        case 0:
            if (seenv == SV0)
                idx = S_brcorn;
            else if (seenv == SV2)
                idx = S_blcorn;
            else if (seenv == SV4)
                idx = S_tlcorn;
            else if (seenv == SV6)
                idx = S_trcorn;
            else if (!(seenv & ~(SV0 | SV1 | SV2)) &&
                     (seenv & SV1 || seenv == (SV0 | SV2)))
                idx = S_tuwall;
            else if (!(seenv & ~(SV2 | SV3 | SV4)) &&
                     (seenv & SV3 || seenv == (SV2 | SV4)))
                idx = S_trwall;
            else if (!(seenv & ~(SV4 | SV5 | SV6)) &&
                     (seenv & SV5 || seenv == (SV4 | SV6)))
                idx = S_tdwall;
            else if (!(seenv & ~(SV0 | SV6 | SV7)) &&
                     (seenv & SV7 || seenv == (SV0 | SV6)))
                idx = S_tlwall;
            else
                idx = S_crwall;
            break;

        case WM_X_TL:
            row = cross_matrix[C_tl];
            seenv = (seenv >> 4 | seenv << 4) & 0xff;
            goto do_crwall;
        case WM_X_TR:
            row = cross_matrix[C_tr];
            seenv = (seenv >> 6 | seenv << 2) & 0xff;
            goto do_crwall;
        case WM_X_BL:
            row = cross_matrix[C_bl];
            seenv = (seenv >> 2 | seenv << 6) & 0xff;
            goto do_crwall;
        case WM_X_BR:
            row = cross_matrix[C_br];
        do_crwall:
            if (seenv == SV4)
                idx = S_stone;
            else {
                seenv = seenv & ~SV4;   /* strip SV4 */
                if (seenv == SV0) {
                    col = C_brcorn;
                } else if (seenv & (SV2 | SV3)) {
                    if (seenv & (SV5 | SV6 | SV7))
                        col = C_crwall;
                    else if (seenv & (SV0 | SV1))
                        col = C_tuwall;
                    else
                        col = C_blcorn;
                } else if (seenv & (SV5 | SV6)) {
                    if (seenv & (SV1 | SV2 | SV3))
                        col = C_crwall;
                    else if (seenv & (SV0 | SV7))
                        col = C_tlwall;
                    else
                        col = C_trcorn;
                } else if (seenv & SV1) {
                    col = seenv & SV7 ? C_crwall : C_tuwall;
                } else if (seenv & SV7) {
                    col = seenv & SV1 ? C_crwall : C_tlwall;
                } else {
                    impossible("wall_angle: bottom of crwall check");
                    col = C_crwall;
                }

                idx = row[col];
            }
            break;

        case WM_X_TLBR:
            if (only(seenv, SV1 | SV2 | SV3))
                idx = S_blcorn;
            else if (only(seenv, SV5 | SV6 | SV7))
                idx = S_trcorn;
            else if (only(seenv, SV0 | SV4))
                idx = S_stone;
            else
                idx = S_crwall;
            break;

        case WM_X_BLTR:
            if (only(seenv, SV0 | SV1 | SV7))
                idx = S_brcorn;
            else if (only(seenv, SV3 | SV4 | SV5))
                idx = S_tlcorn;
            else if (only(seenv, SV2 | SV6))
                idx = S_stone;
            else
                idx = S_crwall;
            break;

        default:
            impossible("wall_angle: unknown crosswall mode");
            idx = S_stone;
            break;
        }
        break;

    default:
        impossible("wall_angle: unexpected wall type %d", loc->typ);
        idx = S_stone;
    }
    return idx;
}

/*display.c*/
