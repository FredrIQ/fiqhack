/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-18 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int picklock(void);
static int forcelock(void);

/* at most one of `door' and `box' should be non-null at any given time */
static struct xlock_s {
    struct rm *door;
    struct obj *box;
    int picktyp, chance, usedtime;
} xlock;

static schar picklock_dx, picklock_dy;

static const char *lock_action(void);
static boolean obstructed(int, int);
static void chest_shatter_msg(struct obj *);

boolean
picking_lock(int *x, int *y)
{
    if (occupation == picklock) {
        *x = u.ux + picklock_dx;
        *y = u.uy + picklock_dy;
        return TRUE;
    } else {
        *x = *y = 0;
        return FALSE;
    }
}

boolean
picking_at(int x, int y)
{
    return (occupation == picklock && xlock.door == &level->locations[x][y]);
}

/* produce an occupation string appropriate for the current activity */
static const char *
lock_action(void)
{
    /* "unlocking"+2 == "locking" */
    static const char *const actions[] = {
        /* [0] */ "unlocking the door",
        /* [1] */ "unlocking the chest",
        /* [2] */ "unlocking the box",
        /* [3] */ "picking the lock"
    };

    /* if the target is currently unlocked, we're trying to lock it now */
    if (xlock.door && !(xlock.door->doormask & D_LOCKED))
        return actions[0] + 2;  /* "locking the door" */
    else if (xlock.box && !xlock.box->olocked)
        return xlock.box->otyp == CHEST ? actions[1] + 2 : actions[2] + 2;
    /* otherwise we're trying to unlock it */
    else if (xlock.picktyp == LOCK_PICK)
        return actions[3];      /* "picking the lock" */
    else if (xlock.picktyp == CREDIT_CARD)
        return actions[3];      /* same as lock_pick */
    else if (xlock.door)
        return actions[0];      /* "unlocking the door" */
    else
        return xlock.box->otyp == CHEST ? actions[1] : actions[2];
}

/* try to open/close a lock */
static int
picklock(void)
{
    xchar x, y;

    if (xlock.box) {
        if ((xlock.box->ox != u.ux) || (xlock.box->oy != u.uy)) {
            return (xlock.usedtime = 0);        /* you or it moved */
        }
    } else {    /* door */
        if (xlock.door !=
            &(level->locations[u.ux + picklock_dx][u.uy + picklock_dy])) {
            return (xlock.usedtime = 0);        /* you moved */
        }
        switch (xlock.door->doormask) {
        case D_NODOOR:
            pline("This doorway has no door.");
            return (xlock.usedtime = 0);
        case D_ISOPEN:
            pline("You cannot lock an open door.");
            return (xlock.usedtime = 0);
        case D_BROKEN:
            pline("This door is broken.");
            return (xlock.usedtime = 0);
        }
    }

    if (xlock.usedtime++ >= 50 || nohands(youmonst.data)) {
        pline("You give up your attempt at %s.", lock_action());
        exercise(A_DEX, TRUE);  /* even if you don't succeed */
        return (xlock.usedtime = 0);
    }

    if (rn2(100) >= xlock.chance)
        return 1;       /* still busy */

    pline("You succeed in %s.", lock_action());
    if (xlock.door) {
        if (xlock.door->doormask & D_TRAPPED) {
            b_trapped("door", FINGER);
            xlock.door->doormask = D_NODOOR;
            unblock_point(u.ux + picklock_dx, u.uy + picklock_dy);
            if (*in_rooms
                (level, u.ux + picklock_dx, u.uy + picklock_dy, SHOPBASE))
                add_damage(u.ux + picklock_dx, u.uy + picklock_dy, 0L);
            newsym(u.ux + picklock_dx, u.uy + picklock_dy);
        } else if (xlock.door->doormask & D_LOCKED)
            xlock.door->doormask = D_CLOSED;
        else
            xlock.door->doormask = D_LOCKED;
        /* player now knows the door's open/closed status, and its
           locked/unlocked status, and also that it isn't trapped (it would
           have exploded otherwise); we haven't recorded the location of the
           door being picked, so scan for it */
        for (x = 1; x < COLNO; x++)
            for (y = 0; y < ROWNO; y++)
                if (picking_at(x, y))
                    magic_map_background(x, y, TRUE);
    } else {
        xlock.box->olocked = !xlock.box->olocked;
        if (xlock.box->otrapped)
            chest_trap(xlock.box, FINGER, FALSE);
    }
    exercise(A_DEX, TRUE);
    return (xlock.usedtime = 0);
}

