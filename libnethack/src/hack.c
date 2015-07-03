/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <limits.h>

#include "hack.h"
#include "hungerstatus.h"

static void maybe_wail(void);
static int moverock(schar dx, schar dy);
static int still_chewing(xchar, xchar);
static void dosinkfall(void);
static boolean findtravelpath(boolean(*)(int, int), schar *, schar *,
                              enum u_interaction_mode);
static struct monst *monstinroom(const struct permonst *, int);
static boolean check_interrupt(struct monst *mtmp);
static boolean couldsee_func(int, int);

static void move_update(boolean);

#define IS_SHOP(x) (level->rooms[x].rtype >= SHOPBASE)

void
clear_travel_direction(void)
{
    turnstate.move.dx = 0;
    turnstate.move.dy = 0;
    memset(turnstate.move.stepped_on, FALSE, sizeof(turnstate.move.stepped_on));
}

boolean
revive_nasty(int x, int y, const char *msg)
{
    struct obj *otmp, *otmp2;
    struct monst *mtmp;
    coord cc;
    boolean revived = FALSE;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        if (otmp->otyp == CORPSE &&
            (is_rider(&mons[otmp->corpsenm]) ||
             otmp->corpsenm == PM_WIZARD_OF_YENDOR)) {
            /* move any living monster already at that location */
            if ((mtmp = m_at(level, x, y)) &&
                enexto(&cc, level, x, y, mtmp->data))
                rloc_to(mtmp, cc.x, cc.y);
            if (msg)
                pline_once("%s", msg);
            revived = revive_corpse(otmp);
        }
    }

    /* this location might not be safe, if not, move revived monster */
    if (revived) {
        mtmp = m_at(level, x, y);
        if (mtmp && !goodpos(level, x, y, mtmp, 0) &&
            enexto(&cc, level, x, y, mtmp->data)) {
            rloc_to(mtmp, cc.x, cc.y);
        }
        /* else impossible? */
    }

    return revived;
}

static int
moverock(schar dx, schar dy)
{
    xchar rx, ry, sx, sy;
    struct obj *otmp;
    struct trap *ttmp;
    struct monst *mtmp;

    sx = u.ux + dx;
    sy = u.uy + dy;     /* boulder starting position */
    while ((otmp = sobj_at(BOULDER, level, sx, sy)) != 0) {
        /* There's a boulder at (sx, sy), so make sure the point stays blocked.
           This should only happen where there's multiple boulders on a tile. */
#ifdef INVISIBLE_OBJECTS
        if (!otmp->oinvis)
#endif
            block_point(sx, sy);

        /* make sure that this boulder is visible as the top object */
        if (otmp != level->objects[sx][sy])
            movobj(otmp, sx, sy);

        rx = u.ux + 2 * dx;     /* boulder destination position */
        ry = u.uy + 2 * dy;
        action_completed();
        if (Levitation || Is_airlevel(&u.uz)) {
            if (Blind)
                feel_location(sx, sy);
            pline("You don't have enough leverage to push %s.",
                  the(xname(otmp)));
            /* Give them a chance to climb over it? */
            return -1;
        }
        if (verysmall(youmonst.data) && !u.usteed) {
            if (Blind)
                feel_location(sx, sy);
            pline("You're too small to push that %s.", xname(otmp));
            goto cannot_push;
        }
        if (isok(rx, ry) && !IS_ROCK(level->locations[rx][ry].typ) &&
            level->locations[rx][ry].typ != IRONBARS &&
            (!IS_DOOR(level->locations[rx][ry].typ) || !(dx && dy) ||
             (!Is_rogue_level(&u.uz) &&
              (level->locations[rx][ry].doormask & ~D_BROKEN) == D_NODOOR)) &&
            !sobj_at(BOULDER, level, rx, ry)) {
            ttmp = t_at(level, rx, ry);
            mtmp = m_at(level, rx, ry);

            /* KMH -- Sokoban doesn't let you push boulders diagonally */
            if (In_sokoban(&u.uz) && dx && dy) {
                if (Blind)
                    feel_location(sx, sy);
                pline("%s won't roll diagonally on this %s.", The(xname(otmp)),
                      surface(sx, sy));
                goto cannot_push;
            }

            if (revive_nasty(rx, ry, "You sense movement on the other side."))
                return -1;

            if (mtmp && !noncorporeal(mtmp->data) &&
                (!mtmp->mtrapped ||
                 !(ttmp &&
                   ((ttmp->ttyp == PIT) || (ttmp->ttyp == SPIKED_PIT))))) {
                if (Blind)
                    feel_location(sx, sy);
                if (canspotmon(mtmp))
                    pline("There's %s on the other side.", a_monnam(mtmp));
                else {
                    You_hear("a monster behind %s.", the(xname(otmp)));
                    map_invisible(rx, ry);
                }
                if (flags.verbose)
                    pline("Perhaps that's why %s cannot move it.",
                          u.usteed ? y_monnam(u.usteed) : "you");
                goto cannot_push;
            }

            if (ttmp)
                switch (ttmp->ttyp) {
                case LANDMINE:
                    if (rn2(10)) {
                        obj_extract_self(otmp);
                        place_object(otmp, level, rx, ry);
                        if (!sobj_at(BOULDER, level, sx, sy))
                            unblock_point(sx, sy);
                        newsym(sx, sy);
                        pline("KAABLAMM!!!  %s %s land mine.",
                              Tobjnam(otmp, "trigger"),
                              ttmp->madeby_u ? "your" : "a");
                        blow_up_landmine(ttmp);
                        /* if the boulder remains, it should fill the pit */
                        fill_pit(level, u.ux, u.uy);
                        if (cansee(rx, ry))
                            newsym(rx, ry);
                        continue;
                    }
                    break;
                case SPIKED_PIT:
                case PIT:
                    obj_extract_self(otmp);
                    /* vision kludge to get messages right; the pit will
                       temporarily be seen even if this is one among multiple
                       boulders */
                    if (!Blind)
                        viz_array[ry][rx] |= IN_SIGHT;
                    if (!flooreffects(otmp, rx, ry, "fall")) {
                        place_object(otmp, level, rx, ry);
                    }
                    if (mtmp && !Blind)
                        newsym(rx, ry);
                    continue;
                case HOLE:
                case TRAPDOOR:
                    if (Blind)
                        pline("Kerplunk!  You no longer feel %s.",
                              the(xname(otmp)));
                    else
                        pline("%s%s and %s a %s in the %s!",
                              Tobjnam(otmp,
                                      (ttmp->ttyp ==
                                       TRAPDOOR) ? "trigger" : "fall"),
                              (ttmp->ttyp == TRAPDOOR) ? "" : " into",
                              otense(otmp, "plug"),
                              (ttmp->ttyp == TRAPDOOR) ? "trap door" : "hole",
                              surface(rx, ry));
                    deltrap(level, ttmp);
                    delobj(otmp);
                    bury_objs(level, rx, ry);
                    if (cansee(rx, ry))
                        newsym(rx, ry);
                    continue;
                case TELEP_TRAP:
                    if (u.usteed)
                        pline("%s pushes %s and suddenly it disappears!",
                              msgupcasefirst(y_monnam(u.usteed)),
                              the(xname(otmp)));
                    else
                        pline("You push %s and suddenly it disappears!",
                              the(xname(otmp)));
                    rloco(otmp);
                    seetrap(ttmp);
                    continue;
                case LEVEL_TELEP:
                    if (In_endgame(&u.uz)) {
                        pline ("%s strains, but fails to escape the plane.",
                            msgupcasefirst(the(xname(otmp))));
                        deltrap(level, ttmp);
                        goto skipmsg; /*don't print move message*/
                    }

                    int newlev;
                    d_level dest;
                    do
                        newlev = random_teleport_level();
                    while (newlev == depth(&u.uz));

                    if (u.usteed)
                        pline("%s pushes %s and suddenly it disappears!",
                              msgupcasefirst(y_monnam(u.usteed)),
                              the(xname(otmp)));
                    else
                        pline("You push %s and suddenly it disappears!",
                              the(xname(otmp)));

                    obj_extract_self(otmp);
                    get_level(&dest, newlev);
                    deliver_object(otmp, dest.dnum, dest.dlevel,
                                   MIGR_RANDOM);
                    seetrap(ttmp);
                    continue;
                }
            if (closed_door(level, rx, ry))
                goto nopushmsg;
            if (boulder_hits_pool(otmp, rx, ry, TRUE))
                continue;

            {
                /* note: reset to zero after save/restore cycle */
                static long lastmovetime;

                if (!u.usteed) {
                    if (moves > lastmovetime + 2 || moves < lastmovetime)
                        pline("With %s effort you move %s.",
                              throws_rocks(youmonst.data) ? "little" : "great",
                              the(xname(otmp)));
                    exercise(A_STR, TRUE);
                } else
                    pline("%s moves %s.",
                          msgupcasefirst(y_monnam(u.usteed)),
                          the(xname(otmp)));
                lastmovetime = moves;
            }

        skipmsg:
            /*
             * Re-link at top of level->objlist chain so that pile order is
             * preserved when level is restored.
             */
            if (otmp != level->objlist) {
                remove_object(otmp);
                place_object(otmp, level, otmp->ox, otmp->oy);
            }

            /* Move the boulder *after* the message. */
            if (level->locations[rx][ry].mem_invis)
                unmap_object(rx, ry);
            movobj(otmp, rx, ry);       /* does newsym(rx,ry) */
            if (Blind) {
                feel_location(rx, ry);
                feel_location(sx, sy);
            } else {
                newsym(sx, sy);
            }
        } else {
        nopushmsg:
            if (u.usteed)
                pline("%s tries to move %s, but cannot.",
                      msgupcasefirst(y_monnam(u.usteed)),
                      the(xname(otmp)));
            else
                pline("You try to move %s, but in vain.", the(xname(otmp)));
            if (Blind)
                feel_location(sx, sy);
        cannot_push:
            if (throws_rocks(youmonst.data)) {
                if (u.usteed && P_SKILL(P_RIDING) < P_BASIC) {
                    pline("You aren't skilled enough to %s %s from %s.",
                          (flags.pickup && !In_sokoban(&u.uz))
                          ? "pick up" : "push aside", the(xname(otmp)),
                          y_monnam(u.usteed));
                } else {
                    pline("However, you can easily %s.",
                          (flags.pickup && !In_sokoban(&u.uz))
                          ? "pick it up" : "push it aside");
                    if (In_sokoban(&u.uz))
                        change_luck(-1);        /* Sokoban guilt */
                    break;
                }
                break;
            }

            if (!u.usteed &&
                (((!invent || inv_weight() <= -850) &&
                  (!dx || !dy || (IS_ROCK(level->locations[u.ux][sy].typ)
                                  && IS_ROCK(level->locations[sx][u.uy].typ))))
                 || verysmall(youmonst.data))) {
                pline
                    ("However, you can squeeze yourself into a small opening.");
                if (In_sokoban(&u.uz))
                    change_luck(-1);    /* Sokoban guilt */
                break;
            } else
                return -1;
        }
    }
    return 0;
}

/*
 *  still_chewing()
 *
 *  Chew on a wall, door, or boulder.  Returns TRUE if still eating, FALSE
 *  when done.
 */
