/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static boolean uwep_can_force(void);
static int reset_pick(void);
static int picklock(void);
static int forcelock(void);
static struct obj *get_current_unlock_tool(void);
static int get_unlock_chance(void);
static const char *lock_action(void);
static boolean obstructed(int, int, enum msg_channel);
static void chest_shatter_msg(struct obj *);

static boolean
uwep_can_force(void)
{
    if (!uwep ||
        (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep) &&
         uwep->oclass != ROCK_CLASS) ||
        (objects[uwep->otyp].oc_skill < P_DAGGER) ||
        (objects[uwep->otyp].oc_skill > P_LANCE) || uwep->otyp == FLAIL ||
        uwep->otyp == AKLYS || uwep->otyp == RUBBER_HOSE) {
        pline(msgc_cancelled, "You can't force anything without a %sweapon.",
              uwep ? "proper " : "");
        return FALSE;
    }
    return TRUE;
}

/* Always returns 0. (This is so the caller can just do "return reset_pick()";
   the main purpose of this function is to make the code easier to read.) */
static int
reset_pick(void)
{
    u.utracked[tos_lock] = 0;
    u.uoccupation_progress[tos_lock] = 0;
    return 0;
}

/* Finds the current or most recently used unlocking tool. */
static struct obj *
get_current_unlock_tool(void) {
    struct obj *bestpick, *otmp;
    bestpick = NULL;

    for (otmp = invent; otmp; otmp = otmp->nobj) {
        if (otmp->otyp == LOCK_PICK || otmp->otyp == CREDIT_CARD ||
            otmp->otyp == SKELETON_KEY) {
            if (!bestpick || otmp->lastused > bestpick->lastused)
                bestpick = otmp;
        }
    }
    return bestpick;
}

/* Finds the probability per turn of succeeding in unlocking a lock. */
static int
get_unlock_chance(void) {
    struct obj *pick = get_current_unlock_tool();
    int factor;

    if (!pick)
        return 0;
    factor = pick->cursed ? 2 : 1;

    switch (pick->otyp) {
    case CREDIT_CARD:
        return (2 * ACURR(A_DEX) + 20 * Role_if(PM_ROGUE)) / factor;

    case LOCK_PICK:
        return (3 * ACURR(A_DEX) + 30 * Role_if(PM_ROGUE)) / factor;

    case SKELETON_KEY:
        return (70 + ACURR(A_DEX)) / factor;

    default:
        return 0;
    }
}


/* produce an occupation string appropriate for the current activity */
static const char *
lock_action(void)
{
    struct rm *door = NULL;
    struct obj *box = u.utracked[tos_lock];
    struct obj *pick = get_current_unlock_tool();

    /* "unlocking"+2 == "locking" */
    static const char *const actions[] = {
        /* [0] */ "unlocking the door",
        /* [1] */ "unlocking the chest",
        /* [2] */ "unlocking the box",
        /* [3] */ "picking the lock"
    };

    if (box == &zeroobj) {
        box = NULL;
        door = &(level->locations[u.utracked_location[tl_lock].x]
                 [u.utracked_location[tl_lock].y]);
    }

    /* if the target is currently unlocked, we're trying to lock it now */
    if (door && !(door->doormask & D_LOCKED))
        return actions[0] + 2;  /* "locking the door" */
    else if (box && !box->olocked)
        return box->otyp == CHEST ? actions[1] + 2 : actions[2] + 2;
    /* otherwise we're trying to unlock it */
    else if (pick && pick->otyp == LOCK_PICK)
        return actions[3];      /* "picking the lock" */
    else if (pick && pick->otyp == CREDIT_CARD)
        return actions[3];      /* same as lock_pick */
    else if (door)
        return actions[0];      /* "unlocking the door" */
    else
        return box->otyp == CHEST ? actions[1] : actions[2];
}

/* Called every turn during lock-picking. The caller must set
   u.utracked[tos_lock] appropriately: &zeroobj for a door, an object for a
   box. For a door, u.utracked_location[tl_lock] must also be set. */
