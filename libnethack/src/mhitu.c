/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-03-04 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"
#include "hungerstatus.h"
#include "mfndpos.h"

static struct obj *otmp;

static boolean u_slip_free(struct monst *, const struct attack *);
static int passiveum(const struct permonst *, struct monst *,
                     const struct attack *);
static void mayberem(struct obj *, const char *);

static boolean diseasemu(const struct permonst *, const char *);
static int hitmu(struct monst *, const struct attack *, int);
static int gulpmu(struct monst *, const struct attack *);
static int explmu(struct monst *, const struct attack *);
static void missmu(struct monst *, boolean, const struct attack *);
static void hitmsg(struct monst *, const struct attack *);

/* See comment in mhitm.c.  If we use this a lot it probably should be */
/* changed to a parameter to mhitu. */
static int dieroll;

void
mhitmsg(struct monst *magr, struct monst *mdef,
        const struct attack *mattk)
{
    int compat;
    const char *target;
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef ||
                   cansee(magr->mx, magr->my) ||
                   cansee(mdef->mx, mdef->my));
    if (!vis)
        return;

    target = "";
    if (!udef)
        target = msgcat(" ", mon_nam(mdef));

    /* Note: if opposite gender, "seductively" */
    /* If same gender, "engagingly" for nymph, normal msg for others */
    if ((compat = could_seduce(magr, mdef, mattk))
        && !cancelled(magr) && !magr->mspec_used) {
        pline(combat_msgc(magr, mdef, cr_hit),
              "%s %s %s %s.", M_verbs(magr, !uagr && Blind ? "talk" : "smile"),
              !uagr && Blind ? "to" : "at", mon_nam(mdef),
              compat == 2 ? "engagingly" : "seductively");
    } else
        switch (mattk->aatyp) {
        case AT_BITE:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s%s!", M_verbs(magr, "bite"), target);
            break;
        case AT_KICK:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s%s%c", M_verbs(magr, "kick"), target,
                  thick_skinned(mdef->data) ? '.' : '!');
            break;
        case AT_STNG:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s%s!", M_verbs(magr, "sting"), target);
            break;
        case AT_BUTT:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s%s!", M_verbs(magr, "butt"), target);
            break;
        case AT_TUCH:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s %s!", M_verbs(magr, "touch"),
                  mon_nam(mdef));
            break;
        case AT_TENT:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s tentacles suck %s!", s_suffix(Monnam(magr)),
                  mon_nam(mdef));
            break;
        case AT_EXPL:
        case AT_BOOM:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s!", M_verbs(magr, "explode"));
            break;
        default:
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s%s!", M_verbs(magr, "hit"), target);
        }
}

static void
hitmsg(struct monst *mtmp, const struct attack *mattk)
{
    int compat;

    /* Note: if opposite gender, "seductively" */
    /* If same gender, "engagingly" for nymph, normal msg for others */
    if ((compat = could_seduce(mtmp, &youmonst, mattk))
        && !cancelled(mtmp) && !mtmp->mspec_used) {
        pline(combat_msgc(mtmp, &youmonst, cr_hit),
              "%s %s you %s.", Monnam(mtmp), Blind ? "talks to" : "smiles at",
              compat == 2 ? "engagingly" : "seductively");
    } else
        switch (mattk->aatyp) {
        case AT_BITE:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s bites!", Monnam(mtmp));
            break;
        case AT_KICK:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s kicks%c", Monnam(mtmp),
                  thick_skinned(youmonst.data) ? '.' : '!');
            break;
        case AT_STNG:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s stings!", Monnam(mtmp));
            break;
        case AT_BUTT:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s butts!", Monnam(mtmp));
            break;
        case AT_TUCH:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s touches you!", Monnam(mtmp));
            break;
        case AT_TENT:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s tentacles suck you!", s_suffix(Monnam(mtmp)));
            break;
        case AT_EXPL:
        case AT_BOOM:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s explodes!", Monnam(mtmp));
            break;
        default:
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s hits!", Monnam(mtmp));
        }
}

/* monster missed you */
static void
missmu(struct monst *mtmp, boolean nearmiss, const struct attack *mattk)
{
    /* not reveal_monster_at; it's attacking you, not vice versa */
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    if (could_seduce(mtmp, &youmonst, mattk) && !cancelled(mtmp))
        pline(combat_msgc(mtmp, &youmonst, cr_miss),
              "%s pretends to be friendly.", Monnam(mtmp));
    else {
        if (!nearmiss)
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s misses.", Monnam(mtmp));
        else
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s just misses!", Monnam(mtmp));
    }
    action_interrupted();
}

/* return how a poison attack was delivered */
const char *
mpoisons_subj(struct monst *mtmp, const struct attack *mattk)
{
    if (mattk->aatyp == AT_WEAP) {
        struct obj *mwep = (mtmp == &youmonst) ? uwep : MON_WEP(mtmp);

        /* "Foo's attack was poisoned." is pretty lame, but at least it's
           better than "sting" when not a stinging attack... */
        return (!mwep || !mwep->opoisoned) ? "attack" : "weapon";
    } else {
        return (mattk->aatyp == AT_TUCH) ? "contact" :
            (mattk->aatyp == AT_GAZE) ? "gaze" :
            (mattk->aatyp == AT_BITE) ? "bite" : "sting";
    }
}

void
expels(struct monst *mtmp,
       const struct permonst *mdat, /* if mtmp is polymorphed,
                                       mdat != mtmp->data */
       boolean message)
{
    if (message) {
        if (is_animal(mdat))
            pline(msgc_statusend, "You get regurgitated!");
        else {
            char blast[40];
            int i;

            blast[0] = '\0';
            for (i = 0; i < NATTK; i++)
                if (mdat->mattk[i].aatyp == AT_ENGL)
                    break;
            if (i >= NATTK || mdat->mattk[i].aatyp != AT_ENGL)
                impossible("Swallower has no engulfing attack?");
            else {
                if (is_whirly(mdat)) {
                    switch (mdat->mattk[i].adtyp) {
                    case AD_ELEC:
                        strcpy(blast, " in a shower of sparks");
                        break;
                    case AD_COLD:
                        strcpy(blast, " in a blast of frost");
                        break;
                    }
                } else
                    strcpy(blast, " with a squelch");
                pline(msgc_statusend, "You get expelled from %s%s!",
                      mon_nam(mtmp), blast);
            }
        }
    }
    unstuck(mtmp);      /* ball&chain returned in unstuck() */
    mnexto(mtmp);
    newsym(u.ux, u.uy);
    spoteffects(TRUE);
    /* to cover for a case where mtmp is not in a next square */
    if (um_dist(mtmp->mx, mtmp->my, 1))
        pline(msgc_consequence, "Brrooaa...  You land hard at some distance.");
}


/* select a monster's next attack, possibly substituting for its usual one */
const struct attack *
getmattk(const struct permonst *mptr, int indx, int prev_result[],
         struct attack *alt_attk_buf)
{
    const struct attack *attk = &mptr->mattk[indx];

    /* prevent a monster with two consecutive disease or hunger attacks from
       hitting with both of them on the same turn; if the first has already
       hit, switch to a stun attack for the second */
    if (indx > 0 && prev_result[indx - 1] > 0 &&
        (attk->adtyp == AD_DISE || attk->adtyp == AD_PEST ||
         attk->adtyp == AD_FAMN) &&
        attk->adtyp == mptr->mattk[indx - 1].adtyp) {
        *alt_attk_buf = *attk;
        alt_attk_buf->adtyp = AD_STUN;
        attk = alt_attk_buf;
    }
    return attk;
}

/*
 * mattacku: monster attacks you
 *      returns 1 if monster dies (e.g. "yellow light"), 0 otherwise
 * The caller must check that the monster is actually attacking your square.
 * Alternatively, call mattackq(mtmp, mtmp->mux, mtmp->muy), after checking
 * aware_of_u. (If the monster isn't aware of you and you're calling this
 * function anyway, you have bigger problems.)
 */
