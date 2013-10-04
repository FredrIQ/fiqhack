/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static void maybe_wail(void);
static int moverock(schar dx, schar dy);
static int still_chewing(xchar, xchar);
static void dosinkfall(void);
static boolean findtravelpath(boolean(*)(int, int), schar *, schar *);
static struct monst *monstinroom(const struct permonst *, int);
static boolean check_interrupt(struct monst *mtmp);

static void move_update(boolean);

#define IS_SHOP(x) (level->rooms[x].rtype >= SHOPBASE)


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
                Norep("%s", msg);
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
        nomul(0, NULL);
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
                              (ttmp->ttyp == TRAPDOOR) ? nul : " into",
                              otense(otmp, "plug"),
                              (ttmp->ttyp == TRAPDOOR) ? "trap door" : "hole",
                              surface(rx, ry));
                    deltrap(level, ttmp);
                    delobj(otmp);
                    bury_objs(level, rx, ry);
                    if (cansee(rx, ry))
                        newsym(rx, ry);
                    continue;
                case LEVEL_TELEP:
                case TELEP_TRAP:
                    if (u.usteed)
                        pline("%s pushes %s and suddenly it disappears!",
                              upstart(y_monnam(u.usteed)), the(xname(otmp)));
                    else
                        pline("You push %s and suddenly it disappears!",
                              the(xname(otmp)));
                    if (ttmp->ttyp == TELEP_TRAP)
                        rloco(otmp);
                    else {
                        int newlev = random_teleport_level();
                        d_level dest;

                        if (newlev == depth(&u.uz) || In_endgame(&u.uz))
                            continue;
                        obj_extract_self(otmp);

                        get_level(&dest, newlev);
                        deliver_object(otmp, dest.dnum, dest.dlevel,
                                       MIGR_RANDOM);
                    }
                    seetrap(ttmp);
                    continue;
                }
            if (closed_door(level, rx, ry))
                goto nopushmsg;
            if (boulder_hits_pool(otmp, rx, ry, TRUE))
                continue;
            /* 
             * Re-link at top of level->objlist chain so that pile order is preserved
             * when level is restored.
             */
            if (otmp != level->objlist) {
                remove_object(otmp);
                place_object(otmp, level, otmp->ox, otmp->oy);
            }

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
                    pline("%s moves %s.", upstart(y_monnam(u.usteed)),
                          the(xname(otmp)));
                lastmovetime = moves;
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
                      upstart(y_monnam(u.usteed)), the(xname(otmp)));
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

    if (digging.down)   /* not continuing previous dig (w/ pick-axe) */
        memset(&digging, 0, sizeof digging);

    if (!boulder && IS_ROCK(loc->typ) && !may_dig(level, x, y)) {
        pline("You hurt your teeth on the %s.",
              IS_TREE(loc->typ) ? "tree" : "hard stone");
        nomul(0, NULL);
        return 1;
    } else if (digging.pos.x != x || digging.pos.y != y ||
               !on_level(&digging.level, &u.uz)) {
        digging.down = FALSE;
        digging.chew = TRUE;
        digging.warned = FALSE;
        digging.pos.x = x;
        digging.pos.y = y;
        assign_level(&digging.level, &u.uz);
        /* solid rock takes more work & time to dig through */
        digging.effort = (IS_ROCK(loc->typ) &&
                          !IS_TREE(loc->typ) ? 30 : 60) + u.udaminc;
        pline("You start chewing %s %s.",
              (boulder ||
               IS_TREE(loc->typ)) ? "on a" : "a hole in the",
              boulder ? "boulder" : IS_TREE(loc->typ) ?
              "tree" : IS_ROCK(loc->typ) ? "rock" : "door");
        watch_dig(NULL, x, y, FALSE);
        return 1;
    } else if ((digging.effort += (30 + u.udaminc)) <= 100) {
        if (flags.verbose)
            pline("You %s chewing on the %s.",
                  digging.chew ? "continue" : "begin",
                  boulder ? "boulder" : IS_TREE(loc->typ) ?
                  "tree" : IS_ROCK(loc->typ) ? "rock" : "door");
        digging.chew = TRUE;
        watch_dig(NULL, x, y, FALSE);
        return 1;
    }

    /* Okay, you've chewed through something */
    u.uconduct.food++;
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
            memset(&digging, 0, sizeof digging);
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
        pline(digtxt);  /* after newsym */
    if (dmgtxt)
        pay_for_damage(dmgtxt, FALSE);
    memset(&digging, 0, sizeof digging);
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

    if (is_floater(youmonst.data) || (HLevitation & FROMOUTSIDE)) {
        pline("You wobble unsteadily for a moment.");
    } else {
        long save_ELev = ELevitation, save_HLev = HLevitation;

        /* fake removal of levitation in advance so that final disclosure will
           be right in case this turns out to be fatal; fortunately the fact
           that rings and boots are really still worn has no effect on bones
           data */
        ELevitation = HLevitation = 0L;
        pline("You crash to the floor!");
        losehp(rn1(8, 25 - (int)ACURR(A_CON)), fell_on_sink, NO_KILLER_PREFIX);
        exercise(A_DEX, FALSE);
        selftouch("Falling, you", "crashing to the floor while wielding");
        for (obj = level->objects[u.ux][u.uy]; obj; obj = obj->nexthere)
            if (obj->oclass == WEAPON_CLASS || is_weptool(obj)) {
                pline("You fell on %s.", doname(obj));
                losehp(rnd(3), fell_on_sink, NO_KILLER_PREFIX);
                exercise(A_CON, FALSE);
            }
        ELevitation = save_ELev;
        HLevitation = save_HLev;
    }

    ELevitation &= ~W_ARTI;
    HLevitation &= ~(I_SPECIAL | TIMEOUT);
    HLevitation++;
    if (uleft && uleft->otyp == RIN_LEVITATION) {
        obj = uleft;
        Ring_off(obj);
        off_msg(obj);
    }
    if (uright && uright->otyp == RIN_LEVITATION) {
        obj = uright;
        Ring_off(obj);
        off_msg(obj);
    }
    if (uarmf && uarmf->otyp == LEVITATION_BOOTS) {
        obj = uarmf;
        Boots_off();
        off_msg(obj);
    }
    HLevitation--;
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
    if (iflags.autoexplore) {
        char tmp[BUFSZ];

        strcpy(tmp, text);
        pline("%s blocks your way.", upstart(tmp));
    }
}

