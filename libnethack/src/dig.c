/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "edog.h"
/* #define DEBUG *//* turn on for diagnostics */

static boolean rm_waslit(void);
static void mkcavepos(xchar, xchar, int, boolean, boolean);
static void mkcavearea(boolean);
static int dig_typ(struct obj *, xchar, xchar);
static int dig(void);
static schar fillholetyp(int, int);
static void dig_up_grave(void);
static boolean dighole(boolean, boolean);
static struct obj *bury_an_obj(struct obj *);

/* Indices returned by dig_typ() */
#define DIGTYP_UNDIGGABLE 0
#define DIGTYP_ROCK       1
#define DIGTYP_STATUE     2
#define DIGTYP_BOULDER    3
#define DIGTYP_DOOR       4
#define DIGTYP_TREE       5


static boolean
rm_waslit(void)
{
    xchar x, y;

    if (level->locations[u.ux][u.uy].typ == ROOM &&
        level->locations[u.ux][u.uy].waslit)
        return TRUE;
    for (x = u.ux - 2; x < u.ux + 3; x++)
        for (y = u.uy - 1; y < u.uy + 2; y++)
            if (isok(x, y) && level->locations[x][y].waslit)
                return TRUE;
    return FALSE;
}

/* Change level topology.  Messes with vision tables and ignores things like
 * boulders in the name of a nice effect.  Vision will get fixed up again
 * immediately after the effect is complete.
 */
static void
mkcavepos(xchar x, xchar y, int dist, boolean waslit, boolean rockit)
{
    struct rm *loc;

    if (!isok(x, y))
        return;
    loc = &level->locations[x][y];

    if (rockit) {
        struct monst *mtmp;

        if (IS_ROCK(loc->typ))
            return;
        if (t_at(level, x, y))
            return;     /* don't cover the portal */
        if ((mtmp = m_at(level, x, y)) != 0)    /* make sure crucial monsters
                                                   survive */
            if (!passes_walls(mtmp->data))
                rloc(mtmp, FALSE);
    } else if (loc->typ == ROOM)
        return;

    unblock_point(x, y);     /* make sure vision knows this location is open */

    /* fake out saved state */
    loc->seenv = 0;
    loc->doormask = 0;
    if (dist < 3)
        loc->lit = (rockit ? FALSE : TRUE);
    if (waslit)
        loc->waslit = (rockit ? FALSE : TRUE);
    loc->horizontal = FALSE;
    viz_array[y][x] = (dist < 3) ?
        (IN_SIGHT | COULD_SEE) :     /* short-circuit vision recalc */
        COULD_SEE;
    loc->typ = (rockit ? STONE : ROOM);
    if (dist >= 3)
        impossible("mkcavepos called with dist %d", dist);
    if (Blind)
        feel_location(x, y);
    else
        newsym(x, y);
}

static void
mkcavearea(boolean rockit)
{
    int dist;
    xchar xmin = u.ux, xmax = u.ux;
    xchar ymin = u.uy, ymax = u.uy;
    xchar i;
    boolean waslit = rm_waslit();

    if (rockit)
        pline(msgc_consequence, "Crash!  The ceiling collapses around you!");
    else
        pline(msgc_consequence, "A mysterious force %s cave around you!",
              (level->locations[u.ux][u.uy].typ ==
               CORR) ? "creates a" : "extends the");
    win_pause_output(P_MESSAGE);

    for (dist = 1; dist <= 2; dist++) {
        xmin--;
        xmax++;

        /* top and bottom */
        if (dist < 2) { /* the area is wider that it is high */
            ymin--;
            ymax++;
            for (i = xmin + 1; i < xmax; i++) {
                mkcavepos(i, ymin, dist, waslit, rockit);
                mkcavepos(i, ymax, dist, waslit, rockit);
            }
        }

        /* left and right */
        for (i = ymin; i <= ymax; i++) {
            mkcavepos(xmin, i, dist, waslit, rockit);
            mkcavepos(xmax, i, dist, waslit, rockit);
        }

        flush_screen(); /* make sure the new glyphs shows up */
        win_delay_output();
    }

    if (!rockit && level->locations[u.ux][u.uy].typ == CORR) {
        level->locations[u.ux][u.uy].typ = ROOM;
        if (waslit)
            level->locations[u.ux][u.uy].waslit = TRUE;
        newsym(u.ux, u.uy);     /* in case player is invisible */
    }

    turnstate.vision_full_recalc = TRUE;     /* everything changed */
}

/* When digging into location <x,y>, what are you actually digging into? */
static int
dig_typ(struct obj *otmp, xchar x, xchar y)
{
    boolean ispick = is_pick(otmp);

    return (ispick && sobj_at(STATUE, level, x, y) ? DIGTYP_STATUE :
            ispick && sobj_at(BOULDER, level, x, y) ? DIGTYP_BOULDER :
            closed_door(level, x, y) ? DIGTYP_DOOR :
            IS_TREE(level->locations[x][y].typ) ? (ispick ? DIGTYP_UNDIGGABLE :
                                                   DIGTYP_TREE) : ispick &&
            IS_ROCK(level->locations[x][y].typ) &&
            (!level->flags.arboreal || IS_WALL(level->locations[x][y].typ)) ?
            DIGTYP_ROCK : DIGTYP_UNDIGGABLE);
}

boolean
is_digging(void)
{
    return flags.occupation == occ_dig;
}

#define BY_YOU          (&youmonst)
#define BY_OBJECT       (NULL)

boolean
dig_check(struct monst *madeby, enum msg_channel msgc, int x, int y)
{
    struct trap *ttmp = t_at(level, x, y);
    const char *verb = (madeby == BY_YOU && uwep &&
                        is_axe(uwep)) ? "chop" : "dig in";

    if (On_stairs(x, y)) {
        if ((x == level->dnladder.sx && y == level->upladder.sy) ||
            (x == level->upladder.sx && y == level->upladder.sy)) {
            pline(msgc, "The ladder resists your effort.");
        } else
            pline(msgc, "The stairs are too hard to %s.", verb);
        return FALSE;
    } else if (IS_THRONE(level->locations[x][y].typ) && madeby != BY_OBJECT) {
        pline(msgc, "The throne is too hard to break apart.");
        return FALSE;
    } else if (IS_ALTAR(level->locations[x][y].typ) &&
               (madeby != BY_OBJECT ||
                (level->locations[x][y].altarmask & AM_SANCTUM))) {
        pline(msgc, "The altar is too hard to break apart.");
        return FALSE;
    } else if (Is_airlevel(&u.uz)) {
        pline(msgc, "You cannot %s thin air.", verb);
        return FALSE;
    } else if (Is_waterlevel(&u.uz)) {
        pline(msgc, "The water splashes and subsides.");
        return FALSE;
    } else if ((IS_ROCK(level->locations[x][y].typ) &&
                level->locations[x][y].typ != SDOOR &&
                (level->locations[x][y].wall_info & W_NONDIGGABLE) != 0) ||
               (ttmp && (ttmp->ttyp == MAGIC_PORTAL ||
                         ttmp->ttyp == VIBRATING_SQUARE ||
                         !can_dig_down(level)))) {
        /* See the next case in dig(), petrified tree, for an explanation of
           the channel handling for nondiggable walls */
        if (msgc == msgc_cancelled1 && !ttmp)
            msgc = msgc_failcurse;
        pline(msgc, "The %s here is too hard to %s.", surface(x, y), verb);
        return FALSE;
    } else if (sobj_at(BOULDER, level, x, y)) {
        pline(msgc, "There isn't enough room to %s here.", verb);
        return FALSE;
    } else if (madeby == BY_OBJECT &&
               /* the block against existing traps is mainly to prevent broken
                  wands from turning holes into pits */
               (ttmp || is_pool(level, x, y) || is_lava(level, x, y))) {
        /* digging by player handles pools separately */
        return FALSE;
    }
    return TRUE;
}