/* try to force a locked chest */
static int
forcelock(void)
{
    struct monst *shkp;
    boolean costly;

    struct obj *otmp;

    if ((xlock.box->ox != u.ux) || (xlock.box->oy != u.uy))
        return (xlock.usedtime = 0);    /* you or it moved */

    if (xlock.usedtime++ >= 50 || !uwep || nohands(youmonst.data)) {
        pline("You give up your attempt to force the lock.");
        if (xlock.usedtime >= 50)       /* you made the effort */
            exercise((xlock.picktyp) ? A_DEX : A_STR, TRUE);
        return (xlock.usedtime = 0);
    }

    if (xlock.picktyp) {        /* blade */

        if (rn2(1000 - (int)uwep->spe) > (992 - greatest_erosion(uwep) * 10) &&
            !uwep->cursed && !obj_resists(uwep, 0, 99)) {
            /* for a +0 weapon, probability that it survives an unsuccessful
               attempt to force the lock is (.992)^50 = .67 */
            pline("%sour %s broke!", (uwep->quan > 1L) ? "One of y" : "Y",
                  xname(uwep));
            useup(uwep);
            pline("You give up your attempt to force the lock.");
            exercise(A_DEX, TRUE);
            return (xlock.usedtime = 0);
        }
    } else      /* blunt */
        wake_nearby();  /* due to hammering on the container */

    if (rn2(100) >= xlock.chance)
        return 1;       /* still busy */

    pline("You succeed in forcing the lock.");
    xlock.box->olocked = 0;
    xlock.box->obroken = 1;
    costly = (*u.ushops && costly_spot(u.ux, u.uy));
    shkp = costly ? shop_keeper(level, *u.ushops) : 0;
    if (!xlock.picktyp && !rn2(3)) {
        long loss = 0L;

        pline("In fact, you've totally destroyed %s.", the(xname(xlock.box)));

        /* Put the contents on ground at the hero's feet. */
        while ((otmp = xlock.box->cobj) != 0) {
            obj_extract_self(otmp);
            if (!rn2(3) || otmp->oclass == POTION_CLASS) {
                chest_shatter_msg(otmp);
                if (costly)
                    loss +=
                        stolen_value(otmp, u.ux, u.uy,
                                     (boolean) shkp->mpeaceful, TRUE);
                if (otmp->quan == 1L) {
                    obfree(otmp, NULL);
                    continue;
                }
                useup(otmp);
            }
            if (xlock.box->otyp == ICE_BOX && otmp->otyp == CORPSE) {
                otmp->age = moves - otmp->age;  /* actual age */
                start_corpse_timeout(otmp);
            }
            place_object(otmp, level, u.ux, u.uy);
            stackobj(otmp);
        }

        if (costly)
            loss +=
                stolen_value(xlock.box, u.ux, u.uy, (boolean) shkp->mpeaceful,
                             TRUE);
        if (loss)
            pline("You owe %ld %s for objects destroyed.", loss,
                  currency(loss));
        delobj(xlock.box);
    } else {
        if (costly) {
            struct obj *cobjbak = xlock.box->cobj;

            xlock.box->cobj = (struct obj *)0;
            verbalize("You damage it, you bought it!");
            bill_dummy_object(xlock.box);
            xlock.box->cobj = cobjbak;
        }
    }
    exercise((xlock.picktyp) ? A_DEX : A_STR, TRUE);
    return (xlock.usedtime = 0);
}