int
mattacku(struct monst *mtmp)
{
    const struct attack *mattk;
    struct attack alt_attk;
    int i, j, tmp, sum[NATTK];
    struct musable musable;

    const struct permonst *mdat = mtmp->data;

    /* Will monsters use ranged or melee attacks against you? */
    boolean ranged = FALSE;
    if (dist2(mtmp->mx, mtmp->my, u.ux, u.uy) > 2 ||
        (mtmp->data->mlet == S_DRAGON && extra_nasty(mtmp->data) &&
         !mtmp->mspec_used && (!cancelled(mtmp) || rn2(3))))
        ranged = TRUE;

    /* Is it near you at all? (Different from above) */
    boolean range2 = !monnear(mtmp, u.ux, u.uy);

    /* Can you see it? Affects messages. For long worms, if they're attacking
       you and you can see any part of the monster, you presumably know it's a
       long worm attacking. */
    boolean youseeit = canseemon(mtmp);

    /* The monster knows your location now, even if it didn't before.

       Exception: the monster has you engulfed, in which case we want to avoid
       setting mux and muy to the monster's own square. */
    if (mtmp->mx != u.ux || mtmp->my != u.uy) {
        mtmp->mux = u.ux;
        mtmp->muy = u.uy;
    }

    /* Might be attacking your image around the corner, or invisible, or you
       might be blind... so we require !ranged to ensure you're aware of it. */
    if (!ranged)
        action_interrupted();

    /* If swallowed, can only be affected by u.ustuck */
    if (Engulfed) {
        if (mtmp != u.ustuck)
            return 0;
        range2 = FALSE;
        ranged = FALSE;
        if (u.uinvulnerable)
            return 0;   /* stomachs can't hurt you! */
    }

    else if (u.usteed) {
        if (mtmp == u.usteed)
            /* Your steed won't attack you */
            return 0;
        /* Orcs like to steal and eat horses and the like */
        if (!rn2(is_orc(mtmp->data) ? 2 : 4) &&
            distu(mtmp->mx, mtmp->my) <= 2) {
            /* Attack your steed instead */
            i = mattackm(mtmp, u.usteed);
            if ((i & MM_AGR_DIED))
                return 1;
            if ((i & MM_DEF_DIED) || u.umoved)
                return 0;
            /* Let your steed retaliate */
            if (u.usteed &&
                (mm_aggression(u.usteed, mtmp, Conflict) & ALLOW_MELEE))
                return !!(mattackm(u.usteed, mtmp) & MM_DEF_DIED);
            return 0;
        }
    }

    /* must be called after the above mattackm check, or the monster could
       summon at both you and your steed in the same attack */
    if (!mpreattack(mtmp, &youmonst, range2))
        return 0;

    if (u.uundetected && !range2 && !Engulfed) {
        u.uundetected = 0;
        if (is_hider(youmonst.data)) {
            coord cc;   /* maybe we need a unexto() function? */
            struct obj *obj;

            if (youmonst.data == &mons[PM_TRAPPER])
                pline(msgc_occstart, "You ambush %s!", mon_nam(mtmp));
            else
                pline(msgc_occstart, "You fall from the %s!",
                      ceiling(u.ux, u.uy));

            /* TODO: This next line is beautifully exploitable (see the
               explanation on TASvideos). */
            if (enexto(&cc, level, u.ux, u.uy, youmonst.data)) {
                remove_monster(level, mtmp->mx, mtmp->my);
                newsym(mtmp->mx, mtmp->my);
                place_monster(mtmp, u.ux, u.uy, TRUE);
                if (mtmp->wormno)
                    worm_move(mtmp);
                teleds(cc.x, cc.y, TRUE);
                set_apparxy(mtmp);
                newsym(u.ux, u.uy);
            } else {
                /* this is a pathological case, so might as well be silly about
                   it... */
                if (youmonst.data == &mons[PM_TRAPPER])
                    pline(msgc_kill, "%s dies from the shock of the ambush.",
                          Monnam(mtmp));
                else
                    pline(msgc_kill, "%s is killed by a falling %s (you)!",
                          Monnam(mtmp), pm_name(&youmonst));
                killed(mtmp);
                newsym(u.ux, u.uy);
                if (!DEADMONSTER(mtmp))
                    return 0;
                else
                    return 1;
            }
            if (youmonst.data->mlet != S_PIERCER)
                return 0;       /* trappers don't attack */

            obj = which_armor(mtmp, os_armh);
            if (obj && is_metallic(obj)) {
                pline(combat_msgc(&youmonst, mtmp, cr_immune),
                      "Your blow glances off %s %s.", s_suffix(mon_nam(mtmp)),
                      helmet_name(obj));
            } else {
                if (3 + find_mac(mtmp) <= rnd(20)) {
                    pline(combat_msgc(&youmonst, mtmp, cr_hit),
                          "%s is hit by a falling piercer (you)!",
                          Monnam(mtmp));
                    if ((mtmp->mhp -= dice(3, 6)) < 1)
                        killed(mtmp);
                } else
                    pline(combat_msgc(&youmonst, mtmp, cr_miss),
                          "%s is almost hit by a falling piercer (you)!",
                          Monnam(mtmp));
            }
        } else {
            if (!youseeit)
                pline(msgc_info, "It tries to move where you are hiding.");
            else {
                /* Ugly kludge for eggs. The message is phrased so as to be
                   directed at the monster, not the player, which makes "laid by
                   you" wrong. For the parallelism to work, we can't rephrase
                   it, so we zap the "laid by you" momentarily instead.

                   The messages are placed in the msgc_youdiscover channel, in
                   order to continue the parallelism. This might be too cute. */
                struct obj *obj = level->objects[u.ux][u.uy];

                if (obj ||
                    (youmonst.data->mlet == S_EEL &&
                     is_pool(level, u.ux, u.uy))) {
                    int save_spe = 0;   /* suppress warning */

                    if (obj) {
                        save_spe = obj->spe;
                        if (obj->otyp == EGG)
                            obj->spe = 0;
                    }
                    if (youmonst.data->mlet == S_EEL)
                        pline(msgc_youdiscover,
                              "Wait, %s!  There's a hidden %s named %s there!",
                              m_monnam(mtmp), pm_name(&youmonst), u.uplname);
                    else
                        pline(msgc_youdiscover, "Wait, %s!  There's a %s named "
                              "%s hiding under %s!", m_monnam(mtmp), pm_name(&youmonst),
                              u.uplname, doname(level->objects[u.ux][u.uy]));
                    if (obj)
                        obj->spe = save_spe;
                } else
                    impossible("hiding under nothing?");
            }
            newsym(u.ux, u.uy);
        }
        return 0;
    }
    if (youmonst.data->mlet == S_MIMIC && youmonst.m_ap_type && !range2 &&
        !Engulfed) {
        /* This is msgc_interrupted both ways around; from the player's point of
           view (their mimicking was cut short by a monster's arrival); and from
           the monster's point of view (their movement was cut short by getting
           stuck on a mimic). */
        if (!youseeit)
            pline(msgc_interrupted, "It gets stuck on you.");
        else
            pline(msgc_interrupted, "Wait, %s!  That's a %s named %s!",
                  m_monnam(mtmp), pm_name(&youmonst), u.uplname);
        u.ustuck = mtmp;
        cancel_mimicking("");
        return 0;
    }

    /* player might be mimicking an object */
    if (youmonst.m_ap_type == M_AP_OBJECT && !range2 && !Engulfed) {
        if (!youseeit)
            pline(msgc_interrupted, "Something %s!",
                  (likes_gold(mtmp->data) && youmonst.mappearance ==
                   GOLD_PIECE) ? "tries to pick you up" : "disturbs you");
        else
            pline(msgc_interrupted, "Wait, %s!  That %s is really %s named %s!",
                  m_monnam(mtmp), mimic_obj_name(&youmonst),
                  an(u.ufemale ? mons[u.umonnum].fname : mons[u.umonnum].mname),
                  u.uplname);

        const char *buf;
        buf = msgprintf("You appear to be %s again.", Upolyd ?
                        (const char *)an(pm_name(&youmonst)) :
                        (const char *)"yourself");
        cancel_mimicking(buf); /* immediately stop mimicking */

        return 0;
    }

    /* Work out the armor class differential   */
    tmp = AC_VALUE(find_mac(&youmonst)) + 10; /* tmp ~= 0 - 20 */
    int ac_after_rnd = tmp;
    tmp += mtmp->m_lev;
    if (u_helpless(hm_all))
        tmp += 4;
    if ((invisible(&youmonst) && !see_invisible(mtmp)) || blind(mtmp))
        tmp -= 2;
    if (mtmp->mtrapped)
        tmp -= 2;
    if (tmp <= 0)
        tmp = 1;
    tmp += hitbon(mtmp);

    /* make eels visible the moment they hit/miss us */
    if (mdat->mlet == S_EEL && invisible(mtmp) && cansee(mtmp->mx, mtmp->my)) {
        set_property(mtmp, INVIS, -2, FALSE); /* FIXME: why? */
        newsym(mtmp->mx, mtmp->my);
    }

    if (u.uinvulnerable) {
        /* monsters won't attack you */

        /* only print messages for monsters that actually can attack */
        for (i = 0; i < NATTK; i++) {
            if (mtmp->data->mattk[i].aatyp != AT_NONE &&
                mtmp->data->mattk[i].aatyp != AT_BOOM)
                break;
        }
        if (i == NATTK)
            return 0;

        if (mtmp == u.ustuck)
            pline(msgc_noconsequence, "%s loosens its grip slightly.",
                  Monnam(mtmp));
        else if (!ranged) {
            if (youseeit || sensemon(mtmp))
                pline(msgc_noconsequence,
                      "%s starts to attack you, but pulls back.", Monnam(mtmp));
            else
                pline(msgc_noconsequence, "You feel something move nearby.");
        }
        return 0;
    }

    /* Unlike defensive stuff, don't let them use item _and_ attack. */
    init_musable(mtmp, &musable);
    if (find_item(mtmp, &musable)) {
        int foo = use_item(&musable);

        if (foo != 0)
            return foo == 1;
    }

    for (i = 0; i < NATTK; i++) {
        sum[i] = 0;
        mattk = getmattk(mdat, i, sum, &alt_attk);
        if (Engulfed && (mattk->aatyp != AT_ENGL))
            continue;
        switch (mattk->aatyp) {
        case AT_CLAW:  /* "hand to hand" attacks */
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            if (!ranged &&
                (!MON_WEP(mtmp) || confused(mtmp) || Conflict ||
                 !touch_petrifies(youmonst.data))) {
                if (tmp > (j = rnd(20 + i))) {
                    if (mattk->aatyp != AT_KICK ||
                        !thick_skinned(youmonst.data))
                        sum[i] = hitmu(mtmp, mattk, ac_after_rnd);
                } else
                    missmu(mtmp, (tmp == j), mattk);
            }
            break;

        case AT_HUGS:  /* automatic if prev two attacks succeed */
            if ((!ranged && i >= 2 && sum[i - 1] && sum[i - 2])
                || mtmp == u.ustuck)
                sum[i] = hitmu(mtmp, mattk, ac_after_rnd);
            break;

        case AT_GAZE:  /* can affect you either ranged or not */
            /* Medusa gaze already operated through m_respond in dochug();
               don't gaze more than once per round. */
            if (mdat != &mons[PM_MEDUSA])
                sum[i] = gazemm(mtmp, &youmonst, mattk);
            break;

        case AT_EXPL:
            /* explmu does hit calculations, but we have to check range */
            if (!ranged)
                sum[i] = explmu(mtmp, mattk);
            break;

        case AT_ENGL:
            if (!ranged) {
                if (Engulfed || tmp > (j = rnd(20 + i))) {
                    /* Force swallowing monster to be displayed even when
                       player is moving away */
                    flush_screen();
                    sum[i] = gulpmu(mtmp, mattk);
                } else {
                    missmu(mtmp, (tmp == j), mattk);
                }
            }
            break;
        case AT_BREA:
            if (ranged)
                sum[i] = breamq(mtmp, u.ux, u.uy, mattk);
            break;
        case AT_SPIT:
            if (ranged)
                sum[i] = spitmq(mtmp, u.ux, u.uy, mattk);
            break;
        case AT_WEAP:
            if (ranged) {
                if (!Is_rogue_level(&u.uz))
                    thrwmq(mtmp, u.ux, u.uy);
            } else {
                int hittmp = 0;

                /* Rare but not impossible.  Normally the monster wields when 2
                   spaces away, but it can be teleported or whatever.... */
                if (mtmp->weapon_check == NEED_WEAPON || !MON_WEP(mtmp)) {
                    mtmp->weapon_check = NEED_HTH_WEAPON;
                    /* mon_wield_item resets weapon_check as appropriate */
                    if (mon_wield_item(mtmp) != 0)
                        break;
                }
                otmp = MON_WEP(mtmp);
                if (otmp) {
                    if (obj_properties(otmp) & opm_nasty) {
                        if (canseemon(mtmp)) {
                            pline(combat_msgc(NULL, mtmp, cr_hit),
                                  "The nasty weapon hurts %s!", mon_nam(mtmp));
                            learn_oprop(otmp, opm_nasty);
                        }
                        mtmp->mhp -= rnd(6);
                        if (mtmp->mhp <= 0) {
                            mondied(mtmp);
                            return 1;
                        }
                    }

                    hittmp = hitval(otmp, &youmonst);
                    tmp += hittmp;
                    mswingsm(mtmp, &youmonst, otmp);
                }
                if (tmp > (j = dieroll = rnd(20 + i)))
                    sum[i] = hitmu(mtmp, mattk, ac_after_rnd);
                else
                    missmu(mtmp, (tmp == j), mattk);
                /* KMH -- Don't accumulate to-hit bonuses */
                if (otmp)
                    tmp -= hittmp;
            }
            break;

        default:       /* no attack */
            break;
        }
        bot();
        /* give player a chance of waking up before dying -kaa */
        if (sum[i] == 1) {      /* successful attack */
            if (u_helpless(hm_asleep) &&
                turnstate.helpless_timers[hr_asleep]) {
                int awakening = rnd(5);
                if (turnstate.helpless_timers[hr_asleep] <= awakening)
                    cancel_helplessness(hm_asleep,
                                        "The combat suddenly awakens you.");
                else
                    turnstate.helpless_timers[hr_asleep] -= awakening;
            }
        }
        if (sum[i] == 2)
            return 1;   /* attacker dead */
        if (sum[i] == 3)
            break;      /* attacker teleported, no more attacks */
        /* sum[i] == 0: unsuccessful attack */
    }
    return 0;
}