static int
dig(void)
{
    struct rm *loc;
    xchar dpx = u.utracked_location[tl_dig].x,
          dpy = u.utracked_location[tl_dig].y;
    boolean ispick = uwep && is_pick(uwep);
    const char *verb = (!uwep || is_pick(uwep)) ? "dig into" : "chop through";
    loc = &level->locations[dpx][dpy];
    boolean down = u.ux == dpx && u.uy == dpy;
    boolean new_dig = u.uoccupation_progress[tos_dig] == 0;

    /* perhaps a nymph stole your pick-axe while you were busy digging */
    /* or perhaps you teleported away */
    if (Engulfed || !uwep || (!ispick && !is_axe(uwep)) || distu(dpx, dpy) > 2)
        return 0;

    if (down) {
        /* technically not always a msgc_cancelled1; dig_check will override the
           channel in the case of undiggable walls, which are currently spoiler
           info that isn't saved */
        if (!dig_check(BY_YOU, msgc_cancelled1, u.ux, u.uy))
            return 0;
    } else {    /* !down */
        /* TODO: Should we be tracking wall undiggability somehow? Really it
           should be msgc_failcurse if the player doesn't know it's undiggable,
           then transform the wall to "known undiggable" and use msgc_cancelled1
           (or preferably just cancel the action) in future; undiggability
           checks probably need to be factored out into a separate function */
        if (IS_TREE(loc->typ) && !may_dig(level, dpx, dpy) &&
            dig_typ(uwep, dpx, dpy) == DIGTYP_TREE) {
            pline(msgc_failcurse, "This tree seems to be petrified.");
            return 0;
        }
        if (IS_ROCK(loc->typ) && !may_dig(level, dpx, dpy) &&
            dig_typ(uwep, dpx, dpy) == DIGTYP_ROCK) {
            pline(msgc_failcurse, "This wall is too hard to %s.", verb);
            return 0;
        }
    }
    if (Fumbling && !rn2(3)) {
        switch (rn2(3)) {
        case 0:
            if (!welded(uwep)) {
                struct obj *otmp = uwep;
                pline(msgc_substitute, "You fumble and drop your %s.",
                      xname(otmp));
                unwield_silently(otmp);
                dropx(otmp);
            } else {
                if (u.usteed)
                    pline(msgc_substitute, "Your %s %s and %s %s!", xname(uwep),
                          otense(uwep, "bounce"), otense(uwep, "hit"),
                          mon_nam(u.usteed));
                else
                    pline(msgc_substitute, "Ouch!  Your %s %s and %s you!",
                          xname(uwep), otense(uwep, "bounce"),
                          otense(uwep, "hit"));
                set_wounded_legs(RIGHT_SIDE, 5 + rnd(5));
            }
            break;
        case 1:
            pline(msgc_failrandom, "Bang!  You hit with the broad side of %s!",
                  the(xname(uwep)));
            break;
        default:
            pline(msgc_failrandom, "Your swing misses its mark.");
            break;
        }
        return 0;
    }

    u.uoccupation_progress[tos_dig] +=
        10 + rn2(5) + abon() + uwep->spe - greatest_erosion(uwep) + u.udaminc;
    /* TODO: This formula looks /very/ suspicious, becuse the exponential
       factor is going to override almost anyting else. */
    if (Race_if(PM_DWARF))
        u.uoccupation_progress[tos_dig] *= 2;
    if (down) {
        struct trap *ttmp;

        if (u.uoccupation_progress[tos_dig] > 250) {
            dighole(FALSE, FALSE);
            u.uoccupation_progress[tos_dig] = 0;
            return 0;   /* done with digging */
        }

        if (u.uoccupation_progress[tos_dig] <= 50 ||
            ((ttmp = t_at(level, dpx, dpy)) != 0 &&
             (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT ||
              ttmp->ttyp == TRAPDOOR || ttmp->ttyp == HOLE)))
            return 1;

        if (IS_ALTAR(loc->typ)) {
            altar_wrath(dpx, dpy);
            angry_priest();
        }

        if (!dighole(TRUE, FALSE)) {
            action_completed();
            u.uoccupation_progress[tos_dig] = 0;
        }
        /* No digging state reset, but we return 0. This way, we can continue
           digging, but the user will be prompted. */
        return 0;
    }

    if (u.uoccupation_progress[tos_dig] > 100) {
        const char *digtxt, *dmgtxt = NULL;
        struct obj *obj;
        boolean shopedge = *in_rooms(level, dpx, dpy, SHOPBASE);

        if ((obj = sobj_at(STATUE, level, dpx, dpy)) != 0) {
            if (break_statue(obj))
                digtxt = "The statue shatters.";
            else
                /* it was a statue trap; break_statue() printed a message and
                   updated the screen */
                digtxt = NULL;
        } else if ((obj = sobj_at(BOULDER, level, dpx, dpy)) != 0) {
            struct obj *bobj;

            fracture_rock(obj);
            if ((bobj = sobj_at(BOULDER, level, dpx, dpy)) != 0) {
                /* another boulder here, restack it to the top */
                obj_extract_self(bobj);
                place_object(bobj, level, dpx, dpy);
            }
            digtxt = "The boulder falls apart.";
        } else if (loc->typ == STONE || loc->typ == SCORR ||
                   IS_TREE(loc->typ)) {
            if (Is_earthlevel(&u.uz)) {
                if (uwep->blessed && !rn2(3)) {
                    mkcavearea(FALSE);
                    goto cleanup;
                } else if ((uwep->cursed && !rn2(4)) ||
                           (!uwep->blessed && !rn2(6))) {
                    mkcavearea(TRUE);
                    goto cleanup;
                }
            }
            if (IS_TREE(loc->typ)) {
                digtxt = "You cut down the tree.";
                loc->typ = ROOM;
                /* Don't bother with a custom RNG for this: it would desync
                   between kicked fruits and cut-down fruits. (And if you think
                   that's irrelevant, I agree with you, but that implies that
                   there's no purpose in having a custom RNG in the first
                   place.) */
                if (!rn2(5))
                    rnd_treefruit_at(dpx, dpy);
            } else {
                digtxt = "You succeed in cutting away some rock.";
                loc->typ = CORR;
            }
        } else if (IS_WALL(loc->typ)) {
            if (shopedge) {
                add_damage(dpx, dpy, 10L * ACURRSTR);
                dmgtxt = "damage";
            }
            if (level->flags.is_maze_lev) {
                loc->typ = ROOM;
            } else if (level->flags.is_cavernous_lev && !in_town(dpx, dpy)) {
                loc->typ = CORR;
            } else {
                loc->typ = DOOR;
                loc->doormask = D_NODOOR;
            }
            digtxt = "You make an opening in the wall.";
        } else if (loc->typ == SDOOR) {
            cvt_sdoor_to_door(loc, &u.uz);      /* ->typ = DOOR */
            digtxt = "You break through a secret door!";
            if (!(loc->doormask & D_TRAPPED))
                loc->doormask = D_BROKEN;
        } else if (closed_door(level, dpx, dpy)) {
            digtxt = "You break through the door.";
            if (shopedge) {
                add_damage(dpx, dpy, 400L);
                dmgtxt = "break";
            }
            if (!(loc->doormask & D_TRAPPED))
                loc->doormask = D_BROKEN;
        } else
            return 0;   /* statue or boulder got taken */

        if (!does_block(level, dpx, dpy))
            unblock_point(dpx, dpy);    /* vision: can see through */
        if (Blind)
            feel_location(dpx, dpy);
        else
            newsym(dpx, dpy);
        if (digtxt)
            pline(msgc_actionok, "%s", digtxt);      /* after newsym */
        if (dmgtxt)
            pay_for_damage(dmgtxt, FALSE);

        if (Is_earthlevel(&u.uz) && !rn2(3)) {
            struct monst *mtmp;

            switch (rn2(2)) {
            case 0:
                mtmp =
                    makemon(&mons[PM_EARTH_ELEMENTAL], level, dpx, dpy,
                            NO_MM_FLAGS);
                break;
            default:
                mtmp = makemon(&mons[PM_XORN], level, dpx, dpy, NO_MM_FLAGS);
                break;
            }
            if (mtmp)
                pline(msgc_levelwarning,
                      "The debris from your digging comes to life!");
        }
        if (IS_DOOR(loc->typ) && (loc->doormask & D_TRAPPED)) {
            loc->doormask = D_NODOOR;
            b_trapped("door", 0);
            newsym(dpx, dpy);
        }
    cleanup:
        u.uoccupation_progress[tos_dig] = 0;
        return 0;
    } else {    /* not enough effort has been spent yet */
        static const char *const d_target[6] = {
            "", "rock", "statue", "boulder", "door", "tree"
        };
        int dig_target = dig_typ(uwep, dpx, dpy);

        if (IS_WALL(loc->typ) || dig_target == DIGTYP_DOOR) {
            if (*in_rooms(level, dpx, dpy, SHOPBASE)) {
                /* TODO: The timing properties here are just bizarre. */
                pline(msgc_cancelled1, "This %s seems too hard to %s.",
                      IS_DOOR(loc->typ) ? "door" : "wall", verb);
                return 0;
            }
        } else if (!IS_ROCK(loc->typ) && dig_target == DIGTYP_ROCK)
            return 0;   /* statue or boulder got taken */
        if (new_dig)
            pline(msgc_occstart, "You hit the %s with all your might.",
                  d_target[dig_target]);
    }
    return 1;
}

