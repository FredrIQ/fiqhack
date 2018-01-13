/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-13 */
/* Copyright (c) Kevin Hugo, 1998-1999. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Monsters that might be ridden */
static const char steeds[] = {
    S_QUADRUPED, S_UNICORN, S_ANGEL, S_CENTAUR, S_DRAGON, S_JABBERWOCK, '\0'
};

static boolean mount_steed(struct monst *, boolean);
static boolean landing_spot(coord *, int, int);


/* caller has decided that hero can't reach something while mounted */
void
rider_cant_reach(void)
{
    pline(msgc_cancelled, "You aren't skilled enough to reach from %s.",
          y_monnam(u.usteed));
}

/*** Putting the saddle on ***/

/* Can this monster wear a saddle? */
boolean
can_saddle(struct monst *mtmp)
{
    const struct permonst *ptr = mtmp->data;

    return (strchr(steeds, ptr->mlet) && (ptr->msize >= MZ_MEDIUM) &&
            (!humanoid(ptr) || ptr->mlet == S_CENTAUR) && !amorphous(ptr) &&
            !noncorporeal(ptr) && !is_whirly(ptr) && !unsolid(ptr));
}


int
use_saddle(struct obj *otmp, const struct nh_cmd_arg *arg)
{
    struct monst *mtmp;
    const struct permonst *ptr;
    int chance;
    const char *s;
    schar dx, dy, dz;

    /* Can you use it? */
    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You have no hands!"); /* not `body_part(HAND)' */
        return 0;
    } else if (!freehand()) {
        pline(msgc_cancelled, "You have no free %s.", body_part(HAND));
        return 0;
    }

    /* Select an animal */
    if (Engulfed || Underwater || !getargdir(arg, NULL, &dx, &dy, &dz)) {
        pline(msgc_cancelled, "Never mind.");
        return 0;
    }
    if (!dx && !dy) {
        pline(msgc_cancelled, "Saddle yourself?  Very funny...");
        return 0;
    }
    if (!isok(u.ux + dx, u.uy + dy) ||
        !((mtmp = m_at(level, u.ux + dx, u.uy + dy))) || !canspotmon(mtmp)) {
        if (knownwormtail(u.ux + dx, u.uy + dy))
            pline(msgc_cancelled, "It's hard to strap a saddle to a tail.");
        else
            pline(msgc_cancelled, "I see nobody there.");
        return 0;
    }

    /* Is this a valid monster? */
    if (mtmp->misc_worn_check & W_MASK(os_saddle) ||
        which_armor(mtmp, os_saddle)) {
        pline(msgc_cancelled, "%s doesn't need another one.", Monnam(mtmp));
        return 0;
    }
    ptr = mtmp->data;
    if (!uarmg && touched_monster(ptr - mons)) {
        pline(msgc_fatal_predone, "You touch %s.", mon_nam(mtmp));
        instapetrify(killer_msg(STONING, msgcat("attempting to saddle ",
                                                an(pm_name(mtmp)))));
    }
    if (ptr == &mons[PM_INCUBUS]) {
        pline(msgc_yafm, "Shame on you!");
        exercise(A_WIS, FALSE);
        return 1;
    }
    if (mx_epri(mtmp) || mx_eshk(mtmp) || mx_egd(mtmp) || mtmp->iswiz) {
        pline(msgc_cancelled, "I think %s would mind.", mon_nam(mtmp));
        return 0;
    }
    if (!can_saddle(mtmp)) {
        pline(msgc_cancelled, "You can't saddle such a creature.");
        return 0;
    }

    /* Calculate your chance */
    chance = ACURR(A_DEX) + ACURR(A_CHA) / 2 + 2 * mtmp->mtame;
    chance += u.ulevel * (mtmp->mtame ? 20 : 5);
    if (!mtmp->mtame)
        chance -= 10 * mtmp->m_lev;
    if (Role_if(PM_KNIGHT))
        chance += 20;
    switch (P_SKILL(P_RIDING)) {
    case P_ISRESTRICTED:
    case P_UNSKILLED:
    default:
        chance -= 20;
        break;
    case P_BASIC:
        break;
    case P_SKILLED:
        chance += 15;
        break;
    case P_EXPERT:
        chance += 30;
        break;
    }
    if (confused(&youmonst) || fumbling(&youmonst) ||
        slippery_fingers(&youmonst))
        chance -= 20;
    else if (uarmg && (s = OBJ_DESCR(objects[uarmg->otyp])) != NULL &&
             !strncmp(s, "riding ", 7))
        /* Bonus for wearing "riding" (but not fumbling) gloves */
        chance += 10;
    else if (uarmf && (s = OBJ_DESCR(objects[uarmf->otyp])) != NULL &&
             !strncmp(s, "riding ", 7))
        /* ... or for "riding boots" */
        chance += 10;
    if (otmp->cursed)
        chance -= 50;

    /* Make the attempt */
    if (rn2(100) < chance) {
        pline(msgc_actionok, "You put the saddle on %s.", mon_nam(mtmp));
        if (otmp->owornmask)
            remove_worn_item(otmp, FALSE);
        freeinv(otmp);
        /* mpickobj may free otmp it if merges, but we have already checked for
           a saddle above, so no merger should happen */
        mpickobj(mtmp, otmp, NULL);
        mtmp->misc_worn_check |= W_MASK(os_saddle);
        otmp->owornmask = W_MASK(os_saddle);
        otmp->leashmon = mtmp->m_id;
    } else
        pline(msgc_failrandom, "%s resists!", Monnam(mtmp));
    return 1;
}


