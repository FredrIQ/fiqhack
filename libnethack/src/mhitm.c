/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"
#include "edog.h"


static boolean vis, far_noise;
static long noisetime;
static struct obj *otmp;

static const char brief_feeling[] =
    "You have a %s feeling for a moment, then it passes.";

static const char *mon_nam_too(struct monst *, struct monst *);
static int hitmm(struct monst *, struct monst *, const struct attack *);
static int gazemm(struct monst *, struct monst *, const struct attack *);
static int gulpmm(struct monst *, struct monst *, const struct attack *);
static int explmm(struct monst *, struct monst *, const struct attack *);
static int mdamagem(struct monst *, struct monst *, const struct attack *);
static void mswingsm(struct monst *, struct monst *, struct obj *);
static void noises(struct monst *, const struct attack *);
static void missmm(struct monst *, struct monst *, const struct attack *);
static int passivemm(struct monst *, struct monst *, boolean, int);

/* Needed for the special case of monsters wielding vorpal blades (rare).
 * If we use this a lot it should probably be a parameter to mdamagem()
 * instead of a global variable.
 */
static int dieroll;

/* returns mon_nam(mon) relative to other_mon; normal name unless they're
   the same, in which case the reference is to {him|her|it} self */
static const char *
mon_nam_too(struct monst *mon, struct monst *other_mon)
{
    if (mon == other_mon) {
        if (!is_longworm(mon->data))
            impossible("non-longworm attacking itself?");

        switch (pronoun_gender(mon)) {
        case 0:
            return "himself";
        case 1:
            return "herself";
        default:
            return "itself";
        }
    }
    return mon_nam(mon);
}

static void
noises(struct monst *magr, const struct attack *mattk)
{
    boolean farq = (distu(magr->mx, magr->my) > 15);

    if (farq != far_noise || moves - noisetime > 10) {
        far_noise = farq;
        noisetime = moves;
        You_hear("%s%s.",
                 (mattk->aatyp == AT_EXPL) ? "an explosion" : "some noises",
                 farq ? " in the distance" : "");
    }
}

static void
missmm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    const char *fmt, *buf;

    /* TODO: This is badly spaghetti.

       Don't use reveal_monster_at: the player isn't involved here, except
       possibly as an observer. */
    if (vis) {
        if (!canspotmon(magr))
            map_invisible(magr->mx, magr->my);
        if (!canspotmon(mdef))
            map_invisible(mdef->mx, mdef->my);
        if (mdef->m_ap_type)
            seemimic(mdef);
        if (magr->m_ap_type)
            seemimic(magr);
        fmt = (could_seduce(magr, mdef, mattk) &&
               !magr->mcan) ? "%s pretends to be friendly to" : "%s misses";
        buf = msgprintf(fmt, Monnam(magr));
        pline("%s %s.", buf, mon_nam_too(mdef, magr));
    } else
        noises(magr, mattk);
}

/*
 *  fightm()  -- fight some other monster
 *
 *  Returns:
 *      0 - Monster did nothing.
 *      1 - If the monster made an attack.  The monster might have died.
 *
 *  There is an exception to the above.  If mtmp has the hero swallowed,
 *  then we report that the monster did nothing so it will continue to
 *  digest the hero.
 */
int
fightm(struct monst *mtmp)
{       /* have monsters fight each other */
    struct monst *mon, *nmon;
    int result, has_u_swallowed;

    /* perhaps the monster will resist Conflict */
    if (resist(mtmp, RING_CLASS, 0, 0))
        return 0;

    if (u.ustuck == mtmp) {
        /* perhaps we're holding it... */
        if (itsstuck(mtmp))
            return 0;
    }
    has_u_swallowed = (Engulfed && (mtmp == u.ustuck));

    for (mon = level->monlist; mon; mon = nmon) {
        nmon = mon->nmon;
        if (nmon == mtmp)
            nmon = mtmp->nmon;
        /* Be careful to ignore monsters that are already dead, since we might
           be calling this before we've cleaned them up.  This can happen if
           the monster attacked a cockatrice bare-handedly, for instance. */
        if (mon != mtmp && !DEADMONSTER(mon)) {
            if (monnear(mtmp, mon->mx, mon->my)) {
                if (!Engulfed && (mtmp == u.ustuck)) {
                    if (!rn2(4)) {
                        pline("%s releases you!", Monnam(mtmp));
                        u.ustuck = 0;
                    } else
                        break;
                }

                /* mtmp can be killed */
                bhitpos.x = mon->mx;
                bhitpos.y = mon->my;
                notonhead = 0;
                result = mattackm(mtmp, mon);

                if (result & MM_AGR_DIED)
                    return 1;   /* mtmp died */
                /* 
                 *  If mtmp has the hero swallowed, lie and say there
                 *  was no attack (this allows mtmp to digest the hero).
                 */
                if (has_u_swallowed)
                    return 0;

                /* Allow attacked monsters a chance to hit back. Primarily to
                   allow monsters that resist conflict to respond.

                   Note: in 4.3, this no longer costs movement points, because
                   it throws off the turn alternation. */
                if ((result & MM_HIT) && !(result & MM_DEF_DIED)) {
                    notonhead = 0;
                    mattackm(mon, mtmp);        /* return attack */
                }

                return (result & MM_HIT) ? 1 : 0;
            }
        }
    }
    return 0;
}

/*
 * mattackm() -- a monster attacks another monster.
 *
 * This function returns a result bitfield:
 *
 *          --------- aggressor died
 *         /  ------- defender died
 *        /  /  ----- defender was hit
 *       /  /  /
 *      x  x  x
 *
 *      0x4     MM_AGR_DIED
 *      0x2     MM_DEF_DIED
 *      0x1     MM_HIT
 *      0x0     MM_MISS
 *
 * Each successive attack has a lower probability of hitting.  Some rely on the
 * success of previous attacks.  ** this doen't seem to be implemented -dl **
 *
 * In the case of exploding monsters, the monster dies as well.
 */