/* When will hole be finished? Very rough indication used by shopkeeper. */
int
holetime(void)
{
    if (!u.uoccupation_progress[tos_dig] || !*u.ushops)
        return -1;
    return (250 - u.uoccupation_progress[tos_dig]) / 20;
}

/* Return typ of liquid to fill a hole with, or ROOM, if no liquid nearby */
static schar
fillholetyp(int x, int y)
{
    int x1, y1;
    int lo_x = max(0, x - 1), hi_x = min(x + 1, COLNO - 1), lo_y =
        max(0, y - 1), hi_y = min(y + 1, ROWNO - 1);
    int pool_cnt = 0, moat_cnt = 0, lava_cnt = 0;

    for (x1 = lo_x; x1 <= hi_x; x1++)
        for (y1 = lo_y; y1 <= hi_y; y1++)
            if (is_moat(level, x1, y1))
                moat_cnt++;
            else if (is_pool(level, x1, y1))
                /* This must come after is_moat since moats are pools but not
                   vice-versa. */
                pool_cnt++;
            else if (is_lava(level, x1, y1))
                lava_cnt++;
    pool_cnt /= 3;      /* not as much liquid as the others */

    if (lava_cnt > moat_cnt + pool_cnt && rn2(lava_cnt + 1))
        return LAVAPOOL;
    else if (moat_cnt > 0 && rn2(moat_cnt + 1))
        return MOAT;
    else if (pool_cnt > 0 && rn2(pool_cnt + 1))
        return POOL;
    else
        return ROOM;
}