/*** Riding the monster ***/

/* Can we ride this monster?  Caller should also check can_saddle() */
boolean
can_ride(struct monst * mtmp)
{
    return (mtmp->mtame && humanoid(youmonst.data) && !verysmall(youmonst.data)
            && !bigmonst(youmonst.data) && (!Underwater || swims(mtmp) ||
            unbreathing(mtmp)));
}


int
doride(const struct nh_cmd_arg *arg)
{
    boolean forcemount = FALSE;
    schar dx, dy, dz;

    if (u.usteed)
        dismount_steed(DISMOUNT_BYCHOICE);
    else if (getargdir(arg, NULL, &dx, &dy, &dz) &&
             isok(u.ux + dx, u.uy + dy)) {
        if (wizard && yn("Force the mount to succeed?") == 'y')
            forcemount = TRUE;
        return mount_steed(m_at(level, u.ux + dx, u.uy + dy), forcemount);
    } else
        return 0;
    return 1;
}


/* Start riding, with the given monster */
boolean
mount_steed(struct monst * mtmp,        /* The animal */
            boolean force)              /* quiet, and override some failures */
{
    struct obj *otmp;
    const struct permonst *ptr;

    /* Sanity checks */
    if (u.usteed) {
        pline(msgc_cancelled, "You are already riding %s.", mon_nam(u.usteed));
        return FALSE;
    }

    /* Is the player in the right form? */
    if (Hallucination && !force) {
        pline(msgc_cancelled, "Maybe you should find a designated driver.");
        return FALSE;
    }

    if (Upolyd &&
        (!humanoid(youmonst.data) || verysmall(youmonst.data) ||
         bigmonst(youmonst.data) || slithy(youmonst.data))) {
        pline(msgc_cancelled, "You won't fit on a saddle.");
        return FALSE;
    }
    if (!force && (near_capacity() > SLT_ENCUMBER)) {
        pline(msgc_cancelled,
              "You can't do that while carrying so much stuff.");
        return FALSE;
    }

    /* Can the player reach and see the monster? */
    if (!mtmp ||
        (!force &&
         ((Blind && !Blind_telepat) || mtmp->mundetected ||
          mtmp->m_ap_type == M_AP_FURNITURE ||
          mtmp->m_ap_type == M_AP_OBJECT))) {
        pline(msgc_cancelled, "I see nobody there.");
        return FALSE;
    }

    struct test_move_cache cache;
    init_test_move_cache(&cache);

    if (Engulfed || u.ustuck || u.utrap || Punished ||
        !test_move(u.ux, u.uy, mtmp->mx - u.ux, mtmp->my - u.uy, 0,
                   TEST_MOVE, &cache)) {
        if (Punished || !(Engulfed || u.ustuck || u.utrap))
            pline(msgc_cancelled, "You are unable to swing your %s over.",
                  body_part(LEG));
        else
            pline(msgc_cancelled, "You are stuck here for now.");
        return FALSE;
    }

    /* Is this a valid monster? */
    otmp = which_armor(mtmp, os_saddle);
    if (!otmp) {
        pline(msgc_cancelled, "%s is not saddled.", Monnam(mtmp));
        return FALSE;
    }
    ptr = mtmp->data;
    if (touch_petrifies(ptr) && !Stone_resistance) {
        pline(msgc_cancelled, "You touch %s.", mon_nam(mtmp));
        instapetrify(killer_msg(STONING,
                                msgcat("attempting to ride ", an(pm_name(mtmp)))));
    }
    if (!mtmp->mtame || isminion(mtmp)) {
        pline(msgc_cancelled, "I think %s would mind.", mon_nam(mtmp));
        return FALSE;
    }
    if (mtmp->mtrapped) {
        struct trap *t = t_at(level, mtmp->mx, mtmp->my);

        pline(msgc_cancelled, "You can't mount %s while %s's trapped in %s.",
              mon_nam(mtmp), mhe(mtmp), an(trapexplain[t->ttyp - 1]));
        return FALSE;
    }

    /* TODO: this assumes that the attempt would otherwise succeed; also it
       doesn't take time and accomplishes something (sort-of the reverse of a
       msgc_cancelled1) */
    if (!force && !Role_if(PM_KNIGHT) && !(--mtmp->mtame)) {
        /* no longer tame */
        newsym(mtmp->mx, mtmp->my);
        pline(msgc_petfatal, "%s resists%s!", Monnam(mtmp),
              mtmp->mleashed ? " and its leash comes off" : "");
        if (mtmp->mleashed)
            m_unleash(mtmp, FALSE);
        return FALSE;
    }
    if (!force && Underwater && (!swims(mtmp) || !unbreathing(mtmp))) {
        pline(msgc_cancelled,
              "You can't ride that creature while under water.");
        return FALSE;
    }
    if (!can_saddle(mtmp) || !can_ride(mtmp)) {
        pline(msgc_cancelled, "You can't ride such a creature.");
        return 0;
    }

    /* Is the player impaired? */
    if (!force && !levitates(mtmp) && !flying(mtmp) && levitates(&youmonst) &&
        !Lev_at_will) {
        pline(msgc_cancelled, "You cannot reach %s.", mon_nam(mtmp));
        return FALSE;
    }
    if (!force && uarm && is_metallic(uarm) && greatest_erosion(uarm)) {
        pline(msgc_cancelled,
              "Your %s armor is too stiff to be able to mount %s.",
              uarm->oeroded ? "rusty" : "corroded", mon_nam(mtmp));
        return FALSE;
    }
    if (!force &&
        (confused(&youmonst) || fumbling(&youmonst) ||
         slippery_fingers(&youmonst) || leg_hurt(&youmonst) ||
         otmp->cursed ||
         (u.ulevel + mtmp->mtame < rnd(MAXULEV / 2 + 5)))) {
        if (levitates(&youmonst)) {
            pline(msgc_failrandom, "%s slips away from you.", Monnam(mtmp));
            return FALSE;
        }
        pline(msgc_substitute, "You slip while trying to get on %s.",
              mon_nam(mtmp));

        const char *buf = msgcat(
            "slipped while mounting ",
            /* "a saddled mumak" or "a saddled pony called Dobbin" */
            x_monnam(mtmp, ARTICLE_A, NULL,
                     SUPPRESS_IT | SUPPRESS_INVISIBLE |
                     SUPPRESS_HALLUCINATION | SUPPRESS_ENSLAVEMENT,
                     TRUE));
        losehp(rn1(5, 10), buf);
        return FALSE;
    }

    /* Success */
    if (!force) {
        if (levitates(&youmonst) && !levitates(mtmp) && !flying(mtmp))
            /* Must have Lev_at_will at this point */
            pline_implied(msgc_consequence, "%s magically floats up!",
                          Monnam(mtmp));
        pline(msgc_actionok, "You mount %s.", mon_nam(mtmp));
    }
    /* setuwep handles polearms differently when you're mounted */
    if (uwep && is_pole(uwep))
        u.bashmsg = FALSE;
    u.usteed = mtmp;
    remove_monster(level, mtmp->mx, mtmp->my);
    teleds(mtmp->mx, mtmp->my, TRUE);
    mtmp->mux = COLNO;
    mtmp->muy = ROWNO;
    return TRUE;
}