void
hurtarmor(struct monst *mdef, enum erode_type type)
{
    struct obj *target;

    /* What the following code does: it keeps looping until it finds a target
       for the rust monster. Head, feet, etc... not covered by metal, or
       covered by rusty metal, are not targets.  However, your body always is,
       no matter what covers it. */
    while (1) {
        switch (rn2(5)) {
        case 0:
            target = which_armor(mdef, os_armh);
            if (!target || !erode_obj(target, xname(target), type, TRUE, FALSE))
                continue;
            break;
        case 1:
            target = which_armor(mdef, os_armc);
            if (target) {
                erode_obj(target, xname(target), type, TRUE, TRUE);
                break;
            }
            if ((target = which_armor(mdef, os_arm)) != NULL) {
                erode_obj(target, xname(target), type, TRUE, TRUE);
            } else if ((target = which_armor(mdef, os_armu)) != NULL) {
                erode_obj(target, xname(target), type, TRUE, TRUE);
            }
            break;
        case 2:
            target = which_armor(mdef, os_arms);
            if (!target || !erode_obj(target, xname(target), type, TRUE, FALSE))
                continue;
            break;
        case 3:
            target = which_armor(mdef, os_armg);
            if (!target || !erode_obj(target, xname(target), type, TRUE, FALSE))
                continue;
            break;
        case 4:
            target = which_armor(mdef, os_armf);
            if (!target || !erode_obj(target, xname(target), type, TRUE, FALSE))
                continue;
            break;
        }
        break;  /* Out of while loop */
    }
}


static boolean
diseasemu(const struct permonst *mdat, const char *hitmsg)
{
    if (resists_sick(&youmonst)) {
        pline(msgc_playerimmune, "You feel a slight illness.");
        return FALSE;
    } else {
        /* avoid double-tab */
        if (hitmsg)
            pline_implied(combat_msgc(NULL, &youmonst, cr_hit), "%s", hitmsg);
        make_sick(&youmonst, 20 + rn2_on_rng(ACURR(A_CON), rng_ddeath_dconp20),
                  mdat->mname, TRUE, SICK_NONVOMITABLE);
        return TRUE;
    }
}

/* check whether slippery clothing protects from hug or wrap attack */
static boolean
u_slip_free(struct monst *mtmp, const struct attack *mattk)
{
    struct obj *obj = (uarmc ? uarmc : uarm ? uarm : uarmu);

    if (mattk->adtyp == AD_DRIN)
        obj = uarmh;

    uint64_t props = 0;
    if (obj)
        props = obj_properties(obj);

    /* if your cloak/armor is greased, monster slips off; this protection might
       fail (33% chance) when the armor is cursed */
    if (obj && (obj->greased || obj->otyp == OILSKIN_CLOAK ||
                (props & opm_oilskin)) &&
        (!obj->cursed || rn2(3))) {
        pline(combat_msgc(mtmp, &youmonst, cr_miss), "%s %s your %s %s!",
              Monnam(mtmp), (mattk->adtyp == AD_WRAP) ? "slips off of" :
              "grabs you, but cannot hold onto",
              obj->greased ? "greased" : "slippery",
              /* avoid "slippery slippery cloak" for undiscovered oilskin
                 cloak */
              (obj->greased || objects[obj->otyp].oc_name_known ||
               obj->otyp != OILSKIN_CLOAK) ?
              xname(obj) : cloak_simple_name(obj));

        if (obj->greased && !rn2(2)) {
            pline(msgc_itemloss, "The grease wears off.");
            obj->greased = 0;
            update_inventory();
        }
        return TRUE;
    }
    return FALSE;
}

/* armor that sufficiently covers the body might be able to block magic */
int
magic_negation(struct monst *mon)
{
    struct obj *armor;
    int armpro = 0;
    enum objslot i;

    /* Loop over all the armor slots. Armor types for shirt, gloves, shoes, and
       shield don't currently provide any magic cancellation, but we might as
       well be complete. */
    for (i = 0; i <= os_last_armor; i++) {
        armor = which_armor(mon, i);
        if (armor && armpro < objects[armor->otyp].a_can)
            armpro = objects[armor->otyp].a_can;
    }

    /* this one is really a stretch... */
    armor = which_armor(mon, os_saddle);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;

    return armpro;
}

/*
 * hitmu: monster hits you
 *        returns 2 if monster dies (e.g. "yellow light"), 1 otherwise
 *        3 if the monster lives but teleported/paralyzed, so it can't keep
 *            attacking you
 */