void
reset_pick(void)
{
    xlock.usedtime = xlock.chance = xlock.picktyp = 0;
    xlock.door = 0;
    xlock.box = 0;
}

/* pick a lock with a given object */
int
pick_lock(struct obj *pick, schar dx, schar dy, schar dz)
{
    int picktyp, c, ch, got_dir = FALSE;
    coord cc;
    struct rm *door;
    struct obj *otmp;
    char qbuf[QBUFSZ];

    picktyp = pick->otyp;
    pick->lastused = moves;

    /* check whether we're resuming an interrupted previous attempt */
    if (xlock.usedtime && picktyp == xlock.picktyp) {
        static const char no_longer[] =
            "Unfortunately, you can no longer %s %s.";

        if (nohands(youmonst.data)) {
            const char *what = (picktyp == LOCK_PICK) ? "pick" : "key";

            if (picktyp == CREDIT_CARD)
                what = "card";
            pline(no_longer, "hold the", what);
            reset_pick();
            return 0;
        } else if (xlock.box && !can_reach_floor()) {
            pline(no_longer, "reach the", "lock");
            reset_pick();
            return 0;
        } else {
            const char *action = lock_action();

            pline("You resume your attempt at %s.", action);
            set_occupation(picklock, action);
            return 1;
        }
    }

    if (nohands(youmonst.data)) {
        pline("You can't hold %s -- you have no hands!", doname(pick));
        return 0;
    }

    if ((picktyp != LOCK_PICK && picktyp != CREDIT_CARD &&
         picktyp != SKELETON_KEY)) {
        impossible("picking lock with object %d?", picktyp);
        return 0;
    }
    ch = 0;     /* lint suppression */

    if (dx != -2 && dy != -2 && dz != -2) {  /* -2 signals no direction given */
        cc.x = u.ux + dx;
        cc.y = u.uy + dy;
        if (isok(cc.x, cc.y))
            got_dir = TRUE;
    }

    if (!got_dir &&
        !get_adjacent_loc(NULL, "Invalid location!", u.ux, u.uy, &cc, &dz))
        return 0;

    if (!got_dir) {
        dx = cc.x - u.ux;
        dy = cc.y - u.uy;
        dz = 0;
    }

    if (cc.x == u.ux && cc.y == u.uy) { /* pick lock on a container */
        const char *verb;
        boolean it;
        int count;

        if (dz < 0) {
            pline("There isn't any sort of lock up %s.",
                  Levitation ? "here" : "there");
            return 0;
        } else if (is_lava(level, u.ux, u.uy)) {
            pline("Doing that would probably melt your %s.", xname(pick));
            return 0;
        } else if (is_pool(level, u.ux, u.uy) && !Underwater) {
            pline("The water has no lock.");
            return 0;
        }

        count = 0;
        c = 'n';        /* in case there are no boxes here */
        for (otmp = level->objects[cc.x][cc.y]; otmp; otmp = otmp->nexthere)
            if (Is_box(otmp)) {
                ++count;
                if (!can_reach_floor()) {
                    pline("You can't reach %s from up here.", the(xname(otmp)));
                    return 0;
                }
                it = 0;
                if (otmp->obroken)
                    verb = "fix";
                else if (!otmp->olocked)
                    verb = "lock", it = 1;
                else if (picktyp != LOCK_PICK)
                    verb = "unlock", it = 1;
                else
                    verb = "pick";
                sprintf(qbuf, "There is %s here, %s %s?",
                        safe_qbuf("",
                                  sizeof ("There is  here, unlock its lock?"),
                                  doname(otmp), an(simple_typename(otmp->otyp)),
                                  "a box"), verb, it ? "it" : "its lock");

                c = ynq(qbuf);
                if (c == 'q')
                    return 0;
                if (c == 'n')
                    continue;

                if (otmp->obroken) {
                    pline("You can't fix its broken lock with %s.",
                          doname(pick));
                    return 0;
                } else if (picktyp == CREDIT_CARD && !otmp->olocked) {
                    /* credit cards are only good for unlocking */
                    pline("You can't do that with %s.", doname(pick));
                    return 0;
                }
                switch (picktyp) {
                case CREDIT_CARD:
                    ch = ACURR(A_DEX) + 20 * Role_if(PM_ROGUE);
                    break;
                case LOCK_PICK:
                    ch = 4 * ACURR(A_DEX) + 25 * Role_if(PM_ROGUE);
                    break;
                case SKELETON_KEY:
                    ch = 75 + ACURR(A_DEX);
                    break;
                default:
                    ch = 0;
                }
                if (otmp->cursed)
                    ch /= 2;

                xlock.picktyp = picktyp;
                xlock.box = otmp;
                xlock.door = 0;
                break;
            }
        if (c != 'y') {
            if (!count)
                pline("There doesn't seem to be any sort of lock here.");
            return 0;   /* decided against all boxes */
        }
    } else {    /* pick the lock in a door */
        struct monst *mtmp;

        if (u.utrap && u.utraptype == TT_PIT) {
            pline("You can't reach over the edge of the pit.");
            return 0;
        }

        door = &level->locations[cc.x][cc.y];
        if ((mtmp = m_at(level, cc.x, cc.y)) && canseemon(mtmp)
            && mtmp->m_ap_type != M_AP_FURNITURE &&
            mtmp->m_ap_type != M_AP_OBJECT) {
            if (picktyp == CREDIT_CARD &&
                (mtmp->isshk || mtmp->data == &mons[PM_ORACLE]))
                verbalize("No checks, no credit, no problem.");
            else
                pline("I don't think %s would appreciate that.", mon_nam(mtmp));
            return 0;
        }
        if (mtmp && (mtmp->m_ap_type == M_AP_FURNITURE) &&
            (mtmp->mappearance == S_hcdoor || mtmp->mappearance == S_vcdoor) &&
            !Protection_from_shape_changers) {
            stumble_onto_mimic(mtmp, dx, dy);
            return (1);
        }
        if (!IS_DOOR(door->typ)) {
            if (is_drawbridge_wall(cc.x, cc.y) >= 0)
                pline("You %s no lock on the drawbridge.",
                      Blind ? "feel" : "see");
            else
                pline("You %s no door there.", Blind ? "feel" : "see");
            return 0;
        }
        switch (door->doormask) {
        case D_NODOOR:
            pline("This doorway has no door.");
            return 0;
        case D_ISOPEN:
            pline("You cannot lock an open door.");
            return 0;
        case D_BROKEN:
            pline("This door is broken.");
            return 0;
        default:
            /* credit cards are only good for unlocking */
            if (picktyp == CREDIT_CARD && !(door->doormask & D_LOCKED)) {
                pline("You can't lock a door with a credit card.");
                return 0;
            }

            /* At this point, the player knows that the door is a door, and
               whether it's locked, but not whether it's trapped; to do this,
               we set the mem_door_l flag and call map_background, which will
               clear it if necessary (i.e. not a door after all). */
            level->locations[cc.x][cc.y].mem_door_l = 1;
            map_background(cc.x, cc.y, TRUE);

            switch (picktyp) {
            case CREDIT_CARD:
                ch = 2 * ACURR(A_DEX) + 20 * Role_if(PM_ROGUE);
                break;
            case LOCK_PICK:
                ch = 3 * ACURR(A_DEX) + 30 * Role_if(PM_ROGUE);
                break;
            case SKELETON_KEY:
                ch = 70 + ACURR(A_DEX);
                break;
            default:
                ch = 0;
            }
            xlock.door = door;
            xlock.box = 0;
        }
    }
    flags.move = 0;
    xlock.chance = ch;
    xlock.picktyp = picktyp;
    xlock.usedtime = 0;
    picklock_dx = dx;
    picklock_dy = dy;
    set_occupation(picklock, lock_action());
    return 1;
}