/* You and your steed have moved */
void
exercise_steed(void)
{
    if (!u.usteed)
        return;

    /* It takes many turns of riding to exercise skill */
    if (u.urideturns++ >= 100) {
        u.urideturns = 0;
        use_skill(P_RIDING, 1);
    }
    return;
}


/* The player kicks or whips the steed */
void
kick_steed(void)
{
    /* TODO: convert to use msgupcasefirst */
    char He[4];

    if (!u.usteed)
        return;

    /* [ALI] Various effects of kicking sleeping/paralyzed steeds */
    if (u.usteed->msleeping || !u.usteed->mcanmove) {
        /* We assume a message has just been output of the form "You kick
           <steed>." */
        strcpy(He, mhe(u.usteed));
        *He = highc(*He);
        if ((u.usteed->mcanmove || u.usteed->mfrozen) && !rn2(2)) {
            if (u.usteed->mcanmove)
                u.usteed->msleeping = 0;
            else if (u.usteed->mfrozen > 2)
                u.usteed->mfrozen -= 2;
            else {
                u.usteed->mfrozen = 0;
                u.usteed->mcanmove = 1;
            }
            if (u.usteed->msleeping || !u.usteed->mcanmove)
                pline(msgc_actionok, "%s stirs.", He);
            else
                pline(msgc_actionok, "%s rouses %sself!",
                      He, mhim(u.usteed));
        } else
            pline(msgc_failrandom, "%s does not respond.", He);
        return;
    }

    /* Make the steed less tame and check if it resists */
    if (u.usteed->mtame)
        u.usteed->mtame--;
    if (!u.usteed->mtame && u.usteed->mleashed)
        m_unleash(u.usteed, TRUE);
    if (!u.usteed->mtame ||
        (u.ulevel + u.usteed->mtame < rnd(MAXULEV / 2 + 5))) {
        newsym(u.usteed->mx, u.usteed->my);
        dismount_steed(DISMOUNT_THROWN);
        return;
    }

    pline(msgc_actionok, "%s gallops!", Monnam(u.usteed));
    u.ugallop += rn1(20, 30);
    return;
}