int
mattackm(struct monst *magr, struct monst *mdef)
{
    int i,      /* loop counter */
        tmp,    /* amour class difference */
        strike, /* hit this attack */
        attk,   /* attack attempted this time */
        struck = 0,     /* hit at least once */
        res[NATTK];     /* results of all attacks */
    const struct attack *mattk;
    struct attack alt_attk;
    const struct permonst *pa, *pd;

    if (mdef == &youmonst)
        return mattacku(magr) ? MM_AGR_DIED : 0;

    if (!magr || !mdef)
        return MM_MISS; /* mike@genat */
    if (!magr->mcanmove || magr->msleeping)
        return MM_MISS;
    pa = magr->data;
    pd = mdef->data;

    if (!mpreattack(magr, distmin(mdef->mx, mdef->my, magr->mx, magr->my) > 1))
        return FALSE;

    /* Grid bugs cannot attack at an angle. */
    if (pa == &mons[PM_GRID_BUG] && magr->mx != mdef->mx &&
        magr->my != mdef->my)
        return MM_MISS;

    /* Calculate the armor class differential. */
    tmp = find_mac(mdef) + magr->m_lev;

    if (mdef->mconf || !mdef->mcanmove || mdef->msleeping) {
        tmp += 4;
        mdef->msleeping = 0;
    }

    /* undetect monsters become un-hidden if they are attacked */
    if (mdef->mundetected &&
        dist2(mdef->mx, mdef->my, magr->mx, magr->my) > 2) {
        mdef->mundetected = 0;
        newsym(mdef->mx, mdef->my);
        if (canseemon(mdef) && !sensemon(mdef)) {
            if (u_helpless(hm_asleep))
                pline("You dream of %s.",
                      (mdef-> data->geno & G_UNIQ) ? a_monnam(mdef) :
                          makeplural(m_monnam(mdef)));
            else
                pline("Suddenly, you notice %s.", a_monnam(mdef));
        }
    }

    /* Elves hate orcs. */
    if (is_elf(pa) && is_orc(pd))
        tmp++;


    /* Set up the visibility of action */
    vis = (cansee(magr->mx, magr->my) && cansee(mdef->mx, mdef->my) &&
           (canspotmon(magr) || canspotmon(mdef)));

    /* Set flag indicating monster has moved this turn.  Necessary since a
       monster might get an attack out of sequence (i.e. before its move) in
       some cases, in which case this still counts as its move for the round
       and it shouldn't move again. */
    magr->mlstmv = moves;

    /* Now perform all attacks for the monster. */
    for (i = 0; i < NATTK; i++) {
        int tmphp = mdef->mhp;

        res[i] = MM_MISS;
        mattk = getmattk(pa, i, res, &alt_attk);
        otmp = NULL;
        attk = 1;
        switch (mattk->aatyp) {
        case AT_WEAP:  /* weapon attacks */
            if (dist2(magr->mx, magr->my, mdef->mx, mdef->my) > 2) {
                thrwmq(magr, mdef->mx, mdef->my);
                if (tmphp > mdef->mhp)
                    res[i] = MM_HIT;
                else
                    res[i] = MM_MISS;
                if (DEADMONSTER(mdef))
                    res[i] = MM_DEF_DIED;
                if (DEADMONSTER(magr))
                    res[i] = MM_AGR_DIED;
                strike = 0;
                break;
            }
            if (magr->weapon_check == NEED_WEAPON || !MON_WEP(magr)) {
                magr->weapon_check = NEED_HTH_WEAPON;
                if (mon_wield_item(magr) != 0)
                    return 0;
            }
            possibly_unwield(magr, FALSE);
            otmp = MON_WEP(magr);

            if (otmp) {
                if (vis)
                    mswingsm(magr, mdef, otmp);
                tmp += hitval(otmp, mdef);
            }
            /* fall through */
        case AT_CLAW:
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            /* Nymph that teleported away on first attack? */
            if (dist2(magr->mx, magr->my, mdef->mx, mdef->my) > 2) {
                strike = 0;
                break;  /* might have more ranged attacks */
            }
            /* Monsters won't attack cockatrices physically if they have a
               weapon instead.  This instinct doesn't work for players, or
               under conflict or confusion. */
            if (!magr->mconf && !Conflict && otmp && mattk->aatyp != AT_WEAP &&
                touch_petrifies(mdef->data)) {
                strike = 0;
                break;
            }
            dieroll = rnd(20 + i);
            strike = (tmp > dieroll);
            /* KMH -- don't accumulate to-hit bonuses */
            if (otmp)
                tmp -= hitval(otmp, mdef);
            if (strike) {
                res[i] = hitmm(magr, mdef, mattk);
                if ((mdef->data == &mons[PM_BLACK_PUDDING] ||
                     mdef->data == &mons[PM_BROWN_PUDDING])
                    && otmp && objects[otmp->otyp].oc_material == IRON &&
                    mdef->mhp >= 2 && !mdef->mcan) {
                    if (clone_mon(mdef, 0, 0)) {
                        if (vis) {
                            pline("%s divides as %s hits it!",
                                  Monnam(mdef), mon_nam(magr));
                        }
                    }
                }
            } else
                missmm(magr, mdef, mattk);
            break;

        case AT_HUGS:  /* automatic if prev two attacks succeed */
            strike = (i >= 2 && res[i - 1] == MM_HIT && res[i - 2] == MM_HIT);
            if (strike)
                res[i] = hitmm(magr, mdef, mattk);

            break;

        case AT_BREA:
            breamq(magr, mdef->mx, mdef->my, mattk);
            if (tmphp > mdef->mhp)
                res[i] = MM_HIT;
            else
                res[i] = MM_MISS;
            if (DEADMONSTER(mdef))
                res[i] = MM_DEF_DIED;
            if (DEADMONSTER(magr))
                res[i] = MM_AGR_DIED;
            strike = 0; /* waking up handled by m_throw() */
            break;

        case AT_SPIT:
            spitmq(magr, mdef->mx, mdef->my, mattk);
            if (tmphp > mdef->mhp)
                res[i] = MM_HIT;
            else
                res[i] = MM_MISS;
            if (DEADMONSTER(mdef))
                res[i] = MM_DEF_DIED;
            if (DEADMONSTER(magr))
                res[i] = MM_AGR_DIED;
            strike = 0; /* waking up handled by m_throw() */
            break;

        case AT_GAZE:
            strike = 0; /* will not wake up a sleeper */
            res[i] = gazemm(magr, mdef, mattk);
            break;

        case AT_EXPL:
            if (distmin(magr->mx, magr->my, mdef->mx, mdef->my) > 1) {
                strike = 0;
                break;
            }
            res[i] = explmm(magr, mdef, mattk);
            if (res[i] == MM_MISS) {    /* cancelled--no attack */
                strike = 0;
                attk = 0;
            } else
                strike = 1;     /* automatic hit */
            break;

        case AT_ENGL:
            if (distmin(magr->mx, magr->my, mdef->mx, mdef->my) > 1 ||
                (u.usteed && (mdef == u.usteed))) {
                strike = 0;
                break;
            }
            /* Engulfing attacks are directed at the hero if possible. -dlc */
            if (Engulfed && magr == u.ustuck)
                strike = 0;
            else {
                if ((strike = (tmp > rnd(20 + i))))
                    res[i] = gulpmm(magr, mdef, mattk);
                else
                    missmm(magr, mdef, mattk);
            }
            break;

        case AT_MAGC:
            if (dist2(magr->mx, magr->my, mdef->mx, mdef->my) > 2) {
                strike = 0;
                break;
            }

            res[i] = castmm(magr, mdef, mattk);
            if (res[i] & MM_DEF_DIED)
                return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
            strike = 0; /* waking up handled by spell code */
            break;

        default:       /* no attack */
            strike = 0;
            attk = 0;
            break;
        }

        if (attk && !(res[i] & MM_AGR_DIED) &&
            dist2(magr->mx, magr->my, mdef->mx, mdef->my) < 3)
            res[i] = passivemm(magr, mdef, strike, res[i] & MM_DEF_DIED);

        if (res[i] & MM_DEF_DIED)
            return res[i];

        /* 
         *  Wake up the defender.  NOTE:  this must follow the check
         *  to see if the defender died.  We don't want to modify
         *  unallocated monsters!
         */
        if (strike)
            mdef->msleeping = 0;

        if (res[i] & MM_AGR_DIED)
            return res[i];
        /* return if aggressor can no longer attack */
        if (!magr->mcanmove || magr->msleeping)
            return res[i];
        if (res[i] & MM_HIT)
            struck = 1; /* at least one hit */
    }

    return struck ? MM_HIT : MM_MISS;
}