static int
hitmu(struct monst *mtmp, const struct attack *mattk, int ac_after_rnd)
{
    const struct permonst *mdat = mtmp->data;
    int uncancelled;
    int dmg, armpro, permdmg, zombie_timer;
    const struct permonst *olduasmon = youmonst.data;
    int res;
    struct attack noseduce;
    boolean mercy = FALSE;

    if (!flags.seduce_enabled && mattk->adtyp == AD_SSEX) {
        noseduce = *mattk;
        noseduce.adtyp = AD_DRLI;      /* seduction becoms drain life instead */
        mattk = &noseduce;
    }

    /* not reveal_monster_at; it's attacking you, not the other way round */
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    /* If the monster is undetected & hits you, you should know where the attack
       came from. */
    if (mtmp->mundetected && (hides_under(mdat) || mdat->mlet == S_EEL)) {
        mtmp->mundetected = 0;
        if (!(Blind ? Blind_telepat : Unblind_telepat)) {
            struct obj *obj;
            const char *what;

            if ((obj = level->objects[mtmp->mx][mtmp->my]) != 0) {
                if (Blind && !obj->dknown)
                    what = "something";
                else if (is_pool(level, mtmp->mx, mtmp->my) && !Underwater)
                    what = "the water";
                else
                    what = doname(obj);

                pline(msgc_youdiscover, "%s was hidden under %s!",
                      Amonnam(mtmp), what);
            }
            newsym(mtmp->mx, mtmp->my);
        }
    }

    /* First determine the base damage done */
    dmg = dice((int)mattk->damn, (int)mattk->damd);
    /* extra damage */
    dmg += dambon(mtmp);

    /* Next a cancellation factor. Use uncancelled when the cancellation factor
       takes into account certain armor's special magic protection.  Otherwise just
       use !mtmp->mcan. */
    armpro = magic_negation(&youmonst);
    uncancelled = !cancelled(mtmp) && ((rn2(3) >= armpro) || !rn2(50));

    permdmg = 0;

    /* Now, adjust damages via resistances or specific attacks */
    switch (mattk->adtyp) {
    case AD_PHYS:
        if (mattk->aatyp == AT_HUGS && !sticks(youmonst.data)) {
            if (!u.ustuck && rn2(2)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    u.ustuck = mtmp;
                    pline(msgc_statusbad, "%s grabs you!", Monnam(mtmp));
                }
            } else if (u.ustuck == mtmp) {
                exercise(A_STR, FALSE);
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "You are being %s.", (mtmp->data == &mons[PM_ROPE_GOLEM])
                      ? "choked" : "crushed");
            }
        } else {        /* hand to hand weapon */
            if (mattk->aatyp == AT_WEAP && otmp) {
                if (otmp->otyp == CORPSE &&
                    touch_petrifies(&mons[otmp->corpsenm])) {
                    dmg = 1;
                    pline(resists_ston(&youmonst) ? msgc_fatalavoid :
                          msgc_fatal, "%s hits you with the %s corpse.", Monnam(mtmp),
                          opm_name(otmp));
                    if (!petrifying(&youmonst))
                        goto do_stone;
                }
                dmg += dmgval(otmp, &youmonst);
                /* There's no specific message channel for "incoming attack hits
                   a vulnerability". We could use msgc_fatal but that seems a
                   bit overblown (especially because this can only happen while
                   polymorphed and polymorph gives protection against HP
                   damage). msgc_statusbad makes sense, treating the "extra
                   damage" as a status effect. */
                if (objects[otmp->otyp].oc_material == SILVER &&
                    hates_silver(youmonst.data))
                    pline(msgc_statusbad, "The silver sears your flesh!");

                if (dmg <= 0)
                    dmg = 1;
                if (!((otmp->oartifact || otmp->oprops) &&
                      artifact_hit(mtmp, &youmonst, otmp, &dmg, dieroll)))
                    hitmsg(mtmp, mattk);
                if (obj_properties(otmp) & opm_mercy)
                    mercy = TRUE;

                if (!dmg)
                    break;
                if (u.mh > 1 && u.mh > ((find_mac(&youmonst) > 0) ?
                                            dmg : dmg + find_mac(&youmonst)) &&
                    objects[otmp->otyp].oc_material == IRON &&
                    !mercy &&
                    (u.umonnum == PM_BLACK_PUDDING ||
                     u.umonnum == PM_BROWN_PUDDING)) {
                    /* This redundancy is necessary because you have to take the
                       damage _before_ being cloned. */
                    if (find_mac(&youmonst) < 0)
                        dmg += find_mac(&youmonst);
                    if (dmg < 1)
                        dmg = 1;
                    if (dmg > 1)
                        exercise(A_STR, FALSE);
                    u.mh -= dmg;
                    dmg = 0;
                    if (cloneu())
                        pline(msgc_statusgood, "You divide as %s hits you!",
                              mon_nam(mtmp));
                }
                mrustm(mtmp, &youmonst, otmp);
            } else if (mattk->aatyp != AT_TUCH || dmg != 0 || mtmp != u.ustuck)
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_DISE:
        hitmsg(mtmp, mattk);
        /* 3.4.3 has no message in the case where this works, but that's a bad
           idea because it gives us nothing to hang the delayed instadeath
           warning onto. */
        if (!diseasemu(mdat, msgprintf("As %s hits you, you feel terribly sick.",
                                       mon_nam(mtmp))))
            dmg = 0;
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
        damage(mtmp, &youmonst, mattk);
        return 1;
    case AD_DRIN:
        /* Note about message channels: this is one of the most common ways I
           die, so treat any amount of intelligence drain as a potential
           instadeath deserving of instadeath warnings */
        hitmsg(mtmp, mattk);
        if (defends(AD_DRIN, uwep) || !has_head(youmonst.data)) {
            pline(msgc_playerimmune, "You don't seem harmed.");
            /* Not clear what to do for green slimes */
            break;
        }
        if (u_slip_free(mtmp, mattk))
            break;

        if (uarmh && rn2(8)) {
            /* not body_part(HEAD) */
            pline(msgc_fatalavoid,
                  "Your %s blocks the attack to your head.",
                  helmet_name(uarmh));
            break;
        }
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;
        if (mercy)
            do_mercy(mtmp, &youmonst, otmp, dmg);
        else
            mdamageu(mtmp, dmg);

        if (!uarmh || uarmh->otyp != DUNCE_CAP) {
            pline(ABASE(A_INT) <= ATTRMIN(A_INT) ?
                  msgc_fatal_predone : msgc_fatal, "Your brain is eaten!");
            /* No such thing as mindless players... */
            if (ABASE(A_INT) <= ATTRMIN(A_INT)) {
                int lifesaved = 0;
                struct obj *wore_amulet = uamul;

                while (1) {
                    /* avoid looping on "die(y/n)?" */
                    if (lifesaved && (discover || wizard)) {
                        if (wore_amulet && !uamul) {
                            /* used up AMULET_OF_LIFE_SAVING; still subject to
                               dying from brainlessness */
                            wore_amulet = 0;
                        } else {
                            /* explicitly chose not to die; arbitrarily boost
                               intelligence */
                            ABASE(A_INT) = ATTRMIN(A_INT) + 2;
                            pline(msgc_intrgain, "You feel like a scarecrow.");
                            break;
                        }
                    }

                    if (lifesaved)
                        pline(msgc_fatal_predone,
                              "Unfortunately your brain is still gone.");
                    else
                        pline(msgc_fatal_predone,
                              "Your last thought fades away.");
                    done(DIED, killer_msg(DIED, "brainlessness"));
                    lifesaved++;
                }
            }
        }
        /* adjattrib gives dunce cap message when appropriate */
        adjattrib(A_INT, -rnd(2), FALSE);
        forget_objects(50);     /* lose memory of 50% of objects */
        exercise(A_WIS, FALSE);
        break;
    case AD_PLYS:
        hitmsg(mtmp, mattk);
        if (uncancelled && !u_helpless(hm_all) && !rn2(3)) {
            if (Free_action) {
                pline(msgc_playerimmune, "You momentarily stiffen.");
            } else {
                if (Blind)
                    pline(msgc_statusbad, "You are frozen!");
                else
                    pline(msgc_statusbad, "You are frozen by %s!",
                          mon_nam(mtmp));
                helpless(10, hr_paralyzed, "paralyzed by a monster's touch",
                         NULL);
                exercise(A_DEX, FALSE);
            }
        }
        break;
    case AD_DRLI:
        hitmsg(mtmp, mattk);
        if (!cancelled(mtmp) && !rn2(3) && !Drain_resistance) {
            losexp(msgcat("drained of life by ", k_monnam(mtmp)), FALSE);
        }
        break;
    case AD_LEGS:
        {
            long side = rn2(2) ? RIGHT_SIDE : LEFT_SIDE;
            const char *sidestr = (side == RIGHT_SIDE) ? "right" : "left";

            /* This case is too obvious to ignore, but Nethack is not in
               general very good at considering height--most short monsters
               still _can_ attack you when you're flying or mounted. [TODO:
               why can't a flying attacker overcome this?] */
            if (u.usteed || levitates(&youmonst) || Flying) {
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "%s tries to reach your %s %s!", Monnam(mtmp), sidestr,
                      body_part(LEG));
                dmg = 0;
            } else if (cancelled(mtmp)) {
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s nuzzles against your %s %s!", Monnam(mtmp), sidestr,
                      body_part(LEG));
                dmg = 0;
            } else {
                if (uarmf) {
                    if (rn2(2) &&
                        (uarmf->otyp == LOW_BOOTS || uarmf->otyp == IRON_SHOES))
                        pline(combat_msgc(mtmp, &youmonst, cr_hit),
                              "%s pricks the exposed part of your %s %s!",
                              Monnam(mtmp), sidestr, body_part(LEG));
                    else if (!rn2(5))
                        pline(combat_msgc(mtmp, &youmonst, cr_hit),
                              "%s pricks through your %s boot!", Monnam(mtmp),
                              sidestr);
                    else {
                        pline(combat_msgc(mtmp, &youmonst, cr_miss),
                              "%s scratches your %s boot!", Monnam(mtmp),
                              sidestr);
                        dmg = 0;
                        break;
                    }
                } else
                    pline(combat_msgc(mtmp, &youmonst, cr_hit),
                          "%s pricks your %s %s!", Monnam(mtmp), sidestr,
                          body_part(LEG));
                set_wounded_legs(&youmonst, side, rnd(60 - ACURR(A_DEX)));
                exercise(A_STR, FALSE);
                exercise(A_DEX, FALSE);
            }
            break;
        }
    case AD_STON:      /* cockatrice */
    {
        hitmsg(mtmp, mattk);
        boolean stiffen = !rn2_on_rng(10,rng_slow_stoning);
        if (!rn2(3)) {
            /* no need for fatalavoid if the trice is cancelled; it's not going
               to be stoning you in the future (at least, not with an active
               attack) so no need for a warning */
            if (cancelled(mtmp))
                You_hear(combat_msgc(mtmp, &youmonst, cr_immune),
                         "a cough from %s!", mon_nam(mtmp));
            else {
                if (flags.moonphase == NEW_MOON && !have_lizard())
                    stiffen = TRUE;
                You_hear((Stone_resistance || !stiffen) ? msgc_fatalavoid :
                         msgc_fatal, "%s hissing!", s_suffix(mon_nam(mtmp)));
                if (stiffen) {
                do_stone:
                    if (!petrifying(&youmonst) && !resists_ston(&youmonst) &&
                        !(poly_when_stoned(youmonst.data) &&
                          polymon(PM_STONE_GOLEM, TRUE))) {
                        set_property(&youmonst, STONED, 5, TRUE);
                        set_delayed_killer(STONING,
                                           killer_msg_mon(STONING, mtmp));
                        return 1;
                    }
                }
            }
        }
        break;
    }
    case AD_STCK:
        hitmsg(mtmp, mattk);
        if (uncancelled && !u.ustuck && !sticks(youmonst.data))
            u.ustuck = mtmp;
        break;
    case AD_WRAP:
        if ((!cancelled(mtmp) || u.ustuck == mtmp) && !sticks(youmonst.data)) {
            boolean drownable = is_pool(level, mtmp->mx, mtmp->my) &&
                !Swimming && !Breathless;
            if (!u.ustuck &&
                !rn2_on_rng(10, drownable ? rng_eel_drowning : rng_main)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    pline(msgc_fatal, "%s swings itself around you!",
                          Monnam(mtmp));
                    u.ustuck = mtmp;
                }
            } else if (u.ustuck == mtmp) {
                if (drownable) {
                    pline(msgc_fatal_predone, "%s drowns you...", Monnam(mtmp));
                    done(DROWNING,
                         killer_msg(DROWNING,
                                    msgprintf("%s by %s",
                                              Is_waterlevel(&u.uz)
                                                  ? "the Plane of Water"
                                                  : a_waterbody(mtmp->mx,
                                                                mtmp->my),
                                              an(pm_name(mtmp)))));
                } else if (mattk->aatyp == AT_HUGS)
                    pline(combat_msgc(mtmp, &youmonst, cr_hit),
                          "You are being crushed.");
            } else {
                dmg = 0;
                /* This used to have a check against flags.verbose. However,
                   it's not being put into a low-priority message channel for
                   what I hope are obvious reasons -- AIS */
                pline(msgc_fatalavoid, "%s brushes against your %s.",
                      Monnam(mtmp), body_part(LEG));
            }
        } else
            dmg = 0;
        break;
    case AD_WERE:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4) && u.ulycn == NON_PM &&
            !Protection_from_shape_changers && !defends(AD_WERE, uwep)) {
            pline(msgc_statusbad, "You feel feverish.");
            exercise(A_CON, FALSE);
            u.ulycn = monsndx(mdat);
        }
        break;
    case AD_SGLD:
        hitmsg(mtmp, mattk);
        if (youmonst.data->mlet == mdat->mlet)
            break;
        if (!cancelled(mtmp))
            stealgold(mtmp);
        break;

    case AD_SITM:      /* for now these are the same */
    case AD_SEDU:
        if (is_animal(mtmp->data)) {
            hitmsg(mtmp, mattk);
            if (cancelled(mtmp))
                break;
            /* Continue below */
        } else if (dmgtype(youmonst.data, AD_SEDU)
                   || dmgtype(youmonst.data, AD_SSEX)
            ) {
            pline(msgc_npcvoice, "%s %s.", Monnam(mtmp),
                  mtmp->minvent ?
                  "brags about the goods some dungeon explorer provided" :
                  "makes some remarks about how difficult theft is lately");
            if (!tele_restrict(mtmp))
                rloc(mtmp, TRUE);
            return 3;
        } else if (cancelled(mtmp)) {
            if (!Blind) {
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "%s tries to %s you, but you seem %s.",
                      Adjmonnam(mtmp, "plain"),
                      u.ufemale ? "charm" : "seduce",
                      u.ufemale ? "unaffected" : "uninterested");
            }
            if (rn2(3)) {
                if (!tele_restrict(mtmp))
                    rloc(mtmp, TRUE);
                return 3;
            }
            break;
        }

        {
            const char *buf = "";

            switch (steal(mtmp, &buf)) {
            case -1:
                return 2;
            case 0:
                break;
            default:
                if (!is_animal(mtmp->data) && !tele_restrict(mtmp))
                    rloc(mtmp, TRUE);
                if (is_animal(mtmp->data) && *buf) {
                    if (canseemon(mtmp))
                        pline(msgc_itemloss,
                              "%s tries to %s away with %s.", Monnam(mtmp),
                              locomotion(mtmp->data, "run"), buf);
                }
                monflee(mtmp, 0, FALSE, FALSE);
                return 3;
            }
        }
        break;

    case AD_SSEX:
        if (could_seduce(mtmp, &youmonst, mattk) == 1 && !cancelled(mtmp))
            if (doseduce(mtmp))
                return 3;
        break;

    case AD_SAMU:
        hitmsg(mtmp, mattk);
        /* When covetous monsters hit, maybe steal the item */
        if (Uhave_amulet || Uhave_bell || Uhave_book || Uhave_menorah ||
            Uhave_questart)  /* carrying the Quest Artifact */
            if (!rn2(20))
                stealamulet(mtmp);
        break;

    case AD_TLPT:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline_implied(combat_msgc(mtmp, &youmonst, cr_hit),
                          "Your position suddenly seems very uncertain!");
            tele();
        }
        break;
    case AD_RUST:
        hitmsg(mtmp, mattk);
        if (cancelled(mtmp))
            break;
        if (u.umonnum == PM_IRON_GOLEM) {
            pline(msgc_statusend, "You rust!");
            rehumanize(DIED, msgcat("rusted to death by ", k_monnam(mtmp)));
            break;
        }
        hurtarmor(&youmonst, ERODE_RUST);
        break;
    case AD_CORR:
        hitmsg(mtmp, mattk);
        if (cancelled(mtmp))
            break;
        hurtarmor(&youmonst, ERODE_CORRODE);
        break;
    case AD_DCAY:
        hitmsg(mtmp, mattk);
        if (cancelled(mtmp))
            break;
        if (u.umonnum == PM_WOOD_GOLEM || u.umonnum == PM_LEATHER_GOLEM) {
            pline(msgc_statusend, "You rot!");
            rehumanize(DIED, msgcat("rotted to death by ", k_monnam(mtmp)));
            break;
        }
        hurtarmor(&youmonst, ERODE_ROT);
        break;
    case AD_HEAL:
        /* a cancelled nurse is just an ordinary monster */
        if (cancelled(mtmp)) {
            hitmsg(mtmp, mattk);
            break;
        }
        /* this condition must match the one in sounds.c for MS_NURSE */
        if (!(uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep))) &&
            !uarmu && (!uarm || uskin()) && !uarmh && !uarms && !uarmg &&
            !uarmc && !uarmf) {
            boolean goaway = FALSE;
            boolean morehpmax = FALSE;
            boolean hpmaxed = Upolyd ? u.mh == u.mhmax : u.uhp == u.uhpmax;
            
            if (Upolyd) {
                u.mh += rnd(7);
                if (!rn2(7)) {
                    /* no upper limit necessary; effect is temporary */
                    u.mhmax++;
                    if (!rn2(13))
                        goaway = TRUE;
                }
                if (u.mh > u.mhmax)
                    u.mh = u.mhmax;
            } else {
                u.uhp += rnd(7);
                if (!rn2(7)) {
                    /* hard upper limit via nurse care: 25 * ulevel */
                    if (u.uhpmax < 5 * u.ulevel + dice(2 * u.ulevel, 10)) {
                        u.uhpmax++;
                        morehpmax = TRUE;
                    }
                    if (!rn2(13))
                        goaway = TRUE;
                }
                if (u.uhp > u.uhpmax)
                    u.uhp = u.uhpmax;
            }

            /* Don't have the same message in these cases; they can be
               distinguished anyway from the message line, belong in different
               channels, and the channel shouldn't really be a spoiler */
            if (morehpmax)
                pline(msgc_intrgain, "%s hits!  You don't mind that at all.",
                      Monnam(mtmp));
            else if (!hpmaxed)
                pline(msgc_statusheal, "%s hits!  (I hope you don't mind.)",
                      Monnam(mtmp));
            else
                pline(msgc_monneutral, "%s hits!  (But nothing happened?)",
                      Monnam(mtmp));

            if (!rn2(3))
                exercise(A_STR, TRUE);
            if (!rn2(3))
                exercise(A_CON, TRUE);
            if (sick(&youmonst) || zombifying(&youmonst))
                make_sick(&youmonst, 0L, NULL, FALSE, SICK_ALL);
            if (goaway) {
                mongone(mtmp);
                return 2;
            } else if (!rn2(33)) {
                if (!tele_restrict(mtmp))
                    rloc(mtmp, TRUE);
                monflee(mtmp, dice(3, 6), TRUE, FALSE);
                return 3;
            }
            dmg = 0;
            passiveum(olduasmon, mtmp, mattk);
        } else {
            if (Role_if(PM_HEALER)) {
                if (!(moves % 5))
                    verbalize(msgc_hint,
                              "Doc, I can't help you unless you cooperate.");
                dmg = 0;
            } else
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_CURS:
        hitmsg(mtmp, mattk);
        if (!cancelled(mtmp) && !rn2(10)) {
            if (canhear()) {
                /* OK to use a low-priority channel here; we're about to use
                   a much higher one */
                if (Blind)
                    You_hear(msgc_levelwarning, "laughter.");
                else
                    pline_implied(combat_msgc(mtmp, &youmonst, cr_hit),
                                  "%s chuckles.", Monnam(mtmp));
            }
            if (u.umonnum == PM_CLAY_GOLEM) {
                pline(msgc_statusend, "Some writing vanishes from your head!");
                rehumanize(DIED, msgcat("deactivated by ", k_monnam(mtmp)));
                break;
            }
            gremlin_curse(&youmonst);
        }
        break;
    case AD_STUN:
        hitmsg(mtmp, mattk);
        if (!cancelled(mtmp) && !rn2(4)) {
            if (!resists_stun(&youmonst))
                inc_timeout(&youmonst, STUNNED, dmg, FALSE);
            dmg /= 2;
        }
        break;
    case AD_SLOW:
        hitmsg(mtmp, mattk);
        if (!uncancelled || defends(AD_SLOW, uwep) || rn2(4) ||
            resists_slow(&youmonst))
            break;
        inc_timeout(&youmonst, SLOW, dmg, FALSE);
        break;
    case AD_DREN:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4))
            drain_en(dmg);
        dmg = 0;
        break;
    case AD_CONF:
        hitmsg(mtmp, mattk);
        if (!cancelled(mtmp) && !rn2(4) && !mtmp->mspec_used) {
            mtmp->mspec_used = mtmp->mspec_used + (dmg + rn2(6));
            if (Confusion)
                pline(msgc_statusbad, "You are getting even more confused.");
            else
                pline(msgc_statusbad, "You are getting confused.");
            inc_timeout(&youmonst, CONFUSION, dmg, TRUE);
        }
        dmg = 0;
        break;
    case AD_DETH:
        if (resists_death(&youmonst)) {
            pline(msgc_fatalavoid,
                  "%s touches you!  Was that the touch of death?",
                  Monnam(mtmp));
            break;
        }
        switch (rn2_on_rng(20, rng_deathtouch)) {
        case 19:
        case 18:
        case 17:
            if (!Antimagic) {
                pline(msgc_fatal, "%s reaches out with its deadly touch...",
                      Monnam(mtmp));
                done(DIED, killer_msg(DIED, "the touch of Death"));
                dmg = 0;
                break;
            }   /* else FALLTHRU */
        default:       /* case 16: ... case 5: */
            pline(msgc_fatalavoid, "%s reaches out with its deadly touch.",
                  Monnam(mtmp));
            pline(msgc_intrloss, "You feel your life force draining away...");
            permdmg = 1;        /* actual damage done below */
            break;
        case 4:
        case 3:
        case 2:
        case 1:
        case 0:
            pline(msgc_fatalavoid, "%s reaches out with its deadly touch.",
                  Monnam(mtmp));
            if (Antimagic)
                shieldeff(u.ux, u.uy);
            /* can use a lower-priority channel for this because we already gave
               a warning that an instadeath's around */
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "Lucky for you, it didn't work!");
            dmg = 0;
            break;
        }
        break;
    case AD_PEST:
        diseasemu(mdat,
                  msgprintf("%s reaches out, and you feel fever and chills.",
                            Monnam(mtmp)));
        break;
    case AD_FAMN:
        /* A spammy msgc_fatal: that's a new one (to me allocating message
           channels, going through the files in alphabetical order). We reduce
           the spam by checking to see if the player is Hungry or worse. */
        pline(u.uhs >= HUNGRY ? msgc_fatal : msgc_statusbad,
              "%s reaches out, and your body shrivels.", Monnam(mtmp));
        exercise(A_CON, FALSE);
        if (u.uhs != FAINTED)
            morehungry(rn1(40, 40));
        /* plus the normal damage */
        break;
    case AD_SLIM:
        hitmsg(mtmp, mattk);
        if (!uncancelled)
            break;
        if (flaming(youmonst.data) ||
            level->locations[u.ux][u.uy].typ == LAVAPOOL) {
            pline(msgc_fatalavoid, "The slime burns away!");
            dmg = 0;
        } else if (Unchanging || unsolid(youmonst.data) ||
                   youmonst.data == &mons[PM_GREEN_SLIME]) {
            pline(msgc_fatalavoid, "You are unaffected.");
            dmg = 0;
        } else if (!sliming(&youmonst)) {
            set_property(&youmonst, SLIMED, 10, FALSE);
            set_delayed_killer(TURNED_SLIME,
                               killer_msg_mon(TURNED_SLIME, mtmp));
        } else
            pline(msgc_noconsequence, "Yuck!");
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        hitmsg(mtmp, mattk);
        /* uncancelled is sufficient enough; please don't make this attack less
           frequent */
        if (uncancelled) {
            struct obj *obj = some_armor(&youmonst);

            if (drain_item(obj)) {
                pline(msgc_itemloss, "Your %s less effective.",
                      aobjnam(obj, "seem"));
            }
        }
        break;
    case AD_ZOMB:
        hitmsg(mtmp, mattk);
        if (nonliving(youmonst.data) || izombie(&youmonst))
            break;
        zombie_timer = property_timeout(&youmonst, ZOMBIE);
        if (!zombie_timer)
            set_delayed_killer(TURNED_ZOMBIE,
                               killer_msg_mon(TURNED_ZOMBIE, mtmp));
        set_property(&youmonst, ZOMBIE,
                     !zombie_timer ? 100 :
                     zombie_timer <= 10 ? 1 :
                     (zombie_timer - 10), FALSE);
        break;
    default:
        dmg = 0;
        break;
    }

    /* TODO: This is the same message as mdamageu() below. We should be
       consistent about when the damage is applied. */
    if (u.uhp < 1)
        done_in_by(mtmp, NULL);

    /* Negative armor class reduces damage done instead of fully protecting
       against hits. */
    if (dmg && ac_after_rnd < 0) {
        dmg += AC_VALUE(ac_after_rnd);
        if (dmg < 1)
            dmg = 1;
    }

    if (dmg) {
        if (Half_physical_damage
            /* Mitre of Holiness */
            || (Role_if(PM_PRIEST) && uarmh && is_quest_artifact(uarmh) &&
                (is_undead(mtmp->data) || is_demon(mtmp->data))))
            dmg = (dmg + 1) / 2;

        if (permdmg) {  /* Death's life force drain */
            int lowerlimit, *hpmax_p;

            /*
             * Apply some of the damage to permanent hit points:
             *      polymorphed         100% against poly'd hpmax
             *      hpmax > 25*lvl      100% against normal hpmax
             *      hpmax > 10*lvl  50..100%
             *      hpmax >  5*lvl  25..75%
             *      otherwise        0..50%
             * Never reduces hpmax below 1 hit point per level->
             *
             * Note: there's a potential RNG desync here in cases where a player
             * gets hit by the "die instantly" result without MR, then
             * lifesaves, but that's unlikely enough that having touches of
             * death act differently in that game from then on is probably just
             * fine
             */
            permdmg = rn2_on_rng(dmg / 2 + 1, rng_deathtouch);
            if (Upolyd || u.uhpmax > 25 * u.ulevel)
                permdmg = dmg;
            else if (u.uhpmax > 10 * u.ulevel)
                permdmg += dmg / 2;
            else if (u.uhpmax > 5 * u.ulevel)
                permdmg += dmg / 4;

            if (Upolyd) {
                hpmax_p = &u.mhmax;
                /* [can't use youmonst.m_lev] */
                lowerlimit = min((int)youmonst.data->mlevel, u.ulevel);
            } else {
                hpmax_p = &u.uhpmax;
                lowerlimit = u.ulevel;
            }
            if (*hpmax_p - permdmg > lowerlimit)
                *hpmax_p -= permdmg;
            else if (*hpmax_p > lowerlimit)
                *hpmax_p = lowerlimit;
            /* else already at or below minimum threshold; do nothing */
        }

        if (mercy)
            do_mercy(mtmp, &youmonst, otmp, dmg);
        else
            mdamageu(mtmp, dmg);
    }

    if (dmg)
        res = passiveum(olduasmon, mtmp, mattk);
    else
        res = 1;
    action_interrupted();
    return res;
}


