/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-23 */
/* Copyright Scott R. Turner, srt@ucla, 10/27/86                  */
/* NetHack may be freely redistributed.  See license for details. */

/* Code for drinking from fountains. */

#include "hack.h"

static void dowatersnakes(void);
static void dowaterdemon(void);
static void dowaternymph(void);
static void gush(int, int, void *);
static void dofindgem(void);

void
floating_above(const char *what)
{
    pline("You are floating high above the %s.", what);
}

static void
dowatersnakes(void)
{       /* Fountain of snakes! */
    int num = 2 + rn2_on_rng(5, rng_fountain_result);
    struct monst *mtmp;

    if (!(mvitals[PM_WATER_MOCCASIN].mvflags & G_GONE)) {
        if (!Blind)
            pline("An endless stream of %s pours forth!",
                  Hallucination ? makeplural(monnam_for_index(rndmonidx())) :
                  "snakes");
        else
            You_hear("something hissing!");
        while (num-- > 0)
            if ((mtmp =
                 makemon(&mons[PM_WATER_MOCCASIN], level, u.ux, u.uy,
                         NO_MM_FLAGS)) && t_at(level, mtmp->mx, mtmp->my))
                mintrap(mtmp);
    } else
        pline("The fountain bubbles furiously for a moment, then calms.");
}

/* Quantizes the probabilities of wishes, so that wish sources work consistently
   across games with the same starting seed. Handles the range 0-22% (although
   0% and 1% never give wishes), and 80%. The die roll is also available, e.g.
   to get consistent results from failed lamp wishes. */
boolean
wish_available(int percentchance, int *dieroll)
{
    int unused;
    if (!dieroll)
        dieroll = &unused;

    switch (percentchance) {
    case 0: case 1:
        *dieroll = rn2(100);
        return FALSE;
    case 2: case 3: case 4: case 5: case 6: case 7:
        return ((*dieroll = rn2_on_rng(100, rng_wish_5))) < 5;
    case 8: case 9: case 10: case 11: case 12:
        return ((*dieroll = rn2_on_rng(100, rng_wish_10))) < 10;
    case 13: case 14: case 15: case 16: case 17:
        return ((*dieroll = rn2_on_rng(100, rng_wish_15))) < 15;
    case 18: case 19: case 20: case 21: case 22:
        return ((*dieroll = rn2_on_rng(100, rng_wish_20))) < 20;
    case 78: case 79: case 80: case 81: case 82:
        return ((*dieroll = rn2_on_rng(100, rng_wish_80))) < 80;
    default:
        impossible("Unexpected wish probability %d%%", percentchance);
        return ((*dieroll = rn2(100))) < percentchance;
    }
}

static void
dowaterdemon(void)
{       /* Water demon */
    struct monst *mtmp;

    if (!(mvitals[PM_WATER_DEMON].mvflags & G_GONE)) {
        if ((mtmp =
             makemon(&mons[PM_WATER_DEMON], level, u.ux, u.uy, NO_MM_FLAGS))) {
            if (!Blind)
                pline("You unleash %s!", a_monnam(mtmp));
            else
                pline("You feel the presence of evil.");

            /* A slightly better chance of a wish at early levels */
            if (wish_available(20 - level_difficulty(&u.uz), NULL)) {
                pline("Grateful for %s release, %s grants you a wish!",
                      mhis(mtmp), mhe(mtmp));
                makewish();
                mongone(mtmp);
            } else if (t_at(level, mtmp->mx, mtmp->my))
                mintrap(mtmp);
        }
    } else
        pline("The fountain bubbles furiously for a moment, then calms.");
}


static void
dowaternymph(void)
{       /* Water Nymph */
    struct monst *mtmp;

    if (!(mvitals[PM_WATER_NYMPH].mvflags & G_GONE) &&
        (mtmp =
         makemon(&mons[PM_WATER_NYMPH], level, u.ux, u.uy, NO_MM_FLAGS))) {
        if (!Blind)
            pline("You attract %s!", a_monnam(mtmp));
        else
            You_hear("a seductive voice.");
        mtmp->msleeping = 0;
        if (t_at(level, mtmp->mx, mtmp->my))
            mintrap(mtmp);
    } else if (!Blind)
        pline("A large bubble rises to the surface and pops.");
    else
        You_hear("a loud pop.");
}