static int
picklock(void)
{
    int chance = get_unlock_chance();
    int x = u.utracked_location[tl_lock].x;
    int y = u.utracked_location[tl_lock].y;

    struct rm *door = NULL;

    if (u.utracked[tos_lock] != &zeroobj) {
        if (!obj_with_u(u.utracked[tos_lock]))
            return reset_pick();
    } else { /* door */
        door = &(level->locations[x][y]);
        switch (door->doormask) {
        case D_NODOOR:
            pline(msgc_cancelled, "This doorway has no door.");
            return reset_pick();
        case D_ISOPEN:
            pline(msgc_cancelled, "You cannot lock an open door.");
            return reset_pick();
        case D_BROKEN:
            pline(msgc_cancelled, "This door is broken.");
            return reset_pick();
        }
    }
    
    if (!chance) {
        pline(msgc_interrupted, "You seem to have lost your unlocking tools.");
        return reset_pick();
    }

    if (u.uoccupation_progress[tos_lock]++ >= 50 || nohands(youmonst.data)) {
        pline(msgc_failrandom, "You give up your attempt at %s.",
              lock_action());
        if (!nohands(youmonst.data))
            exercise(A_DEX, TRUE);  /* even if you don't succeed */
        return reset_pick();
    }
    
    if (rn2(100) >= chance)
        return 1;       /* still busy */
    
    pline(msgc_actionok, "You succeed in %s.", lock_action());
    if (door) {
        if (door->doormask & D_TRAPPED) {
            b_trapped("door", FINGER);
            door->doormask = D_NODOOR;
            unblock_point(x, y);
            if (*in_rooms(level, x, y, SHOPBASE))
                add_damage(x, y, 0L);
        } else if (door->doormask & D_LOCKED)
            door->doormask = D_CLOSED;
        else
            door->doormask = D_LOCKED;

        /* player now knows the door's open/closed status, and its
           locked/unlocked status, and also that it isn't trapped (it would have
           exploded otherwise); thus, we can safely fully spoil the door's stats
           (the door is the background of the door's location) */
        magic_map_background(x, y, TRUE);

    } else {
        u.utracked[tos_lock]->olocked = !u.utracked[tos_lock]->olocked;
        if (u.utracked[tos_lock]->otrapped)
            chest_trap(u.utracked[tos_lock], FINGER, FALSE);
    }
    exercise(A_DEX, TRUE);

    return reset_pick();
}

/* Called every turn during chest-forcing. The caller must set
   u.utracked[tos_lock] to the chest in question. */
static int
forcelock(void)
{
    struct monst *shkp;
    boolean costly;

    struct obj *otmp;
    struct obj *box = u.utracked[tos_lock];

    if (!obj_with_u(box))
        return reset_pick();

    if (!uwep_can_force()) /* prints the messages; ensures uwep != NULL */
        return reset_pick();

    if (u.uoccupation_progress[tos_lock]++ >= 50 ||
         nohands(youmonst.data)) {
        pline(msgc_failrandom, "You give up your attempt to force the lock.");
        if (!nohands(youmonst.data))
            exercise(is_blade(uwep) ? A_DEX : A_STR, TRUE);
        return reset_pick();
    }

    if (is_blade(uwep)) {
        if (rn2(1000 - (int)uwep->spe) > (992 - greatest_erosion(uwep) * 10) &&
            !uwep->cursed && !obj_resists(uwep, 0, 99)) {
            /* for a +0 weapon, probability that it survives an unsuccessful
               attempt to force the lock is (.992)^50 = .67 */
            pline(msgc_substitute, "%sour %s broke!",
                  (uwep->quan > 1L) ? "One of y" : "Y", xname(uwep));
            useup(uwep);
            pline_implied(msgc_failcurse,
                          "You can't exactly force that lock now.");
            exercise(A_DEX, TRUE);
            return reset_pick();
        }
    } else      /* blunt */
        wake_nearby(FALSE);  /* due to hammering on the container */

    if (rn2(100) >= objects[uwep->otyp].oc_wldam * 2)
        return 1;       /* still busy */

    pline(msgc_actionok, "You succeed in forcing the lock.");
    box->olocked = 0;
    box->obroken = 1;
    costly = (*u.ushops && costly_spot(u.ux, u.uy));
    shkp = costly ? shop_keeper(level, *u.ushops) : 0;
    if (!is_blade(uwep) && !rn2(3)) {
        long loss = 0L;

        pline(msgc_substitute, "In fact, you've totally destroyed %s.",
              the(xname(box)));

        /* Put the contents on ground at the hero's feet. */
        while ((otmp = box->cobj) != 0) {
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
            if (box->otyp == ICE_BOX && otmp->otyp == CORPSE) {
                otmp->age = moves - otmp->age;  /* actual age */
                start_corpse_timeout(otmp);
            }
            place_object(otmp, level, u.ux, u.uy);
            stackobj(otmp);
        }

        if (costly)
            loss +=
                stolen_value(box, u.ux, u.uy, (boolean) shkp->mpeaceful,
                             TRUE);
        if (loss)
            pline(msgc_unpaid, "You owe %ld %s for objects destroyed.",
                  loss, currency(loss));
        delobj(box);
    } else {
        if (costly) {
            struct obj *cobjbak = box->cobj;

            box->cobj = (struct obj *)0;
            verbalize(msgc_unpaid, "You damage it, you bought it!");
            bill_dummy_object(box);
            box->cobj = cobjbak;
        }
    }
    exercise(is_blade(uwep) ? A_DEX : A_STR, TRUE);

    return reset_pick();
}

