/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-07-13 */
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
static int mdamagem(struct monst *, struct monst *, const struct attack *);
static void noises(struct monst *, const struct attack *);
static void missmm(struct monst *, struct monst *, const struct attack *);
static int passivemm(struct monst *, struct monst *, boolean, int);
static void set_at_area(int, int, void *);
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
s       this is more for instadeaths and for after the damage is dealt. */
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

            msgc_combatgood,    msgc_failrandom,    /* player vs. hostile */
            msgc_combatgood,    msgc_failrandom,    /* player vs. peaceful */
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

    if (farq != far_noise || moves - noisetime > 10) {
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
    boolean conflicted = (Conflict && !resist(mon, RING_CLASS, 0) &&
                          m_canseeu(mon) && distu(mon->mx, mon->my) < (BOLT_LIM * BOLT_LIM));

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
        if (!mtmp || (!mm_aggression(mon, mtmp) && !conflicted))
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

/* A wrapper to mhitm_inner() to handle a few special cases */
int
mhitm(struct monst *magr, struct monst *mdef)
{
    /* Used to track steed redirection, to give the steed
       an opportunity to retaliate */
    boolean steed = FALSE;

    /* Override hitting steeds to hitting the steed rider.
       This is going to be more general once monsters can
       ride steeds. */
    if (mdef == u.usteed)
        mdef = &youmonst;

    /* However, sometimes monsters end up hitting the steed
       instead. Orcs in particular favour this sort of hit,
       because they like to steal, eat horses and stuff */
    if (mdef == &youmonst &&
        rn2(is_orc(magr->data) ? 2 : 4)) {
        steed = TRUE;
        mdef = u.usteed;
    }

    int ret = 0;
    int ret2 = 0; /* steed retaliation */

    /* Attacker isn't a worm */
    if (!magr->wormno) {
        ret = mhitm_inner(magr, mdef);
        if (!(ret & MM_DIED) && steed) {
            ret2 = mhitm_inner(mdef, magr);
            /* since retalitory attacks have reversed roles,
               we need to fix some return values by hand... */
            if (ret2 & MM_AGR_DIED)
                ret |= MM_DEF_DIED;
            if (ret2 & MM_DEF_DIED)
                ret |= MM_AGR_DIED;
        }
        return ret;
    }

    int ret = 0;
    int wnum = magr->wormno;
    struct wseg *seg;

    /* FIXME: make worm attacks check for steed on each attack
       rather than the whole sequence, and allow steeds to
       retaliate (once) */
    for (seg = level->wtails[wnum]; seg; seg = seg->nseg) {
        if (dist2(seg->wx, seg->wy, mdef->mx, mdef->my) < 3) {
            ret |= mhitm_inner(magr, mdef);
            /* If attacker or defender dies, bail out */
            if (ret & MM_DIED)
                return ret;
        }
    }

    return MM_MISS;
}

/*
 * Main melee combat function
 *
 * Returns a bitfield with the following values:
 * 0x1 - Whether or not the attack hit (MM_MISS or MM_HIT)
 * 0x2 - The defender died (MM_DEF_DIED)
 * 0x4 - The attacker died (MM_AGR_DIED)
 * AT_EXPL monsters always dies, which is reflected in the return value
 */
int
mhitm_inner(struct monst *magr, struct monst *mdef)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef || canseemon(magr) ||
                   canseemon(mdef));

    int hitbon = 1;
    int dambon = 0;
    int wepbon = 0;
    int wepbon2 = 0; /* for off-hand weapons */
    /* wep: current weapon, mwep: main weapon, swep: off-hand weapon */
    struct obj *wep, mwep, swep;
    mwep = m_mwep(magr);
    swep = (!uagr ? NULL : uswapwep); /* FIXME */

    /* To-hit bonuses/penalties */
    if (mon_hitbon(magr)) /* rings */
        hitbon += rnd(mon_hitbon(magr));
    if (luck(magr) > 0)
        hitbon += rnd(luck(magr) / 4);
    else if (luck(magr) < 0)
        hitbon += -rnd(-luck(magr) / 4);
    hitbon += find_mac_hit(mdef); /* effective AC for to-hit */
    hitbon += rnd(m_mlev(magr) / 3);
    hitbon += hitbon_attrib(magr);
    if (stunned(mdef))
        hitbon += 2;
    if (!udef && mdef->mflee)
        hitbon += 2;
    if (!udef && mdef->msleeping) {
        /* well, it's awake now */
        mdef->msleeping = 0;
        hitbon += 2;
    }
    if ((udef && u_helpless(hm_all)) ||
        (!udef && !mdef->mcanmove)) {
        hitbon += 4;
        /* Give the defender a chance to react before they die */
        if (!rn2(10)) {
            if (!udef) {
                mdef->mcanmove = 1;
                mdef->mfrozen = 0;
            }
            else if (u_helpless(hm_asleep) &&
                     turnstate.helpless_timers[hr_asleep] < moves)
                cancel_helplessness(hm_asleep,
                                    "The combat suddenly awakens you.");
        }
    }
    if (is_elf(magr->data) &&
        is_orc(mdef->data))
        hitbon++;
    if (magr->data == &mons[PM_MONK]) {
        /* Monks get special to-hit bonuses or penalties if lightly/heavily
           equipped */
        if (which_armor(magr, os_arm) &&
            (!uagr || !uskin())) {
            /* give people two different ways to turn this off without having
               to suppress msgc_hint altogether */
            if (flags.verbose)
                pline_implied(uagr ? msgc_hint : msgc_monneutral,
                              "%s armor is rather cumbersome...",
                              uagr ? "Your" : s_suffix(Monnam(magr)));
            hitbon -= uagr ? urole.spelarmr : 20;
        } else if (!m_mwep(magr) &&
                 !which_armor(magr, os_arms))
            hitbon += (m_mlev(magr) / 3) + 2;
    }
    if ((uagr && u.utrapped) ||
        (!uagr && magr->mtrapped))
        hitbon -= 3;
    if (uagr && near_capacity() != 0)
        hitbon -= (near_capacity() * 2) - 1;
    if (hitbon <= 0)
        hitbon = 1;

    wepbon += hitval(mwep, mdef);
    /* Monsters have no concept of weapon skills, so this is player-only
       for now */
    if (uagr)
        wepbon += weapon_hit_bonus(mwep);

    /* FIXME: currently, only players can dual-wield. */
    if (uagr && u.twoweap) {
        wepbon2 += hitval(swep, mdef);
        wepbon2 += weapon_hit_bonus(swep);
    }

    /* Damage bonuses and penalties */
    if (mon_dambon(magr)) /* rings */
        dambon += rnd(mon_dambon(magr));
    dambon += find_mac_dam(mdef); /* AC-tied damage reduction */

    if (uagr) {
        check_caitiff(mtmp); /* check for knight dishonorable attacks */

        /* attacking peaceful creatures is bad for the samurai's giri */
        if (Role_if(PM_SAMURAI) && mtmp->mpeaceful && u.ualign.record > -10) {
            pline(msgc_alignbad, "You dishonorably attack the innocent!");
            adjalign(-1);
        }
    }

    /* whether or not we've already performed a weapon attack */
    int wepcount = 0;

    /* shade checks */
    enum objslot silverslot = os_invalid; /* potential silver damage and slot */
    boolean silver = FALSE;
    boolean blessed = FALSE; /* blessing touches the defender */
    boolean shade_bypass = FALSE; /* bypass shade "resistance"  */
    boolean cancelled = FALSE; /* MC procced */
    int jousting = 0;
    int wtype = 0; /* weapon type to exercise */
    const char *killmsg; /* kill messages */

    /* armor slots, for more seamless access */
    struct obj *arm = which_armor(magr, os_arm);
    struct obj *armc = which_armor(magr, os_armc);
    struct obj *armh = which_armor(magr, os_armh);
    struct obj *arms = which_armor(magr, os_arms);
    struct obj *armg = which_armor(magr, os_armg);
    struct obj *armf = which_armor(magr, os_armf);
    struct obj *armu = which_armor(magr, os_armu);

    int sum[NATTK]; /* stores attack results, used for AT_HUGS */
    int hitbon_sum; /* hit bonus including weapon hit bonuses */
    int dmg;
    int i;
    int hitroll = 0; /* used to check if something hit, beheading (vorpal),
                        weapon shattering */
    struct attack *mattk;
    struct attack alt_attk;
    for (i = 0; i < NATTK; i++) {
        /* Damage bonus (or penalty) only applies for the 1st attack */
        if (i > 0)
            dambon = 0;

        sum[i] = 0;
        silverslot = os_invalid;
        silver = FALSE;
        blessed = FALSE;
        shade_bypass = FALSE;
        cancelled = FALSE;
        mattk = getmattk(mdat, i, sum, &alt_attk);

        if (engulfed(mdef) == magr && (mattk->aatyp != AT_ENGL))
            continue;

        wep = NULL; /* allowing checking wep to imply AT_WEAP */
        if (mattk->aatyp == AT_WEAP) {
            wep = mwep;
            wepcount++;
            /* second weapon attack is only performed in two-weapon combat */
            if (wepcount > 1) {
                if (wepcount > 2) {
                    impossible("More than 2 weapon attacks?");
                    return 0;
                }

                if (!uagr || !u.twoweap)
                    continue;
                wep = swep;
            }
        }

        /* Ensure that we are performing a melee attack and nothing else.
           Gaze is special -- it can be used both reflexively in melee
           and at range. */
        if (mattk->aatyp == AT_SPIT ||
            mattk->aatyp == AT_BREA ||
            mattk->aatyp == AT_AREA)
            continue;

        /* An engulfed target is only hit further by the engulf attack */
        if (mattk->aatyp != AT_ENGL &&
            engulfed(mdef) == magr)
            continue;

        /* calculate hit bonus from current attack (weapon can adjust) */
        hitbon_sum = hitbon;
        if (mattk->aatyp == AT_WEAP)
            hitbon_sum += (wepcount == 1 ? wepbon : wepbon2);

        /* did our attack hit? some always do */
        hitroll = rnd(20 + i);
        if (mattk->aatyp != AT_EXPL && /* those 3 always hit */
            mattk->aatyp != AT_BOOM &&
            mattk->aatyp != AT_GAZE &&
            mattk->aatyp != AT_HUGS && /* doesn't always hit, but see below */
            engulfed(mdef) != magr && /* engulfed monsters are never missed */
            hitbon_sum <= hitroll) {
            if (vis)
                pline("%s %s!",
                      M_verbs(magr, "%smiss",
                              hitbon_sum == hitroll ? "just " : ""),
                      udef ? "you" : mon_nam(mdef));
            continue;
        }

        /* engulfing doesn't work on non-engulfers to prevent engulf chains,
           and avoid engulfing if already engulfing something else. Also,
           don't engulf huge monsters because they're too big */
        if (mattk->aatyp == AT_ENGL &&
            (attacktype(mdef->data, AT_ENGL) ||
             mdef->wormno || /* too much hassle for now */
             mdef->data->msize >= MZ_HUGE ||
             (engulfed(mdef) != magr && engulfing(magr))))
            continue;

        /* hugging attacks has a special hit rate -- it succeeds based on
           whether or not the 2 previous attacks succeeded (or if already
           hugging the target), and never vs other huggers
           FIXME: monsters hugging each other isn't implemented yet */
        if (mattk->aatyp == AT_HUGS && stuck(mdef) != magr &&
            (!sticks(mdef->data) || stuck(mdef) ||
             !sum[i - 1] || !sum[i - 2]))
            continue;
        if (mattk->aatyp == AT_HUGS && !uagr && !udef)
            continue;

        /* petrification checks */
        long protector = attk_protection((int)mattk->aatyp);
        if (mattk->aatyp == AT_WEAP && wep)
            protector = 0L; /* attk_protection doesn't check this */

        boolean petrify = FALSE;

        /* Kill message in case petrification does happen */
        const char *stone_killer = "attacking %s directly";
        if (mattk->aatyp == AT_CLAW)
            stone_killer = "clawing %s barehanded";
        else if (mattk->aatyp == AT_KICK)
            stone_killer = "kicking %s barefoot";
        else if (mattk->aatyp == AT_BITE)
            stone_killer = "biting %s";
        else if (mattk->aatyp == AT_STNG)
            stone_killer = "stinging %s";
        else if (mattk->aatyp == AT_TUCH)
            stone_killer = "touching %s barehanded";
        else if (mattk->aatyp == AT_BUTT)
            stone_killer = "headbutting %s with no helmet";
        else if (mattk->aatyp == AT_TENT)
            stone_killer = "sucking %s";
        else if (mattk->aatyp == AT_WEAP)
            stone_killer = "punching %s barehanded";
        else if (mattk->aatyp == AT_HUGS)
            stone_killer = armc ? "hugging %s without gloves" :
                "hugging %s without a cloak";
        else if (mattk->aatyp == AT_ENGL)
            stone_killer = "trying to engulf %s";
        stone_killer = msgprintf(stone_killer, k_monnam(mdef));

        if (protector == 0L || /* no protection needed */
            (protector == W_MASK(os_armg) && !armg && !wep) ||
            (protector == W_MASK(os_armf) && !armf) ||
            (protector == W_MASK(os_armh) && !armh) ||
            (protector == (W_MASK(os_armc) | W_MASK(os_armg)) &&
             (!armc || !armg))) {
            if (!confused(magr) && !stunned(magr) && !Conflict) {
                if (uagr && flags.verbose) /* elaborate on what happened */
                    pline_implied(combat_msgc(magr, mdef, cr_miss),
                                  "You reflexively avoid %s %s.",
                                  mattk->aatyp == AT_CLAW ? "clawing" :
                                  mattk->aatyp == AT_KICK ? "kicking" :
                                  mattk->aatyp == AT_TUCH ? "touching" :
                                  mattk->aatyp == AT_BUTT ? "head-butting" :
                                  mattk->aatyp == AT_WEAP ? "punching" :
                                  mattk->aatyp == AT_BITE ? "biting" :
                                  mattk->aatyp == AT_STNG ? "stinging" :
                                  mattk->aatyp == AT_TENT ? "sucking" :
                                  mattk->aatyp == AT_HUGS &&
                                  mattk->adtyp == AT_WRAP ? "wrapping" :
                                  mattk->aatyp == AT_HUGS ? "grabbing" :
                                  mattk->aatyp == AT_ENGL ? "engulfing" :
                                  "hitting",
                                  mon_nam(mdef));
                continue;
            } else
                petrify = TRUE; /* Not yet, we want to do the hit first */
        }

        /* check for silver/blessed damage bonuses */

#define addbonus(slot) {                                        \
        if (slot && objects[slot].oc_material == SILVER)        \
            silverslot = os_slot;                               \
        if (slot && slot->blessed)                              \
            blessed = TRUE;                                     \
        if (slot && slot->otyp == MIRROR)                       \
            shade_bypass = TRUE; }

        if (wep)
            addbonus(wep);
        else if (mattk->aatyp == AT_CLAW ||
                 mattk->aatyp == AT_WEAP || /* bare handed combat */
                 mattk->aatyp == AT_TUCH ||
                 mattk->aatyp == AT_HUGS) {
            addbonus(armg);

            /* Check rings. Touch attacks doesn't make ring contact, or
               at least not enough to do anything notable */
            if (!armg && mattk->aatyp != AT_TUCH) {
                struct obj *ring;
                int rings = 0;

                ring = which_armor(magr, os_ringl);
                if (ring && objects[ring->otyp].oc_material == SILVER)
                    rings++;
                if (ring && ring->blessed)
                    blessed = TRUE;
                ring = which_armor(magr, os_ringr);
                if (ring && objects[ring->otyp].oc_material == SILVER)
                    rings++;
                if (ring && ring->blessed)
                    blessed = TRUE;

                /* Ring slots are special cased -- os_ringl is
                   regarded as "one ring", os_ringr as two */
                if (rings)
                    silverslot = (rings > 1 ? os_ringr : os_ringl);
            }

            /* Hugs also checks body armor unless the defender is
               a shade (who can't be grabbed properly) */
            if (mattk->aatyp == AT_HUGS &&
                mdef->data != &mons[PM_SHADE]) {
                if (armc)
                    addbonus(armc);
                else if (arm)
                    addbonus(arm);
                else if (armu)
                    addbonus(armu);
            }
        } else if (mattk->aatyp == AT_KICK)
            addbonus(armf);
        else if (mattk->aatyp == AT_BUTT)
            addbonus(armh);

        if (silverslot != os_invalid)
            silver = TRUE;
