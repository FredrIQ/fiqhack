/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

static boolean vis, far_noise;
static long noisetime;
static struct obj *otmp;

static const char brief_feeling[] =
    "You have a %s feeling for a moment, then it passes.";

enum monster_attitude {
    matt_hostile = 0,
    matt_knownpeaceful = 1,
    matt_knowntame = 2,
    matt_player = 3,
    matt_notarget = 4,
};
static enum monster_attitude monster_known_attitude(const struct monst *);
static const char *mon_nam_too(struct monst *, struct monst *);
static int hitmm(struct monst *, struct monst *, const struct attack *);
static int gulpmm(struct monst *, struct monst *, const struct attack *);
static int explmm(struct monst *, struct monst *, const struct attack *);
static int attack_result(struct monst *, struct monst *);
static void do_damage(struct monst *, struct monst *, int,
                      const struct attack *);
static int mdamagem(struct monst *, struct monst *, const struct attack *);
static void noises(struct monst *, const struct attack *);
static void missmm(struct monst *, struct monst *, const struct attack *);
static int passivemm(struct monst *, struct monst *, boolean, int);
static void set_at_area(int, int, void *);
static void maurahitpile(struct monst *, int, int, const struct attack *);
static void maurahitm(struct monst *, struct monst *, const struct attack *);

/* Needed for the special case of monsters wielding vorpal blades (rare). If we
   use this a lot it should probably be a parameter to mdamagem() instead of a
   global variable. */
static int dieroll;

/* Returns the player's belief about a monster/youmonst's attitude, defaulting
   to hostile in the case where the player isn't sure. The monster must be
   onlevel; the caller is responsible for checking this. */
static enum monster_attitude
monster_known_attitude(const struct monst *mtmp)
{
    if (mtmp == &youmonst)
        return matt_player;
    if (mtmp == NULL)
        return matt_notarget;
    if (!canspotmon(mtmp))
        return matt_hostile;
    if (mtmp->mpeaceful)
        return mtmp->mtame ? matt_knowntame : matt_knownpeaceful;
    return matt_hostile;
}


/* Return an appropriate message channel for a message about an attack by magr
   against mdef (taking into account whether the player is involved and the
   tameness of the monsters in question, in addition to whether the attack
   worked).

   mdef can be NULL, for a self-buff or the like, in which case cr_hit means
   that it worked, cr_miss means that it failed, and cr_immune means that it can
   never work. If magr is NULL, it means that the attack was made by a trap or
   similar dungeon feature.  */
enum msg_channel
combat_msgc(const struct monst *magr, const struct monst *mdef,
            enum combatresult cr)
{
    /* Combat on a different level shouldn't give feedback at all. (We need to
       run this check first because we can't do offlevel LOS checks yet.) This
       case shouldn't happen in a single-player game, and because multiplayer is
       not implemented yet, currently generates an impossible() when it reaches
       pline(). */
    if (magr && magr != &youmonst && magr->dlevel != level)
        return msgc_offlevel;
    if (mdef && mdef != &youmonst && mdef->dlevel != level)
        return msgc_offlevel;

    /* If the defender is dying, there are special categories of message for
       that. Assumes that if the player is dying, the caller is about to call
       done(), perhaps indirectly. Note that most callers print the message for
       the hit and death separately, in which case the hit can still use cr_hit;
       this is more for instadeaths and for after the damage is dealt. */
    if (cr == cr_kill || cr == cr_kill0) {
        if (mdef == &youmonst)
            return msgc_fatal_predone;
        if (mdef->mtame)
            return cr == cr_kill ? msgc_petfatal : msgc_petwarning;
        if (magr == &youmonst)
            return msgc_kill;
        if (magr && magr->mtame)
            return msgc_petkill;
        cr = cr_hit;
    }

    /* Immunities normally channelize as misses.  Exception: if the player is
       attacking or defending.  For now, resistances are always channelized like
       hits; that might change in the future. */
    if (cr == cr_immune) {
        if (magr == &youmonst)
            return mdef ? msgc_combatimmune : msgc_yafm;
        if (mdef == &youmonst)
            return msgc_playerimmune;
        cr = cr_miss;
    } else if (cr == cr_resist)
        cr = cr_hit;

    /* We have 50 cases (hostile, peaceful, tame, player, absent for both
       attacker and defender, plus hit/miss, = 5 * 5 * 2). We convert this into
       a single number for a table lookup.  */
    return (monster_known_attitude(magr) * 10 +
            monster_known_attitude(mdef) * 2 + (cr == cr_miss))[
        (const enum msg_channel[50]){
            /* hit */           /* missed */
            msgc_monneutral,    msgc_monneutral,    /* hostile vs. hostile */
            msgc_moncombatbad,  msgc_moncombatgood, /* hostile vs. peaceful */
            msgc_moncombatbad,  msgc_moncombatgood, /* hostile vs. tame */
            msgc_moncombatbad,  msgc_moncombatgood, /* hostile vs. player */
            msgc_moncombatbad,  msgc_moncombatgood, /* hostile undirected */

            msgc_petcombatgood, msgc_petcombatbad,  /* peaceful vs. hostile */
            msgc_monneutral,    msgc_monneutral,    /* peaceful vs. peaceful */
            msgc_moncombatbad,  msgc_moncombatbad,  /* peaceful vs. tame */
            msgc_moncombatbad,  msgc_moncombatbad,  /* peaceful vs. player */
            msgc_monneutral,    msgc_monneutral,    /* peaceful undirected */

            msgc_petcombatgood, msgc_petcombatbad,  /* tame vs. hostile */
            msgc_petcombatgood, msgc_petcombatbad,  /* tame vs. peaceful */
            msgc_petfatal,      msgc_petfatal,      /* tame vs. tame */
            msgc_petwarning,    msgc_petwarning,    /* tame vs. player */
            msgc_petneutral,    msgc_petneutral,    /* tame undirected */

            msgc_combatgood,    msgc_failrandom,     /* player vs. hostile */
            msgc_combatgood,    msgc_failrandom,     /* player vs. peaceful */
            msgc_badidea,       msgc_failrandom,    /* player vs. tame */
            msgc_badidea,       msgc_failrandom,    /* attacking yourself */
            msgc_actionok,      msgc_failrandom,    /* player undirected */

            msgc_monneutral,    msgc_monneutral,    /* trap vs. hostile */
            msgc_monneutral,    msgc_monneutral,    /* trap vs. peaceful */
            msgc_petwarning,    msgc_nonmongood,    /* trap vs. tame */
            msgc_nonmonbad,     msgc_nonmongood,    /* trap vs. player */
            msgc_monneutral,    msgc_monneutral,    /* trap vs. unattended */
        }];
}

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

    if (farq != far_noise || moves - noisetime > 1) {
        far_noise = farq;
        noisetime = moves;
        You_hear(msgc_levelsound, "%s%s.",
                 (mattk->aatyp == AT_EXPL) ? "an explosion" : "some noises",
                 farq ? " in the distance" : " nearby");
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
               !cancelled(magr)) ? "%s pretends to be friendly to" : "%s misses";
        buf = msgprintf(fmt, Monnam(magr));
        pline(combat_msgc(magr, mdef, cr_miss),
              "%s %s.", buf, mon_nam_too(mdef, magr));
    } else
        noises(magr, mattk);
}

/* fightm() -- find some other monster to fight in melee. Returns 1 if the monster
   attacked something, 0 otherwise */