/* pick a lock on a chest or door with a given object */
int
pick_lock(struct obj *pick, const struct nh_cmd_arg *arg)
{
    int picktyp, c;
    coord cc;
    schar dx, dy, dz;
    struct rm *door;
    struct obj *otmp;
    const char *qbuf;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;
    cc.x = u.ux + dx;
    cc.y = u.uy + dy;
    if (!isok(cc.x, cc.y))
        return 0;

    picktyp = pick->otyp;
    pick->lastused = moves;

    /* Check whether we're resuming an interrupted previous attempt.  For a
       floor pick, we have u.utracked[tos_lock] as a non-zeroobj and dx and dy
       as 0.  For a door, we have u.utracked_location[tl_lock] specifying the
       location and u.utracked[tos_lock] as &zeroobj. */
    if (u.uoccupation_progress[tos_lock] &&
        ((u.utracked_location[tl_lock].x == cc.x &&
          u.utracked_location[tl_lock].y == cc.y &&
          u.utracked[tos_lock] == &zeroobj) ||
         (dx == 0 && dy == 0 && u.utracked[tos_lock] != &zeroobj))) {
        static const char no_longer[] =
            "Unfortunately, you can no longer %s %s.";

        if (nohands(youmonst.data)) {
            const char *what = (picktyp == LOCK_PICK) ? "pick" : "key";

            if (picktyp == CREDIT_CARD)
                what = "card";
            pline(msgc_interrupted, no_longer, "hold the", what);
            return reset_pick();
        } else if (u.utracked[tos_lock] != &zeroobj && !can_reach_floor()) {
            pline(msgc_interrupted, no_longer, "reach the", "lock");
            return reset_pick();
        } else {
            const char *action = lock_action();

            if (turnstate.continue_message)
                pline(msgc_occstart, "You resume your attempt at %s.", action);

            one_occupation_turn(picklock, "picking the lock", occ_lock);
            return 1;
        }
    }

    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You can't hold %s -- you have no hands!",
              doname(pick));
        return 0;
    }

    if ((picktyp != LOCK_PICK && picktyp != CREDIT_CARD &&
         picktyp != SKELETON_KEY)) {
        impossible("picking lock with object %d?", picktyp);
        return 0;
    }

    if (!dx && !dy) { /* pick lock on a container */
        const char *verb;
        boolean it;
        int count;

        if (dz < 0) {
            pline(msgc_cancelled, "There isn't any sort of lock up %s.",
                  Levitation ? "here" : "there");
            return 0;
        } else if (is_lava(level, u.ux, u.uy)) {
            pline(msgc_cancelled, "Doing that would probably melt your %s.",
                  xname(pick));
            return 0;
        } else if (is_pool(level, u.ux, u.uy) && !Underwater) {
            /* better YAFM - AIS */
            pline(msgc_cancelled,
                  "Canals might have locks, but this water doesn't.");
            return 0;
        }

        count = 0;
        c = 'n';        /* in case there are no boxes here */
        for (otmp = level->objects[cc.x][cc.y]; otmp; otmp = otmp->nexthere)
            if (Is_box(otmp)) {
                ++count;
                if (!can_reach_floor()) {
                    pline(msgc_cancelled, "You can't reach %s from up here.",
                          the(xname(otmp)));
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
                qbuf = msgprintf(
                    "There is %s here, %s %s?",
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
                    pline(msgc_cancelled,
                          "You can't fix its broken lock with %s.",
                          doname(pick));
                    return 0;
                } else if (picktyp == CREDIT_CARD && !otmp->olocked) {
                    /* credit cards are only good for unlocking */
                    pline(msgc_cancelled, "You can't do that with %s.",
                          doname(pick));
                    return 0;
                }

                u.utracked[tos_lock] = otmp;
                u.uoccupation_progress[tos_lock] = 0;
                break;
            }
        if (c != 'y') {
            if (!count)
                pline(msgc_cancelled,
                      "There doesn't seem to be any sort of lock here.");
            return 0;   /* decided against all boxes */
        }
    } else {    /* pick the lock in a door */
        struct monst *mtmp;

        if (u.utrap && u.utraptype == TT_PIT) {
            pline(msgc_cancelled,
                  "You can't reach over the edge of the pit.");
            return 0;
        }

        door = &level->locations[cc.x][cc.y];
        if ((mtmp = m_at(level, cc.x, cc.y)) && canseemon(mtmp)) {
            if (picktyp == CREDIT_CARD &&
                (mtmp->isshk || mtmp->data == &mons[PM_ORACLE]))
                verbalize(msgc_npcvoice, "No checks, no credit, no problem.");
            else
                pline(msgc_mispaste, "I don't think %s would appreciate that.",
                      mon_nam(mtmp));
            return 0;
        }
        if (mtmp && (mtmp->m_ap_type == M_AP_FURNITURE) &&
            (mtmp->mappearance == S_hcdoor || mtmp->mappearance == S_vcdoor) &&
            !Protection_from_shape_changers) {
            stumble_onto_mimic(mtmp, dx, dy);
            return 1;
        }
        if (!IS_DOOR(door->typ)) {
            if (is_drawbridge_wall(cc.x, cc.y) >= 0)
                pline(msgc_cancelled, "You %s no lock on the drawbridge.",
                      Blind ? "feel" : "see");
            else
                pline(msgc_mispaste, "You %s no door there.",
                      Blind ? "feel" : "see");
            return 0;
        }
        switch (door->doormask) {
        case D_NODOOR:
            pline(msgc_cancelled, "This doorway has no door.");
            return 0;
        case D_ISOPEN:
            pline(msgc_cancelled, "You cannot lock an open door.");
            return 0;
        case D_BROKEN:
            pline(msgc_cancelled, "This door is broken.");
            return 0;
        default:
            /* credit cards are only good for unlocking */
            if (picktyp == CREDIT_CARD && !(door->doormask & D_LOCKED)) {
                pline(msgc_cancelled,
                      "You can't lock a door with a credit card.");
                return 0;
            }

            /* At this point, the player knows that the door is a door, and
               whether it's locked, but not whether it's trapped; to do this,
               we set the mem_door_l flag and call map_background, which will
               clear it if necessary (i.e. not a door after all). */
            level->locations[cc.x][cc.y].mem_door_l = 1;
            map_background(cc.x, cc.y, TRUE);

            u.utracked[tos_lock] = &zeroobj;
            u.utracked_location[tl_lock] = cc;
            u.uoccupation_progress[tos_lock] = 0;
        }
    }

    one_occupation_turn(picklock, "picking the lock", occ_lock);
    return 1;
}