void
digactualhole(int x, int y, struct monst *madeby, int ttyp)
{
    struct obj *oldobjs, *newobjs;
    struct trap *ttmp;
    const char *surface_type;
    struct rm *loc = &level->locations[x][y];
    boolean shopdoor;
    struct monst *mtmp = m_at(level, x, y);     /* may be madeby */
    boolean madeby_u = (madeby == BY_YOU);
    boolean madeby_obj = (madeby == BY_OBJECT);
    boolean at_u = (x == u.ux) && (y == u.uy);
    boolean wont_fall = Levitation || Flying;

    if (u.utrap && u.utraptype == TT_INFLOOR)
        u.utrap = 0;

    /* these furniture checks were in dighole(), but wand breaking bypasses
       that routine and calls us directly */
    if (IS_FOUNTAIN(loc->typ)) {
        dogushforth(FALSE);
        SET_FOUNTAIN_WARNED(x, y);      /* force dryup */
        dryup(x, y, madeby_u);
        return;
    } else if (IS_SINK(loc->typ)) {
        breaksink(x, y);
        return;
    } else if (loc->typ == DRAWBRIDGE_DOWN || (is_drawbridge_wall(x, y) >= 0)) {
        int bx = x, by = y;

        /* if under the portcullis, the bridge is adjacent */
        find_drawbridge(&bx, &by);
        destroy_drawbridge(bx, by);
        return;
    }

    if (ttyp != PIT && !can_dig_down(level)) {
        impossible("digactualhole: can't dig %s on this level.",
                   trapexplain[ttyp - 1]);
        ttyp = PIT;
    }

    /* maketrap(level, ) might change it, also, in this situation, surface()
       returns an inappropriate string for a grave */
    if (IS_GRAVE(loc->typ))
        surface_type = "grave";
    else
        surface_type = surface(x, y);
    shopdoor = IS_DOOR(loc->typ) && *in_rooms(level, x, y, SHOPBASE);
    oldobjs = level->objects[x][y];
    ttmp = maketrap(level, x, y, ttyp, rng_main);
    if (!ttmp)
        return;
    newobjs = level->objects[x][y];
    ttmp->tseen = (madeby_u || cansee(x, y));
    ttmp->madeby_u = madeby_u;
    newsym(ttmp->tx, ttmp->ty);

    if (ttyp == PIT) {
        /* Assume that long worms can't dig pits. */

        if (madeby_u) {
            pline(msgc_actionok, "You dig a pit in the %s.", surface_type);
            if (shopdoor)
                pay_for_damage("ruin", FALSE);
        } else if (!madeby_obj && canseemon(madeby))
            pline(msgc_monneutral, "%s digs a pit in the %s.", Monnam(madeby),
                  surface_type);
        else if (cansee(x, y) && flags.verbose)
            pline(msgc_youdiscover, "A pit appears in the %s.", surface_type);

        if (at_u) {
            if (!wont_fall) {
                if (!Passes_walls)
                    u.utrap = rn1(4, 2);
                u.utraptype = TT_PIT;
                turnstate.vision_full_recalc = TRUE; /* vision limits change */
            } else
                u.utrap = 0;
            if (oldobjs != newobjs)     /* something unearthed */
                pickup(1, flags.interaction_mode);      /* detects pit */
        } else if (mtmp) {
            if (is_flyer(mtmp->data) || is_floater(mtmp->data)) {
                if (canseemon(mtmp))
                    pline(msgc_monneutral, "%s %s over the pit.", Monnam(mtmp),
                          (is_flyer(mtmp->data)) ? "flies" : "floats");
            } else if (mtmp != madeby)
                mintrap(mtmp);
        }
    } else {    /* was TRAPDOOR now a HOLE */

        if (madeby_u)
            pline(msgc_actionok, "You dig a hole through the %s.",
                  surface_type);
        else if (!madeby_obj && canseemon(madeby))
            pline(msgc_monneutral, "%s digs a hole through the %s.",
                  Monnam(madeby), surface_type);
        else if (cansee(x, y))
            pline(msgc_youdiscover, "A hole appears in the %s.", surface_type);

        if (at_u) {
            if (!u.ustuck && !wont_fall && !next_to_u()) {
                pline(madeby_u ? msgc_failcurse : msgc_playerimmune,
                      "You are jerked back by your pet!");
                wont_fall = TRUE;
            }

            /* Floor objects get a chance of falling down.  The case where the
               hero does NOT fall down is treated here.  The case where the
               hero does fall down is treated in goto_level(). */
            if (u.ustuck || wont_fall) {
                if (newobjs)
                    impact_drop(NULL, x, y, 0);
                if (oldobjs != newobjs)
                    pickup(1, flags.interaction_mode);
                if (shopdoor && madeby_u)
                    pay_for_damage("ruin", FALSE);

            } else {
                d_level newlevel;

                if (*u.ushops && madeby_u)
                    shopdig(1); /* shk might snatch pack */
                /* handle earlier damage, eg breaking wand of digging */
                else if (!madeby_u)
                    pay_for_damage("dig into", TRUE);

                pline(madeby_u ? msgc_actionok : msgc_statusbad,
                      "You fall through...");
                /* Earlier checks must ensure that the destination level exists
                   and is in the present dungeon. */
                newlevel.dnum = u.uz.dnum;
                newlevel.dlevel = u.uz.dlevel + 1;
                goto_level(&newlevel, FALSE, TRUE, FALSE);
                /* messages for arriving in special rooms */
                spoteffects(FALSE);
            }
        } else {
            if (shopdoor && madeby_u)
                pay_for_damage("ruin", FALSE);
            if (newobjs)
                impact_drop(NULL, x, y, 0);
            /* TODO: Figure out when this is called. Digging down while engulfed
               is one potential case (although I'm not sure that's possible),
               but the code sort-of implise there are others. */
            if (mtmp) {
                /* [don't we need special sokoban handling here?] */
                if (is_flyer(mtmp->data) || is_floater(mtmp->data) ||
                    mtmp->data == &mons[PM_WUMPUS] ||
                    (mtmp->wormno && count_wsegs(mtmp) > 5) ||
                    mtmp->data->msize >= MZ_HUGE)
                    return;
                if (mtmp == u.ustuck)   /* probably a vortex */
                    return;     /* temporary? kludge */

                if (teleport_pet(mtmp, FALSE)) {
                    d_level tolevel;

                    if (Is_stronghold(&u.uz)) {
                        assign_level(&tolevel, &valley_level);
                    } else if (Is_botlevel(&u.uz)) {
                        /* TODO: I can't figure out what this does or if the
                           canseemon() is the correct check. */
                        if (canseemon(mtmp))
                            pline(msgc_noidea, "%s avoids the trap.",
                                  Monnam(mtmp));
                        return;
                    } else {
                        get_level(&tolevel, depth(&u.uz) + 1);
                    }
                    if (mtmp->isshk)
                        make_angry_shk(mtmp, 0, 0);
                    migrate_to_level(mtmp, ledger_no(&tolevel), MIGR_RANDOM,
                                     NULL);
                }
            }
        }
    }
}

/* Returns TRUE if digging succeeded, FALSE otherwise.

   Call only if the digging is performed intentionally by the player. */
static boolean
dighole(boolean pit_only, boolean instant)
{
    struct trap *ttmp = t_at(level, u.ux, u.uy);
    struct rm *loc = &level->locations[u.ux][u.uy];
    struct obj *boulder_here;
    schar typ;
    boolean nohole = !can_dig_down(level);

    /* TODO: The checks in dig() see if it's possible to make any progress
       digging, and cancel after 2 actions if it isn't. The checks here see if
       it's possible to complete digging, some of which duplicate the checks in
       dig(). Although the distinction is code-wise clear, it doesn't make a lot
       of gameplay sense; we should probably have just have typical cancelled
       and failed checks on the turn where digging starts (via a function), and
       then check again every turn of digging in case things change while we're
       digging.

       This function also handles digging completing instantly as a result of a
       wand zap or the like, in which case instant is true. In such a case, the
       checks haven't been done yet, and we return msg_yafm in cases where the
       player was aware that the digging would fail (it's not an action that
       should be cancelled, as it still wastes wand charges / Pw). */
    if ((ttmp &&
         (ttmp->ttyp == MAGIC_PORTAL || ttmp->ttyp == VIBRATING_SQUARE ||
          nohole)) || (IS_ROCK(loc->typ) && loc->typ != SDOOR &&
                       (loc->wall_info & W_NONDIGGABLE) != 0)) {
        pline((ttmp && instant) ? msgc_yafm : msgc_failcurse,
              "The %s here is too hard to dig in.",
              surface(u.ux, u.uy));

    } else if (is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)) {
        pline(msgc_badidea,
              "The %s sloshes furiously for a moment, then subsides.",
              is_lava(level, u.ux, u.uy) ? "lava" : "water");
        wake_nearby(FALSE);  /* splashing */

    } else if (loc->typ == DRAWBRIDGE_DOWN ||
               (is_drawbridge_wall(u.ux, u.uy) >= 0)) {
        /* drawbridge_down is the platform crossing the moat when the bridge is
           extended; drawbridge_wall is the open "doorway" or closed "door"
           where the portcullis/mechanism is located */
        if (pit_only) {
            pline(instant ? msgc_yafm : msgc_failcurse,
                  "The drawbridge seems too hard to dig through.");
            return FALSE;
        } else {
            int x = u.ux, y = u.uy;

            /* if under the portcullis, the bridge is adjacent */
            find_drawbridge(&x, &y);
            destroy_drawbridge(x, y);
            return TRUE;
        }

    } else if ((boulder_here = sobj_at(BOULDER, level, u.ux, u.uy)) != 0) {
        if (ttmp && (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT) && rn2(2)) {
            pline(msgc_consequence, "The boulder settles into the pit.");
            ttmp->ttyp = PIT;   /* crush spikes */
        } else {
            /*
             * digging makes a hole, but the boulder immediately
             * fills it.  Final outcome:  no hole, no boulder.
             */
            pline(msgc_consequence, "KADOOM! The boulder falls in!");
            delfloortrap(level, ttmp);
        }
        delobj(boulder_here);
        return TRUE;

    } else if (IS_GRAVE(loc->typ)) {
        digactualhole(u.ux, u.uy, BY_YOU, PIT);
        dig_up_grave();
        return TRUE;
    } else if (loc->typ == DRAWBRIDGE_UP) {
        /* must be floor or ice, other cases handled above */
        /* dig "pit" and let fluid flow in (if possible) */
        typ = fillholetyp(u.ux, u.uy);

        if (typ == ROOM) {
            /* We can't dig a hole here since that will destroy the drawbridge.
               The following is a cop-out. --dlc

               And so the message channel is also a cop-out. -- AIS */
            pline(msgc_failcurse, "The %s here is too hard to dig in.",
                  surface(u.ux, u.uy));
            return FALSE;
        }

        loc->drawbridgemask = (loc->drawbridgemask & ~DB_UNDER);
        loc->drawbridgemask |= (typ == LAVAPOOL) ? DB_LAVA : DB_MOAT;

    liquid_flow:
        if (ttmp)
            delfloortrap(level, ttmp);
        /* if any objects were frozen here, they're released now */
        unearth_objs(level, u.ux, u.uy);

        pline(msgc_badidea, "As you dig, the hole fills with %s!",
              typ == LAVAPOOL ? "lava" : "water");
        if (!Levitation && !Flying) {
            if (typ == LAVAPOOL)
                lava_effects();
            else if (!Wwalking)
                drown();
        }
        return TRUE;

        /* the following two are here for the wand of digging */
    } else if (IS_THRONE(loc->typ)) {
        pline(instant ? msgc_yafm : msgc_impossible,
              "The throne is too hard to break apart.");

    } else if (IS_ALTAR(loc->typ)) {
        pline(instant ? msgc_yafm : msgc_impossible,
              "The altar is too hard to break apart.");

    } else {
        typ = fillholetyp(u.ux, u.uy);

        if (typ != ROOM) {
            loc->typ = typ;
            goto liquid_flow;
        }

        /* finally we get to make a hole */
        if (nohole || pit_only)
            digactualhole(u.ux, u.uy, BY_YOU, PIT);
        else
            digactualhole(u.ux, u.uy, BY_YOU, HOLE);

        return TRUE;
    }

    return FALSE;
}