#undef addbonus

        /* calculate general damage */
        dmg = 0;
        if (mattk->damd)
            dmg = dice(mattk->damn ? mattk->damn : m_mlev(magr) + 1,
                       mattk->damd);

        switch (mattk->aatyp) {
        case AT_CLAW: /* Ordinary melee attacks (with various flavour) */
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            if (vis) {
                if (mattk->aatyp == AT_TENT)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s tentacles suck %s!",
                          uagr ? "Your" : s_suffix(Monnam(magr)),
                          udef ? "you" : mon_nam(mdef));
                else
                    pline(combat_msgc(magr, mdef, cr_hit), "%s %s!",
                          M_verbs(magr, hitverb(magr, mattk)),
                          udef ? "you" : mon_nam(mdef));
            } else
                noises(magr, mattk);
            break;
        case AT_HUGS:
            if (mdef->data == &mons[PM_SHADE]) {
                /* Silvered or blessed gloves/rings still hurt,
                   even if the shade can't be grabbed */
                if (vis)
                    pline(combat_msgc(magr, mdef, silver || blessed ?
                                      cr_resist : cr_immune),
                          "%s hug passes %sthrough %s.",
                          uagr ? "Your" : s_suffix(Monnam(magr)),
                          !silver && !blessed ? "harmlessly " : "",
                          udef ? "you" : mon_nam(mdef));
                dmg = 0;
                break;
            }

            if (stuck(mdef) != magr) {
                boolean drownable = is_pool(level, m_mx(magr), m_my(mdef)) &&
                    !swims(mdef) && !unbreathing(mdef) &&
                    mattk->adtyp == AD_WRAP;
                /* ad_wrap has lower success chance */
                if (mattk->adtyp == AD_WRAP &&
                    (cancelled(magr) ||
                     rn2(10, drownable && udef ?
                         rng_eel_drowning :
                         rng_main))) {
                    if (vis)
                        pline(udef && drownable ? msgc_fatalavoid :
                              combat_msgc(magr, mdef, cr_miss),
                              "%s against %s %s.", M_verbs(magr, "brush"),
                              udef ? "your" : s_suffix(mon_nam(mdef)),
                              mbodypart(mdef, LEG));
                    dmg = 0;
                    if (petrify) {
                        if (uagr)
                            instapetrify(killer_msg(STONING, stone_killer));
                        else
                            minstapetrify(magr, mdef);
                        return uagr ? 0 : DEADMONSTER(magr) ?
                            MM_HIT | MM_AGR_DIED : 0;
                    }
                    break;
                } else if ((mattk->adtyp != AD_WRAP && rn2(2) ||
                            slip_free(magr, mdef, mattk))) {
                    dmg = 0;
                    break; /* no message for now */
                }

                if (vis) {
                    if (mattk->adtyp == AD_WRAP)
                        pline(udef && drownable ? msgc_fatal :
                              combat_msgc(magr, mdef, cr_hit),
                              "%s %sself around %s!",
                              M_verbs(magr, "swing"),
                              uagr ? "your" : "it",
                              udef ? "you" : mon_nam(mdef));
                    else
                        pline(combat_msgc(magr, mdef, cr_hit),
                              "%s %s!", M_verbs(magr, "grab"),
                              udef ? "you" : mon_nam(mdef));
                } else
                    noises(magr, mattk);
                if (petrify) {
                    if (uagr)
                        instapetrify(killer_msg(STONING, stone_killer));
                    else
                        minstapetrify(magr, mdef);
                    return uagr ? 0 : DEADMONSTER(magr) ?
                        MM_HIT | MM_AGR_DIED : 0;
                }
                u.ustuck = (uagr ? mdef : magr); /* FIXME */
                break;
            } else {
                if (mattk->adtyp == AD_WRAP &&
                    is_pool(level, m_mx(magr), m_my(magr)) &&
                    !cancelled(magr)) {
                    /* Petrification checks, just in case
                       (this code should never run, but might do if polymorph
                       doesn't check stuff properly) */
                    if (petrify) {
                        if (uagr)
                            instapetrify(killer_msg(STONING, stone_killer));
                        else
                            minstapetrify(magr, mdef);
                        return uagr ? 0 : DEADMONSTER(magr) ?
                            MM_HIT | MM_AGR_DIED : 0;
                    }

                    /* At this point, the eel/kraken/whatever is going to
                       attempt a drowning. No matter the outcome, this causes
                       water damage to inventory. Do this first in case the
                       end result is a dead player (if this happens, we can't
                       do it later) */
                    water_damage_chain(m_minvent(mdef), FALSE);

                    /* Check resists */
                    if (!swims(mdef) && !unbreathing(mdef)) {
                        if (vis)
                            pline(combat_msgc(magr, mdef, cr_kill),
                                  "%s %s...", M_verbs(magr, "drown"),
                                  udef ? "you" : mon_nam(mdef));
                        else if (mdef->mtame)
                            pline(msgc_petfatal,
                                  "You have a sad feeling for a moment, "
                                  "then it passes.");
                        else
                            noises(magr, mattk);
                        if (udef) {
                            done(DROWNING,
                                 killer_msg(DROWNING,
                                            msgprintf("%s by %s",
                                                      Is_waterlevel(&u.uz)
                                                      ? "the Plane of Water"
                                                      : a_waterbody(mtmp->mx,
                                                                    mtmp->my),
                                                      an(mtmp->data->mname))));
                            return 0; /* lifesaved */
                        } else {
                            mdef->mhp = 0;
                            monkilled(magr, mdef, "", (int)mattk->adtyp);
                            if (!DEADMONSTER(mdef))
                                return 0;
                            return MM_DEF_DIED |
                                (grow_up(magr, mdef) ? 0 : MM_AGR_DIED);
                        }
                    } else if (vis)
                        pline(combat_msgc(magr, mdef, cr_immune),
                              "%s to drown %s.",
                              M_verbs(magr, "try"),
                              udef ? "you" : mon_nam(mdef));
                    else
                        noises(magr, mattk);
                } else {
                    if (udef)
                        exercise(A_STR, FALSE);
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s being %s.", M_verbs(magr, "are"),
                          (magr->data == &mons[PM_ROPE_GOLEM]) ?
                          "choked" : "crushed");
                }
            }
            break;
        case AT_GAZE:
            shade_bypass = TRUE;
            /* Medusa gaze already operated through m_respond in dochug();
               don't gaze more than once per round. */
            if (magr->mdata != &mons[PM_MEDUSA])
                sum[i] = gazemm(magr, mdef, mattk);
            dmg = 0;
            break;
        case AT_EXPL:
            shade_bypass = TRUE;
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s!", M_verbs(magr, "explode"));
            else
                noises(magr, mattk);
            if (cancelled(magr))
                return MM_HIT | MM_AGR_DIED; /* we're done here */
            if (magr->data->mlet == S_LIGHT) {
                /* light explosions only affects those who can see */
                if (blind(mdef) ||
                    (mattk->adtyp == AD_BLND && invisible(magr) &&
                     !see_invisible(mdef))) {
                    if (vis && (blind(mdef) || !udef))
                        pline(combat_msgc(magr, mdef, cr_immune),
                              "%s unaffected.", M_verbs(mdef, "seem"));
                    else if (vis) /* lack of see invis vs blinding invis */
                        pline(msgc_yafm,
                              "You get the impression it was not terribly bright.");
                    return MM_HIT | MM_AGR_DIED;
                }
            } else {
                if (acurr(mdef, A_DEX) > rnd(20)) {
                    if (vis)
                        pline(combat_msgc(magr, mdef, cr_resist),
                              "%s some of the blast.", M_verbs(mdef, "duck"));
                    dmg /= 2;
                } else if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s blasted!", M_verbs(mdef, "are"));
            }
            sum[i] |= MM_HIT | MM_AGR_DIED;
            break;
        case AT_ENGL:
            shade_bypass = TRUE; /* maybe not? */
            if (engulfing(magr)) { /* already engulfed */
                mdef->swallowtime--;
                break;
            }

            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s %s!",
                      M_verbs(magr, mattk->adtyp == AD_DGST ? "swallow" :
                              "engulf"), udef ? "you" : mon_nam(mdef));
            else
                noises(magr, mattk);

            if (udef) {
                if (Punished)
                    unplacebc(); /* get rid of ball for now */
                action_interrupted();
                reset_occupations(TRUE);
                if (number_leashed()) {
                    pline(msgc_petwarning, "The %s loose.",
                          number_leashed() > 1 ? "leashes snap" :
                          "leash snaps");
                    unleash_all();
                }
            }

            /* dismount hero to prevent oddities */
            if ((uagr || udef) && u.usteed)
                dismount_steed(DISMOUNT_ENGULFED);

            /* old location */
            int x = m_mx(magr);
            int y = m_my(magr);

            if (uagr) {
                if (u.utrap) {
                    pline(msgc_statusheal,
                          "You release yourself from the %s.",
                          u.utraptype == TT_WEB ? "web" : "trap");
                    u.utrap = 0;
                }
                u.ux = m_mx(mdef);
                u.uy = m_my(mdef);
            } else {
                magr->mtrapped = 0;
                remove_monster(m_dlevel(magr), m_mx(mon), m_my(mon));
                place_monster(magr, m_mx(mdef), m_my(mdef), TRUE);

                /* Clear muxy if necessary, it can't be equal to mxy */
                if (udef) {
                    magr->mux = COLNO;
                    magr->muy = ROWNO;
                    if (Punished)
                        placebc(); /* place the ball back */
                }
            }
            newsym(x, y);
            newsym(m_mx(magr), m_my(magr));
            if (udef) {
                win_pause_output(P_MESSAGE);
                vision_full_recalc(2);
                swallowed(1); /* special engulf display for hero */
            }

            /* kill light sources */
            struct obj *obj;
            for (obj = m_minvent(mdef); obj; obj = obj->nobj)
                snuff_lit(obj);

            int engulf_timer = 25 - m_mlev(magr);
            if (engulf_timer > 0)
                engulf_timer = rnd(engulf_timer) / 2;
            else if (tim_tmp < 0)
                engulf_timer = -(rnd(-engulf_timer) / 2);
            engulf_timer += -find_mac(mdef) + 10;
            mdef->swallowtime = engulf_timer < 2 ? 2 : engulf_timer;
            break;
        case AT_WEAP:
            /* Needed a bunch of times, so set up killer message right away */
            killmsg = msgcat(s_suffix(k_monnam(mdef)), xname(wep));
            if (uagr)
                killmsg = msgcat_many(uhis(), " own ", xname(wep));

            /* Reset the damage value, we'll recalculate it afterwards. This
               is because we want to calculate weapon damage and perform
               weapon specific stuff here, and then deal an additional damage
               bonus from the base monster damage+type by continuing to that
               relevant codepath. This results in equal behaviour for
               physical weapon attacks, but makes non-physical weapon attacks
               less awkward/buggy. Note that players wont get the secondary
               part unless polymorphed, this will have unintentional side
               effects in balance otherwise. */
            dmg = 0;
            if (!vis)
                noises(magr, mattk);
            else if (!wep) /* bare-handed combat/martial arts */
                pline(combat_msgc(magr, mdef, cr_hit), "%s %s!",
                      M_verbs(magr,
                              ((uagr && (Role_if(PM_MONK) ||
                                         Role_if(PM_SAMURAI))) ||
                               magr->data == &mons[PM_MONK] ||
                               magr->data == &mons[PM_GRAND_MASTER] ||
                               magr->data == &mons[PM_ABBOT] ||
                               magr->data == &mons[PM_MASTER_KAEN] ||
                               magr->data == &mons[PM_SAMURAI] ||
                               magr->data == &mons[PM_LORD_SATO] ||
                               magr->data == &mons[PM_ROSHI] ||
                               magr->data == &mons[PM_ASHIKAGA_TAKAUJI]) ?
                              "strike" : "hit"),
                      udef ? "you" : mon_nam(mdef));
            else if (wep->otyp == CORPSE)
                pline((!touch_petrifies(&mons[wep->corpsenm]) ||
                       resists_ston(mdef)) ?
                      combat_msgc(magr, mdef, cr_hit) :
                      udef ? msgc_fatalavoid : mdef->mtame ? msgc_petfatal :
                      combat_msgc(magr, mdef, cr_hit),
                      "%s %s with the %s corpse.", M_verbs(magr, "hit"),
                      udef ? "you" : mon_nam(mdef),
                      mons[wep->corpsenm].mname);
            else /* taken from L's weapon type patch */
                pline(combat_msgc(magr, mdef, cr_hit),
                      udef ? "%s %s %s %s!" : "%s %s %s %s at %s!",
                      M_verbs(magr,
                              ((objects[wep->otyp].oc_dir & WHACK) &&
                               (!(objects[wep->otyp].oc_dir & PIERCE) ||
                                rn2(2))) ?
                              ((objects[wep->otyp].oc_skill == P_CLUB ||
                                objects[wep->otyp].oc_skill == P_MACE ||
                                objects[wep->otyp].oc_skill == P_MORNING_STAR) ?
                               "club" : "whack") :
                              ((objects[wep->otyp].oc_dir & PIERCE) &&
                               (!(objects[wep->otyp].oc_dir & SLASH) ||
                                rn2(2))) ?
                              (is_blade(wep) ? "stab" : "jab") :
                              ((objects[wep->otyp].oc_dir & SLASH) ?
                               ((uagr && Role_if(PM_BARBARIAN)) ||
                                magr->data == &mons[PM_BARBARIAN]) ?
                               "smite" : rn2(2) ? "hack" : is_axe(wep) ?
                               "hew" : "slash") :
                              (objects[wep->otyp].oc_skill == P_WHIP) ?
                              "whip" : "hit"),
                      uagr ? shk_your(wep) : mhis(magr),
                      singular(wep, xname),
                      udef ? "you" : mon_nam(mdef));

            /* we've gotten the hit message, now check for petrification */
            if (petrify) {
                if (uagr)
                    instapetrify(killer_msg(STONING, stone_killer));
                else
                    minstapetrify(magr, mdef);
                return uagr ? 0 : DEADMONSTER(magr) ?
                    MM_HIT | MM_AGR_DIED : 0;
            }

            /* after the checks below, wep might no longer exist */
            boolean proper = FALSE; /* using a proper weapon or hands */
            if (wep &&
                !is_launcher(wep) &&
                !is_missile(wep) &&
                !is_ammo(wep) &&
                (wep->oclass == WEAPON_CLASS ||
                 is_weptool(wep))) {
                dmg = dmgval(wep, mdef);
                proper = TRUE;

                /* for players: weapon skill damage bonus */
                if (uagr)
                    dmg += weapon_dam_bonus(wep);

                wtype = uwep_skill_type();

                /* skill exercise: only players can exercise skills */
                if (uagr) {
                    /* only a secondary weaopn hit exercises twoweapon since
                       it trains the off-hand capabilities */
                    if ((u.twoweap && wepcount > 1) ||
                        !u.twoweap)
                        use_skill(wtype, 1);
                }
            } else if (wep && wep->oclass == POTION_CLASS) {
                if (wep->quan > 1)
                    wep = splitobj(obj, 1);
                obj_extract_self(wep);
                if (uagr)
                    setnotworn(wep);
                else
                    MON_NOWEP(wep);
                /* TODO: Adjust this function to work properly vs players.
                   Currently it only procs for players for certain thrown
                   potions, the rest is handled in potionbreathe(), which
                   potionhit calls. Rework this so that potionbreathe
                   ignores the "main" target (because he was hit with a
                   harsher effect) */
                potionhit(mdef, wep, magr);
                if ((!udef && DEADMONSTER(mdef)) ||
                    (!uagr && DEADMONSTER(magr))) {
                    /* Abort the rest of the attack */
                    return MM_HIT | (DEADMONSTER(mdef) ? MM_DEF_DIED : 0) |
                        (DEADMONSTER(magr) ? MM_AGR_DIED : 0);
                }
                dmg = 1;
            } else if (wep) {
                dmg = rnd(2); /* exceptions are handled below */
                switch (wep->otyp) {
                case BOULDER:  /* 1d20 */
                case HEAVY_IRON_BALL:  /* 1d25 */
                case IRON_CHAIN:       /* 1d4+1 */
                    dmg = dmgval(wep, mdef);
                    break;
                case BOOMERANG: /* might break */
                    if ((uagr ? rnl(4) : rn2(4)) == 4 - 1) {
                        if (vis)
                            pline(msgc_itemloss,
                                  "As %s %s, %s%s %s breaks into splinters.",
                                  m_verbs(magr, "hit"),
                                  udef ? "you" : mon_nam(mdef),
                                  wep->quan > 1 ? "one of " : "",
                                  uagr ? shk_your(wep) : mhis(wep),
                                  xname(wep));
                        m_useup(magr, wep);
                    }
                    break;
                case MIRROR:
                    if (breaktest(wep)) {
                        if (vis)
                            pline(uagr ? msgc_statusbad : msgc_monneutral,
                                  "%s %s %s.  That's bad luck!",
                                  uagr ? shk_your(wep) : mhis(wep),
                                  simple_typename(wep->otyp));
                        if (uagr)
                            change_luck(-2);
                        m_useup(magr, wep);
                    }
                    dmg = 1;
                    break;
                case EXPENSIVE_CAMERA:
                    if (vis)
                        pline(msgc_itemloss,
                              "%s in destroying %s camera.%s",
                              M_verbs(magr, "succeed"),
                              uagr ? shk_your(wep) : mhis(wep),
                              uagr ? "  Congratulations!" : "");
                    m_useup(magr, wep);
                    break;
                case CORPSE:
                    dmg = (wep->corpsenm >= LOW_PM ?
                           mons[wep->corpsenm].msize : 0) + 1;
                    if (!touch_petrifies(&mons[obj->corpsenm]))
                        break;

                    if (udef) { /* This should probably be in mstiffen */
                        if (!petrifying(&youmonst) &&
                            !resists_ston(&youmonst) &&
                            !(poly_when_stoned(youmonst.data) &&
                              polymon(PM_STONE_GOLEM, TRUE))) {
                            set_property(&youmonst, STONED, 5, TRUE);
                            set_delayed_killer(STONING,
                                               killer_msg_mon(STONING,
                                                              mtmp));
                        }
                    } else if (!petrifying(mdef))
                        mstiffen(mdef, magr);
                    break;
                case EGG:
                    if (!touch_petrifies(mdef) ||
                        pm_resistance(&mons[wep->corpsenm], MR_STONE)) {
                        if (vis)
                            pline(combat_msgc(magr, mdef, cr_hit), "Splat!");
                        if (touch_petrifies(&mons[wep->corpsenm]) &&
                            !resists_ston(mdef) && !petrifying(mdef)) {
                            if (vis) {
                                pline(udef ? msgc_fatal :
                                      mdef->mtame ? msgc_petfatal :
                                      combat_msgc(magr, mdef, cr_hit),
                                      "The egg%s stiffen%s %s!",
                                      wep->quan != 1 ? "s" : "",
                                      wep->quan == 1 ? "s" : "",
                                      udef ? "you" : mon_nam(mdef));
                                learn_egg_type(wep->corpsenm);
                            }
                            if (udef) {
                                if (!petrifying(&youmonst) &&
                                    !resists_ston(&youmonst) &&
                                    !(poly_when_stoned(youmonst.data) &&
                                      polymon(PM_STONE_GOLEM, TRUE))) {
                                    set_property(&youmonst, STONED, 5, TRUE);
                                    set_delayed_killer(STONING,
                                                       killer_msg_mon(STONING,
                                                                      mtmp));
                                }
                            } else
                                mstiffen(mdef, magr);
                        }
                        wep->quan = 1; /* hack: there's no m_useupall */
                        m_useup(magr, wep);
                        break;
                    }

                    /* hitting things that petrifies with non-stone-resistant
                       eggs */
                    if (vis)
                        pline(msgc_consequence,
                              "The egg%s alive any more...",
                              wep->quan > 1 ? "s aren't" : " isn't");
                    poly_obj(wep, ROCK);
                    break;
                case CLOVE_OF_GARLIC:
                    /* FIXME: find a suitable effect for players as opposed
                       to "do nothing" */
                    if (!udef && is_undead(mdef->data))
                        monflee(mdef, dice(2, 4), FALSE, TRUE);
                    break;
                case CREAM_PIE:
                    if (vis)
                        pline(combat_msgc(magr, mdef, cr_hit),
                              "%s splats over %s%s!",
                              An(singular(wep, xname)),
                              udef ? "your" : s_suffix(mon_nam(mdef)),
                              mbodypart(mdef, FACE));
                    else
                        pline(msgc_levelsound, "Splat!");
                    int creaming = rnd(25);
                    if (can_blnd(magr, mdef, AT_WEAP))
                        inc_timeout(mdef, BLINDED, creaming, FALSE);
                    if (udef)
                        u.ucreamed += creaming;
                    break;
                default:
                    dmg = wep->owt / 100;
                    dmg = min(max(dmg, 6), 1); /* min 1, capped at 6 */
                    break;
                }
            } else
                dmg = rnd(uagr && martial_bonus() ? 4 : 2);

            /* Reset weapon pointers in case a weapon was destroyed above */
            mwep = m_mwep(magr);
            swep = (!uagr ? NULL : uswapwep); /* FIXME */
            wep = mwep;
            if (wepcount > 1)
                wep = swep;

            if (proper && wep && wep->opoisoned &&
                is_poisonable(wep)) {
                /* alignment checks */
                if (uagr) {
                    if (Role_if(PM_SAMURAI)) {
                        pline(msgc_alignchaos,
                              "You dishonorably use a poisoned weapon!");
                        adjalign(-sgn(u.ualign.type));
                    } else if (u.ualign.type == A_LAWFUL &&
                               u.ualign.record > -10) {
                        pline(msgc_alignbad,
                              "You feel like an evil coward "
                              "for using a poisoned weapon.");
                        adjalign(-1);
                    }
                }

                poisoned("weapon", A_STR, killer_msg(DIED, killmsg), 30,
                         magr, mdef);

                /* check if the poison should wear off */
                if (!rn2(min(10 - (wep->owt / 10), 2))) {
                    wep->opoisoned = FALSE;
                    if (vis)
                        pline(msgc_itemloss, "The poison wears off.");
                }

                /* did the poison kill? (if it hit the player, it clearly
                   didn't at this point) */
                if (!udef && DEADMONSTER(mdef))
                    return MM_HIT | MM_DEF_DIED;
            }

            /* Some bonuses. These things used to include mon!=u.ustuck.
               I saw no reason to keep this behaviour
               TODO: maybe only allow knives/daggers to backstab? */
            if (proper && wep && (!uagr || !u.twoweap)) {
                /* Backstab. Only true rogues (not polymorph) can do this */
                if (((!udef && mon->mflee) ||
                     (udef && u_helpless(hr_afraid))) &&
                    magr->data == &mons[PM_ROGUE]) {
                    if (vis)
                        pline("%s %s from behind!", M_verbs(magr, "strike"),
                              udef ? "you" : mon_nam(mdef));
                    dmg += rnd(m_mlev(magr));
                }

                /* Weapon shattering with 2-handers or katanas for samurai.
                   For players, this can only happen if they're Skilled or
                   better. Since monsters currently lack weapon skills,
                   this is implemented as just happening 50% of the time
                   (compared to players). Only two-handed weapons can shatter,
                   except for samurai who can do it with merely a katana
                   (but can't be done when twoweaponing). */
                struct obj *defwep = m_mwep(mdef);
                if (dieroll == 2 && defwep && !is_flimsy(defwep) &&
                    (bimanual(wep) ||
                     (magr->data == &mons[PM_SAMURAI] &&
                      wep->otyp == KATANA)) &&
                    ((uagr && wtype != P_NONE &&
                      P_SKILL(wtype) >= P_SKILLED) ||
                     (!uagr && !rn2(2))) &&
                    !obj_resists(defwep, 50 + 15 * greatest_erosion(wep) -
                                 15 * greatest_erosion(defwep))) {
                    if (vis)
                        pline(udef ? msgc_itemloss : msgc_combatalert,
                              "%s %s shatters from the force of your blow!",
                              udef ? "Your" : s_suffix(Monnam(mdef)),
                              cxname2(defwep));
                    else
                        You_hear(msgc_monneutral, "something shatter.");
                    m_useup(mdef, defwep);
                }

                if (uagr && u.usteed && dmg && weapon_type(wep) == P_LANCE &&
                    (jousting = joust(mdef, wep))) {
                    dmg += dice(2, 10);
                    if (vis)
                        pline(msgc_combatalert, "%s %s!",
                              M_verbs(magr, "joust"), udef ? "you" :
                              mon_nam(mdef));
                    if (jousting < 0) { /* lance shattered */
                        if (vis)
                            pline(msgc_itemloss, "%s %s shatters on impact!",
                                  uagr ? "Your" : s_suffix(Monnam(magr)),
                                  xname(wep));
                        else
                            You_hear(msgc_monneutral, "something shatter.");
                        m_useup(magr, wep);

                        /* reset weapon pointers */
                        mwep = m_mwep(magr);
                        swep = (!uagr ? NULL : uswapwep); /* FIXME */
                        wep = mwep;
                        if (wepcount > 1)
                            wep = swep;
                    }
                }
            }

            if (wep && wep->oartifact && artifact_hit(magr, mdef, wep, &dmg,
                                                      dieroll)) {
                /* might have killed the monster */
                if (!udef && DEADMONSTER(mdef))
                    return MM_HIT | MM_DEF_DIED;
                /* TODO: logic copied from uhitm, figure out why it's there */
                if (dmg == 0)
                    return MM_HIT;
            }

            /* Avoid bogus damage numbers */
            if (dmg < 1)
                dmg = 1;

            /* Shades are unharmed by most attacks. */
            if (!silver && !blessed && !shade_bypass &&
                (!wep || !wep->oartifact)) {
                dmg = 0;
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s attack pass%s harmlessly through %s.",
                          uagr ? "Your" : s_suffix(Monnam(magr)),
                          uagr ? "" : "es", udef ? "you" : mon_nam(mdef));
            } else {
                if (udef)
                    losehp(dmg, killer_msgc(DIED, killmsg));
                else {
                    mon->mhp -= dmg;
                    if (mon->mhp <= 0)
                        monkilled(magr, mdef, "", AD_PHYS);
                }

                /* Revert dmg to pre-AT_WEAP state to deal with the
                   adtyp part */
                dmg = 0;
                if (mattk->damd)
                    dmg = dice(mattk->damn ? mattk->damn : m_mlev(magr) + 1,
                               mattk->damd);
            }
            break;
        default: /* no other attyps are active melee */
            break;
        }

        /* All damage logic aside from resistances done, now check
           for MC.
           MC protects against various melee attacks.
           Unlike vanilla, MC no longer nullifies damage unless
           the monster is cancelled, it only cuts it in half.
           The special effects obviously still go away. */
        if ((cancelled(magr) ||
             (rn2(50) && rn2(3) >= magic_negation(mdef))) &&
            (mattk->aatyp == AT_CLAW ||
             mattk->aatyp == AT_BITE ||
             mattk->aatyp == AT_KICK ||
             mattk->aatyp == AT_BUTT ||
             mattk->aatyp == AT_TUCH ||
             mattk->aatyp == AT_STNG ||
             mattk->aatyp == AT_WEAP) &&
            (mattk->adtyp == AD_MAGM ||
             mattk->adtyp == AD_FIRE ||
             mattk->adtyp == AD_COLD ||
             mattk->adtyp == AD_SLEE ||
             mattk->adtyp == AD_DISN ||
             mattk->adtyp == AD_ELEC ||
             mattk->adtyp == AD_DRST ||
             mattk->adtyp == AD_ACID ||
             mattk->adtyp == AD_PLYS ||
             mattk->adtyp == AD_DRLI ||
             mattk->adtyp == AD_TLPT ||
             mattk->adtyp == AD_WERE ||
             mattk->adtyp == AD_DRDX ||
             mattk->adtyp == AD_DRCO ||
             mattk->adtyp == AD_SLIM)) {
            cancelled = TRUE;
            if (cancelled(magr))
                dmg = 0;
            else if (dmg) /* don't turn 0 into 1 */
                dmg = dmg / 2 + 1;
        }

        /* check if the attack was resisted in any form
           or for certain cancelled attacks */
        if (!dmg)
            continue;

        /* deal with weaknesses */
        if (silver && hates_silver(mdef->data)) {
            if (vis)
                pline(udef ? msgc_statusbad : /* no msgc_weakness */
                      combat_msgc(magr, mdef, cr_hit),
                      "%s %s sear%s %s%s!",
                      uagr ? "Your" : s_suffix(Monnam(magr)),
                      silverslot == os_wep ? "silver weapon" :
                      silverslot == os_arm ? "silver armor" :
                      silverslot == os_armc ? "silvery cloak" :
                      silverslot == os_armh ? "silver helmet" :
                      silverslot == os_armf ? "silver boots" :
                      silverslot == os_armg ? "silvery gloves" :
                      silverslot == os_armu ? "silvery shirt" :
                      silverslot == os_ringl ? "silver ring" :
                      silverslot == os_ringr ? "silver rings" :
                      "imaginary silver",
                      silverslot != os_ringr ? "s" : "",
                      udef && !noncorporeal(mdef->data) ? "your" :
                      udef ? "you" :
                      !noncorporeal(mdef->data) ?
                      s_suffix(mon_nam(mdef)) :
                      mon_nam(mdef),
                      !noncorporeal(mdef->data) " flesh" : "");
            dmg += rnd(20);
        }

        if (blessed &&
            (is_undead(mdef->data) || is_demon(mdef->data)))
            dmg += rnd(4);

        if (!silver && !blessed && !shade_bypass &&
            (!wep || !wep->oartifact) &&
            mdef->data == &mons[PM_SHADE]) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s attack pass%s harmlessly through %s.",
                      uagr ? "Your" : s_suffix(Monnam(magr)),
                      uagr ? "" : "es", udef ? "you" : mon_nam(mdef));
            continue;
        }

        switch (mattk->adtyp) {
        case AD_MAGM:
            if (vis)
                pline(combat_msgc(magr, mdef, resists_magm(mdef) ?
                                  cr_immune : cr_hit),
                      "%s hit by a shower of missiles%s",
                      M_verbs(mdef, "are"),
                      !resists_magm(mdef) ? "!" :
                      ", but they bounce off.");
            break;
        case AD_FIRE:
            if (vis)
                pline(combat_msgc(magr, mdef, resists_fire(mdef) ?
                                  cr_immune : cr_hit),
                      "%s %s%s",
                      M_verbs(mdef, "are"),
                      on_fire(mdef->data, mattk),
                      !resists_fire(mdef) ? "!" :
                      udef ? ", but it's not that hot." :
                      ", but seems unharmed.");

            if (!cancelled) {
                if (m_mlev(mdef) > rn2(20))
                    destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
                if (m_mlev(mdef) > rn2(20))
                    destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
                if (m_mlev(mdef) > rn2(25))
                    destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
            }

            break;
        case AD_COLD:
            if (vis)
                pline(combat_msgc(magr, mdef, resists_cold(mdef) ?
                                  cr_immune : cr_hit),
                      "%s covered in frost%s",
                      M_verbs(mdef, "are"),
                      !resists_cold(mdef) ? "!" :
                      udef ? ", but it doesn't seem cold." :
                      ", but seems unharmed.");

            if (!cancelled && m_mlev(mdef) > rn2(20))
                destroy_mitem(mdef, POTION_CLASS, AD_COLD);

            break;
        case AD_ELEC:
            if (vis)
                pline(combat_msgc(magr, mdef, resists_magm(mdef) ?
                                  cr_immune : cr_hit),
                      "%s zapped%s",
                      M_verbs(mdef, "get"),
                      !resists_magm(mdef) ? "!" :
                      udef ? ", but aren't shocked." :
                      ", but isn't shocked.");

            if (!cancelled && m_mlev(mdef) > rn2(20))
                destroy_mitem(mdef, WAND_CLASS, AD_ELEC);

            break;
        case AD_SLEE:
            /* Sleep attacks dealing damage is intentional,
               allbeit unintuitive.
               TODO: improve this behaviour */
            if (cancelled || resists_sleep(mdef))
                break; /* no message */

            if (vis)
                pline(combat_msgc(magr, mdef, resists_magm(mdef) ?
                                  cr_immune : cr_hit),
                      "%s put to sleep!"
                      M_verbs(magr, mdef, "are"));

            if (udef)
                helpless(rnd(10), hr_asleep, "sleeping", NULL);
            else {
                mdef->mcanmove = 0;
                mdef->mfrozen = rnd(10);
                if (mdef->mstrategy == st_waiting)
                    mdef->mstrategy = st_none;
            }

            break;
        case AD_BLND:
            inc_timeout(mdef, BLINDED, dmg, FALSE);
            dmg = 0;
            break;
        case AD_DRST:
        case AD_DRDX:
        case AD_DRCO:
            /* only works 1/8 of the time (even less with MC) */
            if (cancelled || rn2(8))
                break;

            poisoned(msgprintf("%s %s",
                               uagr ? s_suffix(Monnam(mtmp)) :
                               "Your",
                               mpoisons_subj(magr, mattk)),
                     mattk->adtyp == AD_DRST ? A_STR :
                     mattk->adtyp == AD_DRDX ? A_DEX :
                     mattk->adtyp == AD_DRCO ? A_CON :
                     A_STR, killer_msg_mon(POISONING, magr), 30,
                     magr, mdef);
            break;
        case AD_DRIN:
            if (!has_head(mdef->data)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s unaffected.", M_verbs(magr, "are"));
                break;
            }

            if (armh && rn2(8)) {
                if (vis)
                    pline(uagr ? msgc_fatalavoid :
                          combat_msgc(magr, mdef, cr_resist),
                          "%s %s blocks the attack to %s head.",
                          udef ? "Your" : s_suffix(Monnam(mdef)),
                          helmet_name(armh),
                          udef ? "your" : mhis(mdef));
                break;
            }

            if (armh && armh->otyp == DUNCE_CAP) {
                /* copy the constriction message here since the
                   usual one can't be used for monsters */
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s cap constricts briefly, "
                          "then relaxes again.",
                          udef ? "Your" : s_suffix(Monnam(mdef)));
                break;
            }

            /* Players are never mindless.
               TODO: nor are polymorphed non-mindless monsters */
            if (mindless(mdef->data) && !udef) {
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s seem%s unaffected.",
                      udef ? "You" : Monnam(mdef),
                      udef ? "" : "s");
                break;
            }

            pline(udef && ABASE(A_INT) <= ATTRMIN(A_INT) ?
                  msgc_fatal_predone : udef ?
                  msgc_fatal : combat_msgc(magr, mdef, cr_hit),
                  "%s brain is eaten!", udef ? "Your" :
                  s_suffix(Monnam(mdef)));
            break;

            /* Non-player "int loss" is just additional damage.
               Do monster bonus damage here, rather than at the
               same place as player int loss. The reason is that
               it allows monsters to fail a lifesave from
               "brainlessness" similar to players. */
            if (!udef)
                mdef->mhp -= rnd(10);
            if ((udef && ABASE(A_INT) <= ATTRMIN(A_INT)) ||
                (!udef && mdef->mhp <= 0)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_kill),
                          "%s last thought fades away.",
                          udef ? "Your" : s_suffix(Monnam(mdef)));
                if (will_be_lifesaved(mdef)) {
                    if (udef)
                        done(DIED, killer_msg(DIED, "brainlessness"));
                    else
                        monkilled(magr, mdef, "", AD_DRIN);

                    /* lifesave procced */
                    pline(combat_msgc(magr, mdef, cr_kill),
                          "Unfortunately, %s brain is still gone.",
                          udef ? "your" : s_suffix(mon_nam(mdef)));
                }
                if (udef)
                    done(DIED, killer_msg(DIED, "brainlessness"));
                else
                    monkilled(magr, mdef, "", AD_DRIN);

                if (udef || !DEADMONSTER(mdef)) {
                    /* if we are here, monster somehow lifesaved without
                       the intrinsic (or lifesaved twice). This should
                       only happen in explorer mode, and should never
                       happen at all for non-players */
                    if (udef)
                        ABASE(A_INT) = ATTRMIN(A_INT) + 2;
                    else
                        impossible("Monster survived brainlessness?");

                    if (udef)
                        pline(msgc_intrgain,
                              "You feel like a scarecrow.");
                    break;
                } else
                    return MM_HIT | MM_DEF_DIED;
            }

            if (udef) {
                adjattrib(A_INT, -rnd(2), FALSE);
                forget_objects(50);
                exercise(A_WIS, FALSE);
            }

            break;
        case AD_PLYS:
            if (cancelled)
                break;

            if (free_action(mdef)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s momentarily.", M_verbs(mdef,
                                                     "stiffen"));
                break;
            }

            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s frozen!", M_verbs(mdef, "are"));
            if (udef)
                helpless(10, hr_paralyzed, "paralyzed by a monster",
                         NULL);
            else {
                mdef->mcanmove = 0;
                mdef->mfrozen += 10;
            }

            if (udef)
                exercise(A_DEX, FALSE);

            break;
        case AD_DRLI:
            if (cancelled || rn2(3) || resists_drli(mdef))
                break;

            losexp(msgcat("drained of life by ", k_monnam(magr)),
                   FALSE, magr, mdef);
            break;
        case AD_LEGS:
            if (!flying(magr) && !levitates(magr) &&
                (flying(mdef) || levitates(mdef) ||
                 (udef && u.usteed))) {
                pline(combat_msgc(magr, mdef, cr_miss),
                      "%s tries to reach %s %s!",
                      uagr ? "You" : Monnam(magr),
                      udef ? "your" : s_suffix(mon_nam(mdef)),
                      makeplural(mbodypart(mdef, LEG)));
                dmg = 0;
                break;
            }

            int side = rn2(2) ? RIGHT_SIDE : LEFT_SIDE;
            const char *sidestr = (side == RIGHT_SIDE) ? "right" : "left";
            if (cancelled(magr)) {
                pline(combat_msgc(magr, mdef, cr_miss),
                      "%s against %s %s %s!", M_verbs(magr, "nuzzle"),
                      udef ? "your" : s_suffix(mon_nam(mdef)),
                      sidestr, mbodypart(mdef, LEG));
                dmg = 0;
                break;
            }

            if (!armf) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s %s %s %s!", M_verbs(magr, "prick"),
                          udef ? "your" : s_suffix(mon_nam(mdef)),
                          sidestr, mbodypart(mdef, LEG));
            } else if (rn2(2) &&
                       (armf->otyp == LOW_BOOTS ||
                        armf->otyp == IRON_SHOES)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s the exposed part of %s %s %s!",
                          M_verbs(magr, "prick"),
                          udef ? "your" : s_suffix(mon_nam(mdef)),
                          sidestr, mbodypart(mdef, LEG));
            } else if (!rn2(5)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s through %s %s boot!",
                          M_verbs(magr, "prick"),
                          udef ? "your" : s_suffix(mon_nam(mdef)),
                          sidestr);
            } else {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s %s %s boot!", M_verbs(magr, "scratch"),
                          udef ? "your" : s_suffix(mon_nam(mdef)),
                          sidestr);
                dmg = 0;
                break;
            }
            set_wounded_legs(mdef, side, rnd(60 - acurr(mdef, A_DEX)));
            if (udef) {
                exercise(A_STR, FALSE);
                exercise(A_DEX, FALSE);
            }
            break;
        case AD_STON:
            if (rn2(3))
                break;

            if (cancelled(magr)) {
                if (!rn2(3)) {
                    if (!uagr)
                        You_hear(combat_msgc(magr, mdef, cr_immune),
                                 "a cough from %s!", mon_nam(magr));
                    else
                        pline(combat_msgc(magr, mdef, cr_immune),
                              "You cough!");
                }
                break;
            }

            boolean stiffen = !rn2_on_rng(10, udef ? rng_slow_stoning :
                                          rng_main);
            if (flags.moonphase == NEW_MOON && !have_lizard(mdef))
                stiffen = TRUE;
            if (vis && !uagr)
                You_hear(resists_ston(mdef) ?
                         combat_msgc(magr, mdef, cr_immune) :
                         udef && stiffen ? msgc_fatal :
                         udef ? msgc_fatalavoid :
                         mdef->mtame ? msgc_petfatal :
                         combat_msgc(magr, mdef, cr_hit),
                         "%s hissing!", s_suffix(mon_nam(magr)));
            else if (uagr)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "You hiss!");
            if (!stiffen)
                break;

            if (!udef)
                mstiffen(mdef, magr);
            else if (!petrifying(mdef) && !resists_ston(mdef) &&
                     !(poly_when_stoned(mdef->data) &&
                       polymon(PM_STONE_GOLEM, TRUE))) {
                set_property(mdef, STONED, 5, TRUE);
                set_delayed_killer(STONING,
                                   killer_msg_mon(STONING, magr));
                break;
            }
            break;
        case AD_STCK:
            /* FIXME: uvm/mvu-only for now */
            if (!stuck(mdef) && !sticks(mdef->data) &&
                (uagr || udef))
                u.ustuck = (uagr ? mdef : magr);
            break;
        case AD_WERE:
            if (cancelled || rn2(4))
                break;

            if (!udef)
                break; /* TODO */
            if (shapeshift_prot(mdef) || u.ulycn != NON_PM ||
                defends(AD_WERE, m_mwep(mdef)))
                break;

            if (vis)
                pline(msgc_statusbad, "%s %s feverish.",
                      udef ? "You" : Monnam(mdef),
                      udef ? "feel" : "looks");
            if (udef) {
                exercise(A_CON, FALSE);
                u.ulycn = monsndx(magr->data);
            }
            break;
        case AD_SGLD:
            if (cancelled(magr))
                break;

            struct obj *igold = findgold(m_minvent(mdef));
            struct obj *fgold = gold_at(m_dlevel(mdef),
                                        m_mx(mdef),
                                        m_my(mdef));
            /* Floor gold is generally preferred, unless
               the inventory gold is of more value in which
               that gold is taken 80% of the time. */
            if (fgold && (!igold || fgold->quan > igold->quan ||
                          !rn2(5))) {
                obj_extract_self(fgold);
                mpickobj(magr, fgold);
                newsym(m_mx(mdef), m_my(mdef));
                if (vis)
                    pline(udef ? msgc_itemloss :
                          combat_msgc(magr, mdef, cr_hit),
                          "%s some gold from between %s %s!",
                          M_verbs(magr, "quickly snatch"),
                          udef ? "your" : s_suffix(mon_nam(mdef)),
                          makeplural(mbodypart(mdef, FOOT)));
                /* teleport away unless the defender has gold too */
                if (!igold || !rn2(5))
                    mon_tele(magr, !!teleport_control(magr));
            } else if (igold) {
                int amount = min(igold->quan, somegold(igold->quan));
                if (amount < igold->quan)
                    igold = splitobj(igold, amount);
                obj_extract_self(igold);
                mpickobj(magr, igold);
                if (uagr || udef)
                    pline(udef && !uagr ? msgc_itemloss :
                          combat_msgc(magr, mdef, cr_hit),
                          "Your purse feels %s.",
                          uagr && udef ? "tidier" :
                          uagr ? "heavier" :
                          "lighter");
                else if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s steals some gold from %s!",
                          Monnam(magr), mon_nam(mdef));
                mon_tele(magr, !!teleport_control(magr));
            }
            break;
        case AD_SSEX:
            /* If the player is the offender, fallback to nymph
               behaviour. This avoids some awkwardness... */
            if (!uagr) {
                if (could_seduce(magr, mdef, mattk) == 1 &&
                    !cancelled(magr))
                    doseduce(magr, mdef);
                break;
            }
            /* fallthrough */
        case AD_SITM: /* for now, those are the same */
        case AD_SEDU:
            if (is_animal(magr->data) && cancelled(magr))
                break;

            /* Arbitrary assumptions:
               - Opposite-gendered creatures "seduce",
                 otherwise "charm"
               - Non-intelligent creatures (read: lacks hands)
                 are unaffected by any of those */
            boolean compatible =
                female(magr) != female(mdef);
            if (!is_animal(magr->data)) {
                if (dmgtype(mdef->data, AD_SEDU) ||
                    dmgtype(mdef->data, AD_SSEX)) {
                    /* TODO: should probably be changed for
                       players */
                    if (vis)
                        pline(msgc_npcvoice, "%s %s.",
                              M_verbs(magr, m_minvent(magr) ?
                                      "brag" : "make"),
                              m_minvent(magr) ?
                              "about the goods some dungeon "
                              "explorer provided" :
                              "some remarks about how difficult "
                              "theft is lately");
                    mon_tele(magr, !!teleport_control(magr));
                    break;
                } else if (cancelled(magr) ||
                           (!udef && nohands(mdef->data))) {
                    if (vis)
                        pline(combat_msgc(magr, mdef, cr_immune),
                              "%s %s to %s %s, but %s seem %s.",
                              uagr ? "You" : cancelled(magr) ?
                              Adjmonnam(magr, "plain") :
                              Monnam(magr), uagr ? "try" : "tries",
                              compatible ? "seduce" : "charm",
                              udef ? "you" : mon_nam(mdef), mhe(mdef),
                              compatible ? "uninterested" : "unaffected");
                    if (rn2(3))
                        mon_tele(magr, !!teleport_control(magr));
                    break;
                }
            }

            /* now do actual stealing logic */
            if (!m_minvent(mdef) ||
                (udef && inv_cnt(FALSE) == 1 && uskin())) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s to rob %s, but find%s nothing to steal.",
                          M_verbs(magr, "try"), udef ? "you" :
                          mon_nam(mdef), udef ? "" : "s");
                break;
            }

            /* roll a random item, weighted by whether or not you wear
               it */
            int stealroll = 0;
            struct obj *sobj;
            for (sobj = m_minvent(mdef); sobj; sobj = sobj->nobj) {
                if ((sobj == armu && (armc || arm)) ||
                    (sobj == arm && (armc || sobj == uskin())) ||
                    (sobj == armg && m_mwep(mdef)))
                    continue;
                stealroll += ((sobj->owornmask & W_WORN) ? 5 : 1);
            }
            stealroll = rn2(stealroll);
            for (sobj = m_minvent(mdef); sobj; sobj = sobj->nobj) {
                if ((sobj == armu && (armc || arm)) ||
                    (sobj == arm && (armc || sobj == uskin())) ||
                    (sobj == armg && m_mwep(mdef)))
                    continue;
                if ((stealroll -= ((sobj->owornmask & W_WORN) ?
                                   5 : 1)) < 0)
                    break;
            }

            /* Animals can't steal cursed items (or chained balls),
               and usually lacks the patience to steal things that
               take time to remove */
            if (((sobj->cursed &&
                 ((sobj->owornmask & W_WORN) ||
                  (sobj->owornmask & W_MASK(os_wep)) ||
                  sobj->otyp == LOADSTONE ||
                  (sobj->otyp == LEASH && sobj->leashmon)) ||
                  ((sobj->owornmask & W_RING) &&
                   which_armor(mdef, os_armg) &&
                   (which_armor(mdef, os_armg))->cursed)) ||
                 sobj == uball ||
                 (!uagr && !can_carry(magr, sobj)) ||
                 (objects[sobj->otyp].oc_delay >= 1 && rn2(10) &&
                  sobj->owornmask & W_WORN)) &&
                is_animal(magr->data)) {
                static const char *const how[] =
                    {"steal", "snatch", "grab", "take"};
                pline(combat_msgc(magr, mdef, cr_miss),
                      "%s to %s %s %s but gives up.",
                      M_verbs(magr, "try"), how[rn2(SIZE(how))],
                      udef ? "your" : s_suffix(mon_nam(mdef)),
                      (sobj->owornmask & W_WORN) ? equipname(sobj) :
                      cxname(sobj));
                break;
            }

            /* adornment can override other items when applicable */
            if (!is_animal(magr->data) &&
                obj_by_property(mdef, ADORNED, TRUE)) {
                sobj = obj_by_property(mdef, ADORNED, TRUE);
                if (uagr)
                    pline(msgc_consequence,
                          "You put a particular liking to %s.",
                          xname(sobj));
            }

            if (sobj->otyp == LEASH && sobj->leashmon)
                o_unleash(sobj);

            if ((sobj->owornmask & W_WORN) &&
                sobj->oclass == ARMOR_CLASS &&
                !is_animal(magr->data)) {
                int delay = objects[sobj].oc_delay;

                if (udef)
                    cancel_helplessness(hm_fainted,
                                        "Someone revives you.");
                /* Monsters can't be fainted */

                if (m_helpless(mdef, hm_all)) {
                    if (vis)
                        pline(combat_msgc(magr, mdef, cr_miss),
                              "%s to %s %s, but %s dismayed by %s lack "
                              "of response.", M_verbs(magr, "try"),
                              compatible ? "seduce" : "charm",
                              udef ? "you" : mon_nam(mdef), mhis(mdef));
                    break;
                }
                if (compatible && vis)
                    pline(udef ? msgc_itemloss :
                          combat_msgc(magr, mdef, cr_hit),
                          "%s %s and %s gladly %s %s %s.",
                          M_verbs(magr, "charm"),
                          udef ? "you" : mon_nam(mdef), mhe(mdef),
                          sobj->cursed &&
                          (uagr ? u.ufemale : magr->mfemale) ?
                          "let her take" : sobj->cursed ?
                          "let him take" : "hand over",
                          mhis(mdef), equipname(sobj));
                else if (vis)
                    pline(udef ? msgc_itemloss :
                          combat_msgc(magr, mdef, cr_hit),
                          "%s seduce%s %s and %s off %s %s.",
                          uagr ? "You" : Adjmonnam(mdef, "beautiful"),
                          udef ? "you" : mon_nam(mdef),
                          sobj->cursed ? "helps you to take" :
                          objects[sobj].oc_delay > 1 ? "you start taking" :
                          "you take", mhis(mdef), equipname(sobj));
                if (objects[sobj].oc_delay) {
                    if (uagr)
                        helpless(objects[sobj].oc_delay, hr_busy,
                                 "taking off clothes",
                                 "You finish disrobing.");
                    else {
                        mdef->mcanmove = 0;
                        mdef->mfrozen += objects[sobj].oc_delay;
                    }
                }
            }

            if (sobj->owornmask) {
                if (udef)
                    remove_worn_item(sobj, TRUE);
                else {
                    mdef->misc_worn_check &= ~sobj->owornmask;
                    sobj->owornmask = 0;
                }
            }

            /* set avenge bit to allow knight to reclaim stolen gear */
            if (udef && !uagr)
                magr->mavenge = TRUE;

            if (vis)
                pline(udef ? msgc_itemloss :
                      combat_msgc(magr, mdef, cr_hit),
                      "%s stole %s%s", uagr ? "You" : Monnam(magr),
                      doname(sobj), is_animal(magr) ?
                      ", and tries to get away with it." : ".");
            obj_extract_self(sobj);
            mpickobj(magr, sobj);

            /* did it steal a trice corpse w/o gloves? */
            if (sobj->otyp == CORPSE &&
                touch_petrifies(&mons[sobj->corpsenm]) &&
                !which_armor(magr, os_armg)) {
                /* oops... */
                if (uagr)
                    instapetrify(killer_msg(STONING,
                                            msgcat("attempted to steal "
                                                   an(xname(sobj)))));
                else /* do not credit anyone -- avoids breaking pacifist */
                    minstapetrify(magr, NULL);
            }
            if (!is_animal(magr))
                mon_tele(magr, !!teleport_control(magr));

            if (!uagr)
                monflee(magr, 0, FALSE, FALSE);
            break;
        case AD_TLPT:
            if (cancelled(magr))
                break;

            if (vis)
                pline_implied(combat_msgc(magr, mdef, cr_hit),
                              "%s position suddenly seems very uncertain!",
                              udef ? "Your" : s_suffix(Monnam(mdef)));
            mon_tele(mdef, FALSE); /* uncontrolled */

            break;
        case AD_RUST:
        case AD_CORR:
        case AD_DCAY:
            if (cancelled(magr))
                break;

            hurtarmor(mdef, mattk->adtyp);
            break;
        case AD_HEAL:
            if (cancelled(magr))
                break; /* revert to physical damage if cancelled */

            /* wearing armor or equipping weapons is not OK */
            if ((which_armor(mdef, os_arm) &&
                 which_armor(mdef, os_arm) != uskin()) ||
                which_armor(mdef, os_armc) ||
                which_armor(mdef, os_armu) ||
                which_armor(mdef, os_armh) ||
                which_armor(mdef, os_armf) ||
                which_armor(mdef, os_arms) ||
                which_armor(mdef, os_armg) ||
                m_mwep(mdef) ||
                (udef && uswapwep && u.twoweap)) {
                /* vanilla gave polymorphed healers a benefit, this makes
                   no sense, so check current form instead */
                if (mdef == &mons[PM_HEALER]) {
                    dmg = 0;
                    if (!rn2(5))
                        verbalize(udef ? msgc_hint : msgc_monneutral,
                                  "Doc, I can't help you unless you "
                                  "cooperate.");
                }
                break;
            }
            
        }
    }
}