/* try to force a chest with your weapon */
int
doforce(const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    int c;
    const char *qbuf;

    if (!uwep_can_force())
        return 0;

    if (u.utracked[tos_lock] && u.uoccupation_progress[tos_lock]) {
        if (turnstate.continue_message)
            pline(msgc_occstart, "You resume your attempt to force the lock.");
        one_occupation_turn(forcelock, "forcing the lock", occ_lock);
        return 1;
    }

    /* A lock is made only for the honest man, the thief will break it. */
    u.utracked[tos_lock] = NULL;
    u.uoccupation_progress[tos_lock] = 0;
    for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere)
        if (Is_box(otmp)) {
            if (otmp->obroken || !otmp->olocked) {
                pline(msgc_cancelled,
                      "There is %s here, but its lock is already %s.",
                      doname(otmp), otmp->obroken ? "broken" : "unlocked");
                continue;
            }
            qbuf = msgprintf(
                "There is %s here, force its lock?",
                safe_qbuf("", sizeof ("There is  here, force its lock?"),
                          doname(otmp), an(simple_typename(otmp->otyp)),
                          "a box"));

            c = ynq(qbuf);
            if (c == 'q')
                return 0;
            if (c == 'n')
                continue;

            if (is_blade(uwep))
                pline(msgc_occstart,
                      "You force your %s into a crack and pry.", xname(uwep));
            else
                pline(msgc_occstart,
                      "You start bashing it with your %s.", xname(uwep));
            u.utracked[tos_lock] = otmp;
            break;
        }

    if (u.utracked[tos_lock]) {
        one_occupation_turn(forcelock, "forcing the lock", occ_lock);
        return 1;
    } else {
        pline(msgc_cancelled, "You decide not to force the issue.");
        return 0;
    }
}