/*
 * Try to find a dismount point adjacent to the steed's location.
 * If all else fails, try enexto().  Use enexto() as a last resort because
 * enexto() chooses its point randomly, possibly even outside the
 * room's walls, which is not what we want.
 * Adapted from mail daemon code.
 */
static boolean
landing_spot(coord * spot,      /* landing position (we fill it in) */
             int reason, int forceit)
{
    int i = 0, x, y, distance, min_distance = -1;
    boolean found = FALSE;
    struct trap *t;

    /* avoid known traps (i == 0) and boulders, but allow them as a backup */
    if (reason != DISMOUNT_BYCHOICE || Stunned || Confusion || Fumbling)
        i = 1;
    for (; !found && i < 2; ++i) {
        for (x = u.ux - 1; x <= u.ux + 1; x++)
            for (y = u.uy - 1; y <= u.uy + 1; y++) {
                if (!isok(x, y) || (x == u.ux && y == u.uy))
                    continue;

                if (ACCESSIBLE(level->locations[x][y].typ) &&
                    !MON_AT(level, x, y) && !closed_door(level, x, y)) {
                    distance = distu(x, y);
                    if (min_distance < 0 || distance < min_distance ||
                        (distance == min_distance && rn2(2))) {
                        if (i > 0 ||
                            (((t = t_at(level, x, y)) == 0 || !t->tseen) &&
                             (!sobj_at(BOULDER, level, x, y) ||
                              throws_rocks(youmonst.data)))) {
                            spot->x = x;
                            spot->y = y;
                            min_distance = distance;
                            found = TRUE;
                        }
                    }
                }
            }
    }

    /* If we didn't find a good spot and forceit is on, try enexto(). */
    if (forceit && min_distance < 0 &&
        !enexto(spot, level, u.ux, u.uy, youmonst.data))
        return FALSE;

    return found;
}