static int
still_chewing(xchar x, xchar y)
{
    struct rm *loc = &level->locations[x][y];
    struct obj *boulder = sobj_at(BOULDER, level, x, y);
    const char *digtxt = NULL, *dmgtxt = NULL;

    if (!boulder && IS_ROCK(loc->typ) && !may_dig(level, x, y)) {
        pline("You hurt your teeth on the %s.",
              IS_TREE(loc->typ) ? "tree" : "hard stone");
        action_completed();
        return 1;
    } else if (u.utracked_location[tl_dig].x != x ||
               u.utracked_location[tl_dig].y != y ||
               !u.uoccupation_progress[tos_dig]) {
        u.utracked_location[tl_dig].x = x;
        u.utracked_location[tl_dig].y = y;
        /* solid rock takes more work & time to dig through */
        u.uoccupation_progress[tos_dig] =
            (IS_ROCK(loc->typ) && !IS_TREE(loc->typ) ? 30 : 60) + u.udaminc;
        pline("You start chewing %s %s.",
              (boulder ||
               IS_TREE(loc->typ)) ? "on a" : "a hole in the",
              boulder ? "boulder" : IS_TREE(loc->typ) ?
              "tree" : IS_ROCK(loc->typ) ? "rock" : "door");
        watch_warn(NULL, x, y, FALSE);
        return 1;
    } else if ((u.uoccupation_progress[tos_dig] += (30 + u.udaminc)) <= 100) {
        if (flags.verbose)
            pline("You continue chewing on the %s.",
                  boulder ? "boulder" : IS_TREE(loc->typ) ?
                  "tree" : IS_ROCK(loc->typ) ? "rock" : "door");
        watch_warn(NULL, x, y, FALSE);
        return 1;
    }

    /* Okay, you've chewed through something */
    break_conduct(conduct_food);
    u.uhunger += rnd(20);

    if (boulder) {
        delobj(boulder);        /* boulder goes bye-bye */
        pline("You eat the boulder.");  /* yum */

        /*
         *  The location could still block because of
         *      1. More than one boulder
         *      2. Boulder stuck in a wall/stone/door.
         *
         *  [perhaps use does_block() below (from vision.c)]
         */
        if (IS_ROCK(loc->typ) || closed_door(level, x, y) ||
            sobj_at(BOULDER, level, x, y)) {
            block_point(x, y);  /* delobj will unblock the point */
            /* reset dig state */
            u.uoccupation_progress[tos_dig] = 0;
            return 1;
        }

    } else if (IS_WALL(loc->typ)) {
        if (*in_rooms(level, x, y, SHOPBASE)) {
            add_damage(x, y, 10L * ACURRSTR);
            dmgtxt = "damage";
        }
        digtxt = "You chew a hole in the wall.";
        if (level->flags.is_maze_lev) {
            loc->typ = ROOM;
        } else if (level->flags.is_cavernous_lev && !in_town(x, y)) {
            loc->typ = CORR;
        } else {
            loc->typ = DOOR;
            loc->doormask = D_NODOOR;
        }
    } else if (IS_TREE(loc->typ)) {
        digtxt = "You chew through the tree.";
        loc->typ = ROOM;
    } else if (loc->typ == SDOOR) {
        if (loc->doormask & D_TRAPPED) {
            loc->doormask = D_NODOOR;
            b_trapped("secret door", 0);
        } else {
            digtxt = "You chew through the secret door.";
            loc->doormask = D_BROKEN;
        }
        loc->typ = DOOR;

    } else if (IS_DOOR(loc->typ)) {
        if (*in_rooms(level, x, y, SHOPBASE)) {
            add_damage(x, y, 400L);
            dmgtxt = "break";
        }
        if (loc->doormask & D_TRAPPED) {
            loc->doormask = D_NODOOR;
            b_trapped("door", 0);
        } else {
            digtxt = "You chew through the door.";
            loc->doormask = D_BROKEN;
        }

    } else {    /* STONE or SCORR */
        digtxt = "You chew a passage through the rock.";
        loc->typ = CORR;
    }

    unblock_point(x, y);        /* vision */
    newsym(x, y);
    if (digtxt)
        pline("%s", digtxt);  /* after newsym */
    if (dmgtxt)
        pay_for_damage(dmgtxt, FALSE);
    u.uoccupation_progress[tos_dig] = 0;
    return 0;
}


void
movobj(struct obj *obj, xchar ox, xchar oy)
{
    /* optimize by leaving on the level->objlist chain? */
    remove_object(obj);
    newsym(obj->ox, obj->oy);
    place_object(obj, level, ox, oy);
    newsym(ox, oy);
}

static const char fell_on_sink[] = "fell onto a sink";

static void
dosinkfall(void)
{
    struct obj *obj;

    /* Turn off levitation before printing messages */
    HLevitation &= ~(I_SPECIAL | TIMEOUT);
    if (uleft && uleft->otyp == RIN_LEVITATION)
        setequip(os_ringl, NULL, em_magical);
    if (uright && uright->otyp == RIN_LEVITATION)
        setequip(os_ringr, NULL, em_magical);
    if (uarmf && uarmf->otyp == LEVITATION_BOOTS)
        setequip(os_armf, NULL, em_magical);
    for (obj = invent; obj; obj = obj->nobj) {
        if (obj->oartifact && artifact_has_invprop(obj, LEVITATION))
            uninvoke_artifact(obj);
    }

    if (is_floater(youmonst.data) || (HLevitation & FROMOUTSIDE)) {
        pline("You wobble unsteadily for a moment.");
    } else {
        pline("You crash to the floor!");
        losehp(rn1(8, 25 - (int)ACURR(A_CON)), fell_on_sink);
        exercise(A_DEX, FALSE);
        selftouch("Falling, you", "crashing to the floor while wielding");
        for (obj = level->objects[u.ux][u.uy]; obj; obj = obj->nexthere)
            if (obj->oclass == WEAPON_CLASS || is_weptool(obj)) {
                pline("You fell on %s.", doname(obj));
                losehp(rnd(3), fell_on_sink);
                exercise(A_CON, FALSE);
            }
    }
}

boolean
may_dig(struct level *lev, xchar x, xchar y)
/* intended to be called only on ROCKs */
{
    return (!
            (IS_STWALL(lev->locations[x][y].typ) &&
             (lev->locations[x][y].wall_info & W_NONDIGGABLE)));
}

boolean
may_passwall(struct level * lev, xchar x, xchar y)
{
    return (!
            (IS_STWALL(lev->locations[x][y].typ) &&
             (lev->locations[x][y].wall_info & W_NONPASSWALL)));
}


boolean
bad_rock(const struct permonst * mdat, xchar x, xchar y)
{
    return (boolean) ((In_sokoban(&u.uz) && sobj_at(BOULDER, level, x, y)) ||
                      (IS_ROCK(level->locations[x][y].typ)
                       && (!tunnels(mdat) || needspick(mdat) ||
                           !may_dig(level, x, y))
                       && !(passes_walls(mdat) && may_passwall(level, x, y))));
}

boolean
invocation_pos(const d_level * dlev, xchar x, xchar y)
{
    return (boolean) (Invocation_lev(dlev) && x == inv_pos.x && y == inv_pos.y);
}

static void
autoexplore_msg(const char *text, int mode)
{
    if (flags.occupation == occ_autoexplore) {
        pline("%s blocks your way.", msgupcasefirst(text));
    }
}

boolean
travelling(void)
{
    return flags.occupation == occ_travel ||
        flags.occupation == occ_autoexplore;
}

/* Return TRUE if (dx,dy) is an OK place to move. Mode is one of DO_MOVE,
   TEST_MOVE, TEST_TRAV or TEST_TRAP.

   This function takes the values of Blind, Stunned, Fumbling, Hallucination,
   Passes_walls, and Ground_based as arguments; repeatedly recalculating them
   was taking up 34% of the runtime of the entire program before this change. */
boolean
test_move(int ux, int uy, int dx, int dy, int dz, int mode,
          enum u_interaction_mode uim, boolean blind, boolean stunned,
          boolean fumbling, boolean halluc, boolean passwall, boolean grounded)
{
    int x = ux + dx;
    int y = uy + dy;
    struct rm *tmpr = &level->locations[x][y];
    struct rm *ust;

    /*
     *  Check for physical obstacles.  First, the place we are going.
     */
    if (IS_ROCK(tmpr->typ) || tmpr->typ == IRONBARS) {
        if (blind && mode == DO_MOVE)
            feel_location(x, y);
        if (passwall && may_passwall(level, x, y)) {
            ;   /* do nothing */
        } else if (tmpr->typ == IRONBARS) {
            if (!(passwall || passes_bars(youmonst.data)))
                return FALSE;
        } else if (tunnels(youmonst.data) && !needspick(youmonst.data)) {
            /* Eat the rock. */
            if (mode == DO_MOVE && still_chewing(x, y))
                return FALSE;
        } else {
            if (mode == DO_MOVE) {
                if (Is_stronghold(&u.uz) && is_db_wall(x, y))
                    pline("The drawbridge is up!");
                if (passwall && !may_passwall(level, x, y) &&
                    In_sokoban(&u.uz))
                    pline("The Sokoban walls resist your ability.");
            }
            return FALSE;
        }
    } else if (IS_DOOR(tmpr->typ)) {
        if (closed_door(level, x, y)) {
            if (blind && mode == DO_MOVE)
                feel_location(x, y);
            if (passwall)
                ; /* do nothing */
            else if (can_ooze(&youmonst)) {
                if (mode == DO_MOVE)
                    pline("You ooze %s the door.",
                          can_reach_floor() ? "under" : "around");
            } else if (tunnels(youmonst.data) && !needspick(youmonst.data)) {
                /* Eat the door. */
                if (mode == DO_MOVE && still_chewing(x, y))
                    return FALSE;
            } else {
                if (mode == DO_MOVE) {
                    if (amorphous(youmonst.data))
                        pline("You try to ooze %s the door, but can't "
                              "squeeze your possessions through.",
                              can_reach_floor() ? "under" : "around");
                    else if (x == ux || y == uy) {
                        if (blind || stunned || ACURR(A_DEX) < 10 || fumbling) {
                            if (u.usteed) {
                                pline("You can't lead %s through that closed "
                                      "door.",
                                      y_monnam(u.usteed));
                            } else {
                                pline("Ouch!  You bump into a door.");
                                exercise(A_DEX, FALSE);
                            }
                        } else
                            pline("That door is closed.");
                    }
                } else if (mode == TEST_TRAV || mode == TEST_TRAP)
                    goto testdiag;
                return FALSE;
            }
        } else {
        testdiag:
            if (dx && dy && !passwall && ((tmpr->doormask & ~D_BROKEN)
                                          || Is_rogue_level(&u.uz)
                                          || block_door(x, y))) {
                /* Diagonal moves into a door are not allowed. */
                if (blind && mode == DO_MOVE)
                    feel_location(x, y);
                return FALSE;
            }
        }
    }
    if (dx && dy && bad_rock(youmonst.data, ux, y) &&
        bad_rock(youmonst.data, x, uy)) {
        /* Move at a diagonal. */
        if (In_sokoban(&u.uz)) {
            if (mode == DO_MOVE)
                pline("You cannot pass that way.");
            return FALSE;
        }
        if (bigmonst(youmonst.data) && !can_ooze(&youmonst)) {
            if (mode == DO_MOVE)
                pline("Your body is too large to fit through.");
            return FALSE;
        }
        if (invent && (inv_weight() + weight_cap() > 600)) {
            if (mode == DO_MOVE)
                pline("You are carrying too much to get through.");
            return FALSE;
        }
    }
    /* Reduce our willingness to path through traps, water, and lava.  The
       character's current square is always safe to stand on, so don't worry
       about that. Check the character's memory, not the current square
       contents, because it might have changed since the character last saw
       it. */
    if (travelling() && (x != u.ux || y != u.uy)) {
        struct trap *t = t_at(level, x, y);

        if ((t && t->tseen) ||
            (grounded &&
             (level->locations[x][y].mem_bg == S_pool ||
              level->locations[x][y].mem_bg == S_lava))) {
            if (mode == DO_MOVE) {
                if (is_pool(level, x, y))
                    autoexplore_msg("a body of water", mode);
                else if (is_lava(level, x, y))
                    autoexplore_msg("a pool of lava", mode);
                if (travelling())
                    return FALSE;
            }
            return mode == TEST_TRAP || mode == DO_MOVE;
        }
    }

    if (mode == TEST_TRAP)
        return FALSE;   /* not a move through a trap/water/lava */

    ust = &level->locations[ux][uy];

    /* Now see if other things block our way. */
    if (dx && dy && !passwall &&
        (IS_DOOR(ust->typ) && ((ust->doormask & ~D_BROKEN)
                               || Is_rogue_level(&u.uz)
                               || block_entry(x, y))
        )) {
        /* Can't move at a diagonal out of a doorway with door. */
        if (mode == DO_MOVE)
            autoexplore_msg("the doorway", mode);
        return FALSE;
    }

    if (sobj_at(BOULDER, level, x, y) && (In_sokoban(&u.uz) || !passwall)) {
        if (!(blind || halluc) && !ITEM_INTERACTIVE(uim) &&
            mode != TEST_TRAV) {
            if (sobj_at(BOULDER, level, x, y) && mode == DO_MOVE)
                autoexplore_msg("a boulder", mode);
            return FALSE;
        }
        if (mode == DO_MOVE) {
            /* tunneling monsters will chew before pushing */
            if (tunnels(youmonst.data) && !needspick(youmonst.data) &&
                !In_sokoban(&u.uz)) {
                if (still_chewing(x, y))
                    return FALSE;
            } else if (moverock(dx, dy) < 0)
                return FALSE;
        } else if (mode == TEST_TRAV) {
            struct obj *obj;

            /* never travel through boulders in Sokoban */
            if (In_sokoban(&u.uz))
                return FALSE;

            /* don't pick two boulders in a row, unless there's a way thru */
            if (sobj_at(BOULDER, level, ux, uy) && !In_sokoban(&u.uz)) {
                if (!passwall &&
                    !(tunnels(youmonst.data) && !needspick(youmonst.data)) &&
                    !carrying(PICK_AXE) && !carrying(DWARVISH_MATTOCK) &&
                    !((obj = carrying(WAN_DIGGING)) &&
                      !objects[obj->otyp].oc_name_known))
                    return FALSE;
            }
        }
        /* assume you'll be able to push it when you get there... */
    }

    /* OK, it is a legal place to move. */
    return TRUE;
}

/* Returns whether a square might be interesting to autoexplore onto.
   This is done purely in terms of the memory of the square, i.e.
   information the player knows already, to avoid leaking
   information. The algorithm is taken from TAEB: "step on any item we
   haven't stepped on, or any square we haven't stepped on adjacent to
   stone that isn't adjacent to a square that has been stepped on;
   however, never step on a boulder this way". */