static void
dig_up_grave(void)
{
    struct obj *otmp;

    /* Grave-robbing is frowned upon... */
    exercise(A_WIS, FALSE);
    if (Role_if(PM_ARCHEOLOGIST)) {
        adjalign(-sgn(u.ualign.type) * 3);
        pline(msgc_alignchaos, "You feel like a despicable grave-robber!");
    } else if (Role_if(PM_SAMURAI)) {
        adjalign(-sgn(u.ualign.type));
        pline(msgc_alignchaos, "You disturb the honorable dead!");
    } else if ((u.ualign.type == A_LAWFUL) && (u.ualign.record > -10)) {
        adjalign(-sgn(u.ualign.type));
        pline(msgc_alignbad, "You have violated the sanctity of this grave!");
    }

    switch (rn2(5)) {
    case 0:
    case 1:
        pline(msgc_actionok, "You unearth a corpse.");
        if ((otmp = mk_tt_object(level, CORPSE, u.ux, u.uy)))
            otmp->age -= 100; /* this is an *OLD* corpse */
        break;
    case 2:
        if (!Blind)
            pline(msgc_substitute, Hallucination ? "Dude!  The living dead!" :
                  "The grave's owner is very upset!");
        makemon(mkclass(&u.uz, S_ZOMBIE, 0, rng_main), level,
                u.ux, u.uy, NO_MM_FLAGS);
        break;
    case 3:
        if (!Blind)
            pline(msgc_substitute, Hallucination ? "I want my mummy!" :
                  "You've disturbed a tomb!");
        makemon(mkclass(&u.uz, S_MUMMY, 0, rng_main), level,
                u.ux, u.uy, NO_MM_FLAGS);
        break;
    default:
        /* No corpse */
        pline(msgc_noconsequence, "The grave seems unused.  Strange....");
        break;
    }
    level->locations[u.ux][u.uy].typ = ROOM;
    del_engr_at(level, u.ux, u.uy);
    newsym(u.ux, u.uy);
    return;
}