/* try to force a chest with your weapon */
int
doforce(const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    int c, picktyp;
    char qbuf[QBUFSZ];

    (void) arg;

    if (!uwep ||        /* proper type test */
        (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep) &&
         uwep->oclass != ROCK_CLASS) ||
        (objects[uwep->otyp].oc_skill < P_DAGGER) ||
        (objects[uwep->otyp].oc_skill > P_LANCE) || uwep->otyp == FLAIL ||
        uwep->otyp == AKLYS || uwep->otyp == RUBBER_HOSE) {
        pline("You can't force anything without a %sweapon.",
              (uwep) ? "proper " : "");
        return 0;
    }

    picktyp = is_blade(uwep);
    if (xlock.usedtime && xlock.box && picktyp == xlock.picktyp) {
        pline("You resume your attempt to force the lock.");
        set_occupation(forcelock, "forcing the lock");
        return 1;
    }

    /* A lock is made only for the honest man, the thief will break it. */
    xlock.box = NULL;
    for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere)
        if (Is_box(otmp)) {
            if (otmp->obroken || !otmp->olocked) {
                pline("There is %s here, but its lock is already %s.",
                      doname(otmp), otmp->obroken ? "broken" : "unlocked");
                continue;
            }
            sprintf(qbuf, "There is %s here, force its lock?",
                    safe_qbuf("", sizeof ("There is  here, force its lock?"),
                              doname(otmp), an(simple_typename(otmp->otyp)),
                              "a box"));

            c = ynq(qbuf);
            if (c == 'q')
                return 0;
            if (c == 'n')
                continue;

            if (picktyp)
                pline("You force your %s into a crack and pry.", xname(uwep));
            else
                pline("You start bashing it with your %s.", xname(uwep));
            xlock.box = otmp;
            xlock.chance = objects[uwep->otyp].oc_wldam * 2;
            xlock.picktyp = picktyp;
            xlock.usedtime = 0;
            break;
        }

    if (xlock.box)
        set_occupation(forcelock, "forcing the lock");
    else
        pline("You decide not to force the issue.");
    return 1;
}