void
dogushforth(int drinking)
{       /* Gushing forth along LOS from (u.ux, u.uy) */
    int madepool = 0;

    do_clear_area(u.ux, u.uy, 7, gush, &madepool);
    if (!madepool) {
        if (drinking)
            pline("Your thirst is quenched.");
        else
            pline("Water sprays all over you.");
    }
}

static void
gush(int x, int y, void *poolcnt)
{
    struct monst *mtmp;
    struct trap *ttmp;

    if (((x + y) % 2) || (x == u.ux && y == u.uy) ||
        (rn2(1 + distmin(u.ux, u.uy, x, y))) ||
        (level->locations[x][y].typ != ROOM) || (sobj_at(BOULDER, level, x, y))
        || nexttodoor(level, x, y))
        return;

    if ((ttmp = t_at(level, x, y)) != 0 && !delfloortrap(level, ttmp))
        return;

    if (!((*(int *)poolcnt)++))
        pline("Water gushes forth from the overflowing fountain!");

    /* Put a pool at x, y */
    level->locations[x][y].typ = POOL;
    /* No kelp! */
    del_engr_at(level, x, y);
    water_damage_chain(level->objects[x][y], TRUE);

    if ((mtmp = m_at(level, x, y)) != 0)
        minliquid(mtmp);
    else
        newsym(x, y);
}

static void
dofindgem(void)
{       /* Find a gem in the sparkling waters. */
    if (!Blind)
        pline("You spot a gem in the sparkling waters!");
    else
        pline("You feel a gem here!");
    mksobj_at(rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1, rng_random_gem),
              level, u.ux, u.uy, FALSE, FALSE, rng_main);
    SET_FOUNTAIN_LOOTED(u.ux, u.uy);
    newsym(u.ux, u.uy);
    exercise(A_WIS, TRUE);      /* a discovery! */
}

void
dryup(xchar x, xchar y, boolean isyou)
{
    if (IS_FOUNTAIN(level->locations[x][y].typ) &&
        (!rn2_on_rng(3, isyou ? rng_fountain_result : rng_main) ||
         FOUNTAIN_IS_WARNED(x, y))) {
        if (isyou && in_town(x, y) && !FOUNTAIN_IS_WARNED(x, y)) {
            struct monst *mtmp;

            SET_FOUNTAIN_WARNED(x, y);
            /* Warn about future fountain use. */
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if ((mtmp->data == &mons[PM_WATCHMAN] ||
                     mtmp->data == &mons[PM_WATCH_CAPTAIN]) &&
                    couldsee(mtmp->mx, mtmp->my) && mtmp->mpeaceful) {
                    pline("%s yells:", Amonnam(mtmp));
                    verbalize("Hey, stop using that fountain!");
                    break;
                }
            }
            /* You can see or hear this effect */
            if (!mtmp)
                pline("The flow reduces to a trickle.");
            return;
        }

        if (isyou && wizard) {
            if (yn("Dry up fountain?") == 'n')
                return;
        }

        /* replace the fountain with ordinary floor */
        level->locations[x][y].typ = ROOM;
        level->locations[x][y].looted = 0;
        level->locations[x][y].blessedftn = 0;
        if (cansee(x, y))
            pline("The fountain dries up!");
        /* The location is seen if the hero/monster is invisible */
        /* or felt if the hero is blind.  */
        newsym(x, y);
        if (isyou && in_town(x, y))
            angry_guards(FALSE);
    }
}