int
fightm(struct monst *mon)
{
    int result;
    boolean conflicted = (Conflict && !resist(&youmonst, mon, RING_CLASS, 0, 0) &&
                          m_canseeu(mon) && distu(mon->mx, mon->my) < (BOLT_LIM * BOLT_LIM));
    boolean mercy = FALSE;
    if (mon->mw && (obj_properties(mon->mw) & opm_mercy) &&
        mon->mw->mknown)
        mercy = TRUE;

    /* perhaps we're holding it... */
    if (itsstuck(mon))
        return 0;

    /* ignore tame monsters for now, it has its' own movement logic alltogether */
    if (mon->mtame)
        return 0;

    int dirx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    int diry[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

    /* try each in a random order, so do a shuffle */
    int try[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    int i, old, change;
    for (i = 0; i < 8; i++) {
        change = rn2(8);
        old = try[i];
        try[i] = try[change];
        try[change] = old;
    }

    /* check for monsters on positions */
    struct monst *mtmp;
    int x, y;
    for (i = 0; i < 8; i++) {
        x = mon->mx + dirx[try[i]];
        y = mon->my + diry[try[i]];
        mtmp = m_at(level, x, y);
        if (!mtmp || (!mm_aggression(mon, mtmp) && !conflicted &&
                      !mercy))
            continue;
        if (mercy && mon->mpeaceful != (mtmp == &youmonst ? 1 :
                                        mtmp->mpeaceful))
            continue;

        /* TODO: why are these needed... */
        bhitpos.x = mtmp->mx;
        bhitpos.y = mtmp->my;
        notonhead = 0; /* what if it is? */
        result = mattackm(mon, mtmp);

        /* For engulfers, a spent turn also gives an opportunity to continue
           hitting the hero (digestion, cold attacks, whatever) */
        if ((result & MM_AGR_DIED) ||
            (Engulfed && mon == u.ustuck && mattacku(mon)))
            return 1; /* monster died */

        /* Allow attacked monsters a chance to hit back. Primarily to
           allow monsters that resist conflict to respond.

           Note: in 4.3, this no longer costs movement points, because
           it throws off the turn alternation.
           TODO: might want to handle sanity checks before retaliating,
           since this is no longer exclusively called by conflict */
        if ((result & MM_HIT) && !(result & MM_DEF_DIED)) {
            notonhead = 0;
            mattackm(mtmp, mon); /* retaliation */
        }

        /* Turn is spent, so return 1. This used to return 0 if the attack was
           a miss or if the monster has you engulfed, but missing attacks shouldn't
           allow further movement, and engulfement is taken care of above */
        return 1;
    }
    return 0; /* no suitable target */
}

/*
 * mattackm() -- a monster attacks another monster.
 *
 * This function returns a result bitfield:
 *
 *           ----------- defender was "hurriedly expelled"
 *          /  --------- aggressor died
 *         /  /  ------- defender died
 *        /  /  /  ----- defender was hit
 *       /  /  /  /
 *      x  x  x  x
 *
 *      0x8     MM_EXPELLED
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

    if (confused(mdef) || !mdef->mcanmove || mdef->msleeping) {
        tmp += 4;
        mdef->msleeping = 0;
    }
    tmp += hitbon(magr);

    /* undetect monsters become un-hidden if they are attacked */
    if (mdef->mundetected &&
        dist2(mdef->mx, mdef->my, magr->mx, magr->my) > 2) {
        mdef->mundetected = 0;
        newsym(mdef->mx, mdef->my);
        if (canseemon(mdef) && !sensemon(mdef)) {
            if (u_helpless(hm_asleep))
                pline(msgc_levelsound, "You dream of %s.",
                      (mdef-> data->geno & G_UNIQ) ? a_monnam(mdef) :
                          makeplural(m_monnam(mdef)));
            else
                pline(mdef->mpeaceful ? msgc_youdiscover : msgc_levelwarning,
                      "Suddenly, you notice %s.", a_monnam(mdef));
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

        boolean ranged_ok = FALSE;
        if (dist2(magr->mx, magr->my, mdef->mx, mdef->my) > 2 ||
            (magr->data->mlet == S_DRAGON && extra_nasty(magr->data) &&
             !magr->mspec_used && (!cancelled(magr) || rn2(3))))
            ranged_ok = TRUE;

        switch (mattk->aatyp) {
        case AT_WEAP:  /* weapon attacks */
            if (ranged_ok) {
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
                if (obj_properties(otmp) & opm_nasty) {
                    if (vis) {
                        pline(combat_msgc(NULL, magr, cr_hit),
                              "The nasty weapon hurts %s!", mon_nam(magr));
                        learn_oprop(otmp, opm_nasty);
                    }
                    magr->mhp -= rnd(6);
                    if (magr->mhp <= 0) {
                        mondied(magr);
                        return MM_AGR_DIED;
                    }
                }

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
            if (ranged_ok) {
                strike = 0;
                break;  /* might have more ranged attacks */
            }
            /* Monsters won't attack cockatrices physically if they have a
               weapon instead.  This instinct doesn't work for players, or
               under conflict or confusion. */
            if (!confused(magr) && !Conflict && !resists_ston(magr) &&
                otmp && mattk->aatyp != AT_WEAP &&
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
                    !(obj_properties(otmp) & opm_mercy) &&
                    mdef->mhp >= 2 && !cancelled(mdef)) {
                    if (clone_mon(mdef, 0, 0)) {
                        if (vis) {
                            pline(combat_msgc(mdef, NULL, cr_hit),
                                  "%s divides as %s hits it!",
                                  Monnam(mdef), mon_nam(magr));
                        }
                    }
                }
            } else
                missmm(magr, mdef, mattk);
            break;

        case AT_HUGS:  /* automatic if prev two attacks succeed */
            if (ranged_ok)
                break;

            strike = (i >= 2 && res[i - 1] == MM_HIT && res[i - 2] == MM_HIT);
            if (strike)
                res[i] = hitmm(magr, mdef, mattk);

            break;

        case AT_BREA:
            if (ranged_ok)
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
            if (ranged_ok)
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
            /* both melee and ranged */
            strike = 0; /* will not wake up a sleeper */
            res[i] = gazemm(magr, mdef, mattk);
            break;

        case AT_EXPL:
            if (ranged_ok) {
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
            if (ranged_ok) {
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
        if ((compat = could_seduce(magr, mdef, mattk)) && !cancelled(magr)) {
            pline(combat_msgc(magr, mdef, cr_hit), "%s %s %s %s.",
                  Monnam(magr), !blind(mdef) ? "smiles at" : "talks to",
                  mon_nam(mdef), compat == 2 ? "engagingly" : "seductively");
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
            pline(combat_msgc(magr, mdef, cr_hit), "%s %s.",
                  buf, mon_nam_too(mdef, magr));
        }
    } else
        noises(magr, mattk);
    return mdamagem(magr, mdef, mattk);
}


/* Returns the same values as mdamagem(). */
int
gazemm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean visad = (msensem(magr, mdef) & MSENSE_VISION);
    boolean visda = (msensem(mdef, magr) & MSENSE_VISION);
    boolean vis = (cansee(m_mx(magr), m_my(magr)) &&
                   cansee(m_mx(mdef), m_my(mdef)) &&
                   (canspotmon(magr) || canspotmon(mdef)));
    if (uagr) /* not udef: blindness shouldn't reveal monsters attacking you */
        vis = TRUE;

    /* for mvm gazing, clear_path will always be valid -- but not for mvu */
    boolean valid_range = clear_path(m_mx(magr), m_my(magr), m_mx(mdef), m_my(mdef),
                                     viz_array);
    boolean conf = FALSE;
    int damn = mattk->damn;
    int damd = mattk->damd;
    int dmg = 0;
    if (damd)
        dmg = dice(damn ? damn : m_mlev(magr) / 2 + 1, damd);
    int ret = 0;

    /* gaze attacks except for radiance (AD_BLND) and Medusa share common checks */
    if (mattk->adtyp != AD_BLND && magr->data != &mons[PM_MEDUSA]) {
        if (!cancelled(magr) && !blind(magr) && valid_range && visda &&
            (uagr || !magr->mspec_used) && rn2(5)) {
            if (!rn2(3))
                magr->mspec_used += (dmg + rn2(6));
            else
                return MM_MISS;
        }
    }

    switch (mattk->adtyp) {
    case AD_STON:
        /* It'd be nice to give a warning that there's an instadeath around,
           but there's nowhere to put it; you die on the very first turn the
           monster sees you (which is also the first related message).  The
           warning is instead given via special level architecture (and there's
           also a fatalavoid warning if the attack is used against something
           other than the player). */
        if (cancelled(magr) || blind(magr)) {
            /* Monsters can heal blindness with a healing potion, so we should
               produce the "instadeath averted" warning if the blindness is the
               problem. */
            boolean cancelled_medusa =
                magr->data == &mons[PM_MEDUSA] && cancelled(magr);
            if (vis)
                pline(!cancelled_medusa && !uagr ? msgc_fatalavoid :
                      combat_msgc(magr, mdef, cr_immune),
                      "%s %s.", M_verbs(magr, cancelled_medusa ? "do" : "gaze"),
                      cancelled_medusa ?
                      "n't look all that ugly" : "ineffectually");
            break;
        }
        if (reflecting(mdef) && valid_range && magr->data == &mons[PM_MEDUSA]) {
            /* Having reflection at this point is very common and unlikely to
               go away in a hurry, so it seems best to not print an "instadeath
               averted" message if that's the reason. (There isn't anywhere to
               put it, anyway.) */
            if (vis)
                mon_reflects(mdef, magr, FALSE,
                             "%s gaze is reflected by %s %s.",
                             s_suffix(Monnam(magr)));
            if (reflecting(magr)) {
                if (vis)
                    mon_reflects(magr, mdef, TRUE, 
                                 "%s gaze is reflected further by %s %s!",
                                 uagr ? s_suffix(Monnam(magr)) : "Your");
                break;
            }
            if (!visad) { /* probably you're invisible */
                /* not msgc_combatimmune because this may not be an intentional
                   attempt to reflection-petrify, so we shouldn't give alerts
                   that it isn't working; "hostile monster fails to commit
                   suicide" is a monneutral event */
                if (vis && !uagr)
                    pline(msgc_monneutral, "%sn't seem to notice that %s "
                          "gaze was reflected.", M_verbs(magr, "do"),
                          uagr ? "your" : mhis(magr));
                else if (uagr)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "Nothing seems to happen...");
                break;
            }
            if (vis)
                pline(msgc_kill, "%s turned to stone!", M_verbs(magr, "are"));
            stoned = TRUE;
            /* Currently, only Medusa has a stoning gaze, and players can't polymorph
               into her. However, just in case it is implemented later... */
            if (uagr)
                done(STONING, killer_msg(STONING, "a reflected gaze"));
            else if (udef)
                killed(magr);
            else
                monstone(magr);

            return (!uagr && DEADMONSTER(magr)) ? MM_AGR_DIED : 0;
        }
        if (visda && valid_range && !resists_ston(mdef)) {
            pline(combat_msgc(magr, mdef, cr_kill), "%s %s gaze.",
                  M_verbs(magr, "meet"), s_suffix(mon_nam(magr)));
            if (udef) {
                action_interrupted();
                instapetrify(killer_msg(STONING,
                                        msgprintf("catching the eye of %s",
                                                  k_monnam(magr))));
            } else
                minstapetrify(magr, mdef);
            return (!udef && DEADMONSTER(mdef)) ? MM_DEF_DIED : 0;
        }
        break;
    case AD_CONF:
        conf = TRUE;
    case AD_STUN:
        if (cancelled(magr) || !visda)
            break;

        if (!has_property(mdef, conf ? CONFUSION : STUNNED)) {
            if (vis) {
                if (conf)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s gaze confuses %s!",
                          uagr ? "Your" : s_suffix(Monnam(magr)),
                          udef ? "you" : mon_nam(mdef));
                else
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s piercingly at %s!", M_verbs(magr, "stare"),
                          udef ? "you" : mon_nam(mdef));
            }
        } else if (vis && conf)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s getting more and more confused.", M_verbs(mdef, "are"));
        inc_timeout(mdef, conf ? CONFUSION : STUNNED, dmg,
                    conf ? TRUE : FALSE);
        if (udef)
            action_interrupted();
        ret |= MM_HIT;
        break;
    case AD_BLND:
        if (!cancelled(magr) && visda && !resists_blnd(mdef)
            && dist2(m_mx(magr), m_my(magr),
                     m_mx(mdef), m_my(mdef)) <= BOLT_LIM * BOLT_LIM) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit), "%s blinded by %s radiance!",
                      M_verbs(mdef, "are"), uagr ? "your" : s_suffix(mon_nam(mdef)));
            set_property(mdef, BLINDED, dmg, TRUE);
            if (udef)
                action_interrupted();
            /* not blind at this point implies you're wearing the Eyes of the
               Overworld; make them block this particular stun attack too */
            if (!blind(mdef)) {
                if (vis || udef)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s vision quickly clears.", udef ? "Your" : s_suffix(Monnam(mdef)));
            } else
                inc_timeout(mdef, STUNNED, dice(1, 3), FALSE);
            ret |= MM_HIT;
        }
        break;
    case AD_FIRE:
        if (cancelled(magr) || !visda)
            break;

        if (resists_fire(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s at %s, but %s to catch fire.", M_verbs(magr, "glare"),
                      udef ? "you" : mon_nam(mdef), m_verbs(mdef, "fail"));
            dmg = 0;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s %s with a fiery gaze!", M_verbs(magr, "attack"),
                  udef ? "you" : mon_nam(mdef));
        if (udef)
            action_interrupted();
        burn_away_slime(mdef);
        if (m_mlev(magr) > rn2(20))
            destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
        if (m_mlev(magr) > rn2(20))
            destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
        if (m_mlev(magr) > rn2(25))
            destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        /* this used to call mdamageu, but since mdamageu and mdamagem doesn't
           work even remotely similar (mdamageu is essentially a losehp() macro
           in function form, presumably made before losehp() existed), perform
           damage here instead, it's easier... */
        if (!dmg)
            break;
        if (udef)
            losehp(dmg, killer_msg_mon(DIED, magr));
        else {
            mdef->mhp -= dmg;
            if (mdef->mhp <= 0)
                monkilled(magr, mdef, "", AD_FIRE);
        }
        return (!udef && DEADMONSTER(mdef)) ? MM_DEF_DIED : 0;
        /* TODO: golems */
        ret |= MM_HIT;
        break;
    case AD_SLEE:
        if (cancelled(magr) || !visda)
            break;

        if (!resists_sleep(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s gaze makes %s very sleepy...",
                      uagr ? "Your" : s_suffix(Monnam(mdef)),
                      udef ? "you" : mon_nam(mdef));
            sleep_monst(magr, mdef, dmg, 0);
            if (!udef)
                slept_monst(mdef);
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_immune),
                  "%s at %s, but %sn't feel tired!",
                  M_verbs(magr, "gaze"), udef ? "you" : mon_nam(mdef),
                  m_verbs(mdef, "do"));
        ret |= MM_HIT;
        break;
    case AD_SLOW:
        if (cancelled(magr) || !visda)
            break;

        inc_timeout(mdef, SLOW, dmg, FALSE);
        if (udef)
            action_interrupted();
        ret |= MM_HIT;
        break;
    default:
        impossible("Gaze attack %d?", mattk->adtyp);
        break;
    }
    return ret ? ret : MM_MISS;
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
        pline(combat_msgc(magr, mdef, cr_hit), "%s %s.",
              M_verbs(magr, mattk->adtyp == AD_DGST ? "swallow" : "engulf"),
              mon_nam(mdef));

    for (obj = mdef->minvent; obj; obj = obj->nobj)
        snuff_lit(obj);

    /* All of this maniuplation is needed to keep the display correct.
       There is a flush at the next pline(). */
    ax = magr->mx;
    ay = magr->my;
    dx = mdef->mx;
    dy = mdef->my;
    /* Leave the defender in the monster chain at it's current position,
       but don't leave it on the screen.  Move the agressor to the def-
       ender's position. */
    remove_monster(level, ax, ay);
    place_monster(magr, dx, dy, TRUE);
    newsym(ax, ay);     /* erase old position */
    newsym(dx, dy);     /* update new position */

    status = mdamagem(magr, mdef, mattk);

    if ((status & MM_AGR_DIED) && (status & MM_DEF_DIED)) {
        ;       /* both died -- do nothing */
    } else if (status & MM_DEF_DIED) {  /* defender died */
        /* Note:  remove_monster() was called in relmon(), wiping out
           magr from level->monsters[mdef->mx][mdef->my].  We need to
           put it back and display it.     -kd */
        place_monster(magr, dx, dy, TRUE);
        newsym(dx, dy);
    } else if (status & MM_AGR_DIED) {  /* agressor died */
        place_monster(mdef, dx, dy, TRUE);
        newsym(dx, dy);
    } else {    /* both alive, put them back */
        if (cansee(dx, dy)) {
            if (status & MM_EXPELLED) {
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s is regurgitated!", Monnam(mdef));
                pline(combat_msgc(magr, mdef, cr_immune),
                      "Obviously, %s doesn't like %s taste.",
                      mon_nam(magr), s_suffix(mon_nam(mdef)));
            } else
                pline_implied(msgc_monneutral, "%s is expelled!",
                              Monnam(mdef));
        }
        place_monster(magr, ax, ay, TRUE);
        place_monster(mdef, dx, dy, TRUE);
        newsym(ax, ay);
        newsym(dx, dy);
    }

    return status;
}