/* return TRUE if (dx,dy) is an OK place to move
 * mode is one of DO_MOVE, TEST_MOVE, TEST_TRAV or TEST_TRAP
 */
boolean
test_move(int ux, int uy, int dx, int dy, int dz, int mode)
{
    int x = ux + dx;
    int y = uy + dy;
    struct rm *tmpr = &level->locations[x][y];
    struct rm *ust;

    /* 
     *  Check for physical obstacles.  First, the place we are going.
     */
    if (IS_ROCK(tmpr->typ) || tmpr->typ == IRONBARS) {
        if (Blind && mode == DO_MOVE)
            feel_location(x, y);
        if (Passes_walls && may_passwall(level, x, y)) {
            ;   /* do nothing */
        } else if (tmpr->typ == IRONBARS) {
            if (!(Passes_walls || passes_bars(youmonst.data)))
                return FALSE;
        } else if (tunnels(youmonst.data) && !needspick(youmonst.data)) {
            /* Eat the rock. */
            if (mode == DO_MOVE && still_chewing(x, y))
                return FALSE;
        } else if (flags.autodig && !flags.run && !flags.nopick && uwep &&
                   is_pick(uwep)) {
            /* MRKR: Automatic digging when wielding the appropriate tool */
            if (mode == DO_MOVE)
                use_pick_axe2(uwep, dx, dy, dz);
            return FALSE;
        } else {
            if (mode == DO_MOVE) {
                if (Is_stronghold(&u.uz) && is_db_wall(x, y))
                    pline("The drawbridge is up!");
                if (Passes_walls && !may_passwall(level, x, y) &&
                    In_sokoban(&u.uz))
                    pline("The Sokoban walls resist your ability.");
            }
            return FALSE;
        }
    } else if (IS_DOOR(tmpr->typ)) {
        if (closed_door(level, x, y)) {
            if (Blind && mode == DO_MOVE)
                feel_location(x, y);
            if (Passes_walls) ; /* do nothing */
            else if (can_ooze(&youmonst)) {
                if (mode == DO_MOVE)
                    pline("You ooze under the door.");
            } else if (tunnels(youmonst.data) && !needspick(youmonst.data)) {
                /* Eat the door. */
                if (mode == DO_MOVE && still_chewing(x, y))
                    return FALSE;
            } else {
                if (mode == DO_MOVE) {
                    if (amorphous(youmonst.data))
                        pline
                            ("You try to ooze under the door, but can't squeeze your possessions through.");
                    else if (x == ux || y == uy) {
                        if (Blind || Stunned || ACURR(A_DEX) < 10 || Fumbling) {
                            if (u.usteed) {
                                pline
                                    ("You can't lead %s through that closed door.",
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
            if (dx && dy && !Passes_walls && ((tmpr->doormask & ~D_BROKEN)
                                              || Is_rogue_level(&u.uz)
                                              || block_door(x, y))) {
                /* Diagonal moves into a door are not allowed. */
                if (Blind && mode == DO_MOVE)
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
    /* Pick travel path that does not require crossing a trap. Avoid water and
       lava using the usual running rules. (but not u.ux/u.uy because
       findtravelpath walks toward u.ux/u.uy) */
    if (flags.run == 8 && (x != u.ux || y != u.uy)) {
        struct trap *t = t_at(level, x, y);

        if ((t && t->tseen) ||
            (!Levitation && !Flying && !is_clinger(youmonst.data) &&
             (is_pool(level, x, y) || is_lava(level, x, y)) &&
             level->locations[x][y].seenv)) {
            if (mode == DO_MOVE) {
                if (t && t->tseen)
                    autoexplore_msg("a trap", mode);
                else if (is_pool(level, x, y))
                    autoexplore_msg("a body of water", mode);
                else if (is_lava(level, x, y))
                    autoexplore_msg("a pool of lava", mode);
                if (flags.travel)
                    return FALSE;
            }
            return mode == TEST_TRAP || mode == DO_MOVE;
        }
    }

    if (mode == TEST_TRAP)
        return FALSE;   /* not a move through a trap */

    ust = &level->locations[ux][uy];

    /* Now see if other things block our way . . */
    if (dx && dy && !Passes_walls &&
        (IS_DOOR(ust->typ) && ((ust->doormask & ~D_BROKEN)
                               || Is_rogue_level(&u.uz)
                               || block_entry(x, y))
        )) {
        /* Can't move at a diagonal out of a doorway with door. */
        if (mode == DO_MOVE)
            autoexplore_msg("the doorway", mode);
        return FALSE;
    }

    if (sobj_at(BOULDER, level, x, y) && (In_sokoban(&u.uz) || !Passes_walls)) {
        if (!(Blind || Hallucination) && (flags.run >= 2) && mode != TEST_TRAV) {
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

            /* don't pick two boulders in a row, unless there's a way thru */
            if (sobj_at(BOULDER, level, ux, uy) && !In_sokoban(&u.uz)) {
                if (!Passes_walls &&
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
    struct trap *ttmp = t_at(level, x, y);

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
    if (level->locations[x][y].mem_obj == what_obj(BOULDER) + 1)
        return FALSE;
    if (level->locations[x][y].mem_obj && inside_shop(level, x, y))
        return FALSE;
    if (level->locations[x][y].mem_obj)
        return TRUE;
    for (i = -1; i <= 1; i++)
        for (j = -1; j <= 1; j++) {
            if (isok(x + i, y + j) &&
                (level->locations[x + i][y + j].mem_bg == S_stone ||
                 level->locations[x + i][y + j].mem_bg == S_unexplored)) {
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

/*
 * Find a path from the destination (u.tx,u.ty) back to (u.ux,u.uy).
 * A shortest path is returned.  If guess is non-NULL, instead travel
 * as near to the target as you can, using guess as a function that
 * specifies what is considered to be a valid target.
 * Returns TRUE if a path was found.
 */
static boolean
findtravelpath(boolean(*guess) (int, int), schar * dx, schar * dy)
{
    /* if travel to adjacent, reachable location, use normal movement rules */
    if (!guess && iflags.travel1 && distmin(u.ux, u.uy, u.tx, u.ty) == 1) {
        flags.run = 0;
        if (test_move(u.ux, u.uy, u.tx - u.ux, u.ty - u.uy, 0, TEST_MOVE)) {
            *dx = u.tx - u.ux;
            *dy = u.ty - u.uy;
            nomul(0, NULL);
            iflags.travelcc.x = iflags.travelcc.y = -1;
            return TRUE;
        }
        flags.run = 8;
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

                    if (!isok(nx, ny))
                        continue;
                    if ((!Passes_walls && !can_ooze(&youmonst) &&
                         closed_door(level, x, y)) ||
                        sobj_at(BOULDER, level, x, y) ||
                        test_move(x, y, nx - x, ny - y, 0, TEST_TRAP)) {
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
                    if (test_move(x, y, nx - x, ny - y, 0, TEST_TRAP) ||
                        test_move(x, y, nx - x, ny - y, 0, TEST_TRAV)) {
                        if ((level->locations[nx][ny].seenv ||
                             (!Blind && couldsee(nx, ny)))) {
                            if (nx == ux && ny == uy) {
                                if (!guess) {
                                    *dx = x - ux;
                                    *dy = y - uy;
                                    if (x == u.tx && y == u.ty) {
                                        nomul(0, NULL);
                                        /* reset run so domove run checks work */
                                        flags.run = 8;
                                        iflags.travelcc.x = iflags.travelcc.y =
                                            -1;
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

            dist = distmin(ux, uy, tx, ty);
            d2 = dist2(ux, uy, tx, ty);
            if (guess == unexplored) {
                dist = COLNO * ROWNO;
                d2 = COLNO * COLNO * ROWNO * ROWNO;
            }
            for (tx = 1; tx < COLNO; ++tx)
                for (ty = 0; ty < ROWNO; ++ty)
                    if (travel[tx][ty]) {
                        nxtdist = distmin(ux, uy, tx, ty);
                        if (guess == unexplored)
                            nxtdist = travel[tx][ty];
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

            if (px == u.ux && py == u.uy) {
                /* no guesses, just go in the general direction */
                *dx = sgn(u.tx - u.ux);
                *dy = sgn(u.ty - u.uy);
                if (*dx == 0 && *dy == 0) {
                    nomul(0, "");
                    return FALSE;
                }
                if (test_move(u.ux, u.uy, *dx, *dy, 0, TEST_MOVE))
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
    nomul(0, NULL);
    return FALSE;
}

/* A function version of couldsee, so we can take a pointer to it. */
static boolean
couldsee_func(int x, int y)
{
    return couldsee(x, y);
}



int
domove(schar dx, schar dy, schar dz)
{
    struct monst *mtmp;
    struct rm *tmpr;
    xchar x, y;
    struct trap *trap;
    int wtcap;
    boolean on_ice;
    xchar chainx, chainy, ballx, bally; /* ball&chain new positions */
    int bc_control;     /* control for ball&chain */
    boolean cause_delay = FALSE;        /* dragging ball will skip a move */
    const char *predicament;

    if (dz) {
        nomul(0, NULL);
        if (dz < 0)
            return doup();
        else
            return dodown();
    }

    u_wipe_engr(rnd(5));

    if (flags.travel) {
        if (iflags.autoexplore) {
            if (Blind) {
                pline("You can't see where you're going!");
                nomul(0, NULL);
                return 0;
            }
            if (In_sokoban(&u.uz)) {
                pline
                    ("You somehow know the layout of this place without exploring.");
                nomul(0, NULL);
                return 0;
            }
            if (Stunned || Confusion) {
                pline("Your head is spinning too badly to explore.");
                nomul(0, NULL);
                return 0;
            }
            u.tx = u.ux;
            u.ty = u.uy;
            if (!findtravelpath(unexplored, &dx, &dy)) {
                iflags.autoexplore = FALSE;
                pline
                    ("Nowhere else around here can be automatically explored.");
            }
        } else if (!findtravelpath(NULL, &dx, &dy))
            findtravelpath(couldsee_func, &dx, &dy);
        iflags.travel1 = 0;
        if (dx == 0 && dy == 0) {
            nomul(0, NULL);
            return 0;
        }
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
        nomul(0, NULL);
        return 1;
    }
    if (u.uswallow) {
        dx = dy = 0;
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

        x = u.ux + dx;
        y = u.uy + dy;
        if (Stunned || (Confusion && !rn2(5))) {
            int tries = 0;

            do {
                if (tries++ > 50) {
                    nomul(0, NULL);
                    return 1;
                }
                confdir(&dx, &dy);
                x = u.ux + dx;
                y = u.uy + dy;
            } while (!isok(x, y) || bad_rock(youmonst.data, x, y));
        }
        /* turbulence might alter your actual destination */
        if (u.uinwater) {
            water_friction(&dx, &dy);
            if (!dx && !dy) {
                nomul(0, NULL);
                return 1;
            }
            x = u.ux + dx;
            y = u.uy + dy;
        }
        if (!isok(x, y)) {
            nomul(0, NULL);
            return 0;
        }
        if (((trap = t_at(level, x, y)) && trap->tseen) ||
            (Blind && !Levitation && !Flying && !is_clinger(youmonst.data) &&
             (is_pool(level, x, y) || is_lava(level, x, y)) &&
             level->locations[x][y].seenv)) {
            if (flags.run >= 2) {
                nomul(0, NULL);
                return 0;
            } else
                nomul(0, NULL);
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
                   holder is tame and there is no conflict, guaranteed escape. */
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
                    nomul(0, NULL);
                    return 1;
                }
            }
        }

        mtmp = m_at(level, x, y);
        if (mtmp && !is_safepet(mtmp)) {
            /* Don't attack if you're running, and can see it */
            /* We should never get here if forcefight */
            if (flags.run &&
                ((!Blind && mon_visible(mtmp) &&
                  ((mtmp->m_ap_type != M_AP_FURNITURE &&
                    mtmp->m_ap_type != M_AP_OBJECT) ||
                   Protection_from_shape_changers)) || sensemon(mtmp))) {
                nomul(0, NULL);
                autoexplore_msg(Monnam(mtmp), DO_MOVE);
                return 0;
            }
        }
    }

    u.ux0 = u.ux;
    u.uy0 = u.uy;
    bhitpos.x = x;
    bhitpos.y = y;
    tmpr = &level->locations[x][y];

    /* attack monster */
    if (mtmp) {
        if (!is_safepet(mtmp) || flags.forcefight)
            nomul(0, NULL);
        /* only attack if we know it's there */
        /* or if we used the 'F' command to fight blindly */
        /* or if it hides_under, in which case we call attack() to print the
           Wait! message. This is different from ceiling hiders, who aren't
           handled in attack(). */

        /* If they used a 'm' command, trying to move onto a monster prints the 
           below message and wastes a turn.  The exception is if the monster is 
           unseen and the player doesn't remember an invisible monster--then,
           we fall through to attack() and attack_check(), which still wastes a 
           turn, but prints a different message and makes the player remember
           the monster.  */
        if (flags.nopick && !flags.travel &&
            (canspotmon(mtmp) || level->locations[x][y].mem_invis)) {
            if (mtmp->m_ap_type && !Protection_from_shape_changers &&
                !sensemon(mtmp))
                stumble_onto_mimic(mtmp, dx, dy);
            else if (mtmp->mpeaceful && !Hallucination)
                pline("Pardon me, %s.", m_monnam(mtmp));
            else
                pline("You move right into %s.", mon_nam(mtmp));
            return 1;
        }
        if (flags.forcefight || !mtmp->mundetected || sensemon(mtmp) ||
            ((hides_under(mtmp->data) || mtmp->data->mlet == S_EEL) &&
             !is_safepet(mtmp))) {
            gethungry();
            if (wtcap >= HVY_ENCUMBER && moves % 3) {
                if (Upolyd && u.mh > 1) {
                    u.mh--;
                } else if (!Upolyd && u.uhp > 1) {
                    u.uhp--;
                } else {
                    pline("You pass out from exertion!");
                    exercise(A_CON, FALSE);
                    fall_asleep(-10, FALSE);
                }
            }
            if (multi < 0)
                return 1;       /* we just fainted */

            /* try to attack; note that it might evade */
            /* also, we don't attack tame when _safepet_ */
            if (attack(mtmp, dx, dy))
                return 1;
        }
    }

    /* specifying 'F' with no monster wastes a turn */
    if (flags.forcefight ||
        /* remembered an 'I' && didn't use a move command */
        (level->locations[x][y].mem_invis && !flags.nopick)) {
        boolean expl = (Upolyd && attacktype(youmonst.data, AT_EXPL));
        char buf[BUFSZ];

        sprintf(buf, "a vacant spot on the %s", surface(x, y));
        pline("You %s %s.", expl ? "explode at" : "attack",
              !Underwater ? "thin air" : is_pool(level, x,
                                                 y) ? "empty water" : buf);
        unmap_object(x, y);     /* known empty -- remove 'I' if present */
        newsym(x, y);
        nomul(0, NULL);
        if (expl) {
            u.mh = -1;  /* dead in the current form */
            rehumanize();
        }
        return 1;
    }
    if (level->locations[x][y].mem_invis) {
        unmap_object(x, y);
        newsym(x, y);
    }
    /* not attacking an animal, so we try to move */
    if (u.usteed && !u.usteed->mcanmove && (dx || dy)) {
        pline("%s won't move!", upstart(y_monnam(u.usteed)));
        nomul(0, NULL);
        return 1;
    } else if (!youmonst.data->mmove) {
        pline("You are rooted %s.", Levitation || Is_airlevel(&u.uz) ||
              Is_waterlevel(&u.uz) ? "in place" : "to the ground");
        nomul(0, NULL);
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
                      "struggle against the air currents and float" : u.
                      usteed ? "ride" : "crawl");
                fill_pit(level, u.ux, u.uy);
                vision_full_recalc = 1; /* vision limits change */
            } else if (flags.verbose) {
                if (u.usteed)
                    Norep("%s is still in a pit.", upstart(y_monnam(u.usteed)));
                else
                    Norep((Hallucination &&
                           !rn2(5)) ? "You've fallen, and you can't get up." :
                          "You are still in a pit.");
            }
        } else if (u.utraptype == TT_LAVA) {
            if (flags.verbose) {
                predicament = "stuck in the lava";
                if (u.usteed)
                    Norep("%s is %s.", upstart(y_monnam(u.usteed)),
                          predicament);
                else
                    Norep("You are %s.", predicament);
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
                        Norep("%s is %s.", upstart(y_monnam(u.usteed)),
                              predicament);
                    else
                        Norep("You are %s.", predicament);
                }
            } else {
                if (u.usteed)
                    pline("%s breaks out of the web.",
                          upstart(y_monnam(u.usteed)));
                else
                    pline("You disentangle yourself.");
            }
        } else if (u.utraptype == TT_INFLOOR) {
            if (--u.utrap) {
                if (flags.verbose) {
                    predicament = "stuck in the";
                    if (u.usteed)
                        Norep("%s is %s %s.", upstart(y_monnam(u.usteed)),
                              predicament, surface(u.ux, u.uy));
                    else
                        Norep("You are %s %s.", predicament,
                              surface(u.ux, u.uy));
                }
            } else {
                if (u.usteed)
                    pline("%s finally wiggles free.",
                          upstart(y_monnam(u.usteed)));
                else
                    pline("You finally wiggle free.");
            }
        } else {
            if (flags.verbose) {
                predicament = "caught in a bear trap";
                if (u.usteed)
                    Norep("%s is %s.", upstart(y_monnam(u.usteed)),
                          predicament);
                else
                    Norep("You are %s.", predicament);
            }
            if ((dx && dy) || !rn2(5))
                u.utrap--;
        }
        return 1;
    }

    /* If moving into a door, open it. */
    if (IS_DOOR(tmpr->typ) && tmpr->doormask != D_BROKEN &&
        tmpr->doormask != D_NODOOR && tmpr->doormask != D_ISOPEN) {
        if (!doopen(dx, dy, 0)) {
            flags.move = 0;
            nomul(0, NULL);
            return 0;
        }
        return 1;
    }

    if (!test_move(u.ux, u.uy, dx, dy, dz, DO_MOVE)) {
        flags.move = 0;
        nomul(0, NULL);
        return 0;
    }

    /* If no 'm' prefix, veto dangerous moves */
    if (!flags.nopick || flags.run) {
        if (!Levitation && !Flying && !is_clinger(youmonst.data) && !Stunned &&
            !Confusion && (!flags.travel || !iflags.autoexplore) &&
            (is_lava(level, x, y) || !HSwimming) &&
            (is_pool(level, x, y) || is_lava(level, x, y)) &&
            level->locations[x][y].seenv && !is_pool(level, u.ux, u.uy) &&
            !is_lava(level, u.ux, u.uy)) {
            pline(is_pool(level, x, y) ? "You never learned to swim!" :
                  "That lava looks rather dangerous...");
            pline("(Use m-direction to move there anyway.)");
            flags.move = 0;
            nomul(0, "");
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
    u.ux += dx;
    u.uy += dy;
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
    if (is_safepet(mtmp) && !(is_hider(mtmp->data) && mtmp->mundetected)) {
        /* if trapped, there's a chance the pet goes wild */
        if (mtmp->mtrapped) {
            if (!rn2(mtmp->mtame)) {
                mtmp->mtame = mtmp->mpeaceful = mtmp->msleeping = 0;
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
            pline("You stop.  %s won't fit through.", upstart(y_monnam(mtmp)));
        } else {
            char pnambuf[BUFSZ];

            /* save its current description in case of polymorph */
            strcpy(pnambuf, y_monnam(mtmp));
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
                u.uconduct.killer++;
                break;
            default:
                pline("that's strange, unknown mintrap result!");
                break;
            }
        }
    }

    reset_occupations();
    if (flags.run) {
        if (flags.run < 8) {
            if (IS_DOOR(tmpr->typ) || IS_ROCK(tmpr->typ) ||
                IS_FURNITURE(tmpr->typ))
                nomul(0, NULL);
        } else if (flags.travel && iflags.autoexplore) {
            int wallcount = 0;

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
                nomul(0, NULL);
        }
    }

    if (hides_under(youmonst.data))
        u.uundetected = OBJ_AT(u.ux, u.uy);
    else if (youmonst.data->mlet == S_EEL)
        u.uundetected = is_pool(level, u.ux, u.uy) && !Is_waterlevel(&u.uz);
    else if (dx || dy)
        u.uundetected = 0;

    /* 
     * Mimics (or whatever) become noticeable if they move and are
     * imitating something that doesn't move.  We could extend this
     * to non-moving monsters...
     */
    if ((dx || dy) &&
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
        if (!Blind)
            level->locations[u.ux][u.uy].mem_stepped = 1;
    }

    if (Punished)       /* put back ball and chain */
        move_bc(0, bc_control, ballx, bally, chainx, chainy);

    spoteffects(TRUE);

    /* delay next move because of ball dragging */
    /* must come after we finished picking up, in spoteffects() */
    if (cause_delay) {
        nomul(-2, "dragging an iron ball");
        nomovemsg = "";
    }

    if (flags.run && iflags.runmode != RUN_TPORT) {
        /* display every step or every 7th step depending upon mode */
        if (iflags.runmode != RUN_LEAP || !(moves % 7L)) {
            iflags.botl = 1;
            flush_screen();
            win_delay_output();
            if (iflags.runmode == RUN_CRAWL) {
                win_delay_output();
                win_delay_output();
                win_delay_output();
                win_delay_output();
            }
        }
    }
    return 1;
}

void
invocation_message(void)
{
    /* a special clue-msg when on the Invocation position */
    if (invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy)) {
        char buf[BUFSZ];
        struct obj *otmp = carrying(CANDELABRUM_OF_INVOCATION);

        nomul(0, NULL); /* stop running or travelling */
        if (u.usteed)
            sprintf(buf, "beneath %s", y_monnam(u.usteed));
        else if (Levitation || Flying)
            strcpy(buf, "beneath you");
        else
            sprintf(buf, "under your %s", makeplural(body_part(FOOT)));

        pline("You feel a strange vibration %s.", buf);
        if (otmp && otmp->spe == 7 && otmp->lamplit)
            pline("%s %s!", The(xname(otmp)),
                  Blind ? "throbs palpably" : "glows with a strange light");
    }
}


void
spoteffects(boolean pick)
{
    struct monst *mtmp;

    /* clear items remebered by the ui at this location */
    win_list_items(NULL, 0, FALSE);

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
            vision_full_recalc = 1;
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
        if (pick)
            pickup(1);
        if (trap && !pit)
            dotrap(trap, 0);    /* fall into arrow trap, etc. */
    }
    if ((mtmp = m_at(level, u.ux, u.uy)) && !u.uswallow) {
        mtmp->mundetected = mtmp->msleeping = 0;
        switch (mtmp->data->mlet) {
        case S_PIERCER:
            pline("%s suddenly drops from the %s!", Amonnam(mtmp),
                  ceiling(u.ux, u.uy));
            if (mtmp->mtame)    /* jumps to greet you, not attack */
                ;
            else if (uarmh && is_metallic(uarmh))
                pline("Its blow glances off your %s.", helmet_name(uarmh));
            else if (u.uac + 3 <= rnd(20))
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
                mtmp->mpeaceful = 0;
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
            ((typefound = lev->rooms[rno - ROOMOFFSET].rtype) == typewanted) || \
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
    if (x < 1)
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
                verbalize("%s, %s, welcome to Delphi!", Hello(NULL), plname);
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
            if (!search_special(level, rt)) {
                /* No more room of that type */
                switch (rt) {
                case COURT:
                    level->flags.has_court = 0;
                    break;
                case SWAMP:
                    level->flags.has_swamp = 0;
                    break;
                case MORGUE:
                    level->flags.has_morgue = 0;
                    break;
                case ZOO:
                    level->flags.has_zoo = 0;
                    break;
                case BARRACKS:
                    level->flags.has_barracks = 0;
                    break;
                case TEMPLE:
                    level->flags.has_temple = 0;
                    break;
                case BEEHIVE:
                    level->flags.has_beehive = 0;
                    break;
                }
            }
            if (rt == COURT || rt == SWAMP || rt == MORGUE || rt == ZOO)
                for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                    if (!DEADMONSTER(mtmp) && !Stealth && !rn2(3))
                        mtmp->msleeping = 0;
        }
    }

    return;
}


int
dopickup(void)
{
    int count;
    struct trap *traphere = t_at(level, u.ux, u.uy);

    /* awful kludge to work around parse()'s pre-decrement */
    count = multi;
    multi = 0;  /* always reset */
    /* uswallow case added by GAN 01/29/87 */
    if (u.uswallow) {
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

    return pickup(-count);
}


/* stop running if we see something interesting */
/* turn around a corner if that is the only way we can proceed */
/* do not turn left or right twice */
void
lookaround(void)
{
    int x, y, i, x0 = 0, y0 = 0, m0 = 1, i0 = 9;
    int corrct = 0, noturn = 0;
    struct monst *mtmp;
    struct trap *trap;

    /* Grid bugs stop if trying to move diagonal, even if blind.  Maybe */
    /* they polymorphed while in the middle of a long move. */
    if (u.umonnum == PM_GRID_BUG && u.dx && u.dy) {
        nomul(0, NULL);
        return;
    }

    /* If travel_interrupt is set, then stop if there is a hostile nearby. */
    if (flags.run != 1 && iflags.travel_interrupt) {
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
            if (distmin(u.ux, u.uy, mtmp->mx, mtmp->my) <= (BOLT_LIM + 1) &&
                couldsee(mtmp->mx, mtmp->my) && check_interrupt(mtmp)) {
                nomul(0, NULL);
                return;
            }
        }
    }

    if (Blind || flags.run == 0)
        return;
    for (x = u.ux - 1; x <= u.ux + 1; x++)
        for (y = u.uy - 1; y <= u.uy + 1; y++) {
            if (!isok(x, y))
                continue;

            if (u.umonnum == PM_GRID_BUG && x != u.ux && y != u.uy)
                continue;

            if (x == u.ux && y == u.uy)
                continue;

            if ((mtmp = m_at(level, x, y)) && mtmp->m_ap_type != M_AP_FURNITURE
                && mtmp->m_ap_type != M_AP_OBJECT &&
                (!mtmp->minvis || See_invisible) && !mtmp->mundetected) {
                if ((flags.run != 1 && check_interrupt(mtmp)) ||
                    (x == u.ux + u.dx && y == u.uy + u.dy && !flags.travel))
                    goto stop;
            }

            if (level->locations[x][y].typ == STONE)
                continue;
            if (x == u.ux - u.dx && y == u.uy - u.dy)
                continue;

            if (IS_ROCK(level->locations[x][y].typ) ||
                (level->locations[x][y].typ == ROOM) ||
                IS_AIR(level->locations[x][y].typ))
                continue;
            else if (closed_door(level, x, y) ||
                     (mtmp && mtmp->m_ap_type == M_AP_FURNITURE &&
                      (mtmp->mappearance == S_hcdoor ||
                       mtmp->mappearance == S_vcdoor))) {
                if (x != u.ux && y != u.uy)
                    continue;
                if (flags.run != 1)
                    goto stop;
                goto bcorr;
            } else if (level->locations[x][y].typ == CORR) {
            bcorr:
                if (level->locations[u.ux][u.uy].typ != ROOM) {
                    if (flags.run == 1 || flags.run == 3 || flags.run == 8) {
                        i = dist2(x, y, u.ux + u.dx, u.uy + u.dy);
                        if (i > 2)
                            continue;
                        if (corrct == 1 && dist2(x, y, x0, y0) != 1)
                            noturn = 1;
                        if (i < i0) {
                            i0 = i;
                            x0 = x;
                            y0 = y;
                            m0 = mtmp ? 1 : 0;
                        }
                    }
                    corrct++;
                }
                continue;
            } else if ((trap = t_at(level, x, y)) && trap->tseen) {
                if (flags.run == 1)
                    goto bcorr; /* if you must */
                if (x == u.ux + u.dx && y == u.uy + u.dy)
                    goto stop;
                continue;
            } else if (is_pool(level, x, y) || is_lava(level, x, y)) {
                /* water and lava only stop you if directly in front, and stop
                   you even if you are running */
                if (!Levitation && !Flying && !is_clinger(youmonst.data) &&
                    x == u.ux + u.dx && y == u.uy + u.dy)
                    /* No Wwalking check; otherwise they'd be able to test
                       boots by trying to SHIFT-direction into a pool and
                       seeing if the game allowed it */
                    goto stop;
                continue;
            } else {    /* e.g. objects or trap or stairs */
                if (flags.run == 1)
                    goto bcorr;
                if (flags.run == 8)
                    continue;
                if (mtmp)
                    continue;   /* d */
                if (((x == u.ux - u.dx) && (y != u.uy + u.dy)) ||
                    ((y == u.uy - u.dy) && (x != u.ux + u.dx)))
                    continue;
            }
        stop:
            nomul(0, NULL);
            return;
        }       /* end for loops */

    if (corrct > 1 && flags.run == 2)
        goto stop;
    if ((flags.run == 1 || flags.run == 3 || flags.run == 8) && !noturn && !m0
        && i0 && (corrct == 1 || (corrct == 2 && i0 == 1))) {
        /* make sure that we do not turn too far */
        if (i0 == 2) {
            if (u.dx == y0 - u.uy && u.dy == u.ux - x0)
                i = 2;  /* straight turn right */
            else
                i = -2; /* straight turn left */
        } else if (u.dx && u.dy) {
            if ((u.dx == u.dy && y0 == u.uy) || (u.dx != u.dy && y0 != u.uy))
                i = -1; /* half turn left */
            else
                i = 1;  /* half turn right */
        } else {
            if ((x0 - u.ux == y0 - u.uy && !u.dy) ||
                (x0 - u.ux != y0 - u.uy && u.dy))
                i = 1;  /* half turn right */
            else
                i = -1; /* half turn left */
        }

        i += u.last_str_turn;
        if (i <= 2 && i >= -2) {
            u.last_str_turn = i;
            u.dx = x0 - u.ux;
            u.dy = y0 - u.uy;
        }
    }
}

/* Check whether the monster should be considered a threat and interrupt
 * the current action. */
/* Also see the similar check in dochugw() in monmove.c */
static boolean
check_interrupt(struct monst *mtmp)
{
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

void
nomul(int nval, const char *txt)
{
    if (multi < nval)
        return; /* This is a bug fix by ab@unido */
    u.uinvulnerable = FALSE;    /* Kludge to avoid ctrl-C bug -dlc */
    u.usleep = 0;
    multi = nval;
    if (txt && txt[0])
        strncpy(multi_txt, txt, BUFSZ);
    else
        memset(multi_txt, 0, BUFSZ);
    flags.travel = iflags.travel1 = flags.mv = flags.run = 0;
}

/* called when a non-movement, multi-turn action has completed */
void
unmul(const char *msg_override)
{
    boolean previously_unconscious = unconscious();

    multi = 0;  /* caller will usually have done this already */
    memset(multi_txt, 0, BUFSZ);
    if (msg_override)
        nomovemsg = msg_override;
    else if (!nomovemsg)
        nomovemsg = "You can move again.";
    if (*nomovemsg)
        pline(nomovemsg);
    nomovemsg = 0;
    u.usleep = 0;
    if (afternmv)
        (*afternmv) ();
    afternmv = 0;

    if (previously_unconscious ^ unconscious()) {
        see_monsters();
        see_objects();
        vision_full_recalc = 1;
    }
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
                if (u.uprops[powers[i]].intrinsic & INTRINSIC)
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
losehp(int n, const char *knam, boolean k_format)
{
    if (Upolyd) {
        u.mh -= n;
        if (u.mhmax < u.mh)
            u.mhmax = u.mh;
        iflags.botl = 1;
        if (u.mh < 1)
            rehumanize();
        else if (n > 0 && u.mh * 10 < u.mhmax && Unchanging)
            maybe_wail();
        return;
    }

    u.uhp -= n;
    if (u.uhp > u.uhpmax)
        u.uhpmax = u.uhp;       /* perhaps n was negative */
    else
        nomul(0, NULL); /* taking damage stops command repeat */
    iflags.botl = 1;
    if (u.uhp < 1) {
        killer_format = k_format;
        killer = knam;  /* the thing that killed you */
        pline("You die...");
        done(DIED);
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
            if (EWounded_legs & LEFT_SIDE)
                carrcap -= 100;
            if (EWounded_legs & RIGHT_SIDE)
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
    return min(cap, OVERLOADED);
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
            pline(str);
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


int
dofight(int dx, int dy, int dz)
{
    int ret;

    flags.travel = iflags.travel1 = 0;
    flags.run = 0;
    flags.forcefight = 1;

    if (multi)
        flags.mv = TRUE;

    ret = domove(dx, dy, dz);
    flags.forcefight = 0;

    return ret;
}


int
domovecmd(int dx, int dy, int dz)
{
    flags.travel = iflags.travel1 = 0;
    flags.run = 0;      /* only matters here if it was 8 */

    if (multi)
        flags.mv = TRUE;

    return domove(dx, dy, dz);
}


int
domovecmd_nopickup(int dx, int dy, int dz)
{
    int ret;

    flags.nopick = 1;
    ret = domovecmd(dx, dy, dz);
    flags.nopick = 0;

    return ret;
}


static int
do_rush(int dx, int dy, int dz, int runmode, boolean move_only)
{
    int ret;

    flags.travel = iflags.travel1 = 0;
    flags.run = runmode;

    flags.nopick = move_only;

    if (!multi)
        multi = max(COLNO, ROWNO);

    ret = domove(dx, dy, dz);

    flags.nopick = 0;
    return ret;
}


int
dorun(int dx, int dy, int dz)
{
    return do_rush(dx, dy, dz, 1, FALSE);
}


int
dorun_nopickup(int dx, int dy, int dz)
{
    return do_rush(dx, dy, dz, 1, TRUE);
}


int
dogo(int dx, int dy, int dz)
{
    return do_rush(dx, dy, dz, 2, FALSE);
}


int
dogo2(int dx, int dy, int dz)
{
    return do_rush(dx, dy, dz, 3, FALSE);
}

/*hack.c*/