void
drinkfountain(void)
{
    /* What happens when you drink from a fountain? */
    boolean mgkftn = (level->locations[u.ux][u.uy].blessedftn == 1);
    int fate = rn2_on_rng(30, (mgkftn && u.uluck >= 0) ?
                          rng_fountain_magic : rng_fountain_result);

    if (Levitation) {
        floating_above("fountain");
        return;
    }

    if (mgkftn && u.uluck >= 0 && fate >= 10) {
        int i, ii, littleluck = (u.uluck < 4);

        pline("Wow!  This makes you feel great!");
        /* blessed restore ability */
        for (ii = 0; ii < A_MAX; ii++)
            if (ABASE(ii) < AMAX(ii))
                ABASE(ii) = AMAX(ii);
        /* gain ability, blessed if "natural" luck is high */
        i = rn2(A_MAX); /* start at a random attribute */
        for (ii = 0; ii < A_MAX; ii++) {
            if (adjattrib(i, 1, littleluck ? -1 : 0) && littleluck)
                break;
            if (++i >= A_MAX)
                i = 0;
        }
        win_pause_output(P_MESSAGE);
        pline("A wisp of vapor escapes the fountain...");
        exercise(A_WIS, TRUE);
        level->locations[u.ux][u.uy].blessedftn = 0;
        return;
    }

    if (fate < 10) {
        pline("The cool draught refreshes you.");
        u.uhunger += rnd(10);   /* don't choke on water */
        newuhs(FALSE);
        if (mgkftn)
            return;
    } else {
        /* note: must match dipfountain() so that wishes and the like line up
           between dippers and quaffers */
        switch (fate) {
        case 16:       /* Curse items */ 
        {
            struct obj *obj;
            
            pline("This water's no good!");
            morehungry(rn1(20, 11));
            exercise(A_CON, FALSE);
            for (obj = invent; obj; obj = obj->nobj)
                if (!rn2(5))
                    curse(obj);
            break;
        }
        /* 17, 18, 19, 20 are uncurse effects in dipfountain(); match them
           against good effects in drinkfountain() */
        case 17:       /* See invisible */
            if (Blind) {
                if (Invisible) {
                    pline("You feel transparent.");
                } else {
                    pline("You feel very self-conscious.");
                    pline("Then it passes.");
                }
            } else {
                pline("You see an image of someone stalking you.");
                pline("But it disappears.");
            }
            HSee_invisible |= FROMOUTSIDE;
            newsym(u.ux, u.uy);
            exercise(A_WIS, TRUE);
            break;
        case 18:       /* See monsters */
            monster_detect(NULL, 0);
            exercise(A_WIS, TRUE);
            break;
        case 19:       /* Self-knowledge */
            pline("You feel self-knowledgeable...");
            win_pause_output(P_MESSAGE);
            enlightenment(0);
            exercise(A_WIS, TRUE);
            pline("The feeling subsides.");
            break;
        case 20:       /* Scare monsters */ 
        {
            struct monst *mtmp;
            
            pline("This water gives you bad breath!");
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                if (!DEADMONSTER(mtmp))
                    monflee(mtmp, 0, FALSE, FALSE);
            break;
        }
        case 21:       /* Water demon */
            dowaterdemon();
            break;
        case 22:       /* Water nymph */
            dowaternymph();
            break;
        case 23:       /* Fountain of snakes! */
            dowatersnakes();
            break;
        case 24:       /* Find a gem in the sparkling waters. */
            if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
                dofindgem();
                break;
            }
        case 25:       /* Gushing forth in this room */
            dogushforth(TRUE);
            break;
        case 26:       /* Poisonous; strange tingling in dipfountain */
            pline("The water is contaminated!");
            if (Poison_resistance) {
                pline("Perhaps it is runoff from the nearby %s farm.",
                      fruitname(FALSE));
                losehp(rnd(4),
                       killer_msg(DIED, "an unrefrigerated sip of juice"));
                break;
            }
            losestr(rn1(4, 3), DIED, killer_msg(DIED, "contaminated water"),
                    NULL);
            losehp(rnd(10), killer_msg(DIED, "contaminated water"));
            exercise(A_CON, FALSE);
            break;
            /* 27 is a sudden chill (no effect) in dipfountain */
        case 28:       /* Foul water; lose gold in dipfountain */
            pline("The water is foul!  You gag and vomit.");
            morehungry(rn1(20, 11));
            vomit();
            break;
            /* 29 is gain money in dipfountain */
        default:
            pline("This tepid water is tasteless.");
            break;
        }
    }
    dryup(u.ux, u.uy, TRUE);
}