/* monster swallows you, or damage if Engulfed */
static int
gulpmu(struct monst *mtmp, const struct attack *mattk)
{
    struct trap *t = t_at(level, u.ux, u.uy);
    int tmp = dice((int)mattk->damn, (int)mattk->damd);
    int tim_tmp;
    struct obj *otmp2;
    int i;

    if (!Engulfed) {  /* swallows you */
        if (youmonst.data->msize >= MZ_HUGE)
            return 0;
        if ((t && ((t->ttyp == PIT) || (t->ttyp == SPIKED_PIT))) &&
            sobj_at(BOULDER, level, u.ux, u.uy))
            return 0;

        if (Punished)
            unplacebc();        /* ball&chain go away */
        if (is_animal(mtmp->data) && u.usteed) {
            /* Too many quirks presently if hero and steed are swallowed.
               Pretend purple worms don't like horses for now :-) */
            pline(msgc_statusbad, "%s lunges forward and plucks you off %s!",
                  Monnam(mtmp), mon_nam(u.usteed));
            dismount_steed(DISMOUNT_ENGULFED);
        } else
            pline(msgc_statusbad, "%s engulfs you!", Monnam(mtmp));
        remove_monster(level, mtmp->mx, mtmp->my);
        mtmp->mtrapped = 0;     /* no longer on old trap */
        place_monster(mtmp, u.ux, u.uy, TRUE);
        u.ustuck = mtmp;
        newsym(mtmp->mx, mtmp->my);

        action_interrupted();
        reset_occupations(TRUE);    /* behave as if you had moved */

        if (u.utrap) {
            pline(msgc_statusheal, "You are released from the %s!",
                  u.utraptype == TT_WEB ? "web" : "trap");
            u.utrap = 0;
        }

        i = number_leashed();
        if (i > 0) {
            const char *s = (i > 1) ? "leashes" : "leash";

            pline(msgc_petwarning, "The %s %s loose.", s, vtense(s, "snap"));
            unleash_all();
        }

        if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
            expels(mtmp, mtmp->data, FALSE);
            remove_monster(level, mtmp->mx, mtmp->my);
            place_monster(mtmp, u.ux, u.uy, TRUE);
            if (Punished)
                placebc();
            minstapetrify(&youmonst, mtmp);
            if (!DEADMONSTER(mtmp))
                return 0;
            else
                return 2;
        }

        win_pause_output(P_MESSAGE);
        vision_recalc(2);       /* hero can't see anything */
        Engulfed = 1;
        mtmp->mux = COLNO;      /* don't let the muxy equal the mxy */
        mtmp->muy = ROWNO;
        if (Punished)
            placebc();
        /* u.uswldtim always set > 1 */
        tim_tmp = 25 - (int)mtmp->m_lev;
        if (tim_tmp > 0)
            tim_tmp = rnd(tim_tmp) / 2;
        else if (tim_tmp < 0)
            tim_tmp = -(rnd(-tim_tmp) / 2);
        tim_tmp += -find_mac(&youmonst) + 10;
        u.uswldtim = (unsigned)((tim_tmp < 2) ? 2 : tim_tmp);
        swallowed(1);
        for (otmp2 = youmonst.minvent; otmp2; otmp2 = otmp2->nobj)
            snuff_lit(otmp2);
    }

    if (mtmp != u.ustuck)
        return 0;
    if (u.uswldtim > 0)
        u.uswldtim -= 1; /* what about slow digestion? */

    switch (mattk->adtyp) {

    case AD_DGST:
        if (Slow_digestion) {
            /* Messages are handled below */
            u.uswldtim = 0;
            tmp = 0;
        } else if (u.uswldtim == 0) {
            pline(msgc_fatal_predone, "%s totally digests you!", Monnam(mtmp));
            tmp = Upolyd ? u.mh : u.uhp;
            if (Half_physical_damage)
                tmp *= 2;       /* sorry */
        } else {
            pline(u.uswldtim <= 2 ? msgc_fatal : msgc_statusbad,
                  "%s%s digests you!", Monnam(mtmp),
                  (u.uswldtim == 2) ? " thoroughly" :
                  (u.uswldtim == 1) ? " utterly" : "");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_PHYS:
        if (mtmp->data == &mons[PM_FOG_CLOUD]) {
            pline(unbreathing(&youmonst) && !flaming(youmonst.data) ?
                  msgc_playerimmune : msgc_statusbad,
                  "You are laden with moisture and %s",
                  flaming(youmonst.data) ? "are smoldering out!" :
                  Breathless ? "find it mildly uncomfortable." :
                  amphibious(youmonst.data) ? "feel comforted." :
                  "can barely breathe!");
            if (Breathless && !flaming(youmonst.data))
                tmp = 0;
        } else {
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "You are pummeled with debris!");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_ACID:
        if (immune_to_acid(&youmonst)) {
            pline(msgc_playerimmune,
                  "You are covered with a seemingly harmless goo.");
            tmp = 0;
        } else {
            if (Hallucination)
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "Ouch!  You've been slimed!");
            else
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "You are covered in slime!%s",
                      !resists_acid(&youmonst) ? "  It burns!" : "");
            if (resists_acid(&youmonst))
                tmp = (tmp + 1) / 2;
            else
                exercise(A_STR, FALSE);
        }
        break;
    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, NULL)) {
            if (!blind(&youmonst))
                pline(msgc_statusbad, "You can't see in here!");
            inc_timeout(&youmonst, BLINDED,
                        blind(&youmonst) ? 1 : tmp, TRUE);
            if (!blind(&youmonst))
                pline(msgc_statusheal, "Your vision quickly clears.");
        }
        tmp = 0;
        break;
    case AD_ELEC:
        if (!cancelled(mtmp) && rn2(2)) {
            if (immune_to_elec(&youmonst)) {
                shieldeff(u.ux, u.uy);
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "Electricity sparks around, avoiding you.");
                ugolemeffects(AD_ELEC, tmp);
                tmp = 0;
            } else if (resists_elec(&youmonst)) {
                pline(combat_msgc(mtmp, &youmonst, cr_resist),
                      "Electricity sparks around, hurting you a bit!");
                tmp = (tmp + 1) / 2;
            } else
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "The air around you crackles with electricity.");
        } else
            tmp = 0;
        break;
    case AD_COLD:
        if (!cancelled(mtmp) && rn2(2)) {
            if (immune_to_cold(&youmonst)) {
                shieldeff(u.ux, u.uy);
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "You feel mildly chilly.");
                ugolemeffects(AD_COLD, tmp);
                tmp = 0;
            } else if (resists_cold(&youmonst)) {
                pline(combat_msgc(mtmp, &youmonst, cr_resist),
                      "You feel very chilly!");
                tmp = (tmp + 1) / 2;
            } else
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "You are freezing to death!");
        } else
            tmp = 0;
        break;
    case AD_FIRE:
        if (!cancelled(mtmp) && rn2(2)) {
            if (immune_to_fire(&youmonst)) {
                shieldeff(u.ux, u.uy);
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "You feel mildly hot.");
                ugolemeffects(AD_FIRE, tmp);
                tmp = 0;
            } else if (resists_fire(&youmonst)) {
                pline(combat_msgc(mtmp, &youmonst, cr_resist),
                      "You feel very hot!");
                tmp = (tmp + 1) / 2;
            } else
                pline(combat_msgc(mtmp, &youmonst, cr_hit),
                      "You are burning to a crisp!");
            burn_away_slime(&youmonst);
        } else
            tmp = 0;
        break;
    case AD_DISE:
        if (!diseasemu(mtmp->data, "You feel terribly sick."))
            tmp = 0;
        break;
    default:
        tmp = 0;
        break;
    }

    if (Half_physical_damage)
        tmp = (tmp + 1) / 2;

    mdamageu(mtmp, tmp);
    if (tmp)
        action_interrupted();

    if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
        pline(msgc_statusheal, "%s very hurriedly %s you!", Monnam(mtmp),
              is_animal(mtmp->data) ? "regurgitates" : "expels");
        expels(mtmp, mtmp->data, FALSE);
    } else if (!u.uswldtim || youmonst.data->msize >= MZ_HUGE) {
        pline(msgc_statusheal, "You get %s!",
              is_animal(mtmp->data) ? "regurgitated" : "expelled");
        /* TODO: This condition looks very wrong (especially given where it is
           in the code). */
        if (is_animal(mtmp->data) ||
            (dmgtype(mtmp->data, AD_DGST) && Slow_digestion))
            pline_implied(msgc_hint, "Obviously %s doesn't like your taste.",
                          mon_nam(mtmp));
        expels(mtmp, mtmp->data, FALSE);
    }
    return 1;
}