static boolean
unexplored(int x, int y)
{
    int i, j, k, l;
    const struct trap *ttmp;
    int mem_bg;

    if (!isok(x, y))
        return FALSE;
    ttmp = t_at(level, x, y);
    mem_bg = level->locations[x][y].mem_bg;

    if (!isok(x, y))
        return FALSE;
    if (level->locations[x][y].mem_stepped)
        return FALSE;
    if (level->locations[x][y].mem_bg == S_vcdoor ||
        level->locations[x][y].mem_bg == S_hcdoor) {
        if (level->locations[x][y].mem_door_l &&
            (level->locations[x][y].flags & D_LOCKED))
            return FALSE;       /* player knows of a locked door there */
    }

    if (ttmp && ttmp->tseen)
        return FALSE;
    if (level->locations[x][y].mem_obj == BOULDER + 1)
        return FALSE;
    if (level->locations[x][y].mem_obj && inside_shop(level, x, y))
        return FALSE;

    if (mem_bg == S_altar || mem_bg == S_throne || mem_bg == S_sink ||
        mem_bg == S_fountain || mem_bg == S_dnstair || mem_bg == S_upstair ||
        mem_bg == S_dnsstair || mem_bg == S_upsstair || mem_bg == S_dnladder ||
        mem_bg == S_upladder)
        return TRUE;

    if (level->locations[x][y].mem_obj)
        return TRUE;

    for (i = -1; i <= 1; i++)
        for (j = -1; j <= 1; j++) {
            /* corridors with only unexplored diagonals aren't interesting */
            if (mem_bg == S_corr && i && j)
                continue;
            if (isok(x + i, y + j) &&
                level->locations[x + i][y + j].mem_bg == S_unexplored) {
                int flag = TRUE;

                for (k = -1; k <= 1; k++)
                    for (l = -1; l <= 1; l++)
                        if (isok(x + i + k, y + j + l) &&
                            level->locations[x + i + k][y + j + l].mem_stepped)
                            flag = FALSE;
                if (flag)
                    return TRUE;
            }
        }
    return FALSE;
}

/* Returns a distance modified by a constant factor.
 * The lower the value the better.*/
static int
autotravel_weighting(int x, int y, unsigned distance)
{
    const struct rm *loc = &level->locations[x][y];
    int mem_bg = loc->mem_bg;

    /* greedy for items */
    if (loc->mem_obj)
        return distance;

    /* some dungeon features */
    if (mem_bg == S_altar || mem_bg == S_throne || mem_bg == S_sink ||
        mem_bg == S_fountain)
        return distance;

    /* stairs and ladders */
    if (mem_bg == S_dnstair || mem_bg == S_upstair || mem_bg == S_dnsstair ||
        mem_bg == S_upsstair || mem_bg == S_dnladder || mem_bg == S_upladder)
        return distance;

    /* favor rooms, but not closed doors */
    if (loc->roomno && !(mem_bg == S_hcdoor || mem_bg == S_vcdoor))
        return distance * 2;

    /* by default return distance multiplied by a large constant factor */
    return distance * 10;
}

/* Sort-of like findtravelpath, but simplified. This is for monster travel.
   Assumption: monsters know the layout of the dungeon, but not the locations of
   items. Monsters will avoid the square they believe the player to be on. The
   return value is the distance between the two points given. */
void
distmap_init(struct distmap_state *ds, int x1, int y1, struct monst *mtmp)
{
    memset(ds->onmap, 0, sizeof ds->onmap);

    ds->curdist = 0;
    ds->tslen = 1;

    ds->travelstepx[0][0] = x1;
    ds->travelstepy[0][0] = y1;

    ds->mon = mtmp;
}

int
distmap(struct distmap_state *ds, int x2, int y2)
{
    do {
        if (ds->onmap[x2][y2])
            return ds->onmap[x2][y2] - 1;

        int oldtslen = ds->tslen;
        ds->tslen = 0;

        int i;
        for (i = 0; i < oldtslen; i++) {
            int x = ds->travelstepx[ds->curdist % 2][i];
            int y = ds->travelstepy[ds->curdist % 2][i];
            if (ds->onmap[x][y])
                continue;

            ds->onmap[x][y] = ds->curdist + 1;

            int dx, dy;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    if (!isok(x + dx, y + dy))
                        continue;
                    if (!goodpos(ds->mon->dlevel, x + dx, y + dy,
                                 ds->mon, MM_IGNOREMONST | MM_IGNOREDOORS))
                        continue;

                    ds->travelstepx[(ds->curdist + 1) % 2][ds->tslen] = x + dx;
                    ds->travelstepy[(ds->curdist + 1) % 2][ds->tslen] = y + dy;
                    ds->tslen++;
                }
        }

        if (ds->tslen)
            ds->curdist++;
    } while (ds->tslen);

    return COLNO * ROWNO; /* sentinel */
}


/*
 * Find a path from the destination (u.tx,u.ty) back to (u.ux,u.uy).
 * A shortest path is returned.  If guess is non-NULL, instead travel
 * as near to the target as you can, using guess as a function that
 * specifies what is considered to be a valid target.
 * Returns TRUE if a path was found.
 */
static boolean
findtravelpath(boolean(*guess) (int, int), schar *dx, schar *dy,
               enum u_interaction_mode uim)
{
    /* Property caches; recalculating these took up a really noticeable amount
       of time, according to the profiler. */
    boolean blind = !!Blind;
    boolean stunned = !!Stunned;
    boolean fumbling = !!Fumbling;
    boolean halluc = !!Hallucination;
    boolean passwall = !!Passes_walls;
    boolean grounded = !!Ground_based;
    /* If a travel command is sent to an adjacent, reachable location
       (i.e. continue_message is TRUE, meaning that this isn't an implicitly
       continued action), use normal movement rules (and reset the interaction
       mode). This is for mouse-driven ports, to allow clicking on an adjacent
       square to move there. */
    if (!guess && turnstate.continue_message &&
        distmin(u.ux, u.uy, u.tx, u.ty) == 1) {
        if (test_move(u.ux, u.uy, u.tx - u.ux, u.ty - u.uy,
                      0, TEST_MOVE, flags.interaction_mode,
                      blind, stunned, fumbling, halluc, passwall, grounded)) {
            *dx = u.tx - u.ux;
            *dy = u.ty - u.uy;
            action_completed();
            flags.travelcc.x = flags.travelcc.y = -1;
            return TRUE;
        }
    }
    if (u.tx != u.ux || u.ty != u.uy || guess == unexplored) {
        unsigned travel[COLNO][ROWNO];
        xchar travelstepx[2][COLNO * ROWNO];
        xchar travelstepy[2][COLNO * ROWNO];
        xchar tx, ty, ux, uy;
        int n = 1;      /* max offset in travelsteps */
        int set = 0;    /* two sets current and previous */
        int radius = 1; /* search radius */
        int i;

        /* If guessing, first find an "obvious" goal location.  The obvious
           goal is the position the player knows of, or might figure out
           (couldsee) that is closest to the target on a straight path. */
        if (guess) {
            tx = u.ux;
            ty = u.uy;
            ux = u.tx;
            uy = u.ty;
        } else {
            tx = u.tx;
            ty = u.ty;
            ux = u.ux;
            uy = u.uy;
        }

    noguess:
        memset(travel, 0, sizeof (travel));
        travelstepx[0][0] = tx;
        travelstepy[0][0] = ty;

        while (n != 0) {
            int nn = 0;

            for (i = 0; i < n; i++) {
                int dir;
                int x = travelstepx[set][i];
                int y = travelstepy[set][i];
                static const int ordered[] = { 0, 2, 4, 6, 1, 3, 5, 7 };
                /* no diagonal movement for grid bugs */
                int dirmax = u.umonnum == PM_GRID_BUG ? 4 : 8;
                boolean alreadyrepeated = FALSE;

                for (dir = 0; dir < dirmax; ++dir) {
                    int nx = x + xdir[ordered[dir]];
                    int ny = y + ydir[ordered[dir]];

                    /*
                     * When guessing and trying to travel as close as possible
                     * to an unreachable target space, don't include spaces
                     * that would never be picked as a guessed target in the
                     * travel matrix describing player-reachable spaces.
                     * This stops travel from getting confused and moving the
                     * player back and forth in certain degenerate
                     * configurations of sight-blocking obstacles, e.g.
                     *
                     *    T         1. Dig this out and carry enough to not be
                     *      ####       able to squeeze through diagonal gaps.
                     *      #--.---    Stand at @ and target travel at space T.
                     *       @.....
                     *       |.....
                     *
                     *    T         2. couldsee() marks spaces marked a and x as
                     *      ####       eligible guess spaces to move the player
                     *      a--.---    towards.  Space a is closest to T, so it
                     *       @xxxxx    gets chosen.  Travel system moves
                     *       |xxxxx    right to travel to space a.
                     *
                     *    T         3. couldsee() marks spaces marked b, c and x
                     *      ####       as eligible guess spaces to move the
                     *      a--c---    player towards.  Since findtravelpath()
                     *       b@xxxx    is called repeatedly during travel, it
                     *       |xxxxx    doesn't remember that it wanted to go to
                     *                 space a, so in comparing spaces b and c,
                     *                 b is chosen, since it seems like the
                     *                 closest eligible space to T. Travel
                     *                 system moves @ left to go to space b.
                     *
                     *              4. Go to 2.
                     *
                     * By limiting the travel matrix here, space a in the
                     * example above is never included in it, preventing the
                     * cycle.
                     */
                    if (!isok(nx, ny) ||
                        (guess == couldsee_func && !guess(nx, ny)))
                        continue;

                    if ((!passwall && !can_ooze(&youmonst) &&
                         closed_door(level, nx, ny)) ||
                        sobj_at(BOULDER, level, nx, ny) ||
                        test_move(x, y, nx - x, ny - y, 0, TEST_TRAP, uim,
                                  blind, stunned, fumbling, halluc,
                                  passwall, grounded)) {
                        /* closed doors and boulders usually cause a delay, so
                           prefer another path */
                        if ((int)travel[x][y] > radius - 5) {
                            if (!alreadyrepeated) {
                                travelstepx[1 - set][nn] = x;
                                travelstepy[1 - set][nn] = y;
                                /* don't change travel matrix! */
                                nn++;
                                alreadyrepeated = TRUE;
                            }
                            continue;
                        }
                    }
                    if (test_move(x, y, nx - x, ny - y, 0, TEST_TRAP, uim,
                                  blind, stunned, fumbling, halluc,
                                  passwall, grounded) ||
                        test_move(x, y, nx - x, ny - y, 0, TEST_TRAV, uim,
                                  blind, stunned, fumbling, halluc,
                                  passwall, grounded)) {
                        if ((level->locations[nx][ny].seenv ||
                             (!blind && couldsee(nx, ny)))) {
                            if (nx == ux && ny == uy) {
                                if (!guess) {
                                    *dx = x - ux;
                                    *dy = y - uy;
                                    if (x == u.tx && y == u.ty) {
                                        action_completed();
                                        flags.travelcc.x =
                                            flags.travelcc.y = -1;
                                    }
                                    return TRUE;
                                }
                            } else if (!travel[nx][ny]) {
                                travelstepx[1 - set][nn] = nx;
                                travelstepy[1 - set][nn] = ny;
                                travel[nx][ny] = radius;
                                nn++;
                            }
                        }
                    }
                }
            }

            n = nn;
            set = 1 - set;
            radius++;
        }

        /* if guessing, find best location in travel matrix and go there */
        if (guess) {
            int px = tx, py = ty;       /* pick location */
            int dist, nxtdist, d2, nd2;
            boolean autoexploring = (guess == unexplored);

            dist = distmin(ux, uy, tx, ty);
            d2 = dist2(ux, uy, tx, ty);
            if (autoexploring) {
                dist = INT_MAX;
                d2 = INT_MAX;
            }
            for (tx = 0; tx < COLNO; ++tx) {
                for (ty = 0; ty < ROWNO; ++ty) {
                    if (travel[tx][ty]) {
                        nxtdist = distmin(ux, uy, tx, ty);
                        if (autoexploring)
                            nxtdist =
                                autotravel_weighting(tx, ty, travel[tx][ty]);
                        if (nxtdist == dist && guess(tx, ty)) {
                            nd2 = dist2(ux, uy, tx, ty);
                            if (nd2 < d2) {
                                /* prefer non-zigzag path */
                                px = tx;
                                py = ty;
                                d2 = nd2;
                            }
                        } else if (nxtdist < dist && guess(tx, ty)) {
                            px = tx;
                            py = ty;
                            dist = nxtdist;
                            d2 = dist2(ux, uy, tx, ty);
                        }
                    }
                }
            }

            if (px == u.ux && py == u.uy) {
                /* no guesses, just go in the general direction */
                *dx = sgn(u.tx - u.ux);
                *dy = sgn(u.ty - u.uy);
                if (*dx == 0 && *dy == 0) {
                    action_completed();
                    return FALSE;
                }
                if (test_move(u.ux, u.uy, *dx, *dy, 0, TEST_MOVE, uim,
                              blind, stunned, fumbling, halluc,
                              passwall, grounded))
                    return TRUE;
                goto found;
            }
            tx = px;
            ty = py;
            ux = u.ux;
            uy = u.uy;
            set = 0;
            n = radius = 1;
            guess = NULL;
            goto noguess;
        }
        return FALSE;
    }

found:
    *dx = 0;
    *dy = 0;
    action_completed();
    return FALSE;
}