/* try to open a door */
int
doopen(const struct nh_cmd_arg *arg)
{
    coord cc;
    struct rm *door;
    struct monst *mtmp;
    boolean got_dir = FALSE;
    schar dx, dy, dz;

    if (nohands(youmonst.data)) {
        pline
            ("You can't open, close, or unlock anything -- you have no hands!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        pline("You can't reach over the edge of the pit.");
        return 0;
    }

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    cc.x = u.ux + dx;
    cc.y = u.uy + dy;
    if (!isok(cc.x, cc.y))
        return 0;

    if ((cc.x == u.ux) && (cc.y == u.uy))
        return 0;

    if ((mtmp = m_at(level, cc.x, cc.y)) && mtmp->m_ap_type == M_AP_FURNITURE &&
        (mtmp->mappearance == S_hcdoor || mtmp->mappearance == S_vcdoor) &&
        !Protection_from_shape_changers) {

        stumble_onto_mimic(mtmp, cc.x - u.ux, cc.y - u.uy);
        return 1;
    }

    door = &level->locations[cc.x][cc.y];

    if (!IS_DOOR(door->typ)) {
        if (is_db_wall(cc.x, cc.y)) {
            pline("There is no obvious way to open the drawbridge.");
            return 0;
        }
        pline("You %s no door there.", Blind ? "feel" : "see");
        return 0;
    }

    if (door->doormask == D_ISOPEN) {
        struct nh_cmd_arg arg;
        arg_from_delta(dx, dy, dz, &arg);
        return doclose(&arg);
    }

    if (!(door->doormask & D_CLOSED)) {
        const char *mesg;

        switch (door->doormask) {
        case D_BROKEN:
            mesg = " is broken";
            break;
        case D_NODOOR:
            mesg = "way has no door";
            break;
        case D_ISOPEN:
            mesg = " is already open";
            break;
        default:
            if (!got_dir && door->mem_door_l) {
                /* With a direction given explicitly, we try to unlock the door 
                   too, if we knew it was locked. */
                struct obj *bestpick = 0;
                struct obj *otmp;

                for (otmp = invent; otmp; otmp = otmp->nobj) {
                    if (otmp->otyp == LOCK_PICK || otmp->otyp == CREDIT_CARD ||
                        otmp->otyp == SKELETON_KEY) {
                        if (!bestpick || otmp->lastused > bestpick->lastused)
                            bestpick = otmp;
                    }
                }
                if (!bestpick)
                    pline("You have nothing to unlock that with.");
                else if (!bestpick->lastused)
                    pline("Use an unlocking tool manually so I know "
                          "which one you want to use.");
                else
                    return pick_lock(bestpick, dx, dy, dz);
            }
            door->mem_door_l = 1;
            map_background(cc.x, cc.y, TRUE);
            mesg = " is locked";
            break;
        }
        pline("This door%s.", mesg);
        if (Blind)
            feel_location(cc.x, cc.y);
        return 0;
    }

    if (verysmall(youmonst.data)) {
        pline("You're too small to pull the door open.");
        return 0;
    }

    /* door is known to be CLOSED */
    if (rnl(20) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
        pline("The door opens.");
        if (door->doormask & D_TRAPPED) {
            b_trapped("door", FINGER);
            door->doormask = D_NODOOR;
            if (*in_rooms(level, cc.x, cc.y, SHOPBASE))
                add_damage(cc.x, cc.y, 0L);
        } else
            door->doormask = D_ISOPEN;
        if (Blind)
            feel_location(cc.x, cc.y);  /* the hero knows she opened it */
        else
            newsym(cc.x, cc.y);
        unblock_point(cc.x, cc.y);      /* vision: new see through there */
    } else {
        exercise(A_STR, TRUE);
        door->mem_door_l = 1;
        map_background(cc.x, cc.y, TRUE);
        pline("The door resists!");
    }

    return 1;
}

static boolean
obstructed(int x, int y)
{
    struct monst *mtmp = m_at(level, x, y);

    if (mtmp && mtmp->m_ap_type != M_AP_FURNITURE) {
        if (mtmp->m_ap_type == M_AP_OBJECT)
            goto objhere;
        pline("%s stands in the way!",
              !canspotmon(mtmp) ? "Some creature" : Monnam(mtmp));
        if (!canspotmon(mtmp))
            map_invisible(mtmp->mx, mtmp->my);
        return TRUE;
    }
    if (OBJ_AT(x, y)) {
    objhere:pline("Something's in the way.");
        return TRUE;
    }
    return FALSE;
}

/* try to close a door */
int
doclose(const struct nh_cmd_arg *arg)
{
    struct rm *door;
    struct monst *mtmp;
    coord cc;
    schar dx, dy, dz;

    if (nohands(youmonst.data)) {
        pline("You can't close anything -- you have no hands!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        pline("You can't reach over the edge of the pit.");
        return 0;
    }

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    cc.x = u.ux + dx;
    cc.y = u.uy + dy;
    if (!isok(cc.x, cc.y))
        return 0;

    if ((cc.x == u.ux) && (cc.y == u.uy)) {
        pline("You are in the way!");
        return 1;
    }

    if ((mtmp = m_at(level, cc.x, cc.y)) && mtmp->m_ap_type == M_AP_FURNITURE &&
        (mtmp->mappearance == S_hcdoor || mtmp->mappearance == S_vcdoor) &&
        !Protection_from_shape_changers) {

        stumble_onto_mimic(mtmp, dx, dy);
        return 1;
    }

    door = &level->locations[cc.x][cc.y];

    if (!IS_DOOR(door->typ)) {
        if (door->typ == DRAWBRIDGE_DOWN)
            pline("There is no obvious way to close the drawbridge.");
        else
            pline("You %s no door there.", Blind ? "feel" : "see");
        return 0;
    }

    if (door->doormask == D_NODOOR) {
        pline("This doorway has no door.");
        return 0;
    }

    if (obstructed(cc.x, cc.y))
        return 0;

    if (door->doormask == D_BROKEN) {
        pline("This door is broken.");
        return 0;
    }

    if (door->doormask & (D_CLOSED | D_LOCKED)) {
        pline("This door is already closed.");
        return 0;
    }

    if (door->doormask == D_ISOPEN) {
        if (verysmall(youmonst.data) && !u.usteed) {
            pline("You're too small to push the door closed.");
            return 0;
        }
        if (u.usteed || rn2(25) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
            pline("The door closes.");
            door->doormask = D_CLOSED;
            door->mem_door_l = 1;
            /* map_background here sets the mem_door flags correctly; and it's
               redundant to both feel_location and newsym with a door.
               Exception: if we remember an invisible monster on the door
               square, but in this case, we want to set the memory of a door
               there anyway because we know there's a door there because we
               just closed it, and in Nitro this doesn't clash with keeping the 
               I there. */
            map_background(cc.x, cc.y, TRUE);
            if (Blind)
                feel_location(cc.x, cc.y);      /* the hero knows she closed it 
                                                 */
            else
                newsym(cc.x, cc.y);
            block_point(cc.x, cc.y);    /* vision: no longer see there */
        } else {
            exercise(A_STR, TRUE);
            pline("The door resists!");
        }
    }

    return 1;
}

/* box obj was hit with spell effect otmp */
/* returns true if something happened */
boolean
boxlock(struct obj * obj, struct obj * otmp)
{
    boolean res = 0;

    switch (otmp->otyp) {
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        if (!obj->olocked) {    /* lock it; fix if broken */
            pline("Klunk!");
            obj->olocked = 1;
            obj->obroken = 0;
            res = 1;
        }       /* else already closed and locked */
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        if (obj->olocked) {     /* unlock; couldn't be broken */
            pline("Klick!");
            obj->olocked = 0;
            res = 1;
        } else  /* silently fix if broken */
            obj->obroken = 0;
        break;
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
        /* maybe start unlocking chest, get interrupted, then zap it; we must
           avoid any attempt to resume unlocking it */
        if (xlock.box == obj)
            reset_pick();
        break;
    }
    return res;
}

/* Door/secret door was hit with spell effect otmp */
/* returns true if something happened */
boolean
doorlock(struct obj * otmp, int x, int y)
{
    struct rm *door = &level->locations[x][y];
    boolean res = TRUE;
    int loudness = 0;
    const char *msg = NULL;
    const char *dustcloud = "A cloud of dust";
    const char *quickly_dissipates = "quickly dissipates";

    if (door->typ == SDOOR) {
        switch (otmp->otyp) {
        case WAN_OPENING:
        case SPE_KNOCK:
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
            door->typ = DOOR;
            door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
            newsym(x, y);
            if (cansee(x, y))
                pline("A door appears in the wall!");
            if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK)
                return TRUE;
            break;      /* striking: continue door handling below */
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
        default:
            return FALSE;
        }
    }

    switch (otmp->otyp) {
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        if (Is_rogue_level(&u.uz)) {
            boolean vis = cansee(x, y);

            /* Can't have real locking in Rogue, so just hide doorway */
            if (vis)
                pline("%s springs up in the older, more primitive doorway.",
                      dustcloud);
            else
                You_hear("a swoosh.");
            if (obstructed(x, y)) {
                if (vis)
                    pline("The cloud %s.", quickly_dissipates);
                return FALSE;
            }
            block_point(x, y);
            door->typ = SDOOR;
            if (vis)
                pline("The doorway vanishes!");
            newsym(x, y);
            return TRUE;
        }
        if (obstructed(x, y))
            return FALSE;
        /* Don't allow doors to close over traps.  This is for pits */
        /* & trap doors, but is it ever OK for anything else? */
        if (t_at(level, x, y)) {
            /* maketrap() clears doormask, so it should be NODOOR */
            pline("%s springs up in the doorway, but %s.", dustcloud,
                  quickly_dissipates);
            return FALSE;
        }

        switch (door->doormask & ~D_TRAPPED) {
        case D_CLOSED:
            msg = "The door locks!";
            break;
        case D_ISOPEN:
            msg = "The door swings shut, and locks!";
            break;
        case D_BROKEN:
            msg = "The broken door reassembles and locks!";
            break;
        case D_NODOOR:
            msg =
                "A cloud of dust springs up and assembles itself into a door!";
            break;
        default:
            res = FALSE;
            break;
        }
        block_point(x, y);
        door->doormask = D_LOCKED | (door->doormask & D_TRAPPED);
        newsym(x, y);
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        if (door->doormask & D_LOCKED) {
            msg = "The door unlocks!";
            door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
        } else
            res = FALSE;
        break;
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        if (door->doormask & (D_LOCKED | D_CLOSED)) {
            if (door->doormask & D_TRAPPED) {
                if (MON_AT(level, x, y))
                    mb_trapped(m_at(level, x, y));
                else if (flags.verbose) {
                    if (cansee(x, y))
                        pline("KABOOM!!  You see a door explode.");
                    else if (flags.soundok)
                        You_hear("a distant explosion.");
                }
                door->doormask = D_NODOOR;
                unblock_point(x, y);
                newsym(x, y);
                loudness = 40;
                break;
            }
            door->doormask = D_BROKEN;
            if (flags.verbose) {
                if (cansee(x, y))
                    pline("The door crashes open!");
                else if (flags.soundok)
                    You_hear("a crashing sound.");
            }
            unblock_point(x, y);
            newsym(x, y);
            /* force vision recalc before printing more messages */
            if (vision_full_recalc)
                vision_recalc(0);
            loudness = 20;
        } else
            res = FALSE;
        break;
    default:
        impossible("magic (%d) attempted on door.", otmp->otyp);
        break;
    }
    if (msg && cansee(x, y)) {
        pline(msg);
        /* we know whether it's locked now */
        level->locations[x][y].mem_door_l = 1;
        map_background(x, y, TRUE);
    }
    if (loudness > 0) {
        /* door was destroyed */
        wake_nearto(x, y, loudness);
        if (*in_rooms(level, x, y, SHOPBASE))
            add_damage(x, y, 0L);
    }

    if (res && picking_at(x, y)) {
        /* maybe unseen monster zaps door you're unlocking */
        stop_occupation();
        reset_pick();
    }
    return res;
}

static void
chest_shatter_msg(struct obj *otmp)
{
    const char *disposition;
    const char *thing;
    long save_Blinded;

    if (otmp->oclass == POTION_CLASS) {
        pline("You %s %s shatter!", Blind ? "hear" : "see", an(bottlename()));
        if (!breathless(youmonst.data) || haseyes(youmonst.data))
            potionbreathe(otmp);
        return;
    }
    /* We have functions for distant and singular names, but not one */
    /* which does _both_... */
    save_Blinded = Blinded;
    Blinded = 1;
    thing = singular(otmp, xname);
    Blinded = save_Blinded;
    switch (objects[otmp->otyp].oc_material) {
    case PAPER:
        disposition = "is torn to shreds";
        break;
    case WAX:
        disposition = "is crushed";
        break;
    case VEGGY:
        disposition = "is pulped";
        break;
    case FLESH:
        disposition = "is mashed";
        break;
    case GLASS:
        disposition = "shatters";
        break;
    case WOOD:
        disposition = "splinters to fragments";
        break;
    default:
        disposition = "is destroyed";
        break;
    }
    pline("%s %s!", An(thing), disposition);
}

/*lock.c*/