/* Returns the result of mdamagem(). */
static int
hitmm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    if (vis) {
        int compat;

        if (!canspotmonoritem(magr))
            map_invisible(magr->mx, magr->my);
        if (!canspotmonoritem(mdef))
            map_invisible(mdef->mx, mdef->my);
        if (mdef->m_ap_type)
            seemimic(mdef);
        if (magr->m_ap_type)
            seemimic(magr);
        if ((compat = could_seduce(magr, mdef, mattk)) && !magr->mcan) {
            pline("%s %s %s %s.", Monnam(magr),
                  mdef->mcansee ? "smiles at" : "talks to", mon_nam(mdef),
                  compat == 2 ? "engagingly" : "seductively");
        } else {
            const char *buf = Monnam(magr);

            switch (mattk->aatyp) {
            case AT_BITE:
                buf = msgcat(buf, " bites");
                break;
            case AT_STNG:
                buf = msgcat(buf, " stings");
                break;
            case AT_BUTT:
                buf = msgcat(buf, " butts");
                break;
            case AT_TUCH:
                buf = msgcat(buf, " touches");
                break;
            case AT_TENT:
                buf = msgcat(s_suffix(buf), " tentacles suck");
                break;
            case AT_HUGS:
                if (magr != u.ustuck) {
                    buf = msgcat(buf, " squeezes");
                    break;
                }
                /* fall through */
            default:
                buf = msgcat(buf, " hits");
            }
            pline("%s %s.", buf, mon_nam_too(mdef, magr));
        }
    } else
        noises(magr, mattk);
    return mdamagem(magr, mdef, mattk);
}


/* Returns the same values as mdamagem(). */
static int
gazemm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    if (vis) {
        pline("%s gazes at %s...", Monnam(magr), mon_nam(mdef));
    }

    if (magr->mcan || !magr->mcansee || (magr->minvis && !perceives(mdef->data))
        || !mdef->mcansee || mdef->msleeping) {
        if (vis)
            pline("but nothing happens.");
        return MM_MISS;
    }
    /* call mon_reflects 2x, first test, then, if visible, print message */
    if (magr->data == &mons[PM_MEDUSA] && mon_reflects(mdef, NULL)) {
        if (canseemon(mdef))
            mon_reflects(mdef, "The gaze is reflected away by %s %s.");
        if (mdef->mcansee) {
            if (mon_reflects(magr, NULL)) {
                if (canseemon(magr))
                    mon_reflects(magr, "The gaze is reflected away by %s %s.");
                return MM_MISS;
            }
            if (mdef->minvis && !perceives(magr->data)) {
                if (canseemon(magr)) {
                    pline("%s doesn't seem to notice that %s gaze was "
                          "reflected.", Monnam(magr), mhis(magr));
                }
                return MM_MISS;
            }
            /* TODO: Aren't there other sorts of gaze attacks than this? */
            if (canseemon(magr))
                pline("%s is turned to stone!", Monnam(magr));
            monstone(magr);
            if (!DEADMONSTER(magr))
                return MM_MISS;
            return MM_AGR_DIED;
        }
    }

    return mdamagem(magr, mdef, mattk);
}

/* Returns the same values as mattackm(). */
static int
gulpmm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    xchar ax, ay, dx, dy;
    int status;
    struct obj *obj;

    if (mdef->data->msize >= MZ_HUGE)
        return MM_MISS;

    if (vis)
        pline("%s swallows %s.", Monnam(magr), mon_nam(mdef));

    for (obj = mdef->minvent; obj; obj = obj->nobj)
        snuff_lit(obj);

    /* 
     *  All of this maniuplation is needed to keep the display correct.
     *  There is a flush at the next pline().
     */
    ax = magr->mx;
    ay = magr->my;
    dx = mdef->mx;
    dy = mdef->my;
    /* 
     *  Leave the defender in the monster chain at it's current position,
     *  but don't leave it on the screen.  Move the agressor to the def-
     *  ender's position.
     */
    remove_monster(level, ax, ay);
    place_monster(magr, dx, dy);
    newsym(ax, ay);     /* erase old position */
    newsym(dx, dy);     /* update new position */

    status = mdamagem(magr, mdef, mattk);

    if ((status & MM_AGR_DIED) && (status & MM_DEF_DIED)) {
        ;       /* both died -- do nothing */
    } else if (status & MM_DEF_DIED) {  /* defender died */
        /* 
         *  Note:  remove_monster() was called in relmon(), wiping out
         *  magr from level->monsters[mdef->mx][mdef->my].  We need to
         *  put it back and display it.     -kd
         */
        place_monster(magr, dx, dy);
        newsym(dx, dy);
    } else if (status & MM_AGR_DIED) {  /* agressor died */
        place_monster(mdef, dx, dy);
        newsym(dx, dy);
    } else {    /* both alive, put them back */
        if (cansee(dx, dy))
            pline("%s is regurgitated!", Monnam(mdef));

        place_monster(magr, ax, ay);
        place_monster(mdef, dx, dy);
        newsym(ax, ay);
        newsym(dx, dy);
    }

    return status;
}