/* Returns a proper verb for the "basic" melee attacks */
static const char *
hitverb(const struct monst *mon, const struct attack *mattk)
{
    /* special cases */
    if (mattk->aatyp == AT_BITE &&
        (mon->data == &mons[PM_RAVEN] ||
         mon->data == &mons[PM_TENGU] ||
         mon->data == &mons[PM_VROCK]))
        return "peck";

    if (mattk->aatyp == AT_CLAW &&
        (mon->data == &mons[PM_FIRE_ELEMENTAL] ||
         mon->data == &mons[PM_WATER_ELEMENTAL] ||
         mon->data == &mons[PM_KRAKEN]))
        return "lash";

    if (mattk->aatyp == AT_CLAW &&
        mon->data == &mons[PM_EARTH_ELEMENTAL])
        return "pummel";

    if (mattk->aatyp == AT_CLAW &&
        is_undead(mon->data)) /* perhaps too broad? */
        return "scratch";

    if (mattk->aatyp == AT_CLAW &&
        (!strcmp(mbodypart(mon, HAND), "claw") ||
         !strcmp(mbodypart(mon, HAND), "paw") ||
         !strcmp(mbodypart(mon, HAND), "foreclaw") ||
         is_bird(magr->data)))
        return "claw";

    /* everything else */
    return (mattk->aatyp == AT_KICK ? "kick" :
            mattk->aatyp == AT_BITE ? "bite" :
            mattk->aatyp == AT_STNG ? "sting" :
            mattk->aatyp == AT_TUCH ? "touch" :
            mattk->aatyp == AT_BUTT ? "butt" :
            "hit");
}