static int
explmm(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    int result;

    if (cancelled(magr))
        return MM_MISS;

    /* just petwarning here, because we're about to give a petfatal */
    if (cansee(magr->mx, magr->my))
        pline(magr->mtame ? msgc_petwarning : msgc_monneutral,
              "%s explodes!", Monnam(magr));
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
        pline(msgc_petfatal, brief_feeling, "melancholy");

    return result;
}

/* See mattackm() for return values */
int
damage(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef ||
                   cansee(magr->mx, magr->my) ||
                   cansee(mdef->mx, mdef->my));
    const struct permonst *pa = magr->data;
    const struct permonst *pd = mdef->data;
    struct obj *obj;
    int armpro, num, dmg = dice((int)mattk->damn, (int)mattk->damd);
    boolean cancelled;
    boolean mercy = FALSE;
    int zombie_timer;

    if (touch_petrifies(pd) && !resists_ston(magr)) {
        int protector = attk_protection((int)mattk->aatyp);
        int wornitems = 0;
        for (obj = magr->minvent; obj; obj = obj->nobj)
            wornitems |= obj->owornmask;

        /* wielded weapon gives same protection as gloves here */
        if (m_mwep(magr))
            wornitems |= W_MASK(os_armg);

        if (protector == 0L ||
            (protector != ~0L && (wornitems & protector) != protector)) {
            const char *killer = "attacking %s directly";
            if (protector == W_MASK(os_armg))
                killer = "punching %s barehanded";
            else if (protector == W_MASK(os_armf))
                killer = "kicking %s barefoot";
            else if (protector == W_MASK(os_armh))
                killer = "headbutting %s with no helmet";
            else if (protector == (W_MASK(os_armc) | W_MASK(os_armg)))
                killer = uarmc ? "hugging %s without gloves" :
                    "hugging %s without a cloak";
            uminstapetrify(magr, mdef,
                           killer_msg(STONING, killer));
            if (udef || !DEADMONSTER(mdef))
                return MM_MISS; /* lifesaved */

            return MM_AGR_DIED;
        }
    }
    dmg += dambon(magr);

    /* cancellation factor is the same as when attacking the hero */
    armpro = magic_negation(mdef);
    cancelled = cancelled(magr) || !((rn2(3) >= armpro) || !rn2(50));

    if (udef)
        mhitmsg(magr, mdef, mattk);

    /* for explosions, give a chance to resist similar to players.
       SAVEBREAK: for next savebreak, add AT_FLSH instead for lights
       as to avoid the S_LIGHT special case */
    if (!cancelled(magr) && mattk->aatyp == AT_EXPL &&
        magr->data->mlet != S_LIGHT) {
        if (acurr(mdef, A_DEX) > rnd(20)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_resist),
                      "%s ducks some of the blast.",
                      Monnam(mdef));
            dmg = (dmg + 1) / 2;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s is blasted!", Monnam(mdef));
    }

    switch (mattk->adtyp) {
    case AD_MAGM:
        if (cancelled(magr)) {
            dmg = 0;
            break;
        }
        pline(combat_msgc(magr, mdef, cr_hit),
              "You're hit by a shower of missiles!");
        if (resists_magm(mdef)) {
            pline(combat_msgc(magr, mdef, cr_immune),
                  "The missiles bounce off!");
            dmg = 0;
        }
        break;
    case AD_FIRE:
        if (cancelled) {
            dmg = 0;
            break;
        }
        if (resists_fire(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s %s, but it doesn't do much.",
                      M_verbs(mdef, "are"),
                      on_fire(mdef->data, mattk));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_FIRE, dmg);
            dmg = 0;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit), "%s %s!",
                  M_verbs(mdef, "are"),
                  on_fire(mdef->data, mattk));

        burn_away_slime(mdef);
        if (m_mlev(magr) > rn2(20)) {
            dmg += destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
            dmg += destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
            /* only potions damage resistant players in destroy_item */
            dmg += destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
        }

        if (resists_fire(mdef))
            break;

        if (pd == &mons[PM_STRAW_GOLEM] || pd == &mons[PM_PAPER_GOLEM]) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_kill),
                      "%s!", M_verbs(mdef, "roast"));
            if (udef)
                rehumanize(BURNING, msgcat("roasted to death by ",
                                           k_monnam(magr)));
            else {
                monkilled(magr, mdef, mdef->mtame ? NULL : "", mattk->adtyp);
                if (!udef && mdef->mtame && DEADMONSTER(mdef))
                    pline(msgc_petfatal, "May %s roast in peace.",
                          mon_nam(mdef));
                return attack_result(magr, mdef);
            }
        }
        break;
    case AD_COLD:
        if (cancelled) {
            dmg = 0;
            break;
        }
        if (resists_cold(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s coated in frost, but resist%s the effects.",
                      M_verbs(mdef, "are"),
                      udef ? "" : "s");
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_COLD, dmg);
            dmg = 0;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit), "%s covered in frost!",
                  M_verbs(mdef, "are"));

        if (m_mlev(magr) > rn2(20))
            dmg += destroy_mitem(mdef, POTION_CLASS, AD_COLD);
        break;
    case AD_ELEC:
        if (cancelled) {
            dmg = 0;
            break;
        }
        if (resists_elec(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s zapped, but do%sn't seem shocked.",
                      M_verbs(mdef, "are"),
                      udef ? "" : "es");
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_ELEC, dmg);
            dmg = 0;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit), "%s zapped!",
                  M_verbs(mdef, "are"));

        if (m_mlev(magr) > rn2(20))
            dmg += destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        break;
    case AD_SLEE:
        if (cancelled || magr->mspec_used || resists_sleep(mdef))
            break;

        magr->mspec_used += rnd(10);
        if (sleep_monst(magr, mdef, rnd(10), -1) && vis)
            pline(udef ? msgc_statusbad :
                  combat_msgc(magr, mdef, cr_hit), "%s put to sleep!",
                  M_verbs(mdef, "are"));
        if (!udef)
            slept_monst(mdef);
        break;
    case AD_DRST:
    case AD_DRDX:
    case AD_DRCO:
        if (cancelled || rn2(8))
            break;
        poisoned(mdef, msgprintf("%s %s", s_suffix(Monnam(magr)),
                                 mpoisons_subj(magr, mattk)),
                 mattk->adtyp == AD_DRDX ? A_DEX :
                 mattk->adtyp == AD_DRCO ? A_CON : A_STR,
                 killer_msg_mon(POISONING, magr), 30);
        if (!udef && DEADMONSTER(mdef))
            return attack_result(magr, mdef);
        break;
    case AD_ACID:
        if (cancelled(magr)) {
            dmg = 0;
            break;
        }
        if (resists_acid(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s covered in acid, but it seems harmless.",
                      M_verbs(mdef, "are"));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_ACID, dmg);
            dmg = 0;
        } else {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit), "%s covered in acid!",
                      M_verbs(mdef, "are"));
            if (udef)
                exercise(A_STR, FALSE);
        }

        if (!rn2(30))
            hurtarmor(mdef, ERODE_CORRODE);
        if (!rn2(3)) {
            if (rn2(2))
                acid_damage(m_mwep(mdef));
            else if (udef && u.twoweap)
                acid_damage(uswapwep);
        }
        break;
    case AD_BLND:
        if (can_blnd(magr, mdef, mattk->aatyp, NULL)) {
            if (!blind(mdef) && vis)
                pline(udef ? msgc_statusbad :
                      combat_msgc(magr, mdef, cr_hit),
                      "%s %s!", M_verbs(magr, "blind"), mon_nam(mdef));
            inc_timeout(mdef, BLINDED, dmg, TRUE);
            if (!blind(mdef) && vis)
                pline(udef ? msgc_statusheal :
                      combat_msgc(magr, mdef, cr_immune),
                      "%s vision quickly clears.",
                      s_suffix(Monnam(mdef)));
        }
        dmg = 0;
        break;
    default:
        impossible("Unknown attack in damage(): %d", mattk->adtyp);
        dmg = 0;
        break;
    }
    if (!dmg)
        return MM_MISS;

    /* Why doesn't this only run for physical attacks? Also, TODO: make Mitre undead
       protection an extrinsic or something. */
    obj = which_armor(mdef, os_armh);
    if (half_phys_dam(mdef) ||
        (((udef && Role_if(PM_PRIEST)) ||
          (!udef && mdef->data == &mons[PM_PRIEST])) &&
         obj && obj->oartifact == ART_MITRE_OF_HOLINESS &&
         (is_undead(magr->data) || is_demon(magr->data))))
        dmg = (dmg + 1) / 2;

    obj = m_mwep(magr);
    if (mercy) {
        impossible("Running incomplete mercy checks");
        do_mercy(magr, mdef, obj, dmg);
    } else
        do_damage(magr, mdef, dmg, mattk);

    return attack_result(magr, mdef);
}