int
use_pick_axe(struct obj *obj, const struct nh_cmd_arg *arg)
{
    boolean ispick;
    const char *qbuf;
    const char *verb;
    int wtstatus;
    schar dx, dy, dz;
    int rx, ry;
    struct rm *loc;
    int dig_target;
    const char *verbing;

    ispick = is_pick(obj);
    verb = ispick ? "dig" : "chop";
    verbing = ispick ? "digging" : "chopping";

    wtstatus = wield_tool(obj, "preparing to dig", occ_dig);
    if (wtstatus & 2)
        return 1;
    else if (!(wtstatus & 1))
        return 0;

    if (u.utrap && u.utraptype == TT_WEB) {
        pline(msgc_cancelled, "%s you can't %s while entangled in a web.",
              turnstate.continue_message ? "Unfortunately," : "But", verb);
        return 0;
    }

    qbuf = msgprintf("In what direction do you want to %s?", verb);
    if (!getargdir(arg, qbuf, &dx, &dy, &dz))
        return 0;

    if (Engulfed) {
        enum attack_check_status attack_status =
            attack(u.ustuck, dx, dy, FALSE);
        if (attack_status != ac_continue)
            return attack_status != ac_cancel;
    }

    if (Underwater) {
        pline(msgc_cancelled1, "Turbulence torpedoes your %s attempts.",
              verbing);
    } else if (dz < 0) {
        if (Levitation || Flying)
            pline(msgc_cancelled1, "You don't have enough leverage.");
        else
            pline(msgc_cancelled1, "You can't reach the %s.",
                  ceiling(u.ux, u.uy));
    } else if (!dx && !dy && !dz) {
        const char *buf;
        int dam;

        dam = rnd(2) + dbon() + obj->spe;
        if (dam <= 0)
            dam = 1;
        pline(msgc_badidea, "You hit yourself with %s.", yname(uwep));
        buf = msgprintf("%s own %s", uhis(), OBJ_NAME(objects[obj->otyp]));
        losehp(dam, killer_msg(DIED, buf));
        return 1;
    } else if (dz == 0) {

        /* Which message channel to use if the direction makes no sense. (If
           stunned or confused, digging in an implausible direction can happen
           through no fault of the player.) */
        enum msg_channel bad_msgc =
            (Stunned || Confusion) ? msgc_failrandom : msgc_cancelled1;
        enum msg_channel worse_msgc =
            (Stunned || Confusion) ? msgc_substitute : msgc_badidea;
             
        if (Stunned || (Confusion && !rn2(5)))
            confdir(&dx, &dy);
        rx = u.ux + dx;
        ry = u.uy + dy;
        if (!isok(rx, ry)) {
            pline(bad_msgc, "Clash!");
            return 1;
        }
        loc = &level->locations[rx][ry];
        if (MON_AT(level, rx, ry)) {
            enum attack_check_status attack_status =
                attack(m_at(level, rx, ry), dx, dy, FALSE);
            if (attack_status != ac_continue)
                return attack_status != ac_cancel;
        }
        dig_target = dig_typ(obj, rx, ry);
        if (dig_target == DIGTYP_UNDIGGABLE) {
            /* ACCESSIBLE or POOL */
            struct trap *trap = t_at(level, rx, ry);

            if (trap && trap->ttyp == WEB) {
                if (!trap->tseen) {
                    seetrap(trap);
                    pline(msgc_youdiscover, "There is a spider web there!");
                }
                pline(msgc_substitute, "Your %s entangled in the web.",
                      aobjnam(obj, "become"));
                /* you ought to be able to let go; tough luck */
                /* (maybe `move_into_trap()' would be better) */
                helpless(dice(2, 2), hr_busy, "stuck in a spider web",
                         "You pull free.");
            } else if (loc->typ == IRONBARS) {
                pline(worse_msgc, "Clang!");
                wake_nearby(TRUE);
            } else if (IS_TREE(loc->typ))
                pline(bad_msgc, "You need an axe to cut down a tree.");
            else if (IS_ROCK(loc->typ))
                pline(bad_msgc, "You need a pick to dig rock.");
            else if (!ispick &&
                     (sobj_at(STATUE, level, rx, ry) ||
                      sobj_at(BOULDER, level, rx, ry))) {
                boolean vibrate = !rn2(3);

                pline(worse_msgc, "Sparks fly as you whack the %s.%s",
                      sobj_at(STATUE, level, rx, ry) ? "statue" : "boulder",
                      vibrate ? " The axe-handle vibrates violently!" : "");
                if (vibrate)
                    losehp(2, killer_msg(DIED, "axing a hard object"));
            } else
                pline(msgc_notarget, "You swing your %s through thin air.",
                      aobjnam(obj, NULL));
        } else {
            static const char *const d_action[6] = {
                "swinging",
                "digging",
                "chipping the statue",
                "hitting the boulder",
                "chopping at the door",
                "cutting the tree"
            };
            if (u.utracked_location[tl_dig].x != rx ||
                u.utracked_location[tl_dig].y != ry ||
                u.uoccupation_progress[tos_dig] == 0) {
                u.utracked_location[tl_dig].x = rx;
                u.utracked_location[tl_dig].y = ry;
                u.uoccupation_progress[tos_dig] = 0;
                pline(msgc_occstart, "You start %s.", d_action[dig_target]);
            } else {
                if (turnstate.continue_message)
                    pline(msgc_occstart, "You continue %s.",
                          d_action[dig_target]);
            }
            one_occupation_turn(dig, verbing, occ_dig);
        }

        /* The remaining cases handle digging downwards. */
    } else if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)) {
        /* it must be air -- water checked above */
        pline(msgc_cancelled1, "You swing your %s through thin air.",
              aobjnam(obj, NULL));
    } else if (!can_reach_floor()) {
        pline(msgc_cancelled1, "You can't reach the %s.", surface(u.ux, u.uy));
    } else if (is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)) {
        pline(msgc_badidea, "You swing your %s through the %s below.",
              aobjnam(obj, NULL),
              is_pool(level, u.ux, u.uy) ? "water" : "lava");

        /* TODO: This has to be code duplication, surely. (It's not like you
           can use the Book of the Dead as a pick-axe anyway, AFAIK.) */
        if (is_lava(level, u.ux, u.uy) && is_organic(obj) &&
            !obj->oerodeproof) {
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                if (!Blind)
                    pline(msgc_noconsequence,
                          "%s glows a strange %s, but remains intact.",
                          The(xname(obj)), hcolor("dark red"));
            } else {
                pline(msgc_itemloss, "It bursts into flame!");

                unwield_silently(obj);
                setunequip(obj);
                useupall(obj);
            }
        }
        /* TODO: No rusting when digging water? */
    } else if (!ispick) {
        pline(msgc_yafm, "Your %s merely scratches the %s.",
              aobjnam(obj, NULL), surface(u.ux, u.uy));
        u_wipe_engr(3);
    } else {
        struct trap *t = t_at(level, u.ux, u.uy);

        if (t && (t->ttyp == HOLE || (t->ttyp == TRAPDOOR && t->tseen))) {
            pline(msgc_cancelled, "There's already a %s in the floor here.",
                  t->ttyp == HOLE ? "hole" : "trap door");
            return 0;
        }
        if (u.utracked_location[tl_dig].x != u.ux ||
            u.utracked_location[tl_dig].y != u.uy ||
            u.uoccupation_progress[tos_dig] == 0) {
            u.utracked_location[tl_dig].x = u.ux;
            u.utracked_location[tl_dig].y = u.uy;
            u.uoccupation_progress[tos_dig] = 0;
            pline(msgc_occstart, "You start %s downward.", verbing);
            if (*u.ushops)
                shopdig(0);
        } else if (turnstate.continue_message)
            pline(msgc_occstart, "You continue %s downward.", verbing);
        if (t && t->ttyp == TRAPDOOR) {
            spoteffects(TRUE);  /* trigger the trap */
            return 1;
        }
        one_occupation_turn(dig, verbing, occ_dig);
    }
    return 1;
}

/* The player's been damaging or otherwise messing with a map location (most
   likely digging, but it could also be picking locks). If mtmp is set, that
   means a watchman saw it.  Otherwise, this function checks itself to see if
   anyone saw it. */
void
watch_warn(struct monst *mtmp, xchar x, xchar y, boolean zap)
{
    struct rm *loc = &level->locations[x][y];

    if (in_town(x, y) &&
        (closed_door(level, x, y) || loc->typ == SDOOR || IS_WALL(loc->typ) ||
         IS_FOUNTAIN(loc->typ) || IS_TREE(loc->typ))) {
        if (!mtmp) {
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if ((mtmp->data == &mons[PM_WATCHMAN] ||
                     mtmp->data == &mons[PM_WATCH_CAPTAIN]) &&
                    m_canseeu(mtmp) && mtmp->mpeaceful)
                    break;
            }
        }

        if (mtmp) {
            /* Previously, the warning was a property of the door / digging
               attempt, but that doesn't make any sense. The warning has been
               changed to a property of the monster that caught you
               (mtmp->msuspicious); if the same Watch member catches you twice,
               you're in trouble. */

            if (zap || mtmp->msuspicious) {

                verbalize(msgc_npcanger, "Halt, vandal!  You're under arrest!");
                angry_guards(!canhear());

            } else {
                const char *str;

                mtmp->msuspicious = 1;

                if (IS_DOOR(loc->typ))
                    str = "door";
                else if (IS_TREE(loc->typ))
                    str = "tree";
                else if (IS_ROCK(loc->typ))
                    str = "wall";
                else if (IS_FOUNTAIN(loc->typ))
                    str = "fountain";
                else
                    str = "architecture"; /* should be unreachable */

                pline(msgc_npcvoice,
                      "%s sees what you're doing, and yells at you.",
                      Amonnam(mtmp));
                verbalize(msgc_npcvoice, "Hey, stop messing with that %s!",
                          str);
            }
            action_interrupted();
        }
    }
}