/* Stop riding the current steed */
void
dismount_steed(int reason)
{
    struct monst *mtmp;
    struct obj *otmp;
    coord cc;
    const char *verb = "fall";
    unsigned save_utrap = u.utrap;
    boolean have_spot = landing_spot(&cc, reason, 0);

    mtmp = u.usteed;    /* make a copy of steed pointer */
    /* Sanity check */
    if (!mtmp)  /* Just return silently */
        return;

    /* Check the reason for dismounting */
    otmp = which_armor(mtmp, os_saddle);
    switch (reason) {
    case DISMOUNT_THROWN:
        verb = "are thrown";
    case DISMOUNT_FELL:
        pline(msgc_statusbad, "You %s off of %s!", verb, mon_nam(mtmp));
        if (!have_spot)
            have_spot = landing_spot(&cc, reason, 1);
        losehp(rn1(10, 10), "killed in a riding accident");
        set_wounded_legs(&youmonst, LEFT_SIDE, rn1(5, 5));
        set_wounded_legs(&youmonst, RIGHT_SIDE, rn1(5, 5));
        break;
    case DISMOUNT_POLY:
        pline(msgc_statusbad, "You can no longer ride %s.",
              mon_nam(u.usteed));
        if (!have_spot)
            have_spot = landing_spot(&cc, reason, 1);
        break;
    case DISMOUNT_ENGULFED:
        /* caller displays message */
        break;
    case DISMOUNT_BONES:
        /* hero has just died... */
        break;
    case DISMOUNT_GENERIC:
        /* no messages, just make it so */
        break;
    case DISMOUNT_BYCHOICE:
    default:
        if (otmp && otmp->cursed) {
            pline(otmp->bknown ? msgc_cancelled1 : msgc_failcurse,
                  "You can't.  The saddle %s cursed.",
                  otmp->bknown ? "is" : "seems to be");
            otmp->bknown = TRUE;
            return;
        }
        if (!have_spot) {
            pline(msgc_yafm,
                  "You can't. There isn't anywhere for you to stand.");
            return;
        }
        if (!mx_name(mtmp)) {
            pline(msgc_actionok,
                  "You've been through the dungeon on %s with no name.",
                  an(pm_name(mtmp)));
            if (Hallucination)
                pline_implied(msgc_actionok,
                              "It felt good to get out of the rain.");
        } else
            pline(msgc_actionok, "You dismount %s.", mon_nam(mtmp));
    }

    /* Release the steed and saddle */
    u.usteed = 0;
    u.ugallop = 0L;

    /* Set player and steed's position.  Try moving the player first unless
       we're in the midst of creating a bones file. */
    if (reason == DISMOUNT_BONES) {
        /* move the steed to an adjacent square */
        if (enexto(&cc, level, u.ux, u.uy, mtmp->data))
            rloc_to(mtmp, cc.x, cc.y);
        else    /* evidently no room nearby; move steed elsewhere */
            rloc(mtmp, FALSE);
        return;
    }
    if (!DEADMONSTER(mtmp)) {
        place_monster(mtmp, u.ux, u.uy, TRUE);
        if (!Engulfed && !u.ustuck && have_spot) {
            const struct permonst *mdat = mtmp->data;

            /* The steed may drop into water/lava */
            if (!levitates(mtmp) && !flying(mtmp) && !is_clinger(mdat)) {
                if (is_pool(level, u.ux, u.uy)) {
                    boolean fatal = (!swims(mtmp) && !unbreathing(mtmp));
                    if (!Underwater && !waterwalks(mtmp))
                        pline(fatal ? msgc_petfatal : msgc_petwarning,
                              "%s falls into the %s!", Monnam(mtmp),
                              surface(u.ux, u.uy));
                    if (fatal && !waterwalks(mtmp)) {
                        killed(mtmp);
                        adjalign(-1);
                    }
                } else if (is_lava(level, u.ux, u.uy)) {
                    struct obj *armf = which_armor(mtmp, os_armf);
                    if (armf && !armf->oerodeproof) {
                        pline(msgc_petfatal,
                              "%s burst into flame!", xname(armf));
                        mtmp->misc_worn_check &= ~(armf->owornmask);
                        m_useup(mtmp, armf);
                    }

                    if (!waterwalks(mtmp)) {
                        pline(likes_lava(mdat) ? msgc_petwarning : msgc_petfatal,
                              "%s is pulled into the lava!", Monnam(mtmp));
                        if (!likes_lava(mdat)) {
                            killed(mtmp);
                            adjalign(-1);
                        }
                    }
                }
            }
            /* Steed dismounting consists of two steps: being moved to another
               square, and descending to the floor.  We have functions to do
               each of these activities, but they're normally called
               individually and include an attempt to look at or pick up the
               objects on the floor: teleds() --> spoteffects() --> pickup()
               float_down() --> pickup() We use this kludge to make sure there
               is only one such attempt. Clearly this is not the best way to do
               it.  A full fix would involve having these functions not call
               pickup() at all, instead calling them first and calling pickup()
               afterwards.  But it would take a lot of work to keep this change
               from having any unforseen side effects (for instance, you would
               no longer be able to walk onto a square with a hole, and
               autopickup before falling into the hole). */
            /* [ALI] No need to move the player if the steed died. */
            if (!DEADMONSTER(mtmp)) {
                /* Keep steed here, move the player to cc; teleds() clears
                   u.utrap */
                in_steed_dismounting = TRUE;
                teleds(cc.x, cc.y, TRUE);
                in_steed_dismounting = FALSE;

                /* Put your steed in your trap */
                if (save_utrap)
                    mintrap(mtmp);
            }
            /* Couldn't... try placing the steed */
        } else if (enexto(&cc, level, u.ux, u.uy, mtmp->data)) {
            /* Keep player here, move the steed to cc */
            rloc_to(mtmp, cc.x, cc.y);
            /* Player stays put */
            /* Otherwise, kill the steed */
        } else {
            killed(mtmp);
            adjalign(-1);
        }
    }

    /* Return the player to the floor */
    if (reason != DISMOUNT_ENGULFED) {
        in_steed_dismounting = TRUE;
        int oldcap = near_capacity();
        set_property(&youmonst, LEVITATION, -2, FALSE);
        in_steed_dismounting = FALSE;
        encumber_msg(oldcap);
        turnstate.vision_full_recalc = TRUE;
    } else
    /* polearms behave differently when not mounted */
    if (uwep && is_pole(uwep))
        u.bashmsg = TRUE;
    return;
}

/*steed.c*/