/* monster explodes in your face */
static int
explmu(struct monst *mtmp, const struct attack *mattk)
{
    if (cancelled(mtmp))
        return 0;

    int tmp = dice((int)mattk->damn, (int)mattk->damd);
    boolean not_affected = defends((int)mattk->adtyp, uwep);
    int hurt = 0;

    hitmsg(mtmp, mattk);
    remove_monster(level, mtmp->mx, mtmp->my);
    newsym(mtmp->mx, mtmp->my);

    switch (mattk->adtyp) {
    case AD_COLD:
        if (!immune_to_cold(&youmonst))
            hurt++;
        if (!resists_cold(&youmonst))
            hurt++;
        goto common;
    case AD_FIRE:
        if (!immune_to_fire(&youmonst))
            hurt++;
        if (!resists_fire(&youmonst))
            hurt++;
        goto common;
    case AD_ELEC:
        if (!immune_to_elec(&youmonst))
            hurt++;
        if (!resists_elec(&youmonst))
            hurt++;
    common:
        if (hurt) {
            if (ACURR(A_DEX) > rnd(20)) {
                pline(combat_msgc(mtmp, &youmonst, cr_resist),
                      "You duck some of the blast.");
                tmp = (tmp + 1) / 2;
            } else {
                pline_implied(combat_msgc(mtmp, &youmonst, cr_hit),
                              "You get blasted!");
            }
            if (hurt == 1) {
                pline(combat_msgc(mtmp, &youmonst, cr_resist),
                      "%s!", M_verbs(&youmonst, "resist"));
                tmp = (tmp + 1) / 2;
            }
            if (mattk->adtyp == AD_FIRE)
                burn_away_slime(&youmonst);
            if (Half_physical_damage)
                tmp = (tmp + 1) / 2;
            mdamageu(mtmp, tmp);
        }
        break;

    case AD_BLND:
        not_affected = resists_blnd(&youmonst);
        if (!not_affected) {
            /* sometimes you're affected even if it's invisible */
            if (mon_visible(mtmp) || (rnd(tmp /= 2) > u.ulevel)) {
                pline(msgc_statusbad, "You are blinded by a blast of light!");
                inc_timeout(&youmonst, BLINDED, tmp, TRUE);
                if (!blind(&youmonst))
                    pline(msgc_statusheal, "Your vision quickly clears.");
            } else if (flags.verbose)
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "You get the impression it was not terribly bright.");
        }
        break;

    case AD_HALU:
        not_affected |= (blind(&youmonst) ||
                         u.umonnum == PM_BLACK_LIGHT ||
                         u.umonnum == PM_VIOLET_FUNGUS ||
                         dmgtype(youmonst.data, AD_STUN));
        if (!not_affected) {
            if (!Hallucination)
                pline_implied(msgc_statusbad,
                              "You are caught in a blast of kaleidoscopic light!");
            inc_timeout(&youmonst, HALLUC, tmp, TRUE);
            if (hallucinating(&youmonst))
                pline(msgc_statusbad, "You are freaked out.");
            else
                pline(msgc_playerimmune, "You seem unaffected.");
        }
        break;

    default:
        break;
    }
    if (not_affected) {
        pline(msgc_playerimmune, "You seem unaffected by it.");
        ugolemeffects((int)mattk->adtyp, tmp);
    }

    place_monster(mtmp, mtmp->mx, mtmp->my, TRUE);
    mondead(mtmp);
    wake_nearto(mtmp->mx, mtmp->my, 7 * 7);
    if (!DEADMONSTER(mtmp))
        return 0;
    return 2;   /* it dies */
}