/* Return TRUE if monster died, FALSE otherwise.  Called from m_move(). */
boolean
mdig_tunnel(struct monst *mtmp)
{
    struct rm *here;
    int pile = rnd(12);

    here = &level->locations[mtmp->mx][mtmp->my];
    if (here->typ == SDOOR)
        cvt_sdoor_to_door(here, &mtmp->dlevel->z);      /* ->typ = DOOR */

    /* Eats away door if present & closed or locked */
    if (closed_door(level, mtmp->mx, mtmp->my)) {
        if (*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE))
            add_damage(mtmp->mx, mtmp->my, 0L);
        unblock_point(mtmp->mx, mtmp->my);      /* vision */
        if (here->doormask & D_TRAPPED) {
            here->doormask = D_NODOOR;
            if (mb_trapped(mtmp)) {     /* mtmp is killed */
                newsym(mtmp->mx, mtmp->my);
                return TRUE;
            }
        } else {
            if (!rn2(3) && flags.verbose)       /* not too often.. */
                pline(msgc_levelsound, "You feel an unexpected draft.");
            here->doormask = D_BROKEN;
        }
        newsym(mtmp->mx, mtmp->my);
        return FALSE;
    } else if (!IS_ROCK(here->typ) && !IS_TREE(here->typ))      /* no dig */
        return FALSE;

    /* Only rock, trees, and walls fall through to this point. */
    if ((here->wall_info & W_NONDIGGABLE) != 0) {
        impossible("mdig_tunnel:  %s at (%d,%d) is undiggable",
                   (IS_WALL(here->typ) ? "wall" : "stone"), (int)mtmp->mx,
                   (int)mtmp->my);
        return FALSE;   /* still alive */
    }

    if (IS_WALL(here->typ)) {
        /* KMH -- Okay on arboreal levels (room walls are still stone) */
        if (flags.verbose && !rn2(5))
            You_hear(msgc_levelsound, "crashing rock.");
        if (*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE))
            add_damage(mtmp->mx, mtmp->my, 0L);
        if (level->flags.is_maze_lev) {
            here->typ = ROOM;
        } else if (level->flags.is_cavernous_lev &&
                   !in_town(mtmp->mx, mtmp->my)) {
            here->typ = CORR;
        } else {
            here->typ = DOOR;
            here->doormask = D_NODOOR;
        }
    } else if (IS_TREE(here->typ)) {
        here->typ = ROOM;
        if (pile && pile < 5)
            rnd_treefruit_at(mtmp->mx, mtmp->my);
    } else {
        here->typ = CORR;
        if (pile && pile < 5)
            mksobj_at((pile == 1) ? BOULDER : ROCK, level, mtmp->mx, mtmp->my,
                      TRUE, FALSE, rng_main);
    }
    newsym(mtmp->mx, mtmp->my);
    if (!sobj_at(BOULDER, level, mtmp->mx, mtmp->my))
        unblock_point(mtmp->mx, mtmp->my);      /* vision */

    return FALSE;
}


/* zap_dig: digging via wand zap or spell cast

   dig for digdepth positions; also down on request of Lennart Augustsson. */
void
zap_dig(schar dx, schar dy, schar dz)
{
    struct rm *room;
    struct monst *mtmp;
    struct obj *otmp;
    struct tmp_sym *tsym;
    int zx, zy, digdepth;
    boolean shopdoor, shopwall, maze_dig;

    /* swallowed */
    if (Engulfed) {
        mtmp = u.ustuck;

        if (!is_whirly(mtmp->data)) {
            if (is_animal(mtmp->data))
                pline(msgc_actionok, "You pierce %s %s wall!",
                      s_suffix(mon_nam(mtmp)), mbodypart(mtmp, STOMACH));
            mtmp->mhp = 1;      /* almost dead */
            expels(mtmp, mtmp->data, !is_animal(mtmp->data));
        }
        return;
    }

    /* up or down */
    if (dz) {
        if (!Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz) && !Underwater) {
            if (dz < 0 || On_stairs(u.ux, u.uy)) {
                if (On_stairs(u.ux, u.uy))
                    pline(msgc_badidea,
                          "The beam bounces off the %s and hits the %s.",
                          ((u.ux == level->dnladder.sx &&
                            u.uy == level->dnladder.sy) ||
                           (u.ux == level->upladder.sx &&
                            u.uy == level->upladder.sy)) ? "ladder" : "stairs",
                          ceiling(u.ux, u.uy));
                pline(msgc_badidea, "You loosen a rock from the %s.",
                      ceiling(u.ux, u.uy));
                pline(msgc_nonmonbad, "It falls on your %s!", body_part(HEAD));
                losehp(rnd((uarmh && is_metallic(uarmh)) ? 2 : 6),
                       killer_msg(DIED, msgcat_many(
                                      "collapsing the ceiling on top of ",
                                      uhim(), "self", NULL)));
                otmp = mksobj_at(ROCK, level, u.ux, u.uy, FALSE, FALSE,
                                 rng_main);
                if (otmp) {
                    examine_object(otmp);     /* set dknown, maybe bknown */
                    stackobj(otmp);
                }
                newsym(u.ux, u.uy);
            } else {
                watch_warn(NULL, u.ux, u.uy, TRUE);
                dighole(FALSE, TRUE);
            }
        }
        return;
    }

    /* normal case: digging across the level */
    shopdoor = shopwall = FALSE;
    maze_dig = level->flags.is_maze_lev && !Is_earthlevel(&u.uz);
    zx = u.ux + dx;
    zy = u.uy + dy;
    digdepth = rn1(18, 8);
    tsym = tmpsym_init(DISP_BEAM, dbuf_effect(E_MISC, E_digbeam));
    while (--digdepth >= 0) {
        if (!isok(zx, zy))
            break;
        room = &level->locations[zx][zy];
        tmpsym_at(tsym, zx, zy);
        win_delay_output();     /* wait a little bit */
        if (closed_door(level, zx, zy) || room->typ == SDOOR) {
            if (*in_rooms(level, zx, zy, SHOPBASE)) {
                add_damage(zx, zy, 400L);
                shopdoor = TRUE;
            }
            if (room->typ == SDOOR)
                room->typ = DOOR;
            else if (cansee(zx, zy))
                pline(msgc_actionok, "The door is razed!");
            watch_warn(NULL, zx, zy, TRUE);
            room->doormask = D_NODOOR;
            unblock_point(zx, zy);      /* vision */
            digdepth -= 2;
            if (maze_dig)
                break;
        } else if (maze_dig) {
            /* See dig() for comments about use of msgc_failcurse for undiggable
               walls */
            if (IS_WALL(room->typ)) {
                if (!(room->wall_info & W_NONDIGGABLE)) {
                    if (*in_rooms(level, zx, zy, SHOPBASE)) {
                        add_damage(zx, zy, 200L);
                        shopwall = TRUE;
                    }
                    room->typ = ROOM;
                    unblock_point(zx, zy);      /* vision */
                } else if (!Blind)
                    pline(msgc_failcurse, "The wall glows then fades.");
                break;
            } else if (IS_TREE(room->typ)) {    /* check trees before stone */
                if (!(room->wall_info & W_NONDIGGABLE)) {
                    room->typ = ROOM;
                    unblock_point(zx, zy);      /* vision */
                } else if (!Blind)
                    pline(msgc_failcurse, "The tree shudders but is unharmed.");
                break;
            } else if (room->typ == STONE || room->typ == SCORR) {
                if (!(room->wall_info & W_NONDIGGABLE)) {
                    room->typ = CORR;
                    unblock_point(zx, zy);      /* vision */
                } else if (!Blind)
                    pline(msgc_failcurse, "The rock glows then fades.");
                break;
            }
        } else if (IS_ROCK(room->typ)) {
            if (!may_dig(level, zx, zy))
                break;
            if (IS_WALL(room->typ) || room->typ == SDOOR) {
                if (*in_rooms(level, zx, zy, SHOPBASE)) {
                    add_damage(zx, zy, 200L);
                    shopwall = TRUE;
                }
                watch_warn(NULL, zx, zy, TRUE);
                if (level->flags.is_cavernous_lev && !in_town(zx, zy)) {
                    room->typ = CORR;
                } else {
                    room->typ = DOOR;
                    room->doormask = D_NODOOR;
                }
                digdepth -= 2;
            } else if (IS_TREE(room->typ)) {
                room->typ = ROOM;
                digdepth -= 2;
            } else {    /* IS_ROCK but not IS_WALL or SDOOR */
                room->typ = CORR;
                digdepth--;
            }
            unblock_point(zx, zy);      /* vision */
        }
        zx += dx;
        zy += dy;
    }   /* while */

    tmpsym_end(tsym);
    if (shopdoor || shopwall)
        pay_for_damage(shopdoor ? "destroy" : "dig into", FALSE);
    return;
}