static int
explmm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    int result;

    if (magr->mcan)
        return MM_MISS;

    if (cansee(magr->mx, magr->my))
        pline("%s explodes!", Monnam(magr));
    else
        noises(magr, mattk);

    result = mdamagem(magr, mdef, mattk);

    /* Kill off agressor if it didn't die. */
    if (!(result & MM_AGR_DIED)) {
        mondead(magr);
        if (!DEADMONSTER(magr))
            return result;      /* life saved */
        result |= MM_AGR_DIED;
    }
    if (magr->mtame)    /* give this one even if it was visible */
        pline(brief_feeling, "melancholy");

    return result;
}

/*
 *  See comment at top of mattackm(), for return values.
 */
static int
mdamagem(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    struct obj *obj;
    const struct permonst *pa = magr->data;
    const struct permonst *pd = mdef->data;
    int armpro, num, tmp = dice((int)mattk->damn, (int)mattk->damd);
    boolean cancelled;

    if (touch_petrifies(pd) && !resists_ston(magr)) {
        long protector = attk_protection((int)mattk->aatyp);
        long wornitems = magr->misc_worn_check;

        /* wielded weapon gives same protection as gloves here */
        if (otmp != 0)
            wornitems |= W_MASK(os_armg);

        if (protector == 0L ||
            (protector != ~0L && (wornitems & protector) != protector)) {
            if (poly_when_stoned(pa)) {
                mon_to_stone(magr);
                return MM_HIT;  /* no damage during the polymorph */
            }
            if (vis)
                pline("%s turns to stone!", Monnam(magr));
            monstone(magr);
            if (!DEADMONSTER(magr))
                return 0;
            else if (magr->mtame && !vis)
                pline(brief_feeling, "peculiarly sad");
            return MM_AGR_DIED;
        }
    }

    /* cancellation factor is the same as when attacking the hero */
    armpro = magic_negation(mdef);
    cancelled = magr->mcan || !((rn2(3) >= armpro) || !rn2(50));

    switch (mattk->adtyp) {
    case AD_DGST:
        /* eating a Rider or its corpse is fatal */
        if (is_rider(mdef->data)) {
            if (vis)
                pline("%s %s!", Monnam(magr),
                      mdef->data ==
                      &mons[PM_FAMINE] ? "belches feebly, shrivels up and dies"
                      : mdef->data ==
                      &mons[PM_PESTILENCE] ?
                      "coughs spasmodically and collapses" :
                      "vomits violently and drops dead");
            mondied(magr);
            if (!DEADMONSTER(magr))
                return 0;       /* lifesaved */
            else if (magr->mtame && !vis)
                pline(brief_feeling, "queasy");
            return MM_AGR_DIED;
        }
        if (flags.verbose)
            verbalize("Burrrrp!");
        tmp = mdef->mhp;
        /* Use up amulet of life saving */
        if ((obj = mlifesaver(mdef)))
            m_useup(mdef, obj);

        /* Is a corpse for nutrition possible? It may kill magr */
        if (!corpse_chance(mdef, magr, TRUE) || DEADMONSTER(magr))
            break;

        /* Pets get nutrition from swallowing monster whole. No nutrition from
           G_NOCORPSE monster, eg, undead. DGST monsters don't die from undead
           corpses */
        num = monsndx(mdef->data);
        if (magr->mtame && !magr->isminion &&
            !(mvitals[num].mvflags & G_NOCORPSE)) {
            struct obj *virtualcorpse =
                mksobj(level, CORPSE, FALSE, FALSE, rng_main);
            int nutrit;

            virtualcorpse->corpsenm = num;
            virtualcorpse->owt = weight(virtualcorpse);
            nutrit = dog_nutrition(magr, virtualcorpse);
            dealloc_obj(virtualcorpse);

            /* only 50% nutrition, 25% of normal eating time */
            if (magr->meating > 1)
                magr->meating = (magr->meating + 3) / 4;
            if (nutrit > 1)
                nutrit /= 2;
            EDOG(magr)->hungrytime += nutrit;
        }
        break;
    case AD_STUN:
        if (magr->mcan)
            break;
        if (canseemon(mdef))
            pline("%s %s for a moment.", Monnam(mdef),
                  makeplural(stagger(mdef->data, "stagger")));
        mdef->mstun = 1;
        goto physical;
    case AD_LEGS:
        if (magr->mcan) {
            tmp = 0;
            break;
        }
        goto physical;
    case AD_WERE:
    case AD_HEAL:
    case AD_PHYS:
    physical:
        if (mattk->aatyp == AT_KICK && thick_skinned(pd)) {
            tmp = 0;
        } else if (mattk->aatyp == AT_WEAP) {
            if (otmp) {
                if (otmp->otyp == CORPSE &&
                    touch_petrifies(&mons[otmp->corpsenm]))
                    goto do_stone;
                tmp += dmgval(otmp, mdef);
                if (otmp->oartifact) {
                    artifact_hit(magr, mdef, otmp, &tmp, dieroll);
                    if (DEADMONSTER(mdef))
                        return (MM_DEF_DIED |
                                (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
                }
                if (tmp)
                    mrustm(magr, mdef, otmp);
            }
        } else if (magr->data == &mons[PM_PURPLE_WORM] &&
                   mdef->data == &mons[PM_SHRIEKER]) {
            /* hack to enhance mm_aggression(); we don't want purple worm's
               bite attack to kill a shrieker because then it won't swallow the 
               corpse; but if the target survives, the subsequent engulf attack 
               should accomplish that */
            if (tmp >= mdef->mhp)
                tmp = mdef->mhp - 1;
        }
        break;
    case AD_FIRE:
        if (cancelled) {
            tmp = 0;
            break;
        }
        if (vis)
            pline("%s is %s!", Monnam(mdef), on_fire(mdef->data, mattk));
        if (pd == &mons[PM_STRAW_GOLEM] || pd == &mons[PM_PAPER_GOLEM]) {
            if (vis)
                pline("%s burns completely!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline("May %s roast in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        tmp += destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
        tmp += destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        if (resists_fire(mdef)) {
            if (vis)
                pline("The fire doesn't seem to burn %s!", mon_nam(mdef));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_FIRE, tmp);
            tmp = 0;
        }
        /* only potions damage resistant players in destroy_item */
        tmp += destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
        break;
    case AD_COLD:
        if (cancelled) {
            tmp = 0;
            break;
        }
        if (vis)
            pline("%s is covered in frost!", Monnam(mdef));
        if (resists_cold(mdef)) {
            if (vis)
                pline("The frost doesn't seem to chill %s!", mon_nam(mdef));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_COLD, tmp);
            tmp = 0;
        }
        tmp += destroy_mitem(mdef, POTION_CLASS, AD_COLD);
        break;
    case AD_ELEC:
        if (cancelled) {
            tmp = 0;
            break;
        }
        if (vis)
            pline("%s gets zapped!", Monnam(mdef));
        tmp += destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        if (resists_elec(mdef)) {
            if (vis)
                pline("The zap doesn't shock %s!", mon_nam(mdef));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_ELEC, tmp);
            tmp = 0;
        }
        /* only rings damage resistant players in destroy_item */
        tmp += destroy_mitem(mdef, RING_CLASS, AD_ELEC);
        break;
    case AD_ACID:
        if (magr->mcan) {
            tmp = 0;
            break;
        }
        if (resists_acid(mdef)) {
            if (vis)
                pline("%s is covered in acid, but it seems harmless.",
                      Monnam(mdef));
            tmp = 0;
        } else if (vis) {
            pline("%s is covered in acid!", Monnam(mdef));
            pline("It burns %s!", mon_nam(mdef));
        }
        if (!rn2(30))
            hurtarmor(mdef, ERODE_CORRODE);
        if (!rn2(6))
            acid_damage(MON_WEP(mdef));
        break;
    case AD_RUST:
        if (magr->mcan)
            break;
        if (pd == &mons[PM_IRON_GOLEM]) {
            if (vis)
                pline("%s falls to pieces!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline("May %s rust in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        hurtarmor(mdef, ERODE_RUST);
        mdef->mstrategy &= ~STRAT_WAITFORU;
        tmp = 0;
        break;
    case AD_CORR:
        if (magr->mcan)
            break;
        hurtarmor(mdef, ERODE_CORRODE);
        mdef->mstrategy &= ~STRAT_WAITFORU;
        tmp = 0;
        break;
    case AD_DCAY:
        if (magr->mcan)
            break;
        if (pd == &mons[PM_WOOD_GOLEM] || pd == &mons[PM_LEATHER_GOLEM]) {
            if (vis)
                pline("%s falls to pieces!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline("May %s rot in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        hurtarmor(mdef, ERODE_ROT);
        mdef->mstrategy &= ~STRAT_WAITFORU;
        tmp = 0;
        break;
    case AD_STON:
        if (magr->mcan)
            break;
    do_stone:
        /* may die from the acid if it eats a stone-curing corpse */
        if (munstone(mdef, FALSE))
            goto post_stone;
        if (poly_when_stoned(pd)) {
            mon_to_stone(mdef);
            tmp = 0;
            break;
        }
        if (!resists_ston(mdef)) {
            if (vis)
                pline("%s turns to stone!", Monnam(mdef));
            monstone(mdef);
        post_stone:
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline(brief_feeling, "peculiarly sad");
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        tmp = (mattk->adtyp == AD_STON ? 0 : 1);
        break;
    case AD_TLPT:
        if (!cancelled && tmp < mdef->mhp && !tele_restrict(mdef)) {
            const char *mdef_Monnam = Monnam(mdef);
            /* save the name before monster teleports, otherwise we'll get "it" 
               in the suddenly disappears message */

            mdef->mstrategy &= ~STRAT_WAITFORU;
            rloc(mdef, TRUE);
            if (vis && !canspotmon(mdef) && mdef != u.usteed)
                pline("%s suddenly disappears!", mdef_Monnam);
        }
        break;
    case AD_SLEE:
        if (!cancelled && !mdef->msleeping && sleep_monst(mdef, rnd(10), -1)) {
            if (vis)
                pline("%s is put to sleep by %s.", Monnam(mdef), mon_nam(magr));

            mdef->mstrategy &= ~STRAT_WAITFORU;
            slept_monst(mdef);
        }
        break;
    case AD_PLYS:
        if (!cancelled && mdef->mcanmove) {
            if (vis)
                pline("%s is frozen by %s.", Monnam(mdef), mon_nam(magr));

            mdef->mcanmove = 0;
            mdef->mfrozen = rnd(10);
            mdef->mstrategy &= ~STRAT_WAITFORU;
        }
        break;
    case AD_SLOW:
        if (!cancelled && mdef->mspeed != MSLOW) {
            unsigned int oldspeed = mdef->mspeed;

            mon_adjust_speed(mdef, -1, NULL);
            mdef->mstrategy &= ~STRAT_WAITFORU;
            if (mdef->mspeed != oldspeed && vis)
                pline("%s slows down.", Monnam(mdef));
        }
        break;
    case AD_CONF:
        /* Since confusing another monster doesn't have a real time limit,
           setting spec_used would not really be right (though we still should
           check for it). */
        if (!magr->mcan && !mdef->mconf && !magr->mspec_used) {
            if (vis)
                pline("%s looks confused.", Monnam(mdef));
            mdef->mconf = 1;
            mdef->mstrategy &= ~STRAT_WAITFORU;
        }
        break;
    case AD_BLND:
        if (can_blnd(magr, mdef, mattk->aatyp, NULL)) {
            unsigned rnd_tmp;

            if (vis && mdef->mcansee)
                pline("%s is blinded.", Monnam(mdef));
            rnd_tmp = dice((int)mattk->damn, (int)mattk->damd);
            if ((rnd_tmp += mdef->mblinded) > 127)
                rnd_tmp = 127;
            mdef->mblinded = rnd_tmp;
            mdef->mcansee = 0;
            mdef->mstrategy &= ~STRAT_WAITFORU;
        }
        tmp = 0;
        break;
    case AD_HALU:
        if (!magr->mcan && haseyes(pd) && mdef->mcansee) {
            if (vis)
                pline("%s looks %sconfused.", Monnam(mdef),
                      mdef->mconf ? "more " : "");
            mdef->mconf = 1;
            mdef->mstrategy &= ~STRAT_WAITFORU;
        }
        tmp = 0;
        break;
    case AD_CURS:
        if (!night() && (pa == &mons[PM_GREMLIN]))
            break;
        if (!magr->mcan && !rn2(10)) {
            mdef->mcan = 1;     /* cancelled regardless of lifesave */
            mdef->mstrategy &= ~STRAT_WAITFORU;
            if (is_were(pd) && pd->mlet != S_HUMAN)
                were_change(mdef);
            if (pd == &mons[PM_CLAY_GOLEM]) {
                if (vis) {
                    pline("Some writing vanishes from %s head!",
                          s_suffix(mon_nam(mdef)));
                    pline("%s is destroyed!", Monnam(mdef));
                }
                mondied(mdef);
                if (!DEADMONSTER(mdef))
                    return 0;
                else if (mdef->mtame && !vis)
                    pline(brief_feeling, "strangely sad");
                return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
            }
            if (canhear()) {
                if (!vis)
                    You_hear("laughter.");
                else
                    pline("%s chuckles.", Monnam(magr));
            }
        }
        break;
    case AD_SGLD: {
        tmp = 0;
        if (magr->mcan)
            break;
        /* technically incorrect; no check for stealing gold from between
           mdef's feet... */
        {
            struct obj *gold = findgold(mdef->minvent);

            if (!gold)
                break;
            obj_extract_self(gold);
            add_to_minv(magr, gold);
        }
        mdef->mstrategy &= ~STRAT_WAITFORU;
        const char *magr_Monnam = Monnam(magr); /* name pre-rloc() */

        if (vis)
            pline("%s steals some gold from %s.", magr_Monnam, mon_nam(mdef));

        if (!tele_restrict(magr)) {
            rloc(magr, TRUE);
            if (vis && !canspotmon(magr))
                pline("%s suddenly disappears!", magr_Monnam);
        }
        break;
    } case AD_DRLI:
        if (!cancelled && !rn2(3) && !resists_drli(mdef)) {
            tmp = dice(2, 6);
            if (vis)
                pline("%s suddenly seems weaker!", Monnam(mdef));
            mdef->mhpmax -= tmp;
            if (mdef->m_lev == 0)
                tmp = mdef->mhp;
            else
                mdef->m_lev--;
            /* Automatic kill if drained past level 0 */
        }
        break;
    case AD_SSEX:
    case AD_SITM:      /* for now these are the same */
    case AD_SEDU:
        if (magr->mcan)
            break;
        /* find an object to steal, non-cursed if magr is tame */
        for (obj = mdef->minvent; obj; obj = obj->nobj)
            if (!magr->mtame || !obj->cursed)
                break;

        if (obj) {
            /* make a special x_monnam() call that never omits the saddle, and
               save it for later messages */
            const char *mdefnambuf =
                x_monnam(mdef, ARTICLE_THE, NULL, 0, FALSE);

            otmp = obj;
            if (u.usteed == mdef && otmp == which_armor(mdef, os_saddle))
                /* "You can no longer ride <steed>." */
                dismount_steed(DISMOUNT_POLY);
            obj_extract_self(otmp);
            if (otmp->owornmask) {
                mdef->misc_worn_check &= ~otmp->owornmask;
                if (otmp->owornmask & W_MASK(os_wep))
                    setmnotwielded(mdef, otmp);
                otmp->owornmask = 0L;
                update_mon_intrinsics(mdef, otmp, FALSE, FALSE);
            }

            /* add_to_minv() might free otmp [if it merges] */
            const char *onambuf = doname(otmp);
            add_to_minv(magr, otmp);

            /* In case of teleport */
            const char *magr_Monnam = Monnam(magr);

            if (vis) {
                pline("%s steals %s from %s!",
                      magr_Monnam, onambuf, mdefnambuf);
            }

            possibly_unwield(mdef, FALSE);
            mdef->mstrategy &= ~STRAT_WAITFORU;
            mselftouch(mdef, NULL, FALSE);
            if (DEADMONSTER(mdef))
                return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
            if (magr->data->mlet == S_NYMPH && !tele_restrict(magr)) {
                rloc(magr, TRUE);
                if (vis && !canspotmon(magr))
                    pline("%s suddenly disappears!", magr_Monnam);
            }
        }
        tmp = 0;
        break;
    case AD_DRST:
    case AD_DRDX:
    case AD_DRCO:
        if (!cancelled && !rn2(8)) {
            if (vis)
                pline("%s %s was poisoned!", s_suffix(Monnam(magr)),
                      mpoisons_subj(magr, mattk));
            if (resists_poison(mdef)) {
                if (vis)
                    pline("The poison doesn't seem to affect %s.",
                          mon_nam(mdef));
            } else {
                if (rn2(10))
                    tmp += rn1(10, 6);
                else {
                    if (vis)
                        pline("The poison was deadly...");
                    tmp = mdef->mhp;
                }
            }
        }
        break;
    case AD_DRIN:
        if (notonhead || !has_head(pd)) {
            if (vis)
                pline("%s doesn't seem harmed.", Monnam(mdef));
            /* Not clear what to do for green slimes */
            tmp = 0;
            break;
        }
        if ((mdef->misc_worn_check & W_MASK(os_armh)) && rn2(8)) {
            if (vis) {
                pline("%s %s blocks %s attack to %s head.",
                      s_suffix(Monnam(mdef)),
                      helmet_name(which_armor(mdef, os_armh)),
                      s_suffix(mon_nam(magr)), mhis(mdef));
            }
            break;
        }
        if (vis)
            pline("%s brain is eaten!", s_suffix(Monnam(mdef)));
        if (mindless(pd)) {
            if (vis)
                pline("%s doesn't notice.", Monnam(mdef));
            break;
        }
        tmp += rnd(10); /* fakery, since monsters lack INT scores */
        if (magr->mtame && !magr->isminion) {
            EDOG(magr)->hungrytime += rnd(60);
            magr->mconf = 0;
        }
        if (tmp >= mdef->mhp && vis)
            pline("%s last thought fades away...", s_suffix(Monnam(mdef)));
        break;
    case AD_DETH:
        if (vis)
            pline("%s reaches out with its deadly touch.", Monnam(magr));
        if (is_undead(mdef->data)) {
            /* Still does normal damage */
            if (vis)
                pline("%s looks no deader than before.", Monnam(mdef));
            break;
        }
        switch (rn2(20)) {
        case 19:
        case 18:
        case 17:
            if (!resist(mdef, 0, 0, 0)) {
                monkilled(mdef, "", AD_DETH);
                if (DEADMONSTER(mdef))            /* did it lifesave? */
                    return MM_DEF_DIED;

                tmp = 0;
                break;
            }   /* else FALLTHRU */
        default:       /* case 16: ... case 5: */
            if (vis)
                pline("%s looks weaker!", Monnam(mdef));
            mdef->mhpmax -= rn2(tmp / 2 + 1);   /* mhp will then still be less
                                                   than this value */
            break;
        case 4:
        case 3:
        case 2:
        case 1:
        case 0:
            if (Antimagic)
                shieldeff(mdef->mx, mdef->my);
            if (vis)
                pline("That didn't work...");
            tmp = 0;
            break;
        }
        break;
    case AD_PEST:
        if (vis)
            pline("%s reaches out, and %s looks rather ill.", Monnam(magr),
                  mon_nam(mdef));
        if ((mdef->mhpmax > 3) && !resist(mdef, 0, 0, NOTELL))
            mdef->mhpmax /= 2;
        if ((mdef->mhp > 2) && !resist(mdef, 0, 0, NOTELL))
            mdef->mhp /= 2;
        if (mdef->mhp > mdef->mhpmax)
            mdef->mhp = mdef->mhpmax;
        break;
    case AD_FAMN:
        if (vis)
            pline("%s reaches out, and %s body shrivels.",
                  Monnam(magr), s_suffix(mon_nam(mdef)));
        if (mdef->mtame && !mdef->isminion)
            EDOG(mdef)->hungrytime -= rn1(120, 120);
        else {
            tmp += rnd(10);     /* lacks a food rating */
            if (tmp >= mdef->mhp && vis)
                pline("%s starves.", Monnam(mdef));
        }
        /* plus the normal damage */
        break;
    case AD_SLIM:
        if (cancelled)
            break;      /* physical damage only */
        if (!rn2(4) && !flaming(mdef->data) && !unsolid(mdef->data) &&
            mdef->data != &mons[PM_GREEN_SLIME]) {
            newcham(mdef, &mons[PM_GREEN_SLIME], FALSE, vis);
            mdef->mstrategy &= ~STRAT_WAITFORU;
            tmp = 0;
        }
        break;
    case AD_STCK:
        if (cancelled)
            tmp = 0;
        break;
    case AD_WRAP:      /* monsters cannot grab one another, it's too hard */
        if (magr->mcan)
            tmp = 0;
        break;
    case AD_ENCH:
        /* there's no msomearmor() function, so just do damage */
        /* if (cancelled) break; */
        break;
    default:
        tmp = 0;
        break;
    }
    if (!tmp)
        return MM_MISS;

    if ((mdef->mhp -= tmp) < 1) {
        if (m_at(level, mdef->mx, mdef->my) == magr) {  /* see gulpmm() */
            remove_monster(level, mdef->mx, mdef->my);
            mdef->mhp = 1;      /* otherwise place_monster will complain */
            place_monster(mdef, mdef->mx, mdef->my);
            mdef->mhp = 0;
        }
        monkilled(mdef, "", (int)mattk->adtyp);
        if (!DEADMONSTER(mdef))
            return 0;   /* mdef lifesaved */

        if (mattk->adtyp == AD_DGST) {
            /* various checks similar to dog_eat and meatobj. after monkilled() 
               to provide better message ordering */
            if (mdef->cham != CHAM_ORDINARY) {
                newcham(magr, NULL, FALSE, TRUE);
            } else if (mdef->data == &mons[PM_GREEN_SLIME]) {
                newcham(magr, &mons[PM_GREEN_SLIME], FALSE, TRUE);
            } else if (mdef->data == &mons[PM_WRAITH]) {
                grow_up(magr, NULL);
                /* don't grow up twice */
                return MM_DEF_DIED | (DEADMONSTER(magr) ? MM_AGR_DIED : 0);
            } else if (mdef->data == &mons[PM_NURSE]) {
                magr->mhp = magr->mhpmax;
            }
        }

        return MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED);
    }
    return MM_HIT;
}


int
noattacks(const struct permonst *ptr)
{       /* returns 1 if monster doesn't attack */
    int i;

    for (i = 0; i < NATTK; i++)
        if (ptr->mattk[i].aatyp)
            return 0;

    return 1;
}

/* `mon' is hit by a sleep attack; return 1 if it's affected, 0 otherwise */
int
sleep_monst(struct monst *mon, int amt, int how)
{
    if (resists_sleep(mon) || (how >= 0 && resist(mon, (char)how, 0, NOTELL))) {
        shieldeff(mon->mx, mon->my);
    } else if (mon->mcanmove) {
        amt += (int)mon->mfrozen;
        if (amt > 0) {  /* sleep for N turns */
            mon->mcanmove = 0;
            mon->mfrozen = min(amt, 127);
        } else {        /* sleep until awakened */
            mon->msleeping = 1;
        }
        return 1;
    }
    return 0;
}

/* sleeping grabber releases, engulfer doesn't; don't use for paralysis! */
void
slept_monst(struct monst *mon)
{
    if ((mon->msleeping || !mon->mcanmove) && mon == u.ustuck &&
        !sticks(youmonst.data) && !Engulfed) {
        pline("%s grip relaxes.", s_suffix(Monnam(mon)));
        unstuck(mon);
    }
}


void
mrustm(struct monst *magr, struct monst *mdef, struct obj *obj)
{
    enum erode_type type;

    if (!magr || !mdef || !obj)
        return; /* just in case */

    if (dmgtype(mdef->data, AD_CORR))
        type = ERODE_CORRODE;
    else if (dmgtype(mdef->data, AD_RUST))
        type = ERODE_RUST;
    else
        return;

    if (mdef->mcan)
        return;

    erode_obj(obj, NULL, type, TRUE, TRUE);
}

static void
mswingsm(struct monst *magr, struct monst *mdef, struct obj *otemp)
{
    if (!flags.verbose || Blind || !mon_visible(magr))
        return;

    pline("%s %s %s %s at %s.", Monnam(magr),
          (objects[otemp->otyp].oc_dir & PIERCE) ? "thrusts" : "swings",
          mhis(magr), singular(otemp, xname), mon_nam(mdef));
}

/*
 * Passive responses by defenders.  Does not replicate responses already
 * handled above.  Returns same values as mattackm.
 */
static int
passivemm(struct monst *magr, struct monst *mdef, boolean mhit, int mdead)
{
    const struct permonst *mddat = mdef->data;
    const struct permonst *madat = magr->data;
    int i, tmp;

    for (i = 0;; i++) {
        if (i >= NATTK)
            return mdead | mhit;        /* no passive attacks */
        if (mddat->mattk[i].aatyp == AT_NONE)
            break;
    }
    if (mddat->mattk[i].damn)
        tmp = dice((int)mddat->mattk[i].damn, (int)mddat->mattk[i].damd);
    else if (mddat->mattk[i].damd)
        tmp = dice((int)mddat->mlevel + 1, (int)mddat->mattk[i].damd);
    else
        tmp = 0;

    /* These affect the enemy even if defender killed */
    switch (mddat->mattk[i].adtyp) {
    case AD_ACID:
        if (mhit && !rn2(2)) {
            if (canseemon(magr))
                pline("%s is splashed by %s acid!", Monnam(magr),
                      s_suffix(mon_nam(mdef)));
            if (resists_acid(magr)) {
                if (canseemon(magr))
                    pline("%s is not affected.", Monnam(magr));
                tmp = 0;
            }
        } else
            tmp = 0;
        goto assess_dmg;
    case AD_MAGM:
        /* wrath of gods for attacking Oracle */
        if (resists_magm(magr)) {
            if (canseemon(magr)) {
                shieldeff(magr->mx, magr->my);
                pline("A hail of magic missiles narrowly misses %s!",
                      mon_nam(magr));
            }
        } else {
            if (canseemon(magr)) {
                if (magr->data == &mons[PM_WOODCHUCK]) {
                    pline("ZOT!");
                } else {
                    pline("%s is caught in a hail of magic missiles!",
                          Monnam(magr));
                }
            }
            goto assess_dmg;
        }
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        if (mhit && !mdef->mcan && otmp) {
            drain_item(otmp);
            /* No message */
        }
        break;
    default:
        break;
    }
    if (mdead || mdef->mcan)
        return mdead | mhit;

    /* These affect the enemy only if defender is still alive */
    if (rn2(3))
        switch (mddat->mattk[i].adtyp) {
        case AD_PLYS:  /* Floating eye */
            if (tmp > 127)
                tmp = 127;
            if (mddat == &mons[PM_FLOATING_EYE]) {
                if (!rn2(4))
                    tmp = 127;
                if (magr->mcansee && haseyes(madat) && mdef->mcansee &&
                    (perceives(madat) || !mdef->minvis)) {
                    const char *buf;
                    buf = msgprintf("%s gaze is reflected by %%s %%s.",
                                    s_suffix(mon_nam(mdef)));
                    if (mon_reflects(magr, canseemon(magr) ? buf : NULL))
                        return mdead | mhit;
                    if (canseemon(magr))
                        pline("%s is frozen by %s gaze!", Monnam(magr),
                              s_suffix(mon_nam(mdef)));
                    magr->mcanmove = 0;
                    magr->mfrozen = tmp;
                    return mdead | mhit;
                }
            } else {    /* gelatinous cube */
                if (canseemon(magr))
                    pline("%s is frozen by %s.", Monnam(magr), mon_nam(mdef));
                magr->mcanmove = 0;
                magr->mfrozen = tmp;
                return mdead | mhit;
            }
            return 1;
        case AD_COLD:
            if (resists_cold(magr)) {
                if (canseemon(magr)) {
                    pline("%s is mildly chilly.", Monnam(magr));
                    golemeffects(magr, AD_COLD, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline("%s is suddenly very cold!", Monnam(magr));
            mdef->mhp += tmp / 2;
            if (mdef->mhpmax < mdef->mhp)
                mdef->mhpmax = mdef->mhp;
            if (mdef->mhpmax > ((int)(mdef->m_lev + 1) * 8))
                split_mon(mdef, magr);
            break;
        case AD_STUN:
            if (!magr->mstun) {
                magr->mstun = 1;
                if (canseemon(magr))
                    pline("%s %s...", Monnam(magr),
                          makeplural(stagger(magr->data, "stagger")));
            }
            tmp = 0;
            break;
        case AD_FIRE:
            if (resists_fire(magr)) {
                if (canseemon(magr)) {
                    pline("%s is mildly warmed.", Monnam(magr));
                    golemeffects(magr, AD_FIRE, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline("%s is suddenly very hot!", Monnam(magr));
            break;
        case AD_ELEC:
            if (resists_elec(magr)) {
                if (canseemon(magr)) {
                    pline("%s is mildly tingled.", Monnam(magr));
                    golemeffects(magr, AD_ELEC, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline("%s is jolted with electricity!", Monnam(magr));
            break;
        default:
            tmp = 0;
            break;
    } else
        tmp = 0;

assess_dmg:
    if ((magr->mhp -= tmp) <= 0) {
        monkilled(magr, "", (int)mddat->mattk[i].adtyp);
        return mdead | mhit | MM_AGR_DIED;
    }
    return mdead | mhit;
}

/* "aggressive defense"; what type of armor prevents specified attack
   from touching its target? */
long
attk_protection(int aatyp)
{
    long w_mask = 0L;

    switch (aatyp) {
    case AT_NONE:
    case AT_SPIT:
    case AT_EXPL:
    case AT_BOOM:
    case AT_GAZE:
    case AT_BREA:
    case AT_MAGC:
        w_mask = ~0L;   /* special case; no defense needed */
        break;
    case AT_CLAW:
    case AT_TUCH:
    case AT_WEAP:
        w_mask = W_MASK(os_armg);   /* caller needs to check for weapon */
        break;
    case AT_KICK:
        w_mask = W_MASK(os_armf);
        break;
    case AT_BUTT:
        w_mask = W_MASK(os_armh);
        break;
    case AT_HUGS:
        /* attacker needs both cloak and gloves to be protected */
        w_mask = (W_MASK(os_armc) | W_MASK(os_armg));
        break;
    case AT_BITE:
    case AT_STNG:
    case AT_ENGL:
    case AT_TENT:
    default:
        w_mask = 0L;    /* no defense available */
        break;
    }
    return w_mask;
}

/*mhitm.c*/