void
do_mercy(struct monst *magr, struct monst *mdef,
         struct obj *obj, int dmg)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef ||
                   cansee(magr->mx, magr->my) ||
                   cansee(mdef->mx, mdef->my));
    boolean restored_hp = FALSE;
    boolean saw_something = FALSE;
    if (mdef != &youmonst) {
        if (mdef->mhp < mdef->mhpmax) {
            mdef->mhp += dmg;
            if (mdef->mhp > mdef->mhpmax)
                mdef->mhp = mdef->mhpmax;
            restored_hp = TRUE;
        }
    } else if (Upolyd) {
        if (u.mh < u.mhmax) {
            u.mh += dmg;
            if (u.mh > u.mh)
                u.mh = u.mhmax;
            restored_hp = TRUE;
        }
    } else {
        if (u.uhp < u.uhpmax) {
            u.uhp += dmg;
            if (u.uhp > u.uhpmax)
                u.uhp = u.uhpmax;
            restored_hp = TRUE;
        }
    }

    if (restored_hp && vis) {
        pline(msgc_statusgood, "%s healed!", M_verbs(mdef, "are"));
        saw_something = TRUE;
    }

    if (obj) {
        obj->mknown = 1;
        obj->mbknown = 1;

        if (!obj->cursed) {
            if (uagr || canseemon(magr)) {
                pline(msgc_monneutral, "%s %s for a moment.",
                      Tobjnam(obj, "glow"), hcolor("black"));
                saw_something = TRUE;
            }
            curse(obj);
        }

        if (saw_something) {
            obj->bknown = 1;
            learn_oprop(obj, opm_mercy);
        }
    }
}

/* mtmp hits you for n points damage */
void
mdamageu(struct monst *mtmp, int n)
{
    if (Upolyd) {
        u.mh -= n;
        if (u.mh < 1)
            rehumanize(DIED, killer_msg_mon(DIED, mtmp));
    } else {
        u.uhp -= n;
        if (u.uhp < 1)
            done_in_by(mtmp, NULL);
    }
}


int
could_seduce(struct monst *magr, struct monst *mdef, const struct attack *mattk)
/* returns 0 if seduction impossible,
 *     1 if fine,
 *     2 if wrong gender for nymph */
{
    const struct permonst *pagr;
    boolean agrinvis, defperc;
    xchar genagr, gendef;

    if (is_animal(magr->data))
        return 0;
    if (magr == &youmonst) {
        pagr = youmonst.data;
        agrinvis = (Invis != 0);
        genagr = poly_gender();
    } else {
        pagr = magr->data;
        agrinvis = invisible(magr);
        genagr = gender(magr);
    }
    if (mdef == &youmonst) {
        defperc = (see_invisible(&youmonst));
        gendef = poly_gender();
    } else {
        defperc = see_invisible(mdef);
        gendef = gender(mdef);
    }

    if (agrinvis && !defperc && mattk && mattk->adtyp != AD_SSEX)
        return 0;

    if (pagr->mlet != S_NYMPH &&
        (pagr != &mons[PM_INCUBUS] || (mattk && mattk->adtyp != AD_SSEX)))
        return 0;

    if (genagr == 1 - gendef)
        return 1;
    else
        return (pagr->mlet == S_NYMPH) ? 2 : 0;
}


/* Returns 1 if monster teleported */
int
doseduce(struct monst *mon)
{
    struct obj *ring, *nring;
    boolean fem = mon->female;
    const char *qbuf;

    if (cancelled(mon) || mon->mspec_used) {
        pline(combat_msgc(mon, &youmonst, cr_miss),
              "%s acts as though %s has got a %sheadache.", Monnam(mon),
              mhe(mon), cancelled(mon) ? "severe " : "");
        return 0;
    }

    if (u_helpless(hm_all)) {
        pline(combat_msgc(mon, &youmonst, cr_miss),
              "%s seems dismayed at your lack of response.", Monnam(mon));
        return 0;
    }

    if (Blind)
        pline(combat_msgc(mon, &youmonst, cr_hit),
              "It caresses you...");
    else
        pline(combat_msgc(mon, &youmonst, cr_hit),
              "You feel very attracted to %s.", mon_nam(mon));

    for (ring = youmonst.minvent; ring; ring = nring) {
        nring = ring->nobj;
        if (ring->otyp != RIN_ADORNMENT)
            continue;
        if (fem) {
            if (rn2(20) < ACURR(A_CHA)) {
                qbuf = msgprintf(
                    "\"That %s looks pretty.  May I have it?\"",
                    safe_qbuf("",
                              sizeof
                              ("\"That  looks pretty.  May I have it?\""),
                              xname(ring), simple_typename(ring->otyp),
                              "ring"));
                if (yn(qbuf) == 'n') {
                    tell_discovery(ring);
                    continue;
                }
            } else
                pline(msgc_itemloss,
                      "%s decides she'd like your %s, and takes it.",
                      Blind ? "She" : Monnam(mon), xname(ring));
            tell_discovery(ring);
            unwield_silently(ring);
            setunequip(ring);
            freeinv(ring);
            mpickobj(mon, ring, NULL);
        } else {
            enum objslot slot = os_invalid;

            if (uleft && uright && uleft->otyp == RIN_ADORNMENT &&
                uright->otyp == RIN_ADORNMENT)
                break;
            if (ring == uleft || ring == uright)
                continue;
            if (rn2(20) < ACURR(A_CHA)) {
                qbuf = msgprintf(
                    "\"That %s looks pretty.  Would you wear it for me?\"",
                    safe_qbuf("", sizeof ("\"That  looks pretty.  Would "
                                          "you wear it for me?\""),
                              xname(ring), simple_typename(ring->otyp),
                              "ring"));
                if (yn(qbuf) == 'n') {
                    tell_discovery(ring);
                    continue;
                }
            } else {
                pline(msgc_statusbad,
                      "%s decides you'd look prettier wearing your %s,",
                      Blind ? "He" : Monnam(mon), xname(ring));
                pline(msgc_statusbad, "and puts it on your finger.");
            }
            tell_discovery(ring);
            if (!uright) {
                pline_implied(msgc_info, "%s puts %s on your right %s.",
                              Blind ? "He" : Monnam(mon), the(xname(ring)),
                              body_part(HAND));
                slot = os_ringr;
            } else if (!uleft) {
                pline_implied(msgc_info, "%s puts %s on your left %s.",
                              Blind ? "He" : Monnam(mon), the(xname(ring)),
                              body_part(HAND));
                slot = os_ringl;
            } else if (uright && uright->otyp != RIN_ADORNMENT) {
                pline_implied(msgc_info, "%s replaces your %s with your %s.",
                              Blind ? "He" : Monnam(mon), xname(uright),
                              xname(ring));
                setunequip(uright);
                slot = os_ringr;
            } else if (uleft && uleft->otyp != RIN_ADORNMENT) {
                pline_implied(msgc_info, "%s replaces your %s with your %s.",
                              Blind ? "He" : Monnam(mon), xname(uleft),
                              xname(ring));
                setunequip(uleft);
                slot = os_ringl;
            } else {
                impossible("ring replacement");
                slot = os_ringl;
            }
            unwield_silently(ring);
            setequip(slot, ring, em_silent);
            prinv(NULL, ring, 0L);
        }
    }

    if (!uarmc && !uarmf && !uarmg && !uarms && !uarmh && !uarmu)
        pline_implied(msgc_npcvoice,
                      "%s murmurs sweet nothings into your ear.",
                      Blind ? (fem ? "She" : "He") : Monnam(mon));
    else
        pline_implied(msgc_statusbad,
                      "%s murmurs in your ear, while helping you undress.",
                      Blind ? (fem ? "She" : "He") : Monnam(mon));
    mayberem(uarmc, cloak_simple_name(uarmc));
    if (!uarmc && !uskin())
        mayberem(uarm, "suit");
    mayberem(uarmf, "boots");
    if (!uwep || !welded(uwep))
        mayberem(uarmg, "gloves");
    mayberem(uarms, "shield");
    mayberem(uarmh, maybe_helmet_name(uarmh));
    if (!uarmc && !uarm)
        mayberem(uarmu, "shirt");

    if ((uarm && !uskin()) || uarmc) {
        verbalize(msgc_npcvoice, "You're such a %s; I wish...",
                  u.ufemale ? "sweet lady" : "nice guy");
        if (!tele_restrict(mon))
            rloc(mon, TRUE);
        return 1;
    }
    if (u.ualign.type == A_CHAOTIC)
        adjalign(1);

    /* by this point you have discovered mon's identity, blind or not... */
    pline_implied(msgc_statusgood, "Time stands still while you and %s "
                  "lie in each other's arms...", noit_mon_nam(mon));

    /* Message channelization: avoid intrloss and intrgain channels if those
       channels are going to have messages printed on them via adjattrib,
       both to prevent repeated force-More (if the player set that, it's not
       a default but makes some sense), and so that you don't get intrgain
       colors for maxed-out abilities */
    
    if (rn2_on_rng(35, rng_foocubus_results) > ACURR(A_CHA) + ACURR(A_INT)) {
        /* Don't bother with mspec_used here... it didn't get tired! */
        pline(msgc_statusbad,
              "%s seems to have enjoyed it more than you...", noit_Monnam(mon));
        switch (rn2_on_rng(5, rng_foocubus_results)) {
        case 0:
            pline(msgc_intrloss, "You feel drained of energy.");
            youmonst.pw = 0;
            youmonst.pwmax -= rnd(Half_physical_damage ? 5 : 10);
            exercise(A_CON, FALSE);
            if (youmonst.pwmax < 0)
                youmonst.pwmax = 0;
            break;
        case 1:
            pline(msgc_statusbad, "You are down in the dumps.");
            adjattrib(A_CON, -1, TRUE);
            exercise(A_CON, FALSE);
            break;
        case 2:
            pline(msgc_statusbad, "Your senses are dulled.");
            adjattrib(A_WIS, -1, TRUE);
            exercise(A_WIS, FALSE);
            break;
        case 3:
            if (!resists_drli(&youmonst)) {
                /* If we're near death or outright die from this, use an
                   instadeath warning or instadeath channel */
                pline(u.ulevel > 3 ? msgc_statusbad :
                      u.ulevel > 1 ? msgc_fatal : msgc_fatal_predone,
                      "You feel out of shape.");
                losexp(killer_msg(DIED, "overexertion"), FALSE);
            } else {
                pline(msgc_playerimmune,
                      "You have a curious feeling...");
            }
            break;
        case 4:{
                int tmp;

                pline(msgc_moncombatbad, "You feel exhausted.");
                exercise(A_STR, FALSE);
                tmp = rn1(10, 6);
                if (Half_physical_damage)
                    tmp = (tmp + 1) / 2;
                losehp(tmp, killer_msg(DIED, "exhaustion"));
                break;
            }
        }
    } else {
        mon->mspec_used = rnd(100);     /* monster is worn out */
        pline(msgc_statusgood, "You seem to have enjoyed it more than %s...",
              noit_mon_nam(mon));
        switch (rn2_on_rng(5, rng_foocubus_results)) {
        case 0:
            pline(msgc_intrgain, "You feel raised to your full potential.");
            exercise(A_CON, TRUE);
            youmonst.pw = (youmonst.pwmax += rnd(5));
            break;
        case 1:
            pline(msgc_statusgood, "You feel good enough to do it again.");
            adjattrib(A_CON, 1, TRUE);
            exercise(A_CON, TRUE);
            break;
        case 2:
            pline(msgc_statusgood, "You will always remember %s...",
                  noit_mon_nam(mon));
            adjattrib(A_WIS, 1, TRUE);
            exercise(A_WIS, TRUE);
            break;
        case 3:
            pline(msgc_statusgood, "That was a very educational experience.");
            pluslvl(FALSE);
            exercise(A_WIS, TRUE);
            break;
        case 4:
            pline(msgc_statusheal, "You feel restored to health!");
            u.uhp = u.uhpmax;
            if (Upolyd)
                u.mh = u.mhmax;
            exercise(A_STR, TRUE);
            break;
        }
    }

    if (mon->mtame)     /* don't charge */
        ;
    else if (rn2(20) < ACURR(A_CHA)) {
        pline(msgc_moncombatgood,
              "%s demands that you pay %s, but you refuse...", noit_Monnam(mon),
              Blind ? (fem ? "her" : "him") : mhim(mon));
    } else if (u.umonnum == PM_LEPRECHAUN)
        pline(msgc_playerimmune, "%s tries to take your money, but fails...",
              noit_Monnam(mon));
    else {
        long cost;
        long umoney = money_cnt(youmonst.minvent);

        /* TODO: This is possibly the best overflow check ever. The overflow
           check itself works, but what about the replacement? (int and long
           are the same size on most platforms.) */
        if (umoney > (long)LARGEST_INT - 10L)
            cost = (long)rnd(LARGEST_INT) + 500L;
        else
            cost = (long)rnd((int)umoney + 10) + 500L;
        if (mon->mpeaceful) {
            cost /= 5L;
            if (!cost)
                cost = 1L;
        }
        if (cost > umoney)
            cost = umoney;
        if (!cost)
            verbalize(msgc_moncombatgood, "It's on the house!");
        else {
            pline(msgc_itemloss, "%s takes %ld %s for services rendered!",
                  noit_Monnam(mon), cost, currency(cost));
            money2mon(mon, cost);
        }
    }
    if (!rn2_on_rng(25, rng_foocubus_results))
        set_property(mon, CANCELLED, 0, FALSE);
    if (!tele_restrict(mon))
        rloc(mon, TRUE);
    return 1;
}

