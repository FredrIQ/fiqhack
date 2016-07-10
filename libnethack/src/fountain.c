/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-13 */
/* Copyright Scott R. Turner, srt@ucla, 10/27/86                  */
/* NetHack may be freely redistributed.  See license for details. */

/* Code for drinking from fountains. */

#include "hack.h"

static void dowatersnakes(struct monst *mon);
static void dowaterdemon(struct monst *mon);
static void dowaternymph(struct monst *mon);
static void gush(int, int, void *);
static void dofindgem(struct monst *mon);

void
floating_above(const char *what)
{
    pline(msgc_cancelled1, "You are floating high above the %s.", what);
}

static void
dowatersnakes(struct monst *mon)
{       /* Fountain of snakes! */
    boolean you = (mon == &youmonst);
    int num = 2 + rn2_on_rng(5, rng_fountain_result);
    struct monst *mtmp;
    int snakes_seen = 0;
    boolean made_snakes = FALSE; /* whether or not to print any message in first place */

    if (!(mvitals[PM_WATER_MOCCASIN].mvflags & G_GONE)) {
        if (!Blind)
            pline(msgc_levelwarning, "An endless stream of %s pours forth!",
                  Hallucination ? makeplural(monnam_for_index(rndmonidx())) :
                  "snakes");
        else
            You_hear(msgc_levelwarning, "something hissing!");
        while (num-- > 0) {
            if ((mtmp =
                 makemon(&mons[PM_WATER_MOCCASIN], m_dlevel(mon),
                         mon->mx, mon->my, MM_ADJACENTOK))) {
                made_snakes = TRUE;
                if (canseemon(mtmp))
                    snakes_seen++;
                if (t_at(level, mtmp->mx, mtmp->my))
                    mintrap(mtmp);
            }
        }

        if (made_snakes) {
            if (!snakes_seen)
                You_hear(msgc_levelwarning, "something hissing!");
            else {
                const char *buf = "snake";
                if (Hallucination)
                    buf = monnam_for_index(rndmonidx());
                pline(msgc_levelwarning, "%s %s pours forth!", snakes_seen == 1 ? "A" :
                      snakes_seen <= 3 ? "Some" : "An endless stream of",
                      snakes_seen == 1 ? buf : makeplural(buf));
            }
            return;
        }
    }

    if (you || cansee(mon->mx, mon->my))
        pline(msgc_noconsequence,
              "The fountain bubbles furiously for a moment, then calms.");
    else
        You_hear(msgc_noconsequence, "furious bubbling.");
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

    /* Negative probabilities are allowed because it keeps dowaterdemon
       simple while not changing its behavior relative to 3.4.3 */
    switch ((percentchance < 0) ? 0 : percentchance) {
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
dowaterdemon(struct monst *mon)
{       /* Water demon */
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    struct monst *mtmp;

    if (!(mvitals[PM_WATER_DEMON].mvflags & G_GONE)) {
        if ((mtmp =
             makemon(&mons[PM_WATER_DEMON], level, mon->mx, mon->my, MM_ADJACENTOK))) {
            if (canseemon(mtmp))
                pline(msgc_levelwarning, "%s %s!", M_verbs(mon, "unleash"),
                      a_monnam(mtmp));
            else
                pline(msgc_levelwarning, "You feel the presence of evil.");

            /* A slightly better chance of a wish at early levels */
            if (you ? wish_available(20 - level_difficulty(m_mz(mon)), NULL) :
                (rnd(100) > (80 + level_difficulty(m_mz(mon))))) {
                if (vis || canseemon(mtmp))
                    pline(msgc_intrgain,
                          "Grateful for %s release, %s grants %s a wish!",
                          mhis(mtmp), mhe(mtmp), mon_nam(mon));
                if (you)
                    makewish();
                else
                    mon_makewish(mon);
                mongone(mtmp);
            } else if (t_at(level, mtmp->mx, mtmp->my))
                mintrap(mtmp);
        }
    } else if (you || cansee(mon->mx, mon->my))
        pline(msgc_noconsequence,
              "The fountain bubbles furiously for a moment, then calms.");
    else
        You_hear(msgc_noconsequence, "furious bubbling.");
}


static void
dowaternymph(struct monst *mon)
{       /* Water Nymph */
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    struct monst *mtmp;

    if (!(mvitals[PM_WATER_NYMPH].mvflags & G_GONE) &&
        (mtmp =
         makemon(&mons[PM_WATER_NYMPH], level, mon->mx, mon->my, MM_ADJACENTOK))) {
        if (canseemon(mtmp))
            pline(msgc_levelwarning, "%s %s!", M_verbs(mon, "attract"), a_monnam(mtmp));
        else
            You_hear(msgc_levelwarning, "a seductive voice.");
        mtmp->msleeping = 0;
        if (t_at(level, mtmp->mx, mtmp->my))
            mintrap(mtmp);
    } else if (vis)
        pline(msgc_noconsequence,
              "A large bubble rises to the surface and pops.");
    else
        You_hear(msgc_noconsequence, "a %s pop.", distant(mon) ? "distant" : "loud");
}

/* mon can be null if called from digactualhole */
void
dogushforth(struct monst *mon, boolean drinking)
{       /* Gushing forth along LOS from (mon->mx, mon->my) */
    boolean you = (mon == &youmonst);
    boolean vis = (you || (mon && canseemon(mon)));
    int madepool = 0;

    do_clear_area(mon->mx, mon->my, 7, gush, &madepool);
    if (!madepool) {
        if (drinking && vis)
            pline(msgc_noconsequence, "%s thirst is quenched.", Monnam(mon));
        else
            pline(msgc_noconsequence, "Water sprays all over%s.", !mon || !vis ? "" :
                  msgcat(" ", mon_nam(mon)));
    }
}

static void
gush(int x, int y, void *poolcnt)
{
    struct monst *mtmp;
    struct trap *ttmp;

    if (((x + y) % 2) || (x == youmonst.mx && y == youmonst.my) ||
        (rn2(1 + distmin(youmonst.mx, youmonst.my, x, y))) ||
        (level->locations[x][y].typ != ROOM) || (sobj_at(BOULDER, level, x, y))
        || nexttodoor(level, x, y))
        return;

    if ((ttmp = t_at(level, x, y)) != 0 && !delfloortrap(level, ttmp))
        return;

    if ((cansee(x, y) || monnear(&youmonst, x, y)) && !((*(int *)poolcnt)++))
        pline(msgc_consequence,
              "Water gushes forth from the overflowing fountain!");

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
dofindgem(struct monst *mon)
{       /* Find a gem in the sparkling waters. */
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    if (vis)
        pline(you ? msgc_youdiscover : msgc_monneutral,
              "%s a gem in the sparkling waters!", M_verbs(mon, you ? "spot" : "find"));
    else if (you)
        pline(msgc_youdiscover, "You feel a gem here!");
    mksobj_at(rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1, rng_random_gem),
              level, mon->mx, mon->my, FALSE, FALSE, rng_main);
    SET_FOUNTAIN_LOOTED(mon->mx, mon->my);
    newsym(mon->mx, mon->my);
    if (you)
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
                    pline(msgc_npcvoice, "%s yells:", Amonnam(mtmp));
                    verbalize(msgc_npcvoice,
                              "Hey, stop using that fountain!");
                    break;
                }
            }
            /* You can see or hear this effect */
            if (!mtmp)
                pline(msgc_hint, "The flow reduces to a trickle.");
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
            pline(msgc_consequence, "The fountain dries up!");
        /* The location is seen if the hero/monster is invisible */
        /* or felt if the hero is blind.  */
        newsym(x, y);
        if (isyou && in_town(x, y))
            angry_guards(FALSE);
    }
}