/* A function version of couldsee, so we can take a pointer to it. */
static boolean
couldsee_func(int x, int y)
{
    return couldsee(x, y);
}

/* Note: thismove is occ_none for the single-space movement commands, even if
   they were command-repeated; its purpose is to distinguish between commands */
int
domove(const struct nh_cmd_arg *arg, enum u_interaction_mode uim,
       enum occupation thismove)
{
    struct monst *mtmp;
    struct rm *tmpr;
    xchar x, y;
    struct trap *trap;
    int wtcap;
    boolean on_ice;
    xchar chainx = -1, chainy = -1, ballx = -1, bally = -1;
    int bc_control = 0;     /* control for ball&chain */
    boolean cause_delay = FALSE;        /* dragging ball will skip a move */
    const char *predicament, *killer = 0;
    schar dz = 0;

    /* If we're running, mark the current space to avoid infinite loops. */
    if (thismove == occ_move)
        turnstate.move.stepped_on[u.ux][u.uy] = TRUE;

    /* If we already have values for dx and dy, it means we're running.
       We don't want to overwrite them in that case, or else turning corners
       breaks. Exception: we could have aborted a run into a walk. */
    if (((!turnstate.move.dx && !turnstate.move.dy) || thismove != occ_move) &&
        !getargdir(arg, NULL, &turnstate.move.dx, &turnstate.move.dy, &dz))
        return 0;

    if (dz) {
        action_completed();
        if (dz < 0)
            return doup(uim);
        else
            return dodown(uim);
    }

    u_wipe_engr(rnd(5));

    /* Don't allow running, travel or autoexplore when stunned or confused. */
    if (Stunned || Confusion) {
        const char *stop_which = NULL;

        if (thismove == occ_autoexplore)
            stop_which = "explore";
        else if (thismove == occ_travel)
            stop_which = "travel";
        else if (thismove == occ_move)
            stop_which = "run";

        if (stop_which) {
            pline("Your head is spinning too badly to %s.", stop_which);
            action_completed();
            return 0;
        }
    }

    if (travelling()) {
        if (thismove == occ_autoexplore) {
            if (Blind) {
                pline("You can't see where you're going!");
                action_completed();
                return 0;
            }
            if (In_sokoban(&u.uz)) {
                pline("You somehow know the layout of this place without "
                      "exploring.");
                action_completed();
                return 0;
            }
            if (u.uhs >= WEAK) {
                pline("You feel too weak from hunger to explore.");
                action_completed();
                return 0;
            }
            u.tx = u.ux;
            u.ty = u.uy;
            if (!findtravelpath(unexplored, &turnstate.move.dx,
                &turnstate.move.dy, uim)) {
                pline
                    ("Nowhere else around here can be automatically explored.");
            }
        } else if (!findtravelpath(NULL, &turnstate.move.dx,
                   &turnstate.move.dy, uim)) {
            findtravelpath(couldsee_func, &turnstate.move.dx,
                           &turnstate.move.dy, uim);
        }

        if (turnstate.move.dx == 0 && turnstate.move.dy == 0) {
            action_completed();
            return 0;
        }
    }

    /* Travel hit an obstacle, or domove() was called with dx, dy and dz all
       zero, which they shouldn't do. */
    if (turnstate.move.dx == 0 && turnstate.move.dy == 0) {
        /* dz is always zero here from above */
        action_completed();
        return 0;
    }

    if (((wtcap = near_capacity()) >= OVERLOADED ||
         (wtcap > SLT_ENCUMBER && (Upolyd ? (u.mh < 5 && u.mh != u.mhmax)
                                   : (u.uhp < 10 && u.uhp != u.uhpmax))))
        && !Is_airlevel(&u.uz)) {
        if (wtcap < OVERLOADED) {
            pline("You don't have enough stamina to move.");
            exercise(A_CON, FALSE);
        } else
            pline("You collapse under your load.");
        action_completed();
        return 1;
    }
    if (Engulfed) {
        turnstate.move.dx = turnstate.move.dy = 0;
        u.ux = x = u.ustuck->mx;
        u.uy = y = u.ustuck->my;
        mtmp = u.ustuck;
    } else {
        if (Is_airlevel(&u.uz) && rn2(4) && !Levitation && !Flying) {
            switch (rn2(3)) {
            case 0:
                pline("You tumble in place.");
                exercise(A_DEX, FALSE);
                break;
            case 1:
                pline("You can't control your movements very well.");
                break;
            case 2:
                pline("It's hard to walk in thin air.");
                exercise(A_DEX, TRUE);
                break;
            }
            return 1;
        }

        /* check slippery ice */
        on_ice = !Levitation && is_ice(level, u.ux, u.uy);
        if (on_ice) {
            static int skates = 0;

            if (!skates)
                skates = find_skates();
            if ((uarmf && uarmf->otyp == skates)
                || resists_cold(&youmonst) || Flying ||
                is_floater(youmonst.data) || is_clinger(youmonst.data)
                || is_whirly(youmonst.data))
                on_ice = FALSE;
            else if (!rn2(Cold_resistance ? 3 : 2)) {
                HFumbling |= FROMOUTSIDE;
                HFumbling &= ~TIMEOUT;
                HFumbling += 1; /* slip on next move */
            }
        }
        if (!on_ice && (HFumbling & FROMOUTSIDE))
            HFumbling &= ~FROMOUTSIDE;

        x = u.ux + turnstate.move.dx;
        y = u.uy + turnstate.move.dy;
        if (Stunned || (Confusion && !rn2(5))) {
            int tries = 0;

            do {
                if (tries++ > 50) {
                    action_completed();
                    return 1;
                }
                confdir(&turnstate.move.dx, &turnstate.move.dy);
                x = u.ux + turnstate.move.dx;
                y = u.uy + turnstate.move.dy;
            } while (!isok(x, y) || bad_rock(youmonst.data, x, y));
        }
        /* turbulence might alter your actual destination */
        if (u.uinwater) {
            water_friction(&turnstate.move.dx, &turnstate.move.dy);
            if (!turnstate.move.dx && !turnstate.move.dy) {
                action_completed();
                return 1;
            }
            x = u.ux + turnstate.move.dx;
            y = u.uy + turnstate.move.dy;
        }
        if (!isok(x, y)) {
            action_completed();
            return 0;
        }

        /* Traps can only be entered via the single-step movement commands.
           However, they can be entered even without the 'm' safety prefix. */
        if (((trap = t_at(level, x, y)) && trap->tseen &&
             trap->ttyp != VIBRATING_SQUARE)) {
            if (thismove != occ_none || !turnstate.continue_message) {
                autoexplore_msg("a trap", DO_MOVE);
                action_completed();
                return 0;
            } else
                action_completed();
        }

        if (u.ustuck && (x != u.ustuck->mx || y != u.ustuck->my)) {
            if (distu(u.ustuck->mx, u.ustuck->my) > 2) {
                /* perhaps it fled (or was teleported or ... ) */
                u.ustuck = 0;
            } else if (sticks(youmonst.data)) {
                /* When polymorphed into a sticking monster, u.ustuck means
                   it's stuck to you, not you to it. */
                pline("You release %s.", mon_nam(u.ustuck));
                u.ustuck = 0;
            } else {
                /* If holder is asleep or paralyzed: 37.5% chance of getting
                   away, 12.5% chance of waking/releasing it; otherwise: 7.5%
                   chance of getting away. [strength ought to be a factor] If
                   holder is tame and there is no conflict, guaranteed escape.
                   */
                switch (rn2(!u.ustuck->mcanmove ? 8 : 40)) {
                case 0:
                case 1:
                case 2:
                pull_free:
                    pline("You pull free from %s.", mon_nam(u.ustuck));
                    u.ustuck = 0;
                    break;
                case 3:
                    if (!u.ustuck->mcanmove) {
                        /* it's free to move on next turn */
                        u.ustuck->mfrozen = 1;
                        u.ustuck->msleeping = 0;
                    }
                /*FALLTHRU*/ default:
                    if (u.ustuck->mtame && !Conflict && !u.ustuck->mconf)
                        goto pull_free;
                    pline("You cannot escape from %s!", mon_nam(u.ustuck));
                    action_completed();
                    return 1;
                }
            }
        }

        mtmp = m_at(level, x, y);
        /* Checks for confirm, autoexplore have moved to attack_checks(). */
    }

    /* Call lookaround here if running, because we need to know whether to
     * change directions to round a corner. */
    if (last_command_was("run")) {
        lookaround(uim_displace);
        if (flags.interrupted) {
            turnstate.move.dx = 0;
            turnstate.move.dy = 0;
            return 0;
        }
    }

    u.ux0 = u.ux;
    u.uy0 = u.uy;
    bhitpos.x = x;
    bhitpos.y = y;
    tmpr = &level->locations[x][y];

    /* If the character remembers an invisible monster on the square, then
       if they're a pacifist they're not going to risk attacking it. */
    if (level->locations[x][y].mem_invis && !UIM_AGGRESSIVE(uim)) {
        pline("You don't want to risk attacking something.");
        return 0;
    }