void
dipfountain(struct obj *obj)
{
    if (Levitation) {
        floating_above("fountain");
        return;
    }

    /* Don't grant Excalibur when there's more than one object.  */
    /* (quantity could be > 1 if merged daggers got polymorphed) */
    if (obj->otyp == LONG_SWORD && obj->quan == 1L && u.ulevel >= 5 &&
        !obj->oartifact && !rn2_on_rng(6, rng_excalibur) &&
        !exist_artifact(LONG_SWORD, artiname(ART_EXCALIBUR))) {

        if (u.ualign.type != A_LAWFUL) {
            /* Ha! Trying to cheat her. */
            pline("A freezing mist rises from the water and envelopes the "
                  "sword.");
            pline("The fountain disappears!");
            curse(obj);
            if (obj->spe > -6 && !rn2(3))
                obj->spe--;
            obj->oerodeproof = FALSE;
            exercise(A_WIS, FALSE);
        } else {
            /* The lady of the lake acts! - Eric Backus */
            /* Be *REAL* nice */
            pline("From the murky depths, a hand reaches up to bless the "
                  "sword.");
            pline("As the hand retreats, the fountain disappears!");
            obj = oname(obj, artiname(ART_EXCALIBUR));
            discover_artifact(ART_EXCALIBUR);
            bless(obj);
            obj->oeroded = obj->oeroded2 = 0;
            obj->oerodeproof = TRUE;
            exercise(A_WIS, TRUE);
        }
        update_inventory();
        level->locations[u.ux][u.uy].typ = ROOM;
        level->locations[u.ux][u.uy].looted = 0;
        newsym(u.ux, u.uy);
        if (in_town(u.ux, u.uy))
            angry_guards(FALSE);
        return;
    } else if (water_damage(obj, NULL, TRUE) >= 2 && !rn2(2))
        return;

    /* Acid and water don't mix */
    if (obj->otyp == POT_ACID) {
        useup(obj);
        return;
    }

    switch (rn2_on_rng(30, rng_fountain_result)) {
    case 16:   /* Curse the item */
        curse(obj);
        break;
    case 17:
    case 18:
    case 19:
    case 20:   /* Uncurse the item */
        if (obj->cursed) {
            if (!Blind)
                pline("The water glows for a moment.");
            uncurse(obj);
        } else {
            pline("A feeling of loss comes over you.");
        }
        break;
    case 21:   /* Water Demon */
        dowaterdemon();
        break;
    case 22:   /* Water Nymph */
        dowaternymph();
        break;
    case 23:   /* an Endless Stream of Snakes */
        dowatersnakes();
        break;
    case 24:   /* Find a gem */
        if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
            dofindgem();
            break;
        }
    case 25:   /* Water gushes forth */
        dogushforth(FALSE);
        break;
    case 26:   /* Strange feeling */
        pline("A strange tingling runs up your %s.", body_part(ARM));
        break;
    case 27:   /* Strange feeling */
        pline("You feel a sudden chill.");
        break;
    case 28:   /* Strange feeling */
        pline("An urge to take a bath overwhelms you.");
        {
            long money = money_cnt(invent);
            struct obj *otmp;

            if (money > 10) {
                /* Amount to loose.  Might get rounded up as fountains don't
                   pay change... */
                money = somegold(money) / 10;
                for (otmp = invent; otmp && money > 0; otmp = otmp->nobj)
                    if (otmp->oclass == COIN_CLASS) {
                        int denomination = objects[otmp->otyp].oc_cost;
                        long coin_loss =
                            (money + denomination - 1) / denomination;
                        coin_loss = min(coin_loss, otmp->quan);
                        otmp->quan -= coin_loss;
                        money -= coin_loss * denomination;
                        if (!otmp->quan)
                            delobj(otmp);
                    }
                pline("You lost some of your money in the fountain!");
                CLEAR_FOUNTAIN_LOOTED(u.ux, u.uy);
                exercise(A_WIS, FALSE);
            }
        }
        break;
    case 29:   /* You see coins */

        /* We make fountains have more coins the closer you are to the surface. 
           After all, there will have been more people going by. Just like a
           shopping mall! Chris Woodbury */

        if (FOUNTAIN_IS_LOOTED(u.ux, u.uy))
            break;
        SET_FOUNTAIN_LOOTED(u.ux, u.uy);
        mkgold((rnd((dunlevs_in_dungeon(&u.uz) - dunlev(&u.uz) + 1) * 2) + 5),
               level, u.ux, u.uy, rng_main);
        if (!Blind)
            pline("Far below you, you see coins glistening in the water.");
        exercise(A_WIS, TRUE);
        newsym(u.ux, u.uy);
        break;
    }
    update_inventory();
    dryup(u.ux, u.uy, TRUE);
}