/* move objects from level->objlist/nexthere lists to buriedobjlist, keeping
 * position */
/* information */
static struct obj *
bury_an_obj(struct obj *otmp)
{
    struct obj *otmp2;
    boolean under_ice;

    if (otmp == uball)
        unpunish();
    /* after unpunish(), or might get deallocated chain */
    otmp2 = otmp->nexthere;
    /*
     * obj_resists(,0,0) prevents Rider corpses from being buried.
     * It also prevents The Amulet and invocation tools from being
     * buried.  Since they can't be confined to bags and statues,
     * it makes sense that they can't be buried either, even though
     * the real reason there (direct accessibility when carried) is
     * completely different.
     */
    if (otmp == uchain || obj_resists(otmp, 0, 0))
        return otmp2;

    if (otmp->otyp == LEASH && otmp->leashmon != 0)
        o_unleash(otmp);

    if (otmp->lamplit && otmp->otyp != POT_OIL)
        end_burn(otmp, TRUE);

    obj_extract_self(otmp);

    under_ice = is_ice(otmp->olev, otmp->ox, otmp->oy);
    if (otmp->otyp == ROCK && !under_ice) {
        /* merges into burying material */
        obfree(otmp, NULL);
        return otmp2;
    }
    /*
     * Start a rot on organic material.  Not corpses -- they
     * are already handled.
     */
    if (otmp->otyp == CORPSE) {
        ;       /* should cancel timer if under_ice */
    } else if ((under_ice ? otmp->oclass == POTION_CLASS : is_organic(otmp))
               && !obj_resists(otmp, 5, 95)) {
        start_timer(otmp->olev, (under_ice ? 0L : 250L) + (long)rnd(250),
                    TIMER_OBJECT, ROT_ORGANIC, otmp);
    }
    add_to_buried(otmp);
    return otmp2;
}

void
bury_objs(struct level *lev, int x, int y)
{
    struct obj *otmp, *otmp2;

    for (otmp = lev->objects[x][y]; otmp; otmp = otmp2)
        otmp2 = bury_an_obj(otmp);

    /* don't expect any engravings here, but just in case */
    del_engr_at(lev, x, y);

    if (lev == level)
        newsym(x, y);
}

/* move objects from buriedobjlist to level->objlist/nexthere lists */
void
unearth_objs(struct level *lev, int x, int y)
{
    struct obj *otmp, *otmp2;

    for (otmp = lev->buriedobjlist; otmp; otmp = otmp2) {
        otmp2 = otmp->nobj;
        if (otmp->ox == x && otmp->oy == y) {
            obj_extract_self(otmp);
            if (otmp->timed)
                stop_timer(otmp->olev, ROT_ORGANIC, otmp);
            place_object(otmp, lev, x, y);
            stackobj(otmp);
        }
    }
    del_engr_at(lev, x, y);

    if (lev == level)
        newsym(x, y);
}

/*
 * The organic material has rotted away while buried.  As an expansion,
 * we could add add partial damage.  A damage count is kept in the object
 * and every time we are called we increment the count and reschedule another
 * timeout.  Eventually the object rots away.
 *
 * This is used by buried objects other than corpses.  When a container rots
 * away, any contents become newly buried objects.
 */
void
rot_organic(void *arg, long timeout)
{
    struct obj *obj = (struct obj *)arg;

    (void) timeout;

    while (Has_contents(obj)) {
        /* We don't need to place contained object on the floor first, but we
           do need to update its map coordinates. */
        obj->cobj->ox = obj->ox, obj->cobj->oy = obj->oy;
        /* Everything which can be held in a container can also be buried, so
           bury_an_obj's use of obj_extract_self ensures that Has_contents(obj)
           will eventually become false. */
        bury_an_obj(obj->cobj);
    }
    obj_extract_self(obj);
    obfree(obj, NULL);
}

/*
 * Called when a corpse has rotted completely away.
 */
void
rot_corpse(void *arg, long timeout)
{
    xchar x = 0, y = 0;
    struct obj *obj = (struct obj *)arg;
    boolean on_floor = obj->where == OBJ_FLOOR, in_invent =
        obj->where == OBJ_INVENT;
    struct monst *mtmp;

    if (on_floor) {
        x = obj->ox;
        y = obj->oy;
        if ((mtmp = m_at(level, x, y)) && mtmp->mundetected &&
            hides_under(mtmp->data)) {
            mtmp->mundetected = 0;
        } else if (Upolyd && x == u.ux && y == u.uy && u.uundetected &&
                   hides_under(youmonst.data)) {
            u.uundetected = 0;
        }
    } else if (in_invent) {
        if (flags.verbose) {
            const char *cname = corpse_xname(obj, FALSE);

            pline(msgc_statusend, "Your %s%s %s away%c",
                  obj == uwep ? "wielded " : "", cname, otense(obj, "rot"),
                  obj == uwep ? '!' : '.');
        }
        if (obj == uwep) {
            uwepgone(); /* now bare handed */
            action_interrupted();
        } else if (obj == uswapwep) {
            uswapwepgone();
            action_interrupted();
        } else if (obj == uquiver) {
            uqwepgone();
            action_interrupted();
        }
    } else if (obj->where == OBJ_MINVENT && obj->owornmask) {
        if (obj == MON_WEP(obj->ocarry)) {
            setmnotwielded(obj->ocarry, obj);
            MON_NOWEP(obj->ocarry);
        }
    }
    rot_organic(arg, timeout);
    if (on_floor)
        newsym(x, y);
    else if (in_invent)
        update_inventory();
}

/*dig.c*/