/* try to open a door */
int
doopen(const struct nh_cmd_arg *arg)
{
    coord cc;
    struct rm *door;
    struct monst *mtmp;
    schar dx, dy, dz;

    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You can't open, close, or unlock anything "
              "-- you have no hands!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        pline(msgc_cancelled, "You can't reach over the edge of the pit.");
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
            pline(msgc_cancelled,
                  "There is no obvious way to open the drawbridge.");
            return 0;
        }
        pline(msgc_mispaste, "You %s no door there.", Blind ? "feel" : "see");
        return 0;
    }

    if (door->doormask == D_ISOPEN) {
        struct nh_cmd_arg newarg;
        arg_from_delta(dx, dy, dz, &newarg);
        return doclose(&newarg);
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
            if (last_command_was("open") && door->mem_door_l) {

                /* With the "open" command given explicitly (rather than
                   implicitly via doorbumping), unlock the door. */
                struct obj *bestpick = get_current_unlock_tool();
                struct nh_cmd_arg newarg;

                arg_from_delta(dx, dy, dz, &newarg);
                if (!bestpick)
                    pline(msgc_cancelled,
                          "You have nothing to unlock that with.");
                else if (!bestpick->lastused)
                    /* not msgc_controlhelp, or many players would get
                       no message */
                    pline(msgc_hint,
                          "Use an unlocking tool manually so I know "
                          "which one you want to use.");
                else
                    return pick_lock(bestpick, &newarg);
            }
            door->mem_door_l = 1;
            map_background(cc.x, cc.y, TRUE);
            mesg = " is locked";
            break;
        }
        pline(msgc_cancelled, "This door%s.", mesg);
        if (Blind)
            feel_location(cc.x, cc.y);
        return 0;
    }

    if (verysmall(youmonst.data)) {
        pline(msgc_cancelled, "You're too small to pull the door open.");
        return 0;
    }

    /* door is known to be CLOSED */
    if (rnl(20) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
        pline(msgc_actionok, "The door opens.");
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
        pline(msgc_failrandom, "The door resists!");
    }

    return 1;
}