void
breaksink(int x, int y)
{
    if (cansee(x, y) || (x == u.ux && y == u.uy))
        pline("The pipes break!  Water spurts out!");
    level->locations[x][y].doormask = 0;
    level->locations[x][y].typ = FOUNTAIN;
    newsym(x, y);
}

void
drinksink(void)
{
    struct obj *otmp;
    struct monst *mtmp;

    if (Levitation) {
        floating_above("sink");
        return;
    }
    switch (rn2_on_rng(20, rng_sink_quaff)) {
    case 0:
        pline("You take a sip of very cold water.");
        break;
    case 1:
        pline("You take a sip of very warm water.");
        break;
    case 2:
        pline("You take a sip of scalding hot water.");
        if (Fire_resistance)
            pline("It seems quite tasty.");
        else
            losehp(rnd(6), killer_msg(DIED, "sipping boiling water"));
        break;
    case 3:
        if (mvitals[PM_SEWER_RAT].mvflags & G_GONE)
            pline("The sink seems quite dirty.");
        else {
            mtmp = makemon(&mons[PM_SEWER_RAT], level, u.ux, u.uy, NO_MM_FLAGS);
            if (mtmp)
                pline("Eek!  There's %s in the sink!",
                      (Blind || !canspotmon(mtmp)) ? "something squirmy" :
                      a_monnam(mtmp));
        }
        break;
    case 4:
        do {
            otmp = mkobj(level, POTION_CLASS, FALSE, rng_sink_quaff);
            if (otmp->otyp == POT_WATER) {
                obfree(otmp, NULL);
                otmp = NULL;
            }
        } while (!otmp);
        otmp->cursed = otmp->blessed = 0;
        pline("Some %s liquid flows from the faucet.",
              Blind ? "odd" : hcolor(OBJ_DESCR(objects[otmp->otyp])));
        otmp->dknown = !(Blind || Hallucination);
        otmp->quan++;   /* Avoid panic upon useup() */
        otmp->fromsink = 1;     /* kludge for docall() */
        dopotion(otmp);
        obfree(otmp, NULL);
        break;
    case 5:
        if (!(level->locations[u.ux][u.uy].looted & S_LRING)) {
            pline("You find a ring in the sink!");
            mkobj_at(RING_CLASS, level, u.ux, u.uy, TRUE, rng_sink_ring);
            level->locations[u.ux][u.uy].looted |= S_LRING;
            exercise(A_WIS, TRUE);
            newsym(u.ux, u.uy);
        } else
            pline("Some dirty water backs up in the drain.");
        break;
    case 6:
        breaksink(u.ux, u.uy);
        break;
    case 7:
        pline("The water moves as though of its own will!");
        if ((mvitals[PM_WATER_ELEMENTAL].mvflags & G_GONE)
            || !makemon(&mons[PM_WATER_ELEMENTAL], level, u.ux, u.uy,
                        NO_MM_FLAGS))
            pline("But it quiets down.");
        break;
    case 8:
        pline("Yuk, this water tastes awful.");
        more_experienced(1, 0);
        newexplevel();
        break;
    case 9:
        pline("Gaggg... this tastes like sewage!  You vomit.");
        morehungry(rn1(30 - ACURR(A_CON), 11));
        vomit();
        break;
    case 10:
        pline("This water contains toxic wastes!");
        if (!Unchanging) {
            pline("You undergo a freakish metamorphosis!");
            polyself(FALSE);
        }
        break;
        /* more odd messages --JJB */
    case 11:
        You_hear("clanking from the pipes...");
        break;
    case 12:
        You_hear("snatches of song from among the sewers...");
        break;
    case 19:
        if (Hallucination) {
            pline("From the murky drain, a hand reaches up... --oops--");
            break;
        }
    default:
        pline("You take a sip of %s water.",
              rn2(3) ? (rn2(2) ? "cold" : "warm") : "hot");
    }
}

/*fountain.c*/