    /* There's a monster here. Do we try to attack it?

       The logic here has been simplified from previous via not duplicating half
       of attack_checks(). For anyone wondering where the forcefight checks
       went, they're in is_safepet and attack_checks now. */
    if (mtmp) {
        enum attack_check_status attack_status = ac_continue;

        if (!is_safepet(mtmp, uim)) {
            attack_status = attack_checks(mtmp, uwep, turnstate.move.dx,
                                          turnstate.move.dy, uim);
            action_completed();
        }

        if (attack_status != ac_continue)
            return attack_status != ac_cancel;

        /* attack_checks said "Sure, go ahead with the attack attempt", so at
           this point, we know that the character has a pretty good idea that
           there's a monster there (enough that the attack will be definitely
           attempted at this point). Exception: for safepet, we use the same
           process, even moving into the attack, but attack() rejects the
           attack. This causes displacement to have the same nutrition/exertion
           penalties as attacking, which is 3.4.3 behaviour; it may well have
           been an accident, but players have grown to expect it. */

        gethungry();
        if (wtcap >= HVY_ENCUMBER && moves % 3) {
            if (Upolyd && u.mh > 1) {
                u.mh--;
            } else if (!Upolyd && u.uhp > 1) {
                u.uhp--;
            } else {
                pline("You pass out from exertion!");
                exercise(A_CON, FALSE);
                helpless(10, hr_fainted, "passed out from exertion", NULL);
            }
        }
        if (u_helpless(hm_all))
            return 1;       /* we just fainted */

        if (!is_safepet(mtmp, uim)) {

            /* Try to perform the attack itself. We've already established that
               the player's willing to perform the attack, so we crank the
               interaction mode all the way up to "indiscriminate". This is
               necessary to avoid double peaceful prompts (and also in the
               dubious situation where there's an invisible-monster I over a
               peaceful mimic). Don't downgrade "forcefight", in case there's no
               visible monster there at all. */
            attack_status = attack(mtmp, turnstate.move.dx,
                                   turnstate.move.dy,
                                   uim == uim_forcefight ? uim :
                                   uim_indiscriminate);
            if (attack_status != ac_continue)
                return attack_status != ac_cancel;

            /* This point is only reached if the monster dodged. It used to be
               that you could cheese stumbling when dodged via always using an
               explicit F when attacking leprechauns, but that makes no sense,
               so force forcefight off in such cases, and give a message. */
            pline("You miss wildly and stumble forwards.");
            if (uim == uim_forcefight)
                uim = uim_indiscriminate;

        } else {
            /* This section of code provides protection against accidentally
               hitting peaceful (like '@') and tame (like 'd') monsters.
               Protection is provided as long as player is not: blind, confused,
               hallucinating or stunned.

               Changes by wwp 5/16/85.

               More changes 12/90, -dkh-. If it's tame and safepet, (and
               protected 07/92) then we assume that you're not trying to
               attack. Instead, you'll usually just swap places if this is a
               movement command.

               There are some additional considerations: this won't work if in a
               shop or Punished or you miss a random roll or if you can walk
               through walls and your pet cannot (KAA) or if your pet is a long
               worm (unless someone does better). There's also a chance of
               displacing a "frozen" monster. Sleeping monsters might magically
               walk in their sleep. */
            boolean foo = (Punished || !rn2(7) ||
                           is_longworm(mtmp->data)), inshop = FALSE;
            char *p;

            for (p = in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE); *p; p++)
                if (tended_shop(&level->rooms[*p - ROOMOFFSET])) {
                    inshop = TRUE;
                    break;
                }

            if (inshop || foo ||
                (IS_ROCK(level->locations[u.ux][u.uy].typ) &&
                 !passes_walls(mtmp->data))) {
                monflee(mtmp, rnd(6), FALSE, FALSE);
                pline("You stop.  %s is in the way!",
                      msgupcasefirst(y_monnam(mtmp)));
                action_completed();
                return 1;
            } else if ((mtmp->mfrozen || (!mtmp->mcanmove)
                        || (mtmp->data->mmove == 0)) && rn2(6)) {
                pline("%s doesn't seem to move!", Monnam(mtmp));
                action_completed();
                return 1;
            }
        }
    }

    struct nh_cmd_arg newarg;
    arg_from_delta(turnstate.move.dx, turnstate.move.dy, dz, &newarg);

    boolean was_mem_invis = level->locations[x][y].mem_invis;

    /* Regardless of whether the player was trying to attack or to move onto the
       square, they're going to interact with the square, and will discover if
       it contains a monster as a result. */
    reveal_monster_at(x, y, TRUE);

    /* Either there isn't (any more) a monster there, or there is a safepet
       there. Does the character try to attack the square? They will if there's
       a remembered-monster 'I' and they aren't using a pacifist style of
       movement, or if they were force-fighting. */
    if (uim == uim_forcefight || (was_mem_invis && UIM_AGGRESSIVE(uim))) {
        boolean expl = (Upolyd && attacktype(youmonst.data, AT_EXPL));
        boolean hitsomething = FALSE, ouch = FALSE;
        struct obj *boulder = (sobj_at(BOULDER, level, x, y));
        if (!boulder)
            boulder = sobj_at(STATUE, level, x, y);
        if (boulder) {
            hitsomething = TRUE;
            if (uwep && (is_pick(uwep) || is_axe(uwep)) &&
                use_pick_axe(uwep, &newarg));
                /* use_pick_axe succeeded, don't do anything else */
            else if (uwep) {
                ouch = !rn2(3);
                pline("%s whack the %s.",
                      objects[uwep->otyp].oc_material == IRON ?
                      "Sparks fly as you" : "You",
                      boulder->otyp == STATUE ? "statue" : "boulder");
                if (ouch)
                    killer = msgprintf("hitting %s", killer_xname(boulder));
            } else {
                pline("You %s the %s.", Role_if(PM_MONK) ? "strike" : "bash",
                      boulder->otyp == STATUE ? "statue" : "boulder");
                ouch = TRUE;
                killer = msgprintf("punching %s", killer_xname(boulder));
            }
            /* TODO: Possibly make the player hurtle after striking. */
        }
        struct rm *levloc = &(level->locations[x][y]);
        if (!hitsomething &&
            ((levloc->typ == STAIRS) ||
             ((levloc->typ == LADDER) && (levloc->ladder != LA_DOWN)) ||
             IS_STWALL(levloc->typ) || IS_TREE(levloc->typ) ||
             (levloc->typ == DOOR &&
              !!(levloc->doormask & (D_CLOSED|D_LOCKED)))))
        {
            hitsomething = TRUE;
            if (uwep && ((is_pick(uwep) && !IS_TREE(levloc->typ)) ||
                         (is_axe(uwep) && IS_TREE(levloc->typ))) &&
                use_pick_axe(uwep, &newarg))
                return 1;
            else {
                ouch = (uwep) ? !rn2(3) : TRUE;
                pline("%s %s the %s.",
                      uwep && objects[uwep->otyp].oc_material == IRON ?
                      "Sparks fly as you" : "You",
                      uwep ? "whack" : Role_if(PM_MONK) ? "strike" : "bash",
                      IS_TREE(levloc->typ) ? "tree" :
                      levloc->typ == STAIRS ? "stairs" :
                      levloc->typ == LADDER ? "ladder" :
                      levloc->typ == DOOR ? "door" : "wall");
                killer = msgprintf(
                    "hitting %s",
                    IS_TREE(levloc->typ) ? "tree" :
                    levloc->typ == STAIRS ? "the stairs" :
                    levloc->typ == LADDER ? "a ladder" :
                    levloc->typ == DOOR ? "a door" : "a wall");
            }
        }
        if (ouch) {
            if (uwep)
                pline("%s %s violently!", Yname2(uwep),
                      otense(uwep, "vibrate"));
            else
                pline("Ouch!  That hurts!");
            losehp(2, killer_msg(DIED, killer));
        } else if (!hitsomething) {
            const char *buf = msgcat("a vacant spot on the ", surface(x,y));
            pline("You %s %s.", expl ? "explode at" : "attack",
                  !Underwater ? "thin air" :
                  is_pool(level, x, y) ? "empty water" : buf);
        }

        action_completed();
        if (expl) {
            u.mh = -1;  /* dead in the current form */
            rehumanize(EXPLODED, "exploded in a futile attempt to attack");
        }
        return 1;
    }

    /* Not attacking a monster, for whatever reason; we try to move. */
    if (u.usteed && !u.usteed->mcanmove &&
        (turnstate.move.dx || turnstate.move.dy)) {
        pline("%s won't move!", msgupcasefirst(y_monnam(u.usteed)));
        action_completed();
        return 1;
    } else if (!youmonst.data->mmove) {
        pline("You are rooted %s.", Levitation || Is_airlevel(&u.uz) ||
              Is_waterlevel(&u.uz) ? "in place" : "to the ground");
        action_completed();
        return 1;
    }
    if (u.utrap) {
        if (u.utraptype == TT_PIT) {
            if (!rn2(2) && sobj_at(BOULDER, level, u.ux, u.uy)) {
                pline("Your %s gets stuck in a crevice.", body_part(LEG));
                win_pause_output(P_MESSAGE);
                pline("You free your %s.", body_part(LEG));
            } else if (!(--u.utrap)) {
                pline("You %s to the edge of the pit.",
                      (In_sokoban(&u.uz) &&
                       Levitation) ?
                      "struggle against the air currents and float" : u.usteed ?
                      "ride" : "crawl");
                fill_pit(level, u.ux, u.uy);
                turnstate.vision_full_recalc = TRUE; /* vision limits change */
            } else if (flags.verbose) {
                if (u.usteed)
                    pline_once("%s is still in a pit.",
                               msgupcasefirst(y_monnam(u.usteed)));
                else
                    pline_once((Hallucination && !rn2(5)) ?
                               "You've fallen, and you can't get up." :
                               "You are still in a pit.");
            }
        } else if (u.utraptype == TT_LAVA) {
            if (flags.verbose) {
                predicament = "stuck in the lava";
                if (u.usteed)
                    pline_once("%s is %s.", msgupcasefirst(y_monnam(u.usteed)),
                          predicament);
                else
                    pline_once("You are %s.", predicament);
            }
            if (!is_lava(level, x, y)) {
                u.utrap--;
                if ((u.utrap & 0xff) == 0) {
                    if (u.usteed)
                        pline("You lead %s to the edge of the lava.",
                              y_monnam(u.usteed));
                    else
                        pline("You pull yourself to the edge of the lava.");
                    u.utrap = 0;
                }
            }
            u.umoved = TRUE;
        } else if (u.utraptype == TT_WEB) {
            if (uwep && uwep->oartifact == ART_STING) {
                u.utrap = 0;
                pline("Sting cuts through the web!");
                return 1;
            }
            if (--u.utrap) {
                if (flags.verbose) {
                    predicament = "stuck to the web";
                    if (u.usteed)
                        pline_once("%s is %s.",
                                   msgupcasefirst(y_monnam(u.usteed)),
                                   predicament);
                    else
                        pline_once("You are %s.", predicament);
                }
            } else {
                if (u.usteed)
                    pline("%s breaks out of the web.",
                          msgupcasefirst(y_monnam(u.usteed)));
                else
                    pline("You disentangle yourself.");
            }
        } else if (u.utraptype == TT_INFLOOR) {
            if (--u.utrap) {
                if (flags.verbose) {
                    predicament = "stuck in the";
                    if (u.usteed)
                        pline_once("%s is %s %s.",
                                   msgupcasefirst(y_monnam(u.usteed)),
                                   predicament, surface(u.ux, u.uy));
                    else
                        pline_once("You are %s %s.", predicament,
                                   surface(u.ux, u.uy));
                }
            } else {
                if (u.usteed)
                    pline("%s finally wiggles free.",
                          msgupcasefirst(y_monnam(u.usteed)));
                else
                    pline("You finally wiggle free.");
            }
        } else {
            if (flags.verbose) {
                predicament = "caught in a bear trap";
                if (u.usteed)
                    pline_once("%s is %s.",
                               msgupcasefirst(y_monnam(u.usteed)),
                               predicament);
                else
                    pline_once("You are %s.", predicament);
            }
            if ((turnstate.move.dx && turnstate.move.dy) || !rn2(5))
                u.utrap--;
        }
        return 1;
    }

    /* If moving into a door, open it. */
    if (IS_DOOR(tmpr->typ) && tmpr->doormask != D_BROKEN &&
        tmpr->doormask != D_NODOOR && tmpr->doormask != D_ISOPEN &&
        ITEM_INTERACTIVE(uim)) {
        if (!doopen(&newarg)) {
            action_completed();
            return 0;
        }
        return 1;
    }

    if (!test_move(u.ux, u.uy, turnstate.move.dx, turnstate.move.dy, dz,
                   DO_MOVE, uim, !!Blind, !!Stunned, !!Fumbling,
                   !!Hallucination, !!Passes_walls, !!Ground_based)) {
        /* We can't move there... but maybe we can dig. */
        if (flags.autodig && ITEM_INTERACTIVE(uim) &&
            thismove != occ_move && uwep && is_pick(uwep)) {
            /* MRKR: Automatic digging when wielding the appropriate tool */
            return use_pick_axe(uwep, &newarg);
        }
        action_completed();
        return 0;
    }

    /* If not using 'moveonly', veto dangerous moves. (This uses
       last_command_was, not uim, because the relevant factor is not the
       semantics of the command, but the interface used to enter it.) */
    if (!last_command_was("moveonly")) {
        boolean lava = level->locations[x][y].mem_bg == S_lava;
        boolean pool = level->locations[x][y].mem_bg == S_pool;

        if (!Levitation && !Flying && !is_clinger(youmonst.data) &&
            !Stunned && !Confusion && (lava || (pool && !HSwimming)) &&
            !is_pool(level, u.ux, u.uy) && !is_lava(level, u.ux, u.uy)) {

            if (cansee(x, y))
                pline(is_pool(level, x, y) ? "You never learned to swim!" :
                      "That lava looks rather dangerous...");
            else
                pline("As far as you can remember, it's "
                      "not safe to stand there.");
            pline("(Use the 'moveonly' command to move there anyway.)");
            action_completed();
            return 0;
        }
    }

    /* Move ball and chain.  */
    if (Punished)
        if (!drag_ball
            (x, y, &bc_control, &ballx, &bally, &chainx, &chainy, &cause_delay,
             TRUE))
            return 1;

    /* Check regions entering/leaving */
    if (!in_out_region(level, x, y))
        return 1;       /* unable to enter the region but trying took time */

    /* now move the hero */
    mtmp = m_at(level, x, y);
    u.ux += turnstate.move.dx;
    u.uy += turnstate.move.dy;
    /* Move your steed, too */
    if (u.usteed) {
        u.usteed->mx = u.ux;
        u.usteed->my = u.uy;
        exercise_steed();
    }

    /*
     * If safepet at destination then move the pet to the hero's
     * previous location using the same conditions as in attack().
     * there are special extenuating circumstances:
     * (1) if the pet dies then your god angers,
     * (2) if the pet gets trapped then your god may disapprove,
     * (3) if the pet was already trapped and you attempt to free it
     * not only do you encounter the trap but you may frighten your
     * pet causing it to go wild!  moral: don't abuse this privilege.
     *
     * Ceiling-hiding pets are skipped by this section of code, to
     * be caught by the normal falling-monster code.
     */
    if (is_safepet(mtmp, uim) && !(is_hider(mtmp->data) && mtmp->mundetected)) {
        /* if trapped, there's a chance the pet goes wild */
        if (mtmp->mtrapped) {
            if (!rn2(mtmp->mtame)) {
                mtmp->msleeping = 0;
                msethostility(mtmp, TRUE, FALSE);
                if (mtmp->mleashed)
                    m_unleash(mtmp, TRUE);
                growl(mtmp);
            } else {
                yelp(mtmp);
            }
        }
        mtmp->mundetected = 0;
        if (mtmp->m_ap_type)
            seemimic(mtmp);
        else if (!mtmp->mtame)
            newsym(mtmp->mx, mtmp->my);

        if (mtmp->mtrapped && (trap = t_at(level, mtmp->mx, mtmp->my)) != 0 &&
            (trap->ttyp == PIT || trap->ttyp == SPIKED_PIT) &&
            sobj_at(BOULDER, level, trap->tx, trap->ty)) {
            /* can't swap places with pet pinned in a pit by a boulder */
            u.ux = u.ux0, u.uy = u.uy0; /* didn't move after all */
        } else if (u.ux0 != x && u.uy0 != y && bad_rock(mtmp->data, x, u.uy0) &&
                   bad_rock(mtmp->data, u.ux0, y) &&
                   (bigmonst(mtmp->data) || (curr_mon_load(mtmp) > 600))) {
            /* can't swap places when pet won't fit thru the opening */
            u.ux = u.ux0, u.uy = u.uy0; /* didn't move after all */
            pline("You stop.  %s won't fit through.",
                  msgupcasefirst(y_monnam(mtmp)));
        } else {
            /* save its current description in case of polymorph */
            const char *pnambuf = y_monnam(mtmp);

            mtmp->mtrapped = 0;
            remove_monster(level, x, y);
            place_monster(mtmp, u.ux0, u.uy0);
            pline("You %s %s.", mtmp->mtame ? "displace" : "frighten", pnambuf);

            /* check for displacing it into pools and traps */
            switch (minliquid(mtmp) ? 2 : mintrap(mtmp)) {
            case 0:
                break;
            case 1:    /* trapped */
            case 3:    /* changed levels */
                /* there's already been a trap message, reinforce it */
                abuse_dog(mtmp);
                adjalign(-3);
                break;
            case 2:
                /* it may have drowned or died.  that's no way to treat a pet!
                   your god gets angry. */
                if (rn2(4)) {
                    pline("You feel guilty about losing your pet like this.");
                    u.ugangr++;
                    adjalign(-15);
                }

                /* you killed your pet by direct action. minliquid and mintrap
                   don't know to do this */
                break_conduct(conduct_killer);
                break;
            default:
                pline("that's strange, unknown mintrap result!");
                break;
            }
        }
    }

    reset_occupations(TRUE);
    if (thismove == occ_move) {
        if (IS_DOOR(tmpr->typ) || IS_ROCK(tmpr->typ) ||
            IS_FURNITURE(tmpr->typ))
            action_completed();
    } else if (thismove == occ_autoexplore) {
        int wallcount, mem_bg;

        /* autoexplore stoppers: being orthogonally adjacent to a boulder,
           being orthogonally adjacent to 3 or more walls; this logic could
           be incorrect when blind, but we check for that earlier; while not
           blind, we'll assume the hero knows about adjacent walls and
           boulders due to being able to see them */
        wallcount = 0;
        if (isok(u.ux - 1, u.uy))
            wallcount +=
                IS_ROCK(level->locations[u.ux - 1][u.uy].typ) +
                ! !sobj_at(BOULDER, level, u.ux - 1, u.uy) * 3;
        if (isok(u.ux + 1, u.uy))
            wallcount +=
                IS_ROCK(level->locations[u.ux + 1][u.uy].typ) +
                ! !sobj_at(BOULDER, level, u.ux + 1, u.uy) * 3;
        if (isok(u.ux, u.uy - 1))
            wallcount +=
                IS_ROCK(level->locations[u.ux][u.uy - 1].typ) +
                ! !sobj_at(BOULDER, level, u.ux, u.uy - 1) * 3;
        if (isok(u.ux, u.uy + 1))
            wallcount +=
                IS_ROCK(level->locations[u.ux][u.uy + 1].typ) +
                ! !sobj_at(BOULDER, level, u.ux, u.uy + 1) * 3;
        if (wallcount >= 3)
            action_completed();
        /*
         * More autoexplore stoppers: interesting dungeon features
         * that haven't been stepped on yet.
         */
        mem_bg = tmpr->mem_bg;
        if (tmpr->mem_stepped == 0 &&
            (mem_bg == S_altar || mem_bg == S_throne || mem_bg == S_sink ||
             mem_bg == S_fountain || mem_bg == S_dnstair ||
             mem_bg == S_upstair || mem_bg == S_dnsstair ||
             mem_bg == S_upsstair || mem_bg == S_dnladder ||
             mem_bg == S_upladder))
            action_completed();
    }

    if (hides_under(youmonst.data))
        u.uundetected = OBJ_AT(u.ux, u.uy);
    else if (youmonst.data->mlet == S_EEL)
        u.uundetected = is_pool(level, u.ux, u.uy) && !Is_waterlevel(&u.uz);
    else if (turnstate.move.dx || turnstate.move.dy)
        u.uundetected = 0;

    /*
     * Mimics (or whatever) become noticeable if they move and are
     * imitating something that doesn't move.  We could extend this
     * to non-moving monsters...
     */
    if ((turnstate.move.dx || turnstate.move.dy) &&
        (youmonst.m_ap_type == M_AP_OBJECT ||
         youmonst.m_ap_type == M_AP_FURNITURE))
        youmonst.m_ap_type = M_AP_NOTHING;

    check_leash(u.ux0, u.uy0);

    if (u.ux0 != u.ux || u.uy0 != u.uy) {
        u.umoved = TRUE;
        /* Clean old position -- vision_recalc() will print our new one. */
        newsym(u.ux0, u.uy0);
        /* Since the hero has moved, adjust what can be seen/unseen. */
        vision_recalc(1);       /* Do the work now in the recover time. */
        invocation_message();
    }

    if (Punished)       /* put back ball and chain */
        move_bc(0, bc_control, ballx, bally, chainx, chainy);

    spoteffects(TRUE);

    /* delay next move because of ball dragging */
    /* must come after we finished picking up, in spoteffects() */
    if (cause_delay)
        helpless(2, hr_moving, "dragging an iron ball", NULL);

    return 1;
}