/* Returns the monster that mon is engulfing */
struct monst *
engulfing(const struct monst *mon)
{
    if (!attacktype(mon->data, AT_ENGL))
        return NULL;

    /* The player is not part of dlev->monlist and needs a special case */
    if (mon != &youmonst && mon->mx == u.ux && mon->my == u.uy)
        return &youmonst;

    struct monst *mtmp;
    for (mtmp = m_dlevel(mon)->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (mon == mtmp)
            continue;
        if (m_mx(mon) == m_mx(mtmp) && m_my(mon) == m_my(mtmp))
            return mtmp;
    }
    return NULL;
}

/* Returns the monster that mon is engulfed by if any, or NULL otherwise
   FIXME: consider changing this logic to simply having a
   struct monst engulfed in struct monst */
struct monst *
engulfed(const struct monst *mon)
{
    if (attacktype(mon->data, AT_ENGL))
        return NULL;

    if (mon != &youmonst && mon != u.usteed &&
        mon->mx == u.ux && mon->my == u.uy)
        return &youmonst;

    struct monst *engulfer =
        m_dlevel(mon)->monsters[m_mx(mon)][m_my(mon)];

    if (engulfer != mon)
        return engulfer;
    return NULL;
}

/* Returns if magr is stuck to mdef.
   FIXME: Currently, only players can be stuck to monsters and vice versa.
   Once implemented, this logic can be simplified somewhat. */