static boolean
obstructed(int x, int y, enum msg_channel msgc)
{
    struct monst *mtmp = m_at(level, x, y);

    if (mtmp && mtmp->m_ap_type != M_AP_FURNITURE) {
        if (mtmp->m_ap_type == M_AP_OBJECT)
            goto objhere;
        reveal_monster_at(x, y, TRUE);
        pline(msgc, "%s stands in the way!",
              !canspotmon(mtmp) ? "Some creature" : Monnam(mtmp));
        return TRUE;
    }
    if (OBJ_AT(x, y)) {
    objhere:
        pline(msgc, "Something's in the way.");
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
        pline(msgc_cancelled, "You can't close anything -- you have no hands!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        pline(msgc_cancelled, "You can't reach over the edge of the pit.");
        return 0;
    }

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    cc.x = u.ux + dx;
    cc.y = u.uy + dy;
    if (!isok(cc.x, cc.y))
        return 0;

    if ((cc.x == u.ux) && (cc.y == u.uy)) {
        pline(msgc_cancelled1, "You are in the way!");
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
            pline(msgc_cancelled,
                  "There is no obvious way to close the drawbridge.");
        else
            pline(msgc_mispaste, "You %s no door there.",
                  Blind ? "feel" : "see");
        return 0;
    }

    if (door->doormask == D_NODOOR) {
        pline(msgc_cancelled, "This doorway has no door.");
        return 0;
    }

    if (obstructed(cc.x, cc.y, msgc_cancelled))
        return 0;

    if (door->doormask == D_BROKEN) {
        pline(msgc_cancelled, "This door is broken.");
        return 0;
    }

    if (door->doormask & (D_CLOSED | D_LOCKED)) {
        pline(msgc_cancelled, "This door is already closed.");
        return 0;
    }

    if (door->doormask == D_ISOPEN) {
        if (verysmall(youmonst.data) && !u.usteed) {
            pline(msgc_cancelled, "You're too small to push the door closed.");
            return 0;
        }
        if (u.usteed ||
            rn2(25) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
            pline(msgc_actionok, "The door closes.");
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
                feel_location(cc.x, cc.y);   /* the hero knows she closed it */
            else
                newsym(cc.x, cc.y);
            block_point(cc.x, cc.y);    /* vision: no longer see there */
        } else {
            exercise(A_STR, TRUE);
            pline(msgc_failrandom, "The door resists!");
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
            pline(msgc_actionok, "Klunk!");
            obj->olocked = 1;
            obj->obroken = 0;
            res = 1;
        }       /* else already closed and locked */
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        if (obj->olocked) {     /* unlock; couldn't be broken */
            pline(msgc_actionok, "Klick!");
            obj->olocked = 0;
            res = 1;
        } else  /* silently fix if broken */
            obj->obroken = 0;
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
                pline(msgc_youdiscover, "A door appears in the wall!");
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
                pline(msgc_actionok,
                      "%s springs up in the older, more primitive doorway.",
                      dustcloud);
            else
                You_hear(msgc_actionok, "a swoosh.");
            if (obstructed(x, y, msgc_yafm)) {
                if (vis)
                    pline(msgc_yafm, "The cloud %s.", quickly_dissipates);
                return FALSE;
            }
            block_point(x, y);
            door->typ = SDOOR;
            if (vis)
                pline(msgc_actionok, "The doorway vanishes!");
            newsym(x, y);
            return TRUE;
        }
        if (obstructed(x, y, msgc_yafm))
            return FALSE;
        /* Don't allow doors to close over traps.  This is for pits */
        /* & trap doors, but is it ever OK for anything else? */
        if (t_at(level, x, y)) {
            /* maketrap() clears doormask, so it should be NODOOR */
            pline(msgc_yafm, "%s springs up in the doorway, but %s.",
                  dustcloud, quickly_dissipates);
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
                else {
                    if (cansee(x, y))
                        pline(msgc_substitute,
                              "KABOOM!!  You see a door explode.");
                    else
                        You_hear(msgc_levelsound, "a distant explosion.");
                }
                door->doormask = D_NODOOR;
                unblock_point(x, y);
                newsym(x, y);
                loudness = 40;
                break;
            }
            door->doormask = D_BROKEN;
            if (cansee(x, y))
                pline(msgc_actionok, "The door crashes open!");
            else
                You_hear(msgc_levelsound, "a crashing sound.");
            unblock_point(x, y);
            newsym(x, y);
            /* force vision recalc before printing more messages */
            if (turnstate.vision_full_recalc)
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
        pline(msgc_actionok, "%s", msg);
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

    return res;
}

static void
chest_shatter_msg(struct obj *otmp)
{
    const char *disposition;
    const char *thing;
    long save_Blinded;

    if (otmp->oclass == POTION_CLASS) {
        pline(msgc_itemloss, "You %s %s shatter!", Blind ? "hear" : "see",
              an(bottlename()));
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
    pline(msgc_itemloss, "%s %s!", An(thing), disposition);
}

/*lock.c*/