void
invocation_message(void)
{
    /* a special clue-msg when on the Invocation position */
    if (invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy)) {
        const char *buf;
        struct obj *otmp = carrying(CANDELABRUM_OF_INVOCATION);

        if (flags.occupation == occ_move ||
            flags.occupation == occ_travel ||
            flags.occupation == occ_autoexplore)
            action_completed();

        if (u.usteed)
            buf = msgprintf("beneath %s", y_monnam(u.usteed));
        else if (Levitation || Flying)
            buf = "beneath you";
        else
            buf = msgprintf("under your %s", makeplural(body_part(FOOT)));

        pline("You feel a %s vibration %s.",
              (Hallucination ? "normal" : "strange"), buf);
        if (otmp && otmp->spe == 7 && otmp->lamplit)
            pline("%s %s!", The(xname(otmp)),
                  Blind ? "throbs palpably" :
                  Hallucination ? "glows with a normal light" :
                  "glows with a strange light");
    }
}


void
spoteffects(boolean pick)
{
    struct monst *mtmp;

    /* clear items remembered by the ui at this location */
    win_list_items(NULL, FALSE);

    if (u.uinwater) {
        int was_underwater;

        if (!is_pool(level, u.ux, u.uy)) {
            if (Is_waterlevel(&u.uz))
                pline("You pop into an air bubble.");
            else if (is_lava(level, u.ux, u.uy))
                pline("You leave the water...");        /* oops! */
            else
                pline("You are on solid %s again.",
                      is_ice(level, u.ux, u.uy) ? "ice" : "land");
        } else if (Is_waterlevel(&u.uz))
            goto stillinwater;
        else if (Levitation)
            pline("You pop out of the water like a cork!");
        else if (Flying)
            pline("You fly out of the water.");
        else if (Wwalking)
            pline("You slowly rise above the surface.");
        else
            goto stillinwater;
        was_underwater = Underwater && !Is_waterlevel(&u.uz);
        u.uinwater = 0; /* leave the water */
        if (was_underwater) {   /* restore vision */
            doredraw();
            turnstate.vision_full_recalc = TRUE;
        }
    }

stillinwater:
    if (!Levitation && !u.ustuck && !Flying) {
        /* limit recursive calls through teleds() */
        if (is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)) {
            if (u.usteed && !is_flyer(u.usteed->data) &&
                !is_floater(u.usteed->data) && !is_clinger(u.usteed->data)) {
                dismount_steed(Underwater ? DISMOUNT_FELL : DISMOUNT_GENERIC);
                /* dismount_steed() -> float_down() -> pickup() */
                if (!Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz))
                    pick = FALSE;
            } else if (is_lava(level, u.ux, u.uy)) {
                if (lava_effects())
                    return;
            } else if (!Wwalking && drown())
                return;
        }
    }
    check_special_room(FALSE);
    if (IS_SINK(level->locations[u.ux][u.uy].typ) && Levitation)
        dosinkfall();
    if (!in_steed_dismounting) {        /* if dismounting, we'll check again
                                           later */
        struct trap *trap = t_at(level, u.ux, u.uy);
        boolean pit;

        pit = (trap && (trap->ttyp == PIT || trap->ttyp == SPIKED_PIT));
        if (trap && pit)
            dotrap(trap, 0);    /* fall into pit */
        /* TODO: This might not be correct if you m-direction into a pit. */
        if (pick)
            pickup(1, flags.interaction_mode);
        if (trap && !pit)
            dotrap(trap, 0);    /* fall into arrow trap, etc. */
    }
    if ((mtmp = m_at(level, u.ux, u.uy)) && !Engulfed) {
        mtmp->mundetected = mtmp->msleeping = 0;
        switch (mtmp->data->mlet) {
        case S_PIERCER:
            pline("%s suddenly drops from the %s!", Amonnam(mtmp),
                  ceiling(u.ux, u.uy));
            if (mtmp->mtame)    /* jumps to greet you, not attack */
                ;
            else if (uarmh && is_metallic(uarmh))
                pline("Its blow glances off your %s.", helmet_name(uarmh));
            else if (get_player_ac() + 3 <= rnd(20))
                pline("You are almost hit by %s!",
                      x_monnam(mtmp, ARTICLE_A, "falling", 0, TRUE));
            else {
                int dmg;

                pline("You are hit by %s!",
                      x_monnam(mtmp, ARTICLE_A, "falling", 0, TRUE));
                dmg = dice(4, 6);
                if (Half_physical_damage)
                    dmg = (dmg + 1) / 2;
                mdamageu(mtmp, dmg);
            }
            break;
        default:       /* monster surprises you. */
            if (mtmp->mtame)
                pline("%s jumps near you from the %s.", Amonnam(mtmp),
                      ceiling(u.ux, u.uy));
            else if (mtmp->mpeaceful) {
                pline("You surprise %s!", Blind &&
                      !sensemon(mtmp) ? "something" : a_monnam(mtmp));
                msethostility(mtmp, TRUE, FALSE);
            } else
                pline("%s attacks you by surprise!", Amonnam(mtmp));
            break;
        }
        mnexto(mtmp);   /* have to move the monster */
    }
}

static struct monst *
monstinroom(const struct permonst *mdat, int roomno)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp) && mtmp->data == mdat &&
            strchr(in_rooms(level, mtmp->mx, mtmp->my, 0), roomno + ROOMOFFSET))
            return mtmp;
    return NULL;
}

char *
in_rooms(struct level *lev, xchar x, xchar y, int typewanted)
{
    static char buf[5];
    char rno, *ptr = &buf[4];
    int typefound, min_x, min_y, max_x, max_y_offset, step;
    struct rm *loc;

#define goodtype(rno) (!typewanted || \
            ((typefound = lev->rooms[rno - ROOMOFFSET].rtype) == \
                typewanted) || \
            ((typewanted == SHOPBASE) && (typefound > SHOPBASE)))

    switch (rno = lev->locations[x][y].roomno) {
    case NO_ROOM:
        return ptr;
    case SHARED:
        step = 2;
        break;
    case SHARED_PLUS:
        step = 1;
        break;
    default:   /* i.e. a regular room # */
        if (goodtype(rno))
            *(--ptr) = rno;
        return ptr;
    }

    min_x = x - 1;
    max_x = x + 1;
    if (x < 0)
        min_x += step;
    else if (x >= COLNO)
        max_x -= step;

    min_y = y - 1;
    max_y_offset = 2;
    if (min_y < 0) {
        min_y += step;
        max_y_offset -= step;
    } else if ((min_y + max_y_offset) >= ROWNO)
        max_y_offset -= step;

    for (x = min_x; x <= max_x; x += step) {
        loc = &lev->locations[x][min_y];
        y = 0;
        if (((rno = loc[y].roomno) >= ROOMOFFSET) && !strchr(ptr, rno) &&
            goodtype(rno))
            *(--ptr) = rno;
        y += step;
        if (y > max_y_offset)
            continue;
        if (((rno = loc[y].roomno) >= ROOMOFFSET) && !strchr(ptr, rno) &&
            goodtype(rno))
            *(--ptr) = rno;
        y += step;
        if (y > max_y_offset)
            continue;
        if (((rno = loc[y].roomno) >= ROOMOFFSET) && !strchr(ptr, rno) &&
            goodtype(rno))
            *(--ptr) = rno;
    }
    return ptr;
}

/* is (x,y) in a town? */
boolean
in_town(int x, int y)
{
    s_level *slev = Is_special(&u.uz);
    struct mkroom *sroom;
    boolean has_subrooms = FALSE;

    if (!slev || !slev->flags.town)
        return FALSE;

    /*
     * See if (x,y) is in a room with subrooms, if so, assume it's the
     * town.  If there are no subrooms, the whole level is in town.
     */
    for (sroom = &level->rooms[0]; sroom->hx > 0; sroom++) {
        if (sroom->nsubrooms > 0) {
            has_subrooms = TRUE;
            if (inside_room(sroom, x, y))
                return TRUE;
        }
    }

    return !has_subrooms;
}