struct monst *
stuck(const struct monst *mon)
{
    /* ensure that it doesn't have a hugging attack on its' own, because
       those can't be stuck to each other */
    if (sticks(mon->data))
        return NULL;

    if (mon == &youmonst)
        return u.ustuck;

    if (mon == u.ustuck)
        return &youmonst;
    return NULL;
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
    tmp += mon_hitbon(magr);

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
                             uagr ? s_suffix(Monnam(magr)) : "Your");
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
                break;
            return MM_AGR_DIED;
        }
        if (visad && valid_range && !resists_ston(mdef)) {
            pline(combat_msgc(magr, mdef, cr_kill), "%s %s gaze.",
                  M_verbs(magr, "meet"), s_suffix(mon_nam(magr)));
            if (udef) {
                action_interrupted();
                instapetrify(killer_msg(STONING,
                                        msgprintf("catching the eye of %s",
                                                  k_monnam(magr))));
            } else
                minstapetrify(mdef, magr);
            return (!udef && DEADMONSTER(mdef)) ? MM_DEF_DIED : 0;
        }
        break;
    case AD_CONF:
        conf = TRUE;
    case AD_STUN:
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
        if (!resists_sleep(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s gaze makes %s very sleepy...",
                      uagr ? "Your" : s_suffix(Monnam(mdef)),
                      udef ? "you" : mon_nam(mdef));
            sleep_monst(mdef, dmg, 0);
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

/* See comment at top of mattackm(), for return values. */
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
    tmp += mon_dambon(magr);

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
    case AD_MAGM:
        if (cancelled) {
            tmp = 0;
            break;
        }
        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s is hit by a shower of missiles!", Monnam(mdef));
        if (resists_magm(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "The missiles bounce off!");
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_COLD, tmp);
            tmp = 0;
        }
        break;
    case AD_FIRE:
        if (cancelled) {
            tmp = 0;
            break;
        }
        if (vis && !resists_fire(mdef))
            pline(combat_msgc(magr, mdef, cr_hit), "%s is %s!", Monnam(mdef),
                  on_fire(mdef->data, mattk));
        /* TODO: straw/paper golem with paper items in inventory, and/or who has
           a fire resistance source (i.e. these are in the wrong order) */
        if (pd == &mons[PM_STRAW_GOLEM] || pd == &mons[PM_PAPER_GOLEM]) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_kill),
                      "%s burns completely!", Monnam(mdef));
            mondied(mdef);
            if (!DEADMONSTER(mdef))
                return 0;
            else if (mdef->mtame && !vis)
                pline(msgc_petfatal, "May %s roast in peace.", mon_nam(mdef));
            return (MM_DEF_DIED | (grow_up(magr, mdef) ? 0 : MM_AGR_DIED));
        }
        burn_away_slime(mdef);
        tmp += destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
        tmp += destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        if (resists_fire(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s is on fire, but it doesn't do much.", Monnam(mdef));
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
        if (vis && !resists_cold(mdef))
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s is covered in frost!", Monnam(mdef));
        if (resists_cold(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s is coated in frost, but resists the effects.",
                      Monnam(mdef));
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
        if (vis && !resists_elec(mdef))
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s gets zapped!", Monnam(mdef));
        tmp += destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        if (resists_elec(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s is zapped, but doesn't seem shocked.", Monnam(mdef));
            shieldeff(mdef->mx, mdef->my);
            golemeffects(mdef, AD_ELEC, tmp);
            tmp = 0;
        }
        /* only rings damage resistant players in destroy_item */
        tmp += destroy_mitem(mdef, RING_CLASS, AD_ELEC);
        break;
    case AD_ACID:
        if (cancelled(magr)) {
            tmp = 0;
            break;
        }
        if (resists_acid(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s is covered in acid, but it seems harmless.",
                      Monnam(mdef));
            tmp = 0;
        } else if (vis) {
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s is covered in acid!", Monnam(mdef));
            pline_implied(combat_msgc(magr, mdef, cr_hit),
                          "It burns %s!", mon_nam(mdef));
        }
        if (!rn2(30))
            hurtarmor(mdef, AD_ACID);
        if (!rn2(6))
            acid_damage(MON_WEP(mdef));
        break;
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
        hurtarmor(mdef, AD_RUST);
        if (mdef->mstrategy == st_waiting)
            mdef->mstrategy = st_none;
        tmp = 0;
        break;
    case AD_CORR:
        if (cancelled(magr))
            break;
        hurtarmor(mdef, AD_CORR);
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
        hurtarmor(mdef, AD_DCAY);
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
    case AD_SLEE:
        if (!cancelled && !mdef->msleeping && sleep_monst(mdef, rnd(10), -1)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s is put to sleep by %s.", Monnam(mdef), mon_nam(magr));

            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
            slept_monst(mdef);
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
            set_property(mdef, CONFUSION, tmp, TRUE);
            if (mdef->mstrategy == st_waiting)
                mdef->mstrategy = st_none;
        }
        break;
    case AD_BLND:
        if (can_blnd(magr, mdef, mattk->aatyp, NULL)) {
            unsigned rnd_tmp;

            if (vis && !blind(mdef))
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s is blinded.", Monnam(mdef));
            rnd_tmp = dice((int)mattk->damn, (int)mattk->damd);
            set_property(mdef, BLINDED, rnd_tmp, TRUE);
        }
        tmp = 0;
        break;
    case AD_HALU:
        if (!cancelled(magr) && haseyes(pd) && !blind(mdef) &&
            !resists_hallu(mdef)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s is freaked out.", Monnam(mdef));
            set_property(mdef, CONFUSION, tmp, TRUE);
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
            add_to_minv(magr, gold);
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
        if (!cancelled && !rn2(3) && !resists_drli(mdef)) {
            tmp = dice(2, 6);
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s suddenly seems weaker!", Monnam(mdef));
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
            }

            /* add_to_minv() might free otmp [if it merges] */
            const char *onambuf = doname(otmp);
            add_to_minv(magr, otmp);

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
    case AD_DRST:
    case AD_DRDX:
    case AD_DRCO:
        if (!cancelled && !rn2(8)) {
            if (resists_poison(mdef)) {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s is poisoned, but seems unaffected.",
                          Monnam(mdef));
            } else {
                if (vis)
                    pline(combat_msgc(magr, mdef, cr_hit),
                          "%s %s was poisoned!", s_suffix(Monnam(magr)),
                          mpoisons_subj(magr, mattk));
                if (rn2(10))
                    tmp += rn1(10, 6);
                else {
                    if (vis)
                        pline(combat_msgc(magr, mdef, cr_kill0),
                              "The poison was deadly...");
                    tmp = mdef->mhp;
                }
            }
        }
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
            set_property(mdef, CONFUSION, dice(3, 8), FALSE);
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
            if (!resist(mdef, 0, 0) && !resists_magm(mdef)) {
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
sleep_monst(struct monst *mon, int amt, int how)
{
    if (resists_sleep(mon) || (how >= 0 && mon != &youmonst &&
                               resist(mon, (char)how, NOTELL))) {
        shieldeff(m_mx(mon), m_my(mon));
        return 0;
    }
    if (mon == &youmonst) {
        helpless(amt, hr_asleep, "sleeping", NULL);
        return 1;
    }
    if (!mon->mcanmove)
        return 0;

    amt += (int)mon->mfrozen;
    if (amt > 0) {  /* sleep for N turns */
        mon->mcanmove = 0;
        mon->mfrozen = min(amt, 127);
    } else {        /* sleep until awakened */
        mon->msleeping = 1;
    }
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
        type = AD_CORR;
    else if (dmgtype(mdef->data, AD_RUST))
        type = AD_RUST;
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
    pline_implied(msgc_monneutral, mdef == &youmonst ?
                  "%s %s %s %s." : "%s %s %s %s at %s.", Monnam(magr),
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
                break;
            }
            if (cancelled(mdef) ||
                !(msensem(mdef, magr) & MSENSE_VISION) ||
                !(msensem(magr, mdef) & MSENSE_VISION))
                break;
            if (!slow(mdef) && canseemon(mdef))
                pline(combat_msgc(mdef, magr, cr_hit),
                      "%s down under %s gaze!", M_verbs(mdef, "slow"),
                      s_suffix(mon_nam(magr)));
            inc_timeout(mdef, SLOW, tmp, TRUE);
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
            area[COLNO][0] = 0;
            area[COLNO][0] |= ((uint64_t)1 << i);
            do_clear_area(m_mx(magr), m_my(magr), areaatk[i]->damn,
                          set_at_area, &area);
            i++;
        }
        /* arbitrary cap (we've defined 64 indices, so bail out if we go above
           that cap) */
        if (i >= 64)
            break;
    }

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
            if (!mdef)
                continue;

            /* Find the monsters responsible for the aura */
            for (i = 0; i < 64; i++) {
                if (!(area[x][y] & (((uint64_t)1 >> i) & 1)))
                    continue;

                /* We found what we're looking for. Now, do the attack. */
                maurahitm(areamons[i], mdef, areaatk[i]);
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


/* Aura effects */
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