/* Returns MM_HIT if both alive, otherwise MM_AGR_DIED|MM_DEF_DIED appropriately */
static int
attack_result(struct monst *magr, struct monst *mdef)
{
    int ret = 0;
    if (magr != &youmonst && DEADMONSTER(magr))
        ret |= MM_AGR_DIED;
    if (mdef != &youmonst && DEADMONSTER(mdef))
        ret |= MM_DEF_DIED;

    if (!ret)
        ret |= MM_HIT;

    return ret;
}

/* Deal the damage */
static void
do_damage(struct monst *magr, struct monst *mdef, int dmg,
          const struct attack *mattk)
{
    if (mdef != &youmonst) {
        mdef->mhp -= dmg;
        if (mdef->mhp < 1) {
            if (m_at(level, mdef->mx, mdef->my) == magr) {  /* see gulpmm() */
                remove_monster(level, mdef->mx, mdef->my);
                mdef->mhp = 1;      /* otherwise place_monster will complain */
                place_monster(mdef, mdef->mx, mdef->my, TRUE);
                mdef->mhp = 0;
            }

            monkilled(magr, mdef, "", (int)mattk->adtyp);
        }
    } else if (Upolyd) {
        u.mh -= dmg;
        if (u.mh < 1)
            rehumanize(DIED, killer_msg_mon(DIED, magr));
    } else {
        u.uhp -= dmg;
        if (u.uhp < 1)
            done_in_by(magr, NULL);
    }
}