static void
move_update(boolean newlev)
{
    char *ptr1, *ptr2, *ptr3, *ptr4;

    strcpy(u.urooms0, u.urooms);
    strcpy(u.ushops0, u.ushops);
    if (newlev) {
        u.urooms[0] = '\0';
        u.uentered[0] = '\0';
        u.ushops[0] = '\0';
        u.ushops_entered[0] = '\0';
        strcpy(u.ushops_left, u.ushops0);
        return;
    }
    strcpy(u.urooms, in_rooms(level, u.ux, u.uy, 0));

    for (ptr1 = &u.urooms[0], ptr2 = &u.uentered[0], ptr3 = &u.ushops[0], ptr4 =
         &u.ushops_entered[0]; *ptr1; ptr1++) {
        if (!strchr(u.urooms0, *ptr1))
            *(ptr2++) = *ptr1;
        if (IS_SHOP(*ptr1 - ROOMOFFSET)) {
            *(ptr3++) = *ptr1;
            if (!strchr(u.ushops0, *ptr1))
                *(ptr4++) = *ptr1;
        }
    }
    *ptr2 = '\0';
    *ptr3 = '\0';
    *ptr4 = '\0';

    /* filter u.ushops0 -> u.ushops_left */
    for (ptr1 = &u.ushops0[0], ptr2 = &u.ushops_left[0]; *ptr1; ptr1++)
        if (!strchr(u.ushops, *ptr1))
            *(ptr2++) = *ptr1;
    *ptr2 = '\0';
}

void
check_special_room(boolean newlev)
{
    struct monst *mtmp;
    char *ptr;

    move_update(newlev);

    if (*u.ushops0)
        u_left_shop(u.ushops_left, newlev);

    if (!*u.uentered && !*u.ushops_entered)     /* implied by newlev */
        return; /* no entrance messages necessary */

    /* Did we just enter a shop? */
    if (*u.ushops_entered)
        u_entered_shop(u.ushops_entered);

    for (ptr = &u.uentered[0]; *ptr; ptr++) {
        int roomno = *ptr - ROOMOFFSET;
        int rt = level->rooms[roomno].rtype;

        /* Did we just enter some other special room? */
        /* vault.c insists that a vault remain a VAULT, and temples should
           remain TEMPLEs, but everything else gives a message only the first
           time */
        switch (rt) {
        case ZOO:
            pline("Welcome to David's treasure zoo!");
            break;
        case SWAMP:
            pline("It %s rather %s down here.", Blind ? "feels" : "looks",
                  Blind ? "humid" : "muddy");
            break;
        case COURT:
            pline("You enter an opulent throne room!");
            break;
        case LEPREHALL:
            pline("You enter a leprechaun hall!");
            break;
        case MORGUE:
            if (midnight()) {
                const char *run = locomotion(youmonst.data, "Run");

                pline("%s away!  %s away!", run, run);
            } else
                pline("You have an uncanny feeling...");
            break;
        case BEEHIVE:
            pline("You enter a giant beehive!");
            break;
        case COCKNEST:
            pline("You enter a disgusting nest!");
            break;
        case ANTHOLE:
            pline("You enter an anthole!");
            break;
        case BARRACKS:
            if (monstinroom(&mons[PM_SOLDIER], roomno) ||
                monstinroom(&mons[PM_SERGEANT], roomno) ||
                monstinroom(&mons[PM_LIEUTENANT], roomno) ||
                monstinroom(&mons[PM_CAPTAIN], roomno))
                pline("You enter a military barracks!");
            else
                pline("You enter an abandoned barracks.");
            break;
        case DELPHI:
            if ((mtmp = monstinroom(&mons[PM_ORACLE], roomno)) &&
                mtmp->mpeaceful) {
                verbalize("%s, %s, welcome to Delphi!", Hello(NULL), u.uplname);
            }
            break;
        case TEMPLE:
            intemple(roomno + ROOMOFFSET);
            /* fall through */
        default:
            rt = 0;
        }

        if (rt != 0) {
            level->rooms[roomno].rtype = OROOM;
            if (rt == COURT || rt == SWAMP || rt == MORGUE || rt == ZOO)
                for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                    if (!DEADMONSTER(mtmp) && !Stealth && !rn2(3))
                        mtmp->msleeping = 0;
        }
    }

    return;
}


int
dopickup(const struct nh_cmd_arg *arg)
{
    int count = 0;
    struct trap *traphere = t_at(level, u.ux, u.uy);

    if (arg->argtype & CMD_ARG_LIMIT)
        count = arg->limit;
    if (count < 0)
        count = 0;

    /* Engulfed case added by GAN 01/29/87 */
    if (Engulfed) {
        if (!u.ustuck->minvent) {
            if (is_animal(u.ustuck->data)) {
                pline("You pick up %s tongue.", s_suffix(mon_nam(u.ustuck)));
                pline("But it's kind of slimy, so you drop it.");
            } else
                pline("You don't %s anything in here to pick up.",
                      Blind ? "feel" : "see");
            return 1;
        } else {
            int tmpcount = -count;

            return loot_mon(u.ustuck, &tmpcount, NULL);
        }
    }
    if (is_pool(level, u.ux, u.uy)) {
        if (Wwalking || is_floater(youmonst.data) || is_clinger(youmonst.data)
            || (Flying && !Breathless)) {
            pline("You cannot dive into the water to pick things up.");
            return 0;
        } else if (!Underwater) {
            pline
                ("You can't even see the bottom, let alone pick up something.");
            return 0;
        }
    }
    if (is_lava(level, u.ux, u.uy)) {
        if (Wwalking || is_floater(youmonst.data) || is_clinger(youmonst.data)
            || (Flying && !Breathless)) {
            pline("You can't reach the bottom to pick things up.");
            return 0;
        } else if (!likes_lava(youmonst.data)) {
            pline("You would burn to a crisp trying to pick things up.");
            return 0;
        }
    }
    if (!OBJ_AT(u.ux, u.uy)) {
        pline("There is nothing here to pick up.");
        return 0;
    }
    if (!can_reach_floor()) {
        if (u.usteed && P_SKILL(P_RIDING) < P_BASIC)
            pline("You aren't skilled enough to reach from %s.",
                  y_monnam(u.usteed));
        else
            pline("You cannot reach the %s.", surface(u.ux, u.uy));
        return 0;
    }

    if (traphere && traphere->tseen) {
        /* Allow pickup from holes and trap doors that you escaped from because
           that stuff is teetering on the edge just like you, but not pits,
           because there is an elevation discrepancy with stuff in pits. */
        if ((traphere->ttyp == PIT || traphere->ttyp == SPIKED_PIT) &&
            (!u.utrap || (u.utrap && u.utraptype != TT_PIT)) && !Passes_walls) {
            pline("You cannot reach the bottom of the pit.");
            return (0);
        }
    }

    /* We're picking up items explicitly, so override the interaction mode to
       specify that items are picked up. */
    return pickup(-count, uim_standard);
}

/* Calls action_completed()/action_interrupted() as appropriate for
   movement-related commands. Also responsible for turning corners.

   This code previously worked on a "flags.run" value that was explained only
   with the following obscure comment:

   0: h (etc), 1: H (etc), 2: fh (etc), 3: FH, 4: ff+, 5: ff-, 6: FF+, 7: FF-,
   8: travel

   8 is obvious, and 0 and 1 appear to correspond to simple move and shift-move,
   but if anyone knows what the other entries mean, please let me know, mostly
   out of curiosity.

   Reverse-engineering the code has produced the following findings:

   Aggressive farmove is 1 (and in previous versions, comes in pickup and
   non-pickup varieties, a feature that has been removed because based on
   talking to players, nobody even noticed the difference). "go" and "go2" -
   non-aggressive farmove - are 2 and 3 respectively (3 is documented to ignore
   branching corridors). 8 is travel (obviously), and the rest appear to be
   unused. (0, 4, 5, 6, and 7 all work the same way, but in a way different from
   each of 1, 2, 3, and 8.)

   The new calculation treats aggressive farmove like the former "1", travel
   like the former "8", and otherwise acts like 2 if the last command wasn't
   "go2" or 3 otherwise. This preserves the previous behaviour almost perfectly,
   although is still quite unintuitive. We may want to consider removing one of
   "go" and "go2"; the commands are noticeably different from aggressive farmove
   (and widely used), but players tend to use them interchangeably.
*/
/* Look around you.
 * Look around you.
 * Just look around you.
 * Have you worked out what we're looking for?
 * Correct!  The answer is: autotravel obstacles.
 */
void
lookaround(enum u_interaction_mode uim)
{
    /* x0 and y0 are candidate spaces to move into next; while dx and dy
       define the desired direction of movement, that might not actually be
       possible.

       m0 simply tracks whether there's a monster of any sort in the space we
       wind up picking, if we're considering changing directions.

       i0 is the square of the distance between the current-best option for x0
       and y0 and the desired movement space. Initially set high, x0 and y0
       selection optimizes it to be as low as possible. */
    int x, y, i, x0 = 0, y0 = 0, m0 = 1, i0 = 9;

    /*
     * corrct: The number of corridor spaces we've found around us.
     * noturn: Set to 1 if we shouldn't divert from the dx, dy plan.
     */
    int corrct = 0, noturn = 0;
    struct monst *mtmp;
    struct trap *trap;
    /* farmoving: occ_move, occ_travel, occ_autoexplore. */
    boolean farmoving = travelling() || flags.occupation == occ_move;
    /* aggressive_farmoving: I'm... not actually sure when this is true in any
       case that would matter.  Tempted to delete it with prejudice. */
    boolean aggressive_farmoving = ITEM_INTERACTIVE(uim) && !travelling();

    /* We need to do three things here.  First, make a few checks that are
       independent of the surrounding terrain.  Second, check whether the
       terrain around us makes us want to stop.  And third, if we're doing an
       aggressive directional farmove, make sure we round corners properly. */


    /* Grid bugs stop if trying to move diagonal, even if blind.  Maybe
       they polymorphed while in the middle of a long move. */
    if (u.umonnum == PM_GRID_BUG && turnstate.move.dx && turnstate.move.dy) {
        action_completed();
        return;
    }

    /* If we're about to step into a run-stopping space, stop. */
    if (turnstate.move.stepped_on[u.ux + turnstate.move.dx]
                                 [u.uy + turnstate.move.dy]) {
        action_interrupted();
        return;
    }

    /* If travel_interrupt is set, then stop if there is a hostile nearby.
       Exception: item-interactive (i.e. aggressive) farmoves, such as
       shift-direction. */
    if (farmoving && !aggressive_farmoving && flags.travel_interrupt) {
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
            if (distmin(u.ux, u.uy, mtmp->mx, mtmp->my) <= (BOLT_LIM + 1) &&
                couldsee(mtmp->mx, mtmp->my) && check_interrupt(mtmp)) {
                action_interrupted();
                return;
            }
        }
    }

    if (Blind || !farmoving)
        return;

    for (x = u.ux - 1; x <= u.ux + 1; x++)
        for (y = u.uy - 1; y <= u.uy + 1; y++) {
            /* Ignore squares that aren't within the boundary. */
            if (!isok(x, y))
                continue;

            /* If you're a grid bug, ignore diagonals. */
            if (u.umonnum == PM_GRID_BUG && x != u.ux && y != u.uy)
                continue;

            /* Don't care about the square we're already on. */
            if (x == u.ux && y == u.uy)
                continue;

            /* We already checked for hostile monsters above. Interrupt
               non-aggressive moves if we're going to run into a *peaceful*
               monster. */
            if ((mtmp = m_at(level, x, y)) && mtmp->m_ap_type != M_AP_FURNITURE
                && mtmp->m_ap_type != M_AP_OBJECT &&
                (!mtmp->minvis || See_invisible) && !mtmp->mundetected) {
                if ((!aggressive_farmoving && check_interrupt(mtmp)) ||
                    (x == u.ux + turnstate.move.dx &&
                     y == u.uy + turnstate.move.dy && !travelling()))
                    goto stop;
            }

            /* Adjacent stone won't interrupt anything. */
            if (level->locations[x][y].typ == STONE)
                continue;

            /*
             * We don't need to check the space in the opposite direction of the
             * one we're thinking of entering.
             * IMPORTANT NOTE: This means corrct shouldn't count a corridor
             * directly behind us.  If you're moving along a corridor with no
             * branches, corrct will be 1.
             */
            if (x == u.ux - turnstate.move.dx && y == u.uy - turnstate.move.dy)
                continue;

            /*  More boring cases that don't interrupt anything. */
            if (IS_ROCK(level->locations[x][y].typ) ||
                (level->locations[x][y].typ == ROOM) ||
                IS_AIR(level->locations[x][y].typ))
                continue;

            /* Closed doors, or things we think are closed doors, can interrupt
               travel if orthogonally adjacent (but not diagonally). For some
               reason, we might want to count the door as a corridor, or
               possibly not.  I mean, obviously. */
            else if (closed_door(level, x, y) ||
                     (mtmp && mtmp->m_ap_type == M_AP_FURNITURE &&
                      (mtmp->mappearance == S_hcdoor ||
                       mtmp->mappearance == S_vcdoor))) {
                if (travelling() || (x != u.ux && y != u.uy))
                    continue;
                if (!aggressive_farmoving)
                    goto stop;
                /* We're orthogonal. If we're in a corridor, count the door as a
                   corridor. */
                goto bcorr;
            } else if (level->locations[x][y].typ == CORR) {
                /* Count the square we're looking at now as a corridor. */
            bcorr:
                if (level->locations[u.ux][u.uy].typ != ROOM) {
                    /* If we're okay with changing directions at a branch,
                       we need to figure out whether we're at one and if so
                       what kind.

                       To that end, we check all the corridor spaces around us,
                       and see which one comes closest to the one we came into
                       this function intending to walk into. */
                    if (flags.corridorbranch) {
                        /* i = Euclidean distance between intended space and
                           current candidate.  Use Euclidean distance so that
                           directly adjacent spaces get priority over diagonally
                           adjacent spaces. If candidate isn't adjacent at all
                           to desired, just skip it. */
                        i = dist2(x, y, u.ux + turnstate.move.dx,
                                  u.uy + turnstate.move.dy);
                        if (i > 2)
                            continue;
                        if (corrct == 1 && dist2(x, y, x0, y0) != 1)
                            noturn = 1; // XXX study this harder
                        if (i < i0) {
                            i0 = i;
                            x0 = x;
                            y0 = y;
                            m0 = mtmp ? 1 : 0;
                        }
                    }
                    /* Regardless of whether we've got corridorbranch active,
                       add to the corridor tally. If the above corridorbranch
                       stuff didn't run, this corrct still gets used below. */
                    corrct++;
                }
                continue;
            } else if ((trap = t_at(level, x, y)) && trap->tseen) {
                /* If there's a known trap at the square we'll either mark it as
                   a corridor(?) or halt our movement if we were about to step
                   in it. */
                if (aggressive_farmoving)
                    goto bcorr; /* if you must */
                if (x == u.ux + turnstate.move.dx &&
                    y == u.uy + turnstate.move.dy)
                    goto stop;
                continue;
            } else if (is_pool(level, x, y) || is_lava(level, x, y)) {
                /* Water and lava only stop you if directly in front, and stop
                   you even if you are running. */
                if (!Levitation && !Flying && !is_clinger(youmonst.data) &&
                    x == u.ux + turnstate.move.dx &&
                    y == u.uy + turnstate.move.dy)
                    /* No Wwalking check; otherwise they'd be able to test
                       boots by trying to SHIFT-direction into a pool and
                       seeing if the game allowed it. */
                    /* TODO: It'd probably be better to check for *known* water
                       walking boots. */
                    goto stop;
                continue;
            } else {    /* e.g. objects or trap or stairs */
                if (aggressive_farmoving)
                    goto bcorr;
                if (travelling())
                    continue;
                if (mtmp)
                    continue;   /* d */
                if (((x == u.ux - turnstate.move.dx) &&
                     (y != u.uy + turnstate.move.dy)) ||
                    ((y == u.uy - turnstate.move.dy) &&
                     (x != u.ux + turnstate.move.dx)))
                    continue;
            }
        }       /* end for loops */

    if (corrct > 1 && !flags.corridorbranch)
        /* Whoops, we found a branch; we hates those, precious. Better stop.
           This behavior is going to be incredibly annoying, and it'd probably
           be better to fix it so that directed (not directional) fartravel
           ignores this. */
        goto stop;