static void
mayberem(struct obj *obj, const char *str)
{
    const char *qbuf;

    if (!obj || !obj->owornmask)
        return;

    if (rn2(20) < ACURR(A_CHA)) {
        qbuf = msgprintf("\"Shall I remove your %s, %s?\"", str,
                         (!rn2(2) ? "lover" : !rn2(2) ? "dear" : "sweetheart"));
        if (yn(qbuf) == 'n')
            return;
    } else {
        const char *hairbuf;

        hairbuf = msgprintf("let me run my fingers through your %s",
                            body_part(HAIR));
        verbalize(msgc_statusbad, "Take off your %s; %s.", str,
                  (obj == uarm) ? "let's get a little closer" :
                  (obj == uarmc || obj == uarms) ? "it's in the way" :
                  (obj == uarmf) ? "let me rub your feet" :
                  (obj == uarmg) ? "they're too clumsy" :
                  (obj == uarmu) ? "let me massage you" :
                  /* obj == uarmh */
                  hairbuf);
    }
    remove_worn_item(obj, TRUE);
}


static int
passiveum(const struct permonst *olduasmon, struct monst *mtmp,
          const struct attack *mattk)
{
    int i, tmp;

    for (i = 0;; i++) {
        if (i >= NATTK)
            return 1;
        if (olduasmon->mattk[i].aatyp == AT_NONE ||
            olduasmon->mattk[i].aatyp == AT_BOOM)
            break;
    }
    if (olduasmon->mattk[i].damn)
        tmp =
            dice((int)olduasmon->mattk[i].damn, (int)olduasmon->mattk[i].damd);
    else if (olduasmon->mattk[i].damd)
        tmp = dice((int)olduasmon->mlevel + 1, (int)olduasmon->mattk[i].damd);
    else
        tmp = 0;

    /* These affect the enemy even if you were "killed" (rehumanized) */
    switch (olduasmon->mattk[i].adtyp) {
    case AD_ACID:
        if (!rn2(2)) {
            if (immune_to_acid(mtmp)) {
                pline(combat_msgc(&youmonst, mtmp, cr_immune),
                      "%s is splashed by your acid, but doesn't care.",
                      Monnam(mtmp));
                tmp = 0;
            } else {
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s is splashed by your acid!", Monnam(mtmp));
                if (resists_acid(mtmp))
                    tmp = (tmp + 1) / 2;
            }
        } else
            tmp = 0;
        if (!rn2(30))
            hurtarmor(mtmp, ERODE_CORRODE);
        if (!rn2(6))
            erode_obj(MON_WEP(mtmp), NULL, ERODE_CORRODE, TRUE, TRUE);
        goto assess_dmg;
    case AD_STON:      /* cockatrice */
        {
            long protector = attk_protection((int)mattk->aatyp);
            long wornitems = mtmp->misc_worn_check;

            /* wielded weapon gives same protection as gloves here */
            if (MON_WEP(mtmp) != 0)
                wornitems |= W_MASK(os_armg);

            if (!resists_ston(mtmp) &&
                (protector == 0L ||
                 (protector != ~0L && (wornitems & protector) != protector))) {
                if (poly_when_stoned(mtmp->data)) {
                    mon_to_stone(mtmp);
                    return 1;
                }
                pline(msgc_kill, "%s turns to stone!", Monnam(mtmp));
                stoned = 1;
                xkilled(mtmp, 0);
                if (!DEADMONSTER(mtmp))
                    return 1;
                return 2;
            }
            return 1;
        }
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        if (otmp) {
            drain_item(otmp);
            /* No message */
        }
        return 1;
    default:
        break;
    }
    if (!Upolyd)
        return 1;

    /* These affect the enemy only if you are still a monster */
    if (rn2(3))
        switch (youmonst.data->mattk[i].adtyp) {
        case AD_PHYS:
            if (youmonst.data->mattk[i].aatyp == AT_BOOM) {
                pline(msgc_statusend, "You explode!");
                u.mh = -1;
                rehumanize(EXPLODED, killer_msg_mon(EXPLODED, mtmp));
                goto assess_dmg;
            }
            break;
        case AD_SLOW:
            if (blind(&youmonst)) {
                pline(msgc_statusbad,
                      "Being blind, you cannot defend yourself.");
                /* TODO: rarely lose luck for monster */
                break;
            }

            if (cancelled(&youmonst) ||
                !(msensem(mtmp, &youmonst) & MSENSE_VISION) ||
                !(msensem(&youmonst, mtmp) & MSENSE_VISION))
                break;

            if (resists_slow(mtmp)) {
                pline(combat_msgc(&youmonst, mtmp, cr_immune),
                      "%s down momentarily.",
                      M_verbs(mtmp, "slow"));
                break;
            }

            if (!slow(mtmp) && canseemon(mtmp))
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s down under your gaze!", M_verbs(mtmp, "slow"));
            inc_timeout(mtmp, SLOW, tmp, TRUE);
            break;
        case AD_PLYS:
            if (free_action(mtmp)) {
                if (canseemon(mtmp))
                    pline(combat_msgc(&youmonst, mtmp, cr_immune),
                          "%s momentarily stiffens.",
                          Monnam(mtmp));
                return 1;
            }

            if (canseemon(mtmp))
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s is frozen by you.", Monnam(mtmp));
            mtmp->mcanmove = 0;
            mtmp->mfrozen = min(tmp, 127);
            return 3;
        case AD_COLD:  /* Brown mold or blue jelly */
            if (immune_to_cold(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline(combat_msgc(&youmonst, mtmp, cr_immune),
                      "%s is mildly chilly.", Monnam(mtmp));
                golemeffects(mtmp, AD_COLD, tmp);
                tmp = 0;
                break;
            } else if (resists_cold(mtmp)) {
                pline(combat_msgc(&youmonst, mtmp, cr_resist),
                      "%s very chilly!", M_verbs(mtmp, "are"));
                tmp = (tmp + 1) / 2;
            } else
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s is suddenly very cold!", Monnam(mtmp));
            u.mh += tmp / 2;
            if (u.mhmax < u.mh)
                u.mhmax = u.mh;
            if (u.mhmax > ((youmonst.data->mlevel + 1) * 8))
                split_mon(&youmonst, mtmp);
            break;
        case AD_STUN:  /* Yellow mold */
            if (!resists_stun(mtmp)) {
                inc_timeout(mtmp, STUNNED, tmp, TRUE);
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s %s.", Monnam(mtmp),
                      makeplural(stagger(mtmp->data, "stagger")));
            }
            tmp = 0;
            break;
        case AD_FIRE:  /* Red mold */
            if (immune_to_fire(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline(combat_msgc(&youmonst, mtmp, cr_miss),
                      "%s is mildly warm.", Monnam(mtmp));
                golemeffects(mtmp, AD_FIRE, tmp);
                tmp = 0;
                break;
            } else if (resists_fire(mtmp)) {
                pline(combat_msgc(&youmonst, mtmp, cr_resist),
                      "%s mildly hot!", M_verbs(mtmp, "are"));
                tmp = (tmp + 1) / 2;
                break;
            }
            pline(combat_msgc(&youmonst, mtmp, cr_hit),
                  "%s is suddenly very hot!", Monnam(mtmp));
            break;
        case AD_ELEC:
            if (immune_to_elec(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline(combat_msgc(&youmonst, mtmp, cr_immune),
                      "%s is slightly tingled.", Monnam(mtmp));
                golemeffects(mtmp, AD_ELEC, tmp);
                tmp = 0;
                break;
            } else if (resists_elec(mtmp)) {
                pline(combat_msgc(&youmonst, mtmp, cr_resist),
                      "%s jolted slightly.", M_verbs(mtmp, "are"));
                tmp = (tmp + 1) / 2;
            } else
                pline(combat_msgc(&youmonst, mtmp, cr_hit),
                      "%s is jolted with your electricity!", Monnam(mtmp));
            break;
        default:
            tmp = 0;
            break;
    } else
        tmp = 0;

assess_dmg:
    if ((mtmp->mhp -= tmp) <= 0) {
        pline(msgc_kill, "%s dies!", Monnam(mtmp));
        xkilled(mtmp, 0);
        if (!DEADMONSTER(mtmp))
            return 1;
        return 2;
    }
    return 1;
}


struct monst *
cloneu(void)
{
    struct monst *mon;
    int mndx = monsndx(youmonst.data);

    if (u.mh <= 1)
        return NULL;
    if (mvitals[mndx].mvflags & G_EXTINCT)
        return NULL;

    mon = makemon(youmonst.data, level, u.ux, u.uy, NO_MINVENT | MM_EDOG);
    if (mon) {
        christen_monst(mon, u.uplname);
        initedog(mon);
        mon->m_lev = youmonst.data->mlevel;
        mon->mhpmax = u.mhmax;
        mon->mhp = u.mh / 2;
        u.mh -= mon->mhp;
    }
    return mon;
}


/*mhitu.c*/