/* See comment at top of mattackm(), for return values. */
static int
mdamagem(struct monst *magr, struct monst *mdef, const struct attack *mattk)
{
    struct obj *obj;
    const struct permonst *pa = magr->data;
    const struct permonst *pd = mdef->data;
    int armpro, num, tmp = dice((int)mattk->damn, (int)mattk->damd);
    boolean cancelled;
    boolean mercy = FALSE;
    int zombie_timer;

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
            /* just petwarning here; we're about to give a petfatal */
            if (vis)
                pline(magr->mtame ? msgc_petwarning : msgc_monneutral,
                      "%s turns to stone!", Monnam(magr));
            monstone(magr);
            if (!DEADMONSTER(magr))
                return 0;
            else if (magr->mtame && !vis)
                pline(msgc_petfatal, brief_feeling, "peculiarly sad");
            return MM_AGR_DIED;
        }
    }
    tmp += dambon(magr);

    /* cancellation factor is the same as when attacking the hero */
    armpro = magic_negation(mdef);
    cancelled = cancelled(magr) || !((rn2(3) >= armpro) || !rn2(50));

    /* for explosions, give a chance to resist similar to players.
       SAVEBREAK: for next savebreak, add AT_FLSH instead for lights
       as to avoid the S_LIGHT special case */
    if (!cancelled(magr) && mattk->aatyp == AT_EXPL &&
        magr->data->mlet != S_LIGHT) {
        if (acurr(mdef, A_DEX) > rnd(20)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_resist),
                      "%s ducks some of the blast.",
                      Monnam(mdef));
            tmp = (tmp + 1) / 2;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s is blasted!", Monnam(mdef));
    }

    switch (mattk->adtyp) {
    case AD_DGST:
        if (slow_digestion(mdef))
            return (MM_HIT | MM_EXPELLED);
        /* eating a Rider or its corpse is fatal */
        if (is_rider(mdef->data)) {
            if (vis)
                pline(magr->mtame ? msgc_petwarning : msgc_monneutral, "%s %s!",
                      Monnam(magr), mdef->data == &mons[PM_FAMINE] ?
                      "belches feebly, shrivels up and dies" :
                      mdef->data == &mons[PM_PESTILENCE] ?
                      "coughs spasmodically and collapses" :
                      "vomits violently and drops dead");
            mondied(magr);
            if (!DEADMONSTER(magr))
                return 0;       /* lifesaved */
            else if (magr->mtame && !vis)
                pline(msgc_petfatal, brief_feeling, "queasy");
            return MM_AGR_DIED;
        }
        verbalize(combat_msgc(magr, mdef, cr_hit), "Burrrrp!");
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
        if (magr->mtame && !isminion(magr) &&
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
            mx_edog(magr)->hungrytime += nutrit;
        }
        break;
    case AD_STUN:
        if (cancelled(magr))
            break;
        if (canseemon(mdef))
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s %s for a moment.", Monnam(mdef),
                  makeplural(stagger(mdef->data, "stagger")));
        set_property(mdef, STUNNED, tmp, TRUE);
        goto physical;
    case AD_LEGS:
        if (cancelled(magr)) {
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
                if (otmp->oartifact ||
                    otmp->oprops) {
                    artifact_hit(magr, mdef, otmp, &tmp, dieroll);
                    if (DEADMONSTER(mdef))
                        return (MM_DEF_DIED |
                                (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
                    if (obj_properties(otmp) & opm_mercy)
                        mercy = TRUE;
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
    case AD_MAGM:
    case AD_FIRE:
    case AD_COLD:
    case AD_ELEC:
    case AD_SLEE:
    case AD_DRST:
    case AD_DRDX:
    case AD_DRCO:
    case AD_ACID:
    case AD_BLND:
        return damage(magr, mdef, mattk);
    case AD_RUST:
        if (cancelled(magr))
            break;
        if (pd == &mons[PM_IRON_GOLEM]) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_kill),
                      "%s falls to pieces!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline(msgc_petfatal, "May %s rust in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        hurtarmor(mdef, ERODE_RUST);
        if (mdef->mstrategy == st_waiting)
            mdef->mstrategy = st_none;
        tmp = 0;
        break;
    case AD_CORR:
        if (cancelled(magr))
            break;
        hurtarmor(mdef, ERODE_CORRODE);
        if (mdef->mstrategy == st_waiting)
            mdef->mstrategy = st_none;
        tmp = 0;
        break;
    case AD_DCAY:
        if (cancelled(magr))
            break;
        if (pd == &mons[PM_WOOD_GOLEM] || pd == &mons[PM_LEATHER_GOLEM]) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_kill),
                      "%s falls to pieces!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline(msgc_petfatal, "May %s rot in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        hurtarmor(mdef, ERODE_ROT);
        if (mdef->mstrategy == st_waiting)
            mdef->mstrategy = st_none;
        tmp = 0;
        break;
    case AD_STON:
        if (cancelled(magr))
            break;
    do_stone:
        /* may die from the acid if it eats a stone-curing corpse */
        mstiffen(mdef, magr);
        tmp = (mattk->adtyp == AD_STON ? 0 : 1);
        break;
    case AD_TLPT:
        if (!cancelled && tmp < mdef->mhp && !tele_restrict(mdef)) {
            const char *mdef_Monnam = Monnam(mdef);
            /* save the name before monster teleports, otherwise we'll get "it"
               in the suddenly disappears message */

            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
            rloc(mdef, TRUE);
            if (vis && !canspotmon(mdef) && mdef != u.usteed)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s suddenly disappears!", mdef_Monnam);
        }
        break;
    case AD_PLYS:
        if (!cancelled && mdef->mcanmove) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s is frozen by %s.", Monnam(mdef), mon_nam(magr));

            mdef->mcanmove = 0;
            mdef->mfrozen = rnd(10);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
        }
        break;
    case AD_SLOW:
        if (!cancelled) {
            set_property(mdef, SLOW, tmp, FALSE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
        }
        break;
    case AD_CONF:
        /* Since confusing another monster doesn't have a real time limit,
           setting spec_used would not really be right (though we still should
           check for it). */
        if (!cancelled(magr) && !magr->mspec_used) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s looks confused.", Monnam(mdef));
            inc_timeout(mdef, CONFUSION, tmp, TRUE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
        }
        break;
    case AD_HALU:
        if (!cancelled(magr) && haseyes(pd) && !blind(mdef) &&
            !resists_hallu(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s is freaked out.", Monnam(mdef));
            inc_timeout(mdef, CONFUSION, tmp, TRUE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
        }
        tmp = 0;
        break;
    case AD_CURS:
        if (!night() && (pa == &mons[PM_GREMLIN]))
            break;
        if (!cancelled(magr) && !rn2(10)) {
            if (pd == &mons[PM_CLAY_GOLEM]) {
                if (vis) {
                    pline(combat_msgc(magr, mdef, cr_kill0),
                          "Some writing vanishes from %s head!",
                          s_suffix(mon_nam(mdef)));
                    pline(combat_msgc(magr, mdef, cr_kill),
                          "%s is destroyed!", Monnam(mdef));
                }
                mondied(mdef);
                if (!DEADMONSTER(mdef))
                    return 0;
                else if (mdef->mtame && !vis)
                    pline(msgc_petfatal, brief_feeling, "strangely sad");
                return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
            } else {
                if (canhear()) {
                    if (!vis)
                        You_hear(msgc_levelsound, "laughter.");
                    else
                        pline(msgc_npcvoice, "%s chuckles.", Monnam(magr));
                }
                gremlin_curse(mdef);
            }
        }
        break;
    case AD_SGLD: {
        tmp = 0;
        if (cancelled(magr))
            break;
        /* technically incorrect; no check for stealing gold from between
           mdef's feet... */
        {
            struct obj *gold = findgold(mdef->minvent);

            if (!gold)
                break;
            obj_extract_self(gold);
            add_to_minv(magr, gold, NULL);
        }
        if (mdef->mstrategy == st_waiting)
            mdef->mstrategy = st_none;
        const char *magr_Monnam = Monnam(magr); /* name pre-rloc() */

        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s steals some gold from %s.", magr_Monnam, mon_nam(mdef));

        if (!tele_restrict(magr)) {
            rloc(magr, TRUE);
            if (vis && !canspotmon(magr))
                pline(combat_msgc(magr, NULL, cr_hit),
                      "%s suddenly disappears!", magr_Monnam);
        }
        break;
    } case AD_DRLI:
        if (!cancelled(magr) && !rn2(3) && !resists_drli(mdef))
            mlosexp(magr, mdef, NULL, FALSE);

        if (DEADMONSTER(mdef))
            return attack_result(magr, mdef);

        break;
    case AD_SSEX:
    case AD_SITM:      /* for now these are the same */
    case AD_SEDU:
        if (cancelled(magr))
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
                update_property(mdef, objects[otmp->otyp].oc_oprop, which_slot(otmp));
                update_property_for_oprops(mdef, otmp,
                                           which_slot(otmp));
            }

            /* add_to_minv() might free otmp [if it merges] */
            const char *onambuf = doname(otmp);
            add_to_minv(magr, otmp, NULL);

            /* In case of teleport */
            const char *magr_Monnam = Monnam(magr);

            if (vis) {
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s steals %s from %s!",
                      magr_Monnam, onambuf, mdefnambuf);
            }

            possibly_unwield(mdef, FALSE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
            mselftouch(mdef, NULL, magr);
            if (DEADMONSTER(mdef))
                return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
            if (magr->data->mlet == S_NYMPH && !tele_restrict(magr)) {
                rloc(magr, TRUE);
                if (vis && !canspotmon(magr))
                    pline(combat_msgc(magr, NULL, cr_hit),
                          "%s suddenly disappears!", magr_Monnam);
            }
        }
        tmp = 0;
        break;
    case AD_DRIN:
        if (notonhead || !has_head(pd)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s doesn't seem harmed.", Monnam(mdef));
            /* Not clear what to do for green slimes */
            tmp = 0;
            break;
        }
        if ((mdef->misc_worn_check & W_MASK(os_armh)) && rn2(8)) {
            if (vis) {
                pline(combat_msgc(magr, mdef, cr_miss),
                      "%s %s blocks %s attack to %s head.",
                      s_suffix(Monnam(mdef)),
                      helmet_name(which_armor(mdef, os_armh)),
                      s_suffix(mon_nam(magr)), mhis(mdef));
            }
            break;
        }

        if (mindless(pd)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s doesn't notice its brain being eaten.", Monnam(mdef));
            break;
        } else if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s brain is eaten!", s_suffix(Monnam(mdef)));

        tmp += rnd(10); /* fakery, since monsters lack INT scores */
        if (magr->mtame && !isminion(magr)) {
            mx_edog(magr)->hungrytime += rnd(60);
            inc_timeout(mdef, CONFUSION, dice(3, 8), FALSE);
        }
        if (tmp >= mdef->mhp && vis)
            pline(combat_msgc(magr, mdef, cr_kill0),
                  "%s last thought fades away...", s_suffix(Monnam(mdef)));
        break;
    case AD_DETH:
        if (vis)
            pline_implied(combat_msgc(magr, mdef, cr_hit),
                  "%s reaches out with its deadly touch.", Monnam(magr));
        if (is_undead(mdef->data)) {
            /* Still does normal damage */
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s looks no deader than before.", Monnam(mdef));
            break;
        }
        switch (rn2(20)) {
        case 19:
        case 18:
        case 17:
            if (!resist(magr, mdef, 0, 0, 0) && !resists_magm(mdef)) {
                monkilled(magr, mdef, "", AD_DETH);
                if (DEADMONSTER(mdef))            /* did it lifesave? */
                    return MM_DEF_DIED;

                tmp = 0;
                break;
            }   /* else FALLTHRU */
        default:       /* case 16: ... case 5: */
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s looks weaker!", Monnam(mdef));
            mdef->mhpmax -= rn2(tmp / 2 + 1);   /* mhp will then still be less
                                                   than this value */
            break;
        case 4:
        case 3:
        case 2:
        case 1:
        case 0:
            /* TODO: Figure out what's going on with MR here; the code had a
               check on Antimagic (that isn't in 3.4.3), which is obviously
               wrong for monster vs. monster combat) */
            if (vis)
                pline(combat_msgc(magr, mdef, cr_miss), "That didn't work...");
            tmp = 0;
            break;
        }
        break;
    case AD_PEST:
        if (resists_sick(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s reaches out, but %s doesn't look ill!", Monnam(magr),
                      mon_nam(mdef));
        } else {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s reaches out, and %s looks rather ill.", Monnam(magr),
                      mon_nam(mdef));
            make_sick(mdef, 20 + rn2_on_rng(acurr(mdef, A_CON), rng_main),
                      magr->data->mname, TRUE, SICK_NONVOMITABLE);
        }
        break;
    case AD_FAMN:
        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s reaches out, and %s body shrivels.",
                  Monnam(magr), s_suffix(mon_nam(mdef)));
        if (mdef->mtame && !isminion(mdef))
            mx_edog(mdef)->hungrytime -= rn1(120, 120);
        else {
            tmp += rnd(10);     /* lacks a food rating */
            if (tmp >= mdef->mhp && vis)
                pline(combat_msgc(magr, mdef, cr_kill0),
                      "%s starves.", Monnam(mdef));
        }
        /* plus the normal damage */
        break;
    case AD_SLIM:
        if (cancelled)
            break;      /* physical damage only */
        if (!rn2(4) && !flaming(mdef->data) && !unsolid(mdef->data) &&
            mdef->data != &mons[PM_GREEN_SLIME]) {
            set_property(mdef, SLIMED, 10, FALSE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
            tmp = 0;
        }
        break;
    case AD_STCK:
        if (cancelled)
            tmp = 0;
        break;
    case AD_WRAP:      /* monsters cannot grab one another, it's too hard */
        if (cancelled(magr))
            tmp = 0;
        break;
    case AD_ENCH:
        /* there's no msomearmor() function, so just do damage */
        /* if (cancelled) break; */
        break;
    case AD_ZOMB:
        if (nonliving(mdef->data) || izombie(mdef))
            break;
        zombie_timer = property_timeout(mdef, ZOMBIE);
        set_property(mdef, ZOMBIE,
                     !zombie_timer ? 100 :
                     zombie_timer <= 10 ? 1 :
                     (zombie_timer - 10), FALSE);
        break;
    default:
        tmp = 0;
        break;
    }
    if (!tmp)
        return MM_MISS;

    if (mercy) {
        otmp->mbknown = 1;
        otmp->mknown = 1;
        boolean saw_something = FALSE;
        if (mdef->mhp < mdef->mhpmax) {
            mdef->mhp += tmp;
            if (mdef->mhp > mdef->mhpmax)
                mdef->mhp = mdef->mhpmax;
            if (canseemon(magr) || canseemon(mdef)) {
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s healed!", M_verbs(mdef, "are"));
                saw_something = TRUE;
            }
        }

        if (!otmp->cursed) {
            if (canseemon(magr)) {
                pline(msgc_monneutral, "%s %s for a moment.",
                      Tobjnam(otmp, "glow"), hcolor("black"));
                saw_something = TRUE;
            }
            curse(otmp);
        }

        if (saw_something) {
            learn_oprop(otmp, opm_mercy);
            otmp->bknown = 1; /* autocurses */
        }
    }

    if ((mdef->mhp -= tmp) < 1) {
        if (m_at(level, mdef->mx, mdef->my) == magr) {  /* see gulpmm() */
            remove_monster(level, mdef->mx, mdef->my);
            mdef->mhp = 1;      /* otherwise place_monster will complain */
            place_monster(mdef, mdef->mx, mdef->my, TRUE);
            mdef->mhp = 0;
        }
        monkilled(magr, mdef, "", (int)mattk->adtyp);
        if (!DEADMONSTER(mdef))
            return 0;   /* mdef lifesaved */

        if (mattk->adtyp == AD_DGST) {
            /* various checks similar to dog_eat and meatobj. after monkilled()
               to provide better message ordering */
            if (mdef->cham != CHAM_ORDINARY) {
                newcham(magr, NULL, FALSE, TRUE);
            } else if (mdef->data == &mons[PM_GREEN_SLIME] && !unchanging(magr)) {
                set_property(magr, SLIMED, 5, FALSE);
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
sleep_monst(struct monst *magr, struct monst *mdef, int amt, int how)
{
    if (resists_sleep(mdef) || (how >= 0 && mdef != &youmonst &&
                                resist(magr, mdef, (char)how, TELL, 0))) {
        shieldeff(m_mx(mdef), m_my(mdef));
        return 0;
    }
    if (mdef == &youmonst ? u_helpless(hm_asleep) : !mdef->mcanmove)
        return 0;

    if (mdef == &youmonst) {
        helpless(amt, hr_asleep, "sleeping", NULL);
        return 1;
    }
    amt += (int)mdef->mfrozen;
    if (amt > 0) {  /* sleep for N turns */
        mdef->mcanmove = 0;
        mdef->mfrozen = min(amt, 127);
    } else /* sleep until awakened */
        mdef->msleeping = 1;
    return 1;
}

/* sleeping grabber releases, engulfer doesn't; don't use for paralysis! */
void
slept_monst(struct monst *mon)
{
    if ((mon->msleeping || !mon->mcanmove) && mon == u.ustuck &&
        !sticks(youmonst.data) && !Engulfed) {
        pline(msgc_statusheal, "%s grip relaxes.", s_suffix(Monnam(mon)));
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

    if (cancelled(mdef))
        return;

    erode_obj(obj, NULL, type, TRUE, TRUE);
}

void
mswingsm(struct monst *magr, struct monst *mdef, struct obj *otemp)
{
    if (Blind || !mon_visible(magr))
        return;

    /* in monneutral, a very low-priority message channel, even for pets; this
       message can get pretty spammy. Also note that the format strings have
       different numbers of arguments; printf is specified to ignore spare
       arguments at the end of the format string (C11 7.21.6.1p2) */
    pline_implied(msgc_monneutral, "%s %s %s %s at %s.", Monnam(magr),
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
            if (resists_acid(magr))
                tmp = 0;
            if (canseemon(magr)) {
                if (tmp)
                    pline(combat_msgc(mdef, magr, cr_hit),
                          "%s is splashed by %s acid!",
                          Monnam(magr), s_suffix(mon_nam(mdef)));
                else
                    pline(combat_msgc(mdef, magr, cr_miss),
                          "%s is splashed by %s acid, but doesn't care.",
                          Monnam(magr), s_suffix(mon_nam(mdef)));
            }
        } else
            tmp = 0;
        goto assess_dmg;
    case AD_MAGM:
        /* wrath of gods for attacking Oracle */
        if (resists_magm(magr)) {
            if (canseemon(magr)) {
                shieldeff(magr->mx, magr->my);
                pline(combat_msgc(mdef, magr, cr_miss),
                      "A hail of magic missiles narrowly misses %s!",
                      mon_nam(magr));
            }
        } else {
            if (canseemon(magr)) {
                if (magr->data == &mons[PM_WOODCHUCK]) {
                    pline(combat_msgc(mdef, magr, cr_hit), "ZOT!");
                } else {
                    pline(combat_msgc(mdef, magr, cr_hit),
                          "%s is caught in a hail of magic missiles!",
                          Monnam(magr));
                }
            }
            goto assess_dmg;
        }
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        if (mhit && !cancelled(mdef) && otmp) {
            drain_item(otmp);
            /* No message */
        }
        break;
    default:
        break;
    }
    if (mdead || cancelled(mdef))
        return mdead | mhit;

    /* These affect the enemy only if defender is still alive */
    if (rn2(3))
        switch (mddat->mattk[i].adtyp) {
        case AD_SLOW:
            if (blind(mdef)) {
                if (canseemon(magr))
                    pline(combat_msgc(mdef, magr, cr_miss),
                          "%snot defend %sself.", M_verbs(mdef, "can"),
                          mhim(mdef));
                tmp = 0;
                break;
            }
            if (cancelled(mdef) ||
                !(msensem(mdef, magr) & MSENSE_VISION) ||
                !(msensem(magr, mdef) & MSENSE_VISION)) {
                tmp = 0;
                break;
            }
            if (!slow(mdef) && canseemon(mdef))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s down under %s gaze!", M_verbs(magr, "slow"),
                      s_suffix(mon_nam(mdef)));
            inc_timeout(mdef, SLOW, tmp, TRUE);
            tmp = 0;
            break;
        case AD_PLYS:
            if (free_action(magr)) {
                if (canseemon(magr))
                    pline(combat_msgc(mdef, magr, cr_immune),
                          "%s momentarily stiffens.",
                          Monnam(magr));
                return mdead | mhit;
            }

            if (canseemon(magr))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s is frozen by %s.", Monnam(magr), mon_nam(mdef));
            magr->mcanmove = 0;
            magr->mfrozen = min(tmp, 127);
            return mdead | mhit;
        case AD_COLD:
            if (resists_cold(magr)) {
                if (canseemon(magr)) {
                    pline(combat_msgc(mdef, magr, cr_miss),
                          "%s is mildly chilly.", Monnam(magr));
                    golemeffects(magr, AD_COLD, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s is suddenly very cold!", Monnam(magr));
            mdef->mhp += tmp / 2;
            if (mdef->mhpmax < mdef->mhp)
                mdef->mhpmax = mdef->mhp;
            if (mdef->mhpmax > ((int)(mdef->m_lev + 1) * 8))
                split_mon(mdef, magr);
            break;
        case AD_STUN:
            if (canseemon(magr)) {
                if (stunned(magr))
                    pline(combat_msgc(mdef, magr, cr_hit),
                          "%s struggles to keep %s balance.", Monnam(magr),
                          mhis(magr));
                else
                    pline(combat_msgc(mdef, magr, cr_hit),
                          "%s %s...", Monnam(magr),
                          makeplural(stagger(magr->data, "stagger")));
            }
            set_property(magr, STUNNED, tmp, TRUE);
            tmp = 0;
            break;
        case AD_FIRE:
            burn_away_slime(magr);
            if (resists_fire(magr)) {
                if (canseemon(magr)) {
                    pline(combat_msgc(mdef, magr, cr_miss),
                          "%s is mildly warmed.", Monnam(magr));
                    golemeffects(magr, AD_FIRE, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s is suddenly very hot!", Monnam(magr));
            break;
        case AD_ELEC:
            if (resists_elec(magr)) {
                if (canseemon(magr)) {
                    pline(combat_msgc(mdef, magr, cr_miss),
                          "%s is mildly tingled.", Monnam(magr));
                    golemeffects(magr, AD_ELEC, tmp);
                }
                tmp = 0;
                break;
            }
            if (canseemon(magr))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s is jolted with electricity!", Monnam(magr));
            break;
        default:
            tmp = 0;
            break;
    } else
        tmp = 0;

assess_dmg:
    if ((magr->mhp -= tmp) <= 0) {
        monkilled(mdef, magr, "", (int)mddat->mattk[i].adtyp);
        return mdead | mhit | MM_AGR_DIED;
    }
    return mdead | mhit;
}

/* Perform AT_AREA logic (e.g. area of effect "auras" for certain monsters.
   AT_AREA defines certain "auras" from certain monsters with an
   area-of-effect. For example, floating eyes make monsters within a certain
   radius slow. */
void
do_at_area(struct level *lev)
{
    struct monst *magr, *mdef;
    struct monst *areamons[64];
    const struct attack *areaatk[64];
    uint64_t area[COLNO+1][ROWNO];
    int i = 0;

    /* Clear area from earlier invocations */
    memset(area, 0, sizeof area);

    /* Iterate the monlist, looking for AT_AREA. If one is found, perform a
       do_clear_area on the selected area, contained in the numdice attack
       number */
    for (magr = lev->monlist; magr; magr = (magr->nmon ? magr->nmon :
                                         magr == &youmonst ? NULL :
                                         &youmonst)) {
        if (magr != &youmonst && DEADMONSTER(magr))
            continue;
        if ((areaatk[i] = attacktype_fordmg(magr->data, AT_AREA, AD_ANY))) {
            areamons[i] = magr;
            area[COLNO][0] = ((uint64_t)1 << i);
            do_clear_area(m_mx(magr), m_my(magr), areaatk[i]->damn,
                          set_at_area, &area);
            i++;
        }
        /* arbitrary cap (we've defined 64 indices, so bail out if we go above
           that cap) */
        if (i >= 64)
            break;
    }

    int total = i; /* total amount causing auras */
    if (!total)
        return;

    /* Now, perform the attack itself */
    int x;
    int y;
    for (x = 0; x < COLNO; x++) {
        for (y = 0; y < ROWNO; y++) {
            /* is there an aura here */
            if (!area[x][y])
                continue;
            /* is there a monster here */
            mdef = m_at(lev, x, y);
            if (!mdef && x == u.ux && y == u.uy && lev == level)
                mdef = &youmonst;

            /* Find the monsters responsible for the aura */
            for (i = 0; i < total; i++) {
                if (!(area[x][y] & (((uint64_t)1 >> i) & 1)))
                    continue;

                /* If there is a monster here, perform an attack */
                if (mdef)
                    maurahitm(areamons[i], mdef, areaatk[i]);

                /* If adtyp is AD_ZOMB (zombies), potentially revive stuff */
                if (areaatk[i]->adtyp == AD_ZOMB)
                    maurahitpile(areamons[i], x, y, areaatk[i]);
            }
        }
    }
}


/* Set relevant bit for at_area */
static void
set_at_area(int x, int y, void *area)
{
    (*((uint64_t(*)[COLNO+1][ROWNO])area))[x][y] |=
        (*((uint64_t(*)[COLNO+1][ROWNO])area))[COLNO][0];
}


/* Aura effects vs object piles */
static void
maurahitpile(struct monst *mon, int x, int y, const struct attack *mattk)
{
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon) || cansee(x, y));
    struct obj *obj, *nexthere;
    struct monst *omon;
    for (obj = level->objects[x][y]; obj; obj = nexthere) {
        nexthere = obj->nexthere;
        if (obj->otyp == CORPSE) {
            if (ox_monst(obj)) {
                omon = get_mtraits(obj, FALSE);
                /* We can't use izombie() because normal property checks assume
                   proper values in the monst */
                if ((nonliving(&mons[omon->orig_mnum]) ||
                     (omon->mintrinsic[ZOMBIE] & FROMOUTSIDE_RAW)) && rn2(20))
                    continue;
            }
            omon = revive(obj);
            if (!omon)
                continue;

            if (mon->mtame != omon->mtame) {
                if (mon->mtame)
                    tamedog(omon, NULL);
                else
                    omon->mtame = 0; /* no longer tame */
            }
            if (!mon->mtame)
                msethostility(omon, !mon->mpeaceful, TRUE);
            /* turn into a zombie if applicable */
            int mndx = NON_PM;
            if (is_human(omon->data))
                mndx = PM_HUMAN_ZOMBIE;
            if (is_elf(omon->data))
                mndx = PM_ELF_ZOMBIE;
            if (is_orc(omon->data))
                mndx = PM_ORC_ZOMBIE;
            if (is_gnome(omon->data))
                mndx = PM_GNOME_ZOMBIE;
            if (is_dwarf(omon->data))
                mndx = PM_DWARF_ZOMBIE;
            if (is_giant(omon->data))
                mndx = PM_GIANT_ZOMBIE;
            if (omon->data == &mons[PM_ETTIN])
                mndx = PM_ETTIN_ZOMBIE;
            if (omon->data->mlet == S_KOBOLD)
                mndx = PM_KOBOLD_ZOMBIE;
            if (mndx != NON_PM && omon->data != &mons[mndx] &&
                !newcham(omon, &mons[mndx], FALSE, FALSE)) {
                if (you || vis)
                    pline(msgc_monneutral, "%s from the unholy revival and is destroyed!",
                          M_verbs(omon, "shudder"));
                monkilled(mon, omon, "", -AD_ZOMB);
                continue;
            }

            if (vis)
                pline(msgc_monneutral, "%s from the dead%s!",
                      M_verbs(omon, "raise"),
                      nonliving(omon->data) ? "" :
                      msgcat_many(" under ", s_suffix(mon_nam(mon)),
                                  " power", NULL));
            if (!nonliving(omon->data) && !izombie(omon))
                set_property(omon, ZOMBIE, 0, TRUE);
        }
    }
}

/* Aura effects vs mon */
static void
maurahitm(struct monst *magr, struct monst *mdef,
          const struct attack *mattk)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (cansee(m_mx(magr), m_my(magr)) &&
                   cansee(m_mx(mdef), m_my(mdef)) &&
                   (canspotmon(magr) || canspotmon(mdef)));
    /* Doesn't include damn: it is for the size of the area effect */
    int dmg = mattk->damd;

    switch (mattk->adtyp) {
    case AD_SLOW: /* floating eyes */
        if (!(uagr || magr->mtame) ==
            !(udef || mdef->mtame) ||
            !(msensem(mdef, magr) & MSENSE_VISION))
            break;
        if (!slow(mdef) && (uagr || udef || vis))
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s down under %s gaze.",
                  M_verbs(mdef, "slow"),
                  magr == &youmonst ? "your" :
                  s_suffix(mon_nam(magr)));
        if (property_timeout(mdef, SLOW) < dmg)
            set_property(mdef, SLOW, dmg, TRUE);
        break;
    case AD_ZOMB:
        if (property_timeout(mdef, ZOMBIE) > 100)
            mdef->mintrinsic[ZOMBIE] -= 100; /* don't cure yet */
        break;
    }
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