int
drinkfountain(struct monst *mon)
{
    /* What happens when you drink from a fountain? */
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    boolean mgkftn = (level->locations[mon->mx][mon->my].blessedftn == 1);
    int fate = rn2_on_rng(30, !you ? rng_main : (mgkftn && u.uluck >= 0) ?
                          rng_fountain_magic : rng_fountain_result);
    int nutr = 0;
    struct edog *edog = mx_edog(mon); /* for hunger */

    if (levitates(mon)) {
        if (!you)
            impossible("Monster trying to drink from a fountain while levitating?");
        else
            floating_above("fountain");
        return 0;
    }

    if (!you && vis)
        pline(msgc_monneutral, "%s drinks from the fountain.", Monnam(mon));

    if (mgkftn && (!you || u.uluck >= 0) && fate >= 10) {
        int i, ii, littleluck = (u.uluck < 4);

        if (you)
            pline(msgc_intrgain, "Wow!  This makes you feel great!");
        else if (vis)
            pline(msgc_monneutral, "%s great!", M_verbs(mon, "look"));

        /* TODO: Monsters currently don't have real attributes. Those effects below are
           equavilent to what they are in peffects (but the peffects one are currently
           unused) */

        /* blessed restore ability */
        if (you) {
            for (ii = 0; ii < A_MAX; ii++)
                if (ABASE(ii) < AMAX(ii))
                    ABASE(ii) = AMAX(ii);
        } else {
            if (mon->mhpmax <= (mon->m_lev * 8 - 8)) {
                int raised_maxhp = 0;
                for (i = 0; i < (mon->m_lev - (mon->mhpmax / 8 - 1)); i++)
                    raised_maxhp += rnd(8);

                if (raised_maxhp) {
                    mon->mhpmax += raised_maxhp;
                    mon->mhp += raised_maxhp;
                }
            }
        }

        /* gain ability, blessed if "natural" luck is high */
        if (you) {
            i = rn2(A_MAX); /* start at a random attribute */
            for (ii = 0; ii < A_MAX; ii++) {
                if (adjattrib(i, 1, littleluck ? -1 : 0) && littleluck)
                    break;
                if (++i >= A_MAX)
                    i = 0;
            }
            win_pause_output(P_MESSAGE);
            exercise(A_WIS, TRUE);
        } else {
            if (vis)
                pline(msgc_monneutral, "%s abilities looks improved.",
                      s_suffix(Monnam(mon)));
            if (!grow_up(mon, NULL))
                return 2; /* oops */
        }

        if (vis)
            pline(msgc_consequence, "A wisp of vapor escapes the fountain...");

        level->locations[mon->mx][mon->my].blessedftn = 0;
        return 1;
    }

    if (fate < 10) {
        if (vis)
            pline(msgc_statusheal, "The cool draught refreshes %s.", mon_nam(mon));
        nutr = rnd(10);
        if (you) {
            u.uhunger += nutr;   /* don't choke on water */
            newuhs(FALSE);
        } else if (edog) {
            if (edog->hungrytime < moves)
                edog->hungrytime = moves;
            edog->hungrytime += nutr;
            if (edog->mhpmax_penalty) {
                set_property(mon, CONFUSION, -2, TRUE);
                mon->mhpmax += edog->mhpmax_penalty;
                edog->mhpmax_penalty = 0;
            }
        }
        if (mgkftn)
            return 1;
    } else {
        /* note: must match dipfountain() so that wishes and the like line up
           between dippers and quaffers */
        switch (fate) {
        case 16:       /* Curse items */ 
        {
            struct obj *obj;

            if (vis)
                pline(msgc_itemloss, "This water's no good!");
            if (you) {
                morehungry(rn1(20, 11));
                exercise(A_CON, FALSE);
            }
            for (obj = m_minvent(mon); obj; obj = obj->nobj)
                if (!rn2(5))
                    curse(obj);
            break;
        }
        /* 17, 18, 19, 20 are uncurse effects in dipfountain(); match them
           against good effects in drinkfountain() */
        case 17:       /* See invisible */
            if (!you) { /* no messages */
                set_property(mon, SEE_INVIS, 0, FALSE);
                break;
            }
            if (Blind) {
                if (Invisible) {
                    pline(msgc_intrgain, "You feel transparent.");
                } else {
                    pline(msgc_intrgain, "You feel very self-conscious.");
                    pline_implied(msgc_intrgain, "Then it passes.");
                }
            } else {
                pline(msgc_intrgain,
                      "You see an image of someone stalking you.");
                pline_implied(msgc_intrgain, "But it disappears.");
            }
            set_property(mon, SEE_INVIS, 0, FALSE);
            newsym(mon->mx, mon->my);
            exercise(A_WIS, TRUE);
            break;
        case 18:       /* See monsters */
            if (you) {
                monster_detect(NULL, 0);
                exercise(A_WIS, TRUE);
            } else /* players get 3 turns of monster detection, so monsters do too... */
                inc_timeout(mon, DETECT_MONSTERS, 3, FALSE);
            break;
        case 19:       /* Self-knowledge */
            pline(msgc_youdiscover, "%s self-knowledgeable...",
                  M_verbs(mon, you ? "feel" : "look"));
            if (!you)
                break;
            win_pause_output(P_MESSAGE);
            enlighten_mon(mon, 0, 0);
            exercise(A_WIS, TRUE);
            pline_implied(msgc_info, "The feeling subsides.");
            break;
        case 20:       /* Scare monsters */
            if (vis)
                pline(msgc_statusgood, "This water gives %s bad breath!", mon_nam(mon));
            /* If you are nearby, get disgusted */
            if (!you && vis && !distant(mon)) {
                pline(msgc_statusbad, "Ulch!  What a terrible smell!");
                helpless(2, hr_afraid, "being disgusted by a horrible smell",
                         "You regain your composure.");
            }

            struct monst *mtmp;
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                if (!DEADMONSTER(mtmp) && mon != mtmp)
                    monflee(mtmp, 0, FALSE, FALSE);
            break;
        case 21:       /* Water demon */
            dowaterdemon(mon);
            break;
        case 22:       /* Water nymph */
            dowaternymph(mon);
            break;
        case 23:       /* Fountain of snakes! */
            dowatersnakes(mon);
            break;
        case 24:       /* Find a gem in the sparkling waters. */
            if (!FOUNTAIN_IS_LOOTED(mon->mx, mon->my)) {
                dofindgem(mon);
                break;
            }
        case 25:       /* Gushing forth in this room */
            dogushforth(mon, TRUE);
            break;
        case 26:       /* Poisonous; strange tingling in dipfountain */
            /* avoid this message if we are about to print another one to avoid msgspam */
            if (!resists_poison(mon) || (!you && vis))
                pline(you ? msgc_intrloss : msgc_monneutral,
                      "The water is contaminated!");
            int dmg = resists_poison(mon) ? rnd(4) : rn1(4, 3);
            if (resists_poison(mon)) {
                if (you)
                    pline(msgc_nonmonbad,
                          "Is this water runoff from the nearby %s farm?",
                          fruitname(FALSE));
                if (you)
                    losehp(dmg, killer_msg(DIED, "an unrefrigerated sip of juice"));
                else {
                    mon->mhp -= dmg;
                    if (mon->mhp <= 0)
                        mondied(mon);
                }
                break;
            }
            if (you) {
                losestr(dmg, DIED, killer_msg(DIED, "contaminated water"), NULL);
                losehp(rnd(10), killer_msg(DIED, "contaminated water"));
                exercise(A_CON, FALSE);
            } else {
                mon->mhp -= dice(dmg, 8);
                mon->mhp -= rnd(10); /* and the 1-10 additional HP like players... */
                if (mon->mhp <= 0)
                    mondied(mon);
            }
            break;
        case 27:       /* Foul water; sudden chill in dipfountain */
            if (vis)
                pline(msgc_statusbad, "The water is foul!  %s and vomit.",
                      M_verbs(mon, "gag"));
            if (you)
                morehungry(rn1(20, 11));
            vomit(mon);
            break;
            /* 28 is lose gold in dipfountain */
            /* 29 is gain money in dipfountain */
        default:
            if (you)
                pline(msgc_failrandom, "This tepid water is tasteless.");
            break;
        }
    }
    dryup(mon->mx, mon->my, you);
    return 1;
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
    if (obj->otyp == LONG_SWORD && obj->quan == 1L && youmonst.m_lev >= 5 &&
        !obj->oartifact && !rn2_on_rng(6, rng_excalibur) &&
        !exist_artifact(LONG_SWORD, artiname(ART_EXCALIBUR))) {

        if (u.ualign.type != A_LAWFUL) {
            /* Ha! Trying to cheat her. */
            pline(msgc_itemloss, "A freezing mist rises from the water "
                  "and envelopes the sword.");
            pline(msgc_consequence, "The fountain disappears!");
            curse(obj);
            if (obj->spe > -6 && !rn2(3))
                obj->spe--;
            obj->oerodeproof = FALSE;
            exercise(A_WIS, FALSE);
        } else {
            /* The lady of the lake acts! - Eric Backus */
            /* Be *REAL* nice */
            pline(msgc_intrgain, "From the murky depths, a hand reaches up "
                  "to bless the sword.");
            pline(msgc_consequence, "As the hand retreats, the "
                  "fountain disappears!");
            obj = oname(obj, artiname(ART_EXCALIBUR));
            discover_artifact(ART_EXCALIBUR);
            bless(obj);
            obj->oeroded = obj->oeroded2 = 0;
            obj->oerodeproof = TRUE;
            exercise(A_WIS, TRUE);
        }
        update_inventory();
        level->locations[youmonst.mx][youmonst.my].typ = ROOM;
        level->locations[youmonst.mx][youmonst.my].looted = 0;
        newsym(youmonst.mx, youmonst.my);
        if (in_town(youmonst.mx, youmonst.my))
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
                pline(msgc_itemrepair, "The water glows for a moment.");
            uncurse(obj);
        } else {
            pline(msgc_noconsequence, "A feeling of loss comes over you.");
        }
        break;
    case 21:   /* Water Demon */
        dowaterdemon(&youmonst);
        break;
    case 22:   /* Water Nymph */
        dowaternymph(&youmonst);
        break;
    case 23:   /* an Endless Stream of Snakes */
        dowatersnakes(&youmonst);
        break;
    case 24:   /* Find a gem */
        if (!FOUNTAIN_IS_LOOTED(youmonst.mx, youmonst.my)) {
            dofindgem(&youmonst);
            break;
        }
    case 25:   /* Water gushes forth */
        dogushforth(&youmonst, FALSE);
        break;
    case 26:   /* Strange feeling */
        pline(msgc_failrandom, "A strange tingling runs up your %s.",
              body_part(ARM));
        break;
    case 27:   /* Strange feeling */
        pline(msgc_failrandom, "You feel a sudden chill.");
        break;
    case 28:   /* Strange feeling */
        pline(msgc_nonmonbad, "An urge to take a bath overwhelms you.");
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
                pline(msgc_itemloss,
                      "You lost some of your money in the fountain!");
                CLEAR_FOUNTAIN_LOOTED(youmonst.mx, youmonst.my);
                exercise(A_WIS, FALSE);
            }
        }
        break;
    case 29:   /* You see coins */

        /* We make fountains have more coins the closer you are to the surface. 
           After all, there will have been more people going by. Just like a
           shopping mall! Chris Woodbury */

        if (FOUNTAIN_IS_LOOTED(youmonst.mx, youmonst.my))
            break;
        SET_FOUNTAIN_LOOTED(youmonst.mx, youmonst.my);
        mkgold((rnd((dunlevs_in_dungeon(&u.uz) - dunlev(&u.uz) + 1) * 2) + 5),
               level, youmonst.mx, youmonst.my, rng_main);
        if (!Blind)
            pline(msgc_youdiscover,
                  "Far below you, you see coins glistening in the water.");
        exercise(A_WIS, TRUE);
        newsym(youmonst.mx, youmonst.my);
        break;
    }
    update_inventory();
    dryup(youmonst.mx, youmonst.my, TRUE);
}