#if 0
    if (corrct > 2 && last_command_was("run")) {
        /* We're in a true branch, which is a pretty good place to set a new
           stop space; otherwise we might run into a case where a corridor
           leads diagonally into a loop. */
        turnstate.stop_x = u.ux;
        turnstate.stop_y = u.uy;
    }
#endif
    /* Check whether it's time to turn. */
    if (flags.corridorbranch && !noturn && !m0 && i0 &&
        (corrct == 1 || (corrct == 2 && i0 == 1))) {
        turnstate.move.dx = x0 - u.ux;
        turnstate.move.dy = y0 - u.uy;
    }
    return;

stop:
    action_completed();
    return;
}

/* Check whether the monster should be considered a threat and interrupt
   the current action. */
/* Also see the similar check in dochugw() in monmove.c */
static boolean
check_interrupt(struct monst *mtmp)
{
    /* TODO: Should this stop for long worm tails? */
    return (mtmp->m_ap_type != M_AP_FURNITURE && mtmp->m_ap_type != M_AP_OBJECT
            && (!is_hider(mtmp->data) || !mtmp->mundetected) &&
            (!mtmp->mpeaceful || Hallucination) && !noattacks(mtmp->data) &&
            !mtmp->msleeping && mtmp->data->mmove && mtmp->mcanmove &&
            !onscary(u.ux, u.uy, mtmp) && canspotmon(mtmp));
}


/* something like lookaround, but we are not running */
/* react only to monsters that might hit us */
int
monster_nearby(void)
{
    int x, y;
    struct monst *mtmp;

    for (x = u.ux - 1; x <= u.ux + 1; x++)
        for (y = u.uy - 1; y <= u.uy + 1; y++) {
            if (!isok(x, y))
                continue;
            if (x == u.ux && y == u.uy)
                continue;
            if ((mtmp = m_at(level, x, y)) && check_interrupt(mtmp))
                return 1;
        }
    return 0;
}


static void
maybe_wail(void)
{
    static const short powers[] = { TELEPORT, SEE_INVIS, POISON_RES, COLD_RES,
        SHOCK_RES, FIRE_RES, SLEEP_RES, DISINT_RES,
        TELEPORT_CONTROL, STEALTH, FAST, INVIS
    };

    if (moves <= wailmsg + 50)
        return;

    wailmsg = moves;
    if (Role_if(PM_WIZARD) || Race_if(PM_ELF) || Role_if(PM_VALKYRIE)) {
        const char *who;
        int i, powercnt;

        who = (Role_if(PM_WIZARD) ||
               Role_if(PM_VALKYRIE)) ? urole.name.m : "Elf";
        if (u.uhp == 1) {
            pline("%s is about to die.", who);
        } else {
            for (i = 0, powercnt = 0; i < SIZE(powers); ++i)
                if (u.uintrinsic[powers[i]])
                    ++powercnt;

            pline(powercnt >=
                  4 ? "%s, all your powers will be lost..." :
                  "%s, your life force is running out.", who);
        }
    } else {
        You_hear(u.uhp ==
                 1 ? "the wailing of the Banshee..." :
                 "the howling of the CwnAnnwn...");
    }
}

void
losehp(int n, const char *killer)
{
    if (Upolyd) {
        u.mh -= n;
        if (u.mhmax < u.mh)
            u.mhmax = u.mh;
        if (u.mh < 1)
            rehumanize(DIED, killer);
        else if (n > 0 && u.mh * 10 < u.mhmax && Unchanging)
            maybe_wail();
        return;
    }

    u.uhp -= n;
    if (u.uhp > u.uhpmax)
        u.uhpmax = u.uhp;       /* perhaps n was negative */
    else
        action_interrupted(); /* taking damage stops command repeat */
    if (u.uhp < 1) {
        pline("You die...");
        done(DIED, killer);
    } else if (n > 0 && u.uhp * 10 < u.uhpmax) {
        maybe_wail();
    }
}

int
weight_cap(void)
{
    long carrcap;

    carrcap = 25 * (ACURRSTR + ACURR(A_CON)) + 50;
    if (Upolyd) {
        /* consistent with can_carry() in mon.c */
        if (youmonst.data->mlet == S_NYMPH)
            carrcap = MAX_CARR_CAP;
        else if (!youmonst.data->cwt)
            carrcap = (carrcap * (long)youmonst.data->msize) / MZ_HUMAN;
        else if (!strongmonst(youmonst.data)
                 || (strongmonst(youmonst.data) &&
                     (youmonst.data->cwt > WT_HUMAN)))
            carrcap = (carrcap * (long)youmonst.data->cwt / WT_HUMAN);
    }

    if (Levitation || Is_airlevel(&u.uz)        /* pugh@cornell */
        ||(u.usteed && strongmonst(u.usteed->data)))
        carrcap = MAX_CARR_CAP;
    else {
        if (carrcap > MAX_CARR_CAP)
            carrcap = MAX_CARR_CAP;
        if (!Flying) {
            if (LWounded_legs)
                carrcap -= 100;
            if (RWounded_legs)
                carrcap -= 100;
        }
        if (carrcap < 0)
            carrcap = 0;
    }
    return (int)carrcap;
}

static int wc;  /* current weight_cap(); valid after call to inv_weight() */

/* returns how far beyond the normal capacity the player is currently. */
/* inv_weight() is negative if the player is below normal capacity. */
int
inv_weight(void)
{
    struct obj *otmp = invent;
    int wt = 0;

    while (otmp) {
        if (otmp->oclass == COIN_CLASS)
            wt += (int)(((long)otmp->quan + 50L) / 100L);
        else if (otmp->otyp != BOULDER || !throws_rocks(youmonst.data))
            wt += otmp->owt;
        otmp = otmp->nobj;
    }
    wc = weight_cap();
    return wt - wc;
}

/*
 * Returns 0 if below normal capacity, or the number of "capacity units"
 * over the normal capacity the player is loaded.  Max is 5.
 */
int
calc_capacity(int xtra_wt)
{
    int cap, wt = inv_weight() + xtra_wt;

    if (wt <= 0)
        return UNENCUMBERED;
    if (wc <= 1)
        return OVERLOADED;
    cap = (wt * 2 / wc) + 1;
    return max(0,min(cap, OVERLOADED));
}

int
near_capacity(void)
{
    return calc_capacity(0);
}

int
max_capacity(void)
{
    int wt = inv_weight();

    return wt - (2 * wc);
}

boolean
check_capacity(const char *str)
{
    if (near_capacity() >= EXT_ENCUMBER) {
        if (str)
            pline("%s", str);
        else
            pline("You can't do that while carrying so much stuff.");
        return 1;
    }
    return 0;
}


int
inv_cnt(boolean letter_only)
{
    struct obj *otmp = invent;
    int ct = 0;

    while (otmp) {
        if (letter(otmp->invlet) || !letter_only)
            ct++;
        otmp = otmp->nobj;
    }
    return ct;
}


/* Counts the money in an object chain. */
/* Intended use is for your or some monsters inventory, */
/* now that u.gold/m.gold is gone.*/
/* Counting money in a container might be possible too. */
long
money_cnt(struct obj *otmp)
{
    while (otmp) {
        /* Must change when silver & copper is implemented: */
        if (otmp->oclass == COIN_CLASS)
            return otmp->quan;
        otmp = otmp->nobj;
    }
    return 0;
}

/* Picks a suitable interaction status for farmove and autoexplore. It will
   displace monsters if the player's default status displaces monsters, and
   interact with items if the player's default status interacts with items. It
   never attacks. */
enum u_interaction_mode
exploration_interaction_status(void)
{
    enum u_interaction_mode uim = flags.interaction_mode;

    if (uim <= uim_displace)
        return flags.interaction_mode;

    if (uim == uim_pacifist || uim == uim_standard)
        return uim_displace;

    if (ITEM_INTERACTIVE(uim))
        return uim_onlyitems;

    return uim_nointeraction;
}

int
dofight(const struct nh_cmd_arg *arg)
{
    return domove(arg, uim_forcefight, occ_none);
}


/* This is used by commands that need occupation-like behaviour when given a
   numeric prefix. For instance, if the user sends "20s", they should get a "You
   stop searching." if interrupted, the save file should be a little less
   aggressive about recording diffs until it's finished, and so on. */
void
limited_turns(const struct nh_cmd_arg *arg, enum occupation occ)
{
    const char *verb =
        occ == occ_move ? "running" :
        occ == occ_search ? "searching" : "waiting";

    if (turnstate.continue_message) /* start of the occupation */
        u.uoccupation_progress[tos_limit] = 0;

    if (arg->argtype & CMD_ARG_LIMIT) {
        u.uoccupation_progress[tos_limit]++;
        if (u.uoccupation_progress[tos_limit] >= arg->limit) {
            u.uoccupation_progress[tos_limit] = 0;
            pline("You finish %s.", verb);
            action_completed();
        } else {
            action_incomplete(verb, occ);
        }
    }
}

/* TODO: pre_move_tasks() currently uses a hardcoded list of movement commands.
   For the time being, keep it in sync with this list. Hopefully there will
   eventually be a better solution. */

int
domovecmd(const struct nh_cmd_arg *arg)
{
    limited_turns(arg, occ_move);
    return domove(arg, turnstate.continue_message &&
                  !(arg->argtype & CMD_ARG_LIMIT) ?
                  flags.interaction_mode : exploration_interaction_status(),
                  occ_none);
}

int
domovecmd_nopickup(const struct nh_cmd_arg *arg)
{
    limited_turns(arg, occ_move);
    return domove(arg, uim_nointeraction, occ_none);
}


static int
do_rush(const struct nh_cmd_arg *arg, enum u_interaction_mode uim)
{
    action_incomplete("running", occ_move);
    return domove(arg, uim, occ_move);
}

int
dorun(const struct nh_cmd_arg *arg)
{
    return do_rush(arg, exploration_interaction_status());
}

int
dogo(const struct nh_cmd_arg *arg)
{
    return do_rush(arg, uim_nointeraction);
}

/*hack.c*/