void
breaksink(int x, int y)
{
    if (cansee(x, y) || (x == youmonst.mx && y == youmonst.my))
        pline(msgc_consequence, "The pipes break!  Water spurts out!");
    level->locations[x][y].doormask = 0;
    level->locations[x][y].typ = FOUNTAIN;
    newsym(x, y);
}

int
drinksink(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    struct obj *otmp;
    struct monst *mtmp;

    if (levitates(mon)) {
        if (!you)
            impossible("Monster trying to drink from a sink while levitating?");
        else
            floating_above("sink");
        return 0;
    }

    if (!you)
        pline(msgc_monneutral, "%s from the sink.", M_verbs(mon, "drink"));

    int result = rn2_on_rng(20, you ? rng_sink_quaff : rng_main);
    switch (result) {
    case 0:
    case 1:
    case 2:
        if (!you) {
            if (result == 2 && !resists_fire(mon)) {
                if (vis)
                    pline(msgc_monneutral, "%s scalded!", M_verbs(mon, "are"));
                mon->mhp -= rnd(6);
                if (mon->mhp <= 0)
                    mondied(mon);
            }
            break;
        }

        if (result == 2 && you && resists_fire(mon))
            pline(msgc_playerimmune, "This scalding hot water is quite tasty!");
        else if (vis)
            pline(msgc_failrandom, "You take a sip of %s %s water.",
                  result == 2 ? "scalding " : "very ", !result ? "cold" :
                  result == 1 ? "warm" : "hot");
        if (result == 2)
            losehp(rnd(6), killer_msg(DIED, "sipping boiling water"));
        break;
    case 3:
        if (!(mvitals[PM_SEWER_RAT].mvflags & G_GONE)) {
            mtmp = makemon(&mons[PM_SEWER_RAT], level, mon->mx, mon->my,
                           MM_ADJACENTOK);
            if (mtmp) {
                if (vis)
                    pline(msgc_nonmonbad, "%sThere's %s in the sink!",
                          you ? "Eek!  " : "", !canseemon(mtmp) ?
                          "something squirmy" : a_monnam(mtmp));
                break;
            }
        }
        if (vis)
            pline(msgc_noconsequence, "The sink seems quite dirty.");
        break;
    case 4:
        /* TODO: temporary object? */
        do {
            otmp = mkobj(level, POTION_CLASS, FALSE, you ?
                         rng_sink_quaff : rng_main);
            if (otmp->otyp == POT_WATER) {
                obfree(otmp, NULL);
                otmp = NULL;
            }
        } while (!otmp);
        otmp->cursed = otmp->blessed = 0;
        if (vis)
            pline(msgc_substitute, "Some %s liquid flows from the faucet.",
                  Blind ? "odd" : hcolor(OBJ_DESCR(objects[otmp->otyp])));
        otmp->dknown = (vis && !(Blind || Hallucination));
        otmp->quan++;   /* Avoid panic upon useup() */
        otmp->fromsink = 1;     /* kludge for docall() */
        dopotion(mon, otmp);
        obfree(otmp, NULL);
        break;
    case 5:
        if (!(level->locations[youmonst.mx][youmonst.my].looted & S_LRING)) {
            if (vis)
                pline(msgc_youdiscover, "%s a ring in the sink!",
                      M_verbs(mon, "find"));
            mkobj_at(RING_CLASS, level, mon->mx, mon->my, TRUE, rng_sink_ring);
            level->locations[mon->mx][mon->my].looted |= S_LRING;
            if (you)
                exercise(A_WIS, TRUE);
            newsym(mon->mx, mon->my);
        } else if (you)
            pline(msgc_noconsequence,
                  "Some dirty water backs up in the drain.");
        break;
    case 6:
        breaksink(mon->mx, mon->my);
        break;
    case 7:
        if (vis)
        if (!(mvitals[PM_WATER_ELEMENTAL].mvflags & G_GONE) &&
            (mtmp = makemon(&mons[PM_WATER_ELEMENTAL], level, mon->mx, mon->my,
                            MM_ADJACENTOK))) {
            if (vis)
                pline(msgc_nonmonbad, "The water moves as though of its own will!");
            break;
        }
        if (you || cansee(mon->mx, mon->my))
            pline(msgc_noconsequence, "The water animates briefly.");
        break;
    case 8:
        /* +1 experience point isn't enough to be msgc_intrgain; treat it like
           there was no real effect, unless there was actually a level gain (in
           which case it'll print its own messages) */
        if (you) {
            pline(msgc_failrandom, "Yuk, this water tastes awful.");
            more_experienced(1, 0);
            newexplevel();
        } else {
            mon->mhpmax++;
            mon->mhp++;
            if (mon->mhpmax > (mon->m_lev * 8)) {
                if (!grow_up(mon, NULL))
                    return 2; /* oops */
            }
        }
        break;
    case 9:
        if (you)
            pline(msgc_statusbad, "Gaggg... this tastes like sewage!");
        if (vis)
            pline(you ? msgc_statusbad : msgc_monneutral, "%s.",
                  M_verbs(mon, "vomit"));
        if (you)
            morehungry(rn1(30 - ACURR(A_CON), 11));
        vomit(mon);
        break;
    case 10:
        if (vis)
            pline(!you ? msgc_monneutral : unchanging(mon) ?
                  msgc_playerimmune : msgc_statusbad,
                  "This water contain toxic wastes!");
        if (!unchanging(mon)) {
            if (vis)
                pline_implied(msgc_statusbad,
                              "%s a freakish metamorphosis!",
                              M_verbs(mon, "undergo"));
            if (you)
                polyself(FALSE);
            else
                newcham(mon, NULL, FALSE, TRUE);
        }
        break;
        /* more odd messages --JJB */
    case 11:
        You_hear(msgc_failrandom, "clanking from the pipes...");
        break;
    case 12:
        You_hear(msgc_failrandom, "snatches of song from among the sewers...");
        break;
    case 19:
        if (you && Hallucination) {
            pline(msgc_failrandom,
                  "From the murky drain, a hand reaches up... --oops--");
            break;
        }
    default:
        if (you)
            pline(msgc_failrandom, "You take a sip of %s water.",
                  rn2(3) ? (rn2(2) ? "cold" : "warm") : "hot");
    }
    return 1;
}

/*fountain.c*/

