/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"
#include "hungerstatus.h"

static struct obj *otmp;

static boolean u_slip_free(struct monst *, const struct attack *);
static int passiveum(const struct permonst *, struct monst *,
                     const struct attack *);
static void mayberem(struct obj *, const char *);

static boolean diseasemu(const struct permonst *);
static int hitmu(struct monst *, const struct attack *);
static int gulpmu(struct monst *, const struct attack *);
static int explmu(struct monst *, const struct attack *);
static void missmu(struct monst *, boolean, const struct attack *);
static void mswings(struct monst *, struct obj *);

static void hitmsg(struct monst *, const struct attack *);

/* See comment in mhitm.c.  If we use this a lot it probably should be */
/* changed to a parameter to mhitu. */
static int dieroll;



static void
hitmsg(struct monst *mtmp, const struct attack *mattk)
{
    int compat;

    /* Note: if opposite gender, "seductively" */
    /* If same gender, "engagingly" for nymph, normal msg for others */
    if ((compat = could_seduce(mtmp, &youmonst, mattk))
        && !mtmp->mcan && !mtmp->mspec_used) {
        pline("%s %s you %s.", Monnam(mtmp), Blind ? "talks to" : "smiles at",
              compat == 2 ? "engagingly" : "seductively");
    } else
        switch (mattk->aatyp) {
        case AT_BITE:
            pline("%s bites!", Monnam(mtmp));
            break;
        case AT_KICK:
            pline("%s kicks%c", Monnam(mtmp),
                  thick_skinned(youmonst.data) ? '.' : '!');
            break;
        case AT_STNG:
            pline("%s stings!", Monnam(mtmp));
            break;
        case AT_BUTT:
            pline("%s butts!", Monnam(mtmp));
            break;
        case AT_TUCH:
            pline("%s touches you!", Monnam(mtmp));
            break;
        case AT_TENT:
            pline("%s tentacles suck you!", s_suffix(Monnam(mtmp)));
            break;
        case AT_EXPL:
        case AT_BOOM:
            pline("%s explodes!", Monnam(mtmp));
            break;
        default:
            pline("%s hits!", Monnam(mtmp));
        }
}

/* monster missed you */
static void
missmu(struct monst *mtmp, boolean nearmiss, const struct attack *mattk)
{
    /* not reveal_monster_at; it's attacking you, not vice versa */
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    if (could_seduce(mtmp, &youmonst, mattk) && !mtmp->mcan)
        pline("%s pretends to be friendly.", Monnam(mtmp));
    else {
        if (!flags.verbose || !nearmiss)
            pline("%s misses.", Monnam(mtmp));
        else
            pline("%s just misses!", Monnam(mtmp));
    }
    action_interrupted();
}

/* monster swings obj */
static void
mswings(struct monst *mtmp, struct obj *otemp)
{
    if (!flags.verbose || Blind || !mon_visible(mtmp))
        return;
    pline("%s %s %s %s.", Monnam(mtmp),
          (objects[otemp->otyp].oc_dir & PIERCE) ? "thrusts" : "swings",
          mhis(mtmp), singular(otemp, xname));
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

/* called when your intrinsic speed is taken away */
void
u_slow_down(void)
{
    HFast = 0L;
    if (!Fast)
        pline("You slow down.");
    else        /* speed boots */
        pline("Your quickness feels less natural.");
    exercise(A_DEX, FALSE);
}


void
expels(struct monst *mtmp,
       const struct permonst *mdat, /* if mtmp is polymorphed,
                                       mdat != mtmp->data */
       boolean message)
{
    if (message) {
        if (is_animal(mdat))
            pline("You get regurgitated!");
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
                pline("You get expelled from %s%s!", mon_nam(mtmp), blast);
            }
        }
    }
    unstuck(mtmp);      /* ball&chain returned in unstuck() */
    mnexto(mtmp);
    newsym(u.ux, u.uy);
    spoteffects(TRUE);
    /* to cover for a case where mtmp is not in a next square */
    if (um_dist(mtmp->mx, mtmp->my, 1))
        pline("Brrooaa...  You land hard at some distance.");
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
    boolean ranged = (distu(mtmp->mx, mtmp->my) > 3);

    /* Is it near you? Affects your actions */
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
        range2 = 0;
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
            if (i & MM_DEF_DIED || u.umoved)
                return 0;
            /* Let your steed retaliate */
            return !!(mattackm(u.usteed, mtmp) & MM_DEF_DIED);
        }
    }

    /* must be called after the above mattackm check, or the monster could
       summon at both you and your steed in the same attack */
    if (!mpreattack(mtmp, range2))
        return 0;

    if (u.uundetected && !range2 && !Engulfed) {
        u.uundetected = 0;
        if (is_hider(youmonst.data)) {
            coord cc;   /* maybe we need a unexto() function? */
            struct obj *obj;

            if (youmonst.data == &mons[PM_TRAPPER])
                pline("You ambush %s!", mon_nam(mtmp));
            else
                pline("You fall from the %s!", ceiling(u.ux, u.uy));

            /* TODO: This next line is beautifully exploitable (see the
               explanation on TASvideos). */
            if (enexto(&cc, level, u.ux, u.uy, youmonst.data)) {
                remove_monster(level, mtmp->mx, mtmp->my);
                newsym(mtmp->mx, mtmp->my);
                place_monster(mtmp, u.ux, u.uy);
                if (mtmp->wormno)
                    worm_move(mtmp);
                teleds(cc.x, cc.y, TRUE);
                set_apparxy(mtmp);
                newsym(u.ux, u.uy);
            } else {
                /* this is a pathological case, so might as well be silly about
                   it... */
                if (youmonst.data == &mons[PM_TRAPPER])
                    pline("%s dies from the shock of the ambush.",
                          Monnam(mtmp));
                else
                    pline("%s is killed by a falling %s (you)!", Monnam(mtmp),
                          youmonst.data->mname);
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
                pline("Your blow glances off %s %s.", s_suffix(mon_nam(mtmp)),
                      helmet_name(obj));
            } else {
                if (3 + find_mac(mtmp) <= rnd(20)) {
                    pline("%s is hit by a falling piercer (you)!",
                          Monnam(mtmp));
                    if ((mtmp->mhp -= dice(3, 6)) < 1)
                        killed(mtmp);
                } else
                    pline("%s is almost hit by a falling piercer (you)!",
                          Monnam(mtmp));
            }
        } else {
            if (!youseeit)
                pline("It tries to move where you are hiding.");
            else {
                /* Ugly kludge for eggs.  The message is phrased so as to be
                   directed at the monster, not the player, which makes "laid
                   by you" wrong.  For the parallelism to work, we can't
                   rephrase it, so we zap the "laid by you" momentarily
                   instead. */
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
                        pline("Wait, %s!  There's a hidden %s named %s there!",
                              m_monnam(mtmp), youmonst.data->mname, u.uplname);
                    else
                        pline("Wait, %s!  There's a %s named %s hiding under "
                              "%s!", m_monnam(mtmp), youmonst.data->mname,
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
        if (!youseeit)
            pline("It gets stuck on you.");
        else
            pline("Wait, %s!  That's a %s named %s!", m_monnam(mtmp),
                  youmonst.data->mname, u.uplname);
        u.ustuck = mtmp;
        cancel_mimicking("");
        return 0;
    }

    /* player might be mimicking an object */
    if (youmonst.m_ap_type == M_AP_OBJECT && !range2 && !Engulfed) {
        if (!youseeit)
            pline("Something %s!",
                  (likes_gold(mtmp->data) &&
                   youmonst.mappearance ==
                   GOLD_PIECE) ? "tries to pick you up" : "disturbs you");
        else
            pline("Wait, %s!  That %s is really %s named %s!", m_monnam(mtmp),
                  mimic_obj_name(&youmonst), an(mons[u.umonnum].mname),
                  u.uplname);

        const char *buf;
        buf = msgprintf("You appear to be %s again.",
                        Upolyd ?
                        (const char *)an(youmonst.data->mname) :
                        (const char *)"yourself");
        cancel_mimicking(buf); /* immediately stop mimicking */

        return 0;
    }

    /* Work out the armor class differential   */
    tmp = AC_VALUE(get_player_ac()) + 10; /* tmp ~= 0 - 20 */
    tmp += mtmp->m_lev;
    if (u_helpless(hm_all))
        tmp += 4;
    if ((Invis && !perceives(mdat)) || !mtmp->mcansee)
        tmp -= 2;
    if (mtmp->mtrapped)
        tmp -= 2;
    if (tmp <= 0)
        tmp = 1;

    /* make eels visible the moment they hit/miss us */
    if (mdat->mlet == S_EEL && mtmp->minvis && cansee(mtmp->mx, mtmp->my)) {
        mtmp->minvis = 0;
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
            pline("%s loosens its grip slightly.", Monnam(mtmp));
        else if (!range2) {
            if (youseeit || sensemon(mtmp))
                pline("%s starts to attack you, but pulls back.", Monnam(mtmp));
            else
                pline("You feel something move nearby.");
        }
        return 0;
    }

    /* Unlike defensive stuff, don't let them use item _and_ attack. */
    memset(&musable, 0, sizeof (musable));
    if (find_offensive(mtmp, &musable)) {
        int foo = use_offensive(mtmp, &musable);

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
            if (!range2 &&
                (!MON_WEP(mtmp) || mtmp->mconf || Conflict ||
                 !touch_petrifies(youmonst.data))) {
                if (tmp > (j = rnd(20 + i))) {
                    if (mattk->aatyp != AT_KICK ||
                        !thick_skinned(youmonst.data))
                        sum[i] = hitmu(mtmp, mattk);
                } else
                    missmu(mtmp, (tmp == j), mattk);
            }
            break;

        case AT_HUGS:  /* automatic if prev two attacks succeed */
            if ((!range2 && i >= 2 && sum[i - 1] && sum[i - 2])
                || mtmp == u.ustuck)
                sum[i] = hitmu(mtmp, mattk);
            break;

        case AT_GAZE:  /* can affect you either ranged or not */
            /* Medusa gaze already operated through m_respond in dochug();
               don't gaze more than once per round. */
            if (mdat != &mons[PM_MEDUSA])
                sum[i] = gazemu(mtmp, mattk);
            break;

        case AT_EXPL:
            /* explmu does hit calculations, but we have to check range */
            if (!range2)
                sum[i] = explmu(mtmp, mattk);
            break;

        case AT_ENGL:
            if (!range2) {
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
            if (range2)
                sum[i] = breamq(mtmp, u.ux, u.uy, mattk);
            break;
        case AT_SPIT:
            if (range2)
                sum[i] = spitmq(mtmp, u.ux, u.uy, mattk);
            break;
        case AT_WEAP:
            if (range2) {
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
                    hittmp = hitval(otmp, &youmonst);
                    tmp += hittmp;
                    mswings(mtmp, otmp);
                }
                if (tmp > (j = dieroll = rnd(20 + i)))
                    sum[i] = hitmu(mtmp, mattk);
                else
                    missmu(mtmp, (tmp == j), mattk);
                /* KMH -- Don't accumulate to-hit bonuses */
                if (otmp)
                    tmp -= hittmp;
            }
            break;
        case AT_MAGC:
            if (range2)
                sum[i] = buzzmu(mtmp, mattk);
            else
                sum[i] = castmu(mtmp, mattk, 1);
            break;

        default:       /* no attack */
            break;
        }
        bot();
        /* give player a chance of waking up before dying -kaa */
        if (sum[i] == 1) {      /* successful attack */
            if (u_helpless(hm_asleep) &&
                turnstate.helpless_timers[hr_asleep] < moves && !rn2(10))
                cancel_helplessness(hm_asleep,
                                    "The combat suddenly awakens you.");
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
diseasemu(const struct permonst *mdat)
{
    if (Sick_resistance) {
        pline("You feel a slight illness.");
        return FALSE;
    } else {
        make_sick(Sick ? Sick / 3L + 1L :
                  20 + rn2_on_rng(ACURR(A_CON), rng_ddeath_dconp20),
                  mdat->mname, TRUE, SICK_NONVOMITABLE);
        return TRUE;
    }
}

/* check whether slippery clothing protects from hug or wrap attack */
static boolean
u_slip_free(struct monst *mtmp, const struct attack *mattk)
{
    struct obj *obj = (uarmc ? uarmc : uarm);

    if (!obj)
        obj = uarmu;
    if (mattk->adtyp == AD_DRIN)
        obj = uarmh;

    /* if your cloak/armor is greased, monster slips off; this protection might
       fail (33% chance) when the armor is cursed */
    if (obj && (obj->greased || obj->otyp == OILSKIN_CLOAK) &&
        (!obj->cursed || rn2(3))) {
        pline("%s %s your %s %s!", Monnam(mtmp),
              (mattk->adtyp ==
               AD_WRAP) ? "slips off of" : "grabs you, but cannot hold onto",
              obj->greased ? "greased" : "slippery",
              /* avoid "slippery slippery cloak" for undiscovered oilskin cloak
               */
              (obj->greased ||
               objects[obj->otyp].
               oc_name_known) ? xname(obj) : cloak_simple_name(obj));

        if (obj->greased && !rn2(2)) {
            pline("The grease wears off.");
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
hitmu(struct monst *mtmp, const struct attack *mattk)
{
    const struct permonst *mdat = mtmp->data;
    int uncancelled, ptmp;
    int dmg, armpro, permdmg;
    const struct permonst *olduasmon = youmonst.data;
    int res;
    struct attack noseduce;

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

                pline("%s was hidden under %s!", Amonnam(mtmp), what);
            }
            newsym(mtmp->mx, mtmp->my);
        }
    }

    /* First determine the base damage done */
    dmg = dice((int)mattk->damn, (int)mattk->damd);
    if (is_undead(mdat) && midnight())
        dmg += dice((int)mattk->damn, (int)mattk->damd); /* extra damage */

    /* Next a cancellation factor. Use uncancelled when the cancellation factor
       takes into account certain armor's special magic protection.  Otherwise just
       use !mtmp->mcan. */
    armpro = magic_negation(&youmonst);
    uncancelled = !mtmp->mcan && ((rn2(3) >= armpro) || !rn2(50));

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
                    pline("%s grabs you!", Monnam(mtmp));
                }
            } else if (u.ustuck == mtmp) {
                exercise(A_STR, FALSE);
                pline("You are being %s.", (mtmp->data == &mons[PM_ROPE_GOLEM])
                      ? "choked" : "crushed");
            }
        } else {        /* hand to hand weapon */
            if (mattk->aatyp == AT_WEAP && otmp) {
                if (otmp->otyp == CORPSE &&
                    touch_petrifies(&mons[otmp->corpsenm])) {
                    dmg = 1;
                    pline("%s hits you with the %s corpse.", Monnam(mtmp),
                          mons[otmp->corpsenm].mname);
                    if (!Stoned)
                        goto do_stone;
                }
                dmg += dmgval(otmp, &youmonst);
                if (objects[otmp->otyp].oc_material == SILVER &&
                    hates_silver(youmonst.data))
                    pline("The silver sears your flesh!");

                if (dmg <= 0)
                    dmg = 1;
                if (!
                    (otmp->oartifact &&
                     artifact_hit(mtmp, &youmonst, otmp, &dmg, dieroll)))
                    hitmsg(mtmp, mattk);
                if (!dmg)
                    break;
                if (u.mh > 1 && u.mh > ((get_player_ac() > 0) ?
                                            dmg : dmg + get_player_ac()) &&
                    objects[otmp->otyp].oc_material == IRON &&
                    (u.umonnum == PM_BLACK_PUDDING ||
                     u.umonnum == PM_BROWN_PUDDING)) {
                    /* This redundancy necessary because you have to take the
                       damage _before_ being cloned. */
                    if (get_player_ac() < 0)
                        dmg += get_player_ac();
                    if (dmg < 1)
                        dmg = 1;
                    if (dmg > 1)
                        exercise(A_STR, FALSE);
                    u.mh -= dmg;
                    dmg = 0;
                    if (cloneu())
                        pline("You divide as %s hits you!", mon_nam(mtmp));
                }
                mrustm(mtmp, &youmonst, otmp);
            } else if (mattk->aatyp != AT_TUCH || dmg != 0 || mtmp != u.ustuck)
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_DISE:
        hitmsg(mtmp, mattk);
        if (!diseasemu(mdat))
            dmg = 0;
        break;
    case AD_FIRE:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("You're %s!", on_fire(youmonst.data, mattk));
            if (youmonst.data == &mons[PM_STRAW_GOLEM] ||
                youmonst.data == &mons[PM_PAPER_GOLEM]) {
                pline("You roast!");
                rehumanize(BURNING, msgcat("roasted to death by ",
                                           k_monnam(mtmp)));
                break;
            } else if (Fire_resistance) {
                pline("The fire doesn't feel hot!");
                dmg = 0;
            }
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(SCROLL_CLASS, AD_FIRE);
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_FIRE);
            if ((int)mtmp->m_lev > rn2(25))
                destroy_item(SPBOOK_CLASS, AD_FIRE);
            burn_away_slime();
        } else
            dmg = 0;
        break;
    case AD_COLD:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("You're covered in frost!");
            if (Cold_resistance) {
                pline("The frost doesn't seem cold!");
                dmg = 0;
            }
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_COLD);
        } else
            dmg = 0;
        break;
    case AD_ELEC:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("You get zapped!");
            if (Shock_resistance) {
                pline("The zap doesn't shock you!");
                dmg = 0;
            }
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(WAND_CLASS, AD_ELEC);
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(RING_CLASS, AD_ELEC);
        } else
            dmg = 0;
        break;
    case AD_SLEE:
        hitmsg(mtmp, mattk);
        if (uncancelled && !u_helpless(hm_all) && !rn2(5)) {
            if (Sleep_resistance)
                break;
            helpless(rnd(10), hr_asleep, "sleeping", NULL);
            if (Blind)
                pline("You are put to sleep!");
            else
                pline("You are put to sleep by %s!", mon_nam(mtmp));
        }
        break;
    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, NULL)) {
            if (!Blind)
                pline("%s blinds you!", Monnam(mtmp));
            make_blinded(Blinded + (long)dmg, FALSE);
            if (!Blind)
                pline("Your vision quickly clears.");
        }
        dmg = 0;
        break;
    case AD_DRST:
        ptmp = A_STR;
        goto dopois;
    case AD_DRDX:
        ptmp = A_DEX;
        goto dopois;
    case AD_DRCO:
        ptmp = A_CON;
    dopois:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(8)) {
            poisoned(msgprintf("%s %s", s_suffix(Monnam(mtmp)),
                               mpoisons_subj(mtmp, mattk)),
                     ptmp, killer_msg_mon(POISONING, mtmp), 30);
        }
        break;
    case AD_DRIN:
        hitmsg(mtmp, mattk);
        if (defends(AD_DRIN, uwep) || !has_head(youmonst.data)) {
            pline("You don't seem harmed.");
            /* Not clear what to do for green slimes */
            break;
        }
        if (u_slip_free(mtmp, mattk))
            break;

        if (uarmh && rn2(8)) {
            /* not body_part(HEAD) */
            pline("Your %s blocks the attack to your head.",
                  helmet_name(uarmh));
            break;
        }
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;
        mdamageu(mtmp, dmg);

        if (!uarmh || uarmh->otyp != DUNCE_CAP) {
            pline("Your brain is eaten!");
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
                            pline("You feel like a scarecrow.");
                            break;
                        }
                    }

                    if (lifesaved)
                        pline("Unfortunately your brain is still gone.");
                    else
                        pline("Your last thought fades away.");
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
                pline("You momentarily stiffen.");
            } else {
                if (Blind)
                    pline("You are frozen!");
                else
                    pline("You are frozen by %s!", mon_nam(mtmp));
                helpless(10, hr_paralyzed, "paralyzed by a monster's touch",
                         NULL);
                exercise(A_DEX, FALSE);
            }
        }
        break;
    case AD_DRLI:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(3) && !Drain_resistance) {
            losexp(msgcat("drained of life by ", k_monnam(mtmp)), FALSE);
        }
        break;
    case AD_LEGS:
        {
            long side = rn2(2) ? RIGHT_SIDE : LEFT_SIDE;
            const char *sidestr = (side == RIGHT_SIDE) ? "right" : "left";

            /* This case is too obvious to ignore, but Nethack is not in
               general very good at considering height--most short monsters
               still _can_ attack you when you're flying or mounted. [FIXME:
               why can't a flying attacker overcome this?] */
            if (u.usteed || Levitation || Flying) {
                pline("%s tries to reach your %s %s!", Monnam(mtmp), sidestr,
                      body_part(LEG));
                dmg = 0;
            } else if (mtmp->mcan) {
                pline("%s nuzzles against your %s %s!", Monnam(mtmp), sidestr,
                      body_part(LEG));
                dmg = 0;
            } else {
                if (uarmf) {
                    if (rn2(2) &&
                        (uarmf->otyp == LOW_BOOTS || uarmf->otyp == IRON_SHOES))
                        pline("%s pricks the exposed part of your %s %s!",
                              Monnam(mtmp), sidestr, body_part(LEG));
                    else if (!rn2(5))
                        pline("%s pricks through your %s boot!", Monnam(mtmp),
                              sidestr);
                    else {
                        pline("%s scratches your %s boot!", Monnam(mtmp),
                              sidestr);
                        dmg = 0;
                        break;
                    }
                } else
                    pline("%s pricks your %s %s!", Monnam(mtmp), sidestr,
                          body_part(LEG));
                set_wounded_legs(side, rnd(60 - ACURR(A_DEX)));
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
            if (mtmp->mcan)
                You_hear("a cough from %s!", mon_nam(mtmp));
            else {
                You_hear("%s hissing!", s_suffix(mon_nam(mtmp)));
                if (stiffen ||
                    (flags.moonphase == NEW_MOON && !have_lizard())) {
                do_stone:
                    if (!Stoned && !Stone_resistance &&
                        !(poly_when_stoned(youmonst.data) &&
                          polymon(PM_STONE_GOLEM, TRUE))) {
                        Stoned = 5;
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
        if ((!mtmp->mcan || u.ustuck == mtmp) && !sticks(youmonst.data)) {
            boolean drownable = is_pool(level, mtmp->mx, mtmp->my) &&
                !Swimming && !Amphibious;
            if (!u.ustuck &&
                !rn2_on_rng(10, drownable ? rng_eel_drowning : rng_main)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    pline("%s swings itself around you!", Monnam(mtmp));
                    u.ustuck = mtmp;
                }
            } else if (u.ustuck == mtmp) {
                if (drownable) {
                    pline("%s drowns you...", Monnam(mtmp));
                    done(DROWNING,
                         killer_msg(DROWNING,
                                    msgprintf("%s by %s",
                                              Is_waterlevel(&u.uz)
                                                  ? "the Plane of Water"
                                                  : a_waterbody(mtmp->mx,
                                                                mtmp->my),
                                              an(mtmp->data->mname))));
                } else if (mattk->aatyp == AT_HUGS)
                    pline("You are being crushed.");
            } else {
                dmg = 0;
                if (flags.verbose)
                    pline("%s brushes against your %s.", Monnam(mtmp),
                          body_part(LEG));
            }
        } else
            dmg = 0;
        break;
    case AD_WERE:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4) && u.ulycn == NON_PM &&
            !Protection_from_shape_changers && !defends(AD_WERE, uwep)) {
            pline("You feel feverish.");
            exercise(A_CON, FALSE);
            u.ulycn = monsndx(mdat);
        }
        break;
    case AD_SGLD:
        hitmsg(mtmp, mattk);
        if (youmonst.data->mlet == mdat->mlet)
            break;
        if (!mtmp->mcan)
            stealgold(mtmp);
        break;

    case AD_SITM:      /* for now these are the same */
    case AD_SEDU:
        if (is_animal(mtmp->data)) {
            hitmsg(mtmp, mattk);
            if (mtmp->mcan)
                break;
            /* Continue below */
        } else if (dmgtype(youmonst.data, AD_SEDU)
                   || dmgtype(youmonst.data, AD_SSEX)
            ) {
            pline("%s %s.", Monnam(mtmp),
                  mtmp->minvent ?
                  "brags about the goods some dungeon explorer provided" :
                  "makes some remarks about how difficult theft is lately");
            if (!tele_restrict(mtmp))
                rloc(mtmp, TRUE);
            return 3;
        } else if (mtmp->mcan) {
            if (!Blind) {
                pline("%s tries to %s you, but you seem %s.",
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
                        pline("%s tries to %s away with %s.", Monnam(mtmp),
                              locomotion(mtmp->data, "run"), buf);
                }
                monflee(mtmp, 0, FALSE, FALSE);
                return 3;
            }
        }
        break;

    case AD_SSEX:
        if (could_seduce(mtmp, &youmonst, mattk) == 1 && !mtmp->mcan)
            if (doseduce(mtmp))
                return 3;
        break;

    case AD_SAMU:
        hitmsg(mtmp, mattk);
        /* when the Wiz hits, 1/20 steals the amulet */
        if (Uhave_amulet || Uhave_bell || Uhave_book || Uhave_menorah ||
            Uhave_questart)  /* carrying the Quest Artifact */
            if (!rn2(20))
                stealamulet(mtmp);
        break;

    case AD_TLPT:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            if (flags.verbose)
                pline("Your position suddenly seems very uncertain!");
            tele();
        }
        break;
    case AD_RUST:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        if (u.umonnum == PM_IRON_GOLEM) {
            pline("You rust!");
            rehumanize(DIED, msgcat("rusted to death by ", k_monnam(mtmp)));
            break;
        }
        hurtarmor(&youmonst, ERODE_RUST);
        break;
    case AD_CORR:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        hurtarmor(&youmonst, ERODE_CORRODE);
        break;
    case AD_DCAY:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        if (u.umonnum == PM_WOOD_GOLEM || u.umonnum == PM_LEATHER_GOLEM) {
            pline("You rot!");
            rehumanize(DIED, msgcat("rotted to death by ", k_monnam(mtmp)));
            break;
        }
        hurtarmor(&youmonst, ERODE_ROT);
        break;
    case AD_HEAL:
        /* a cancelled nurse is just an ordinary monster */
        if (mtmp->mcan) {
            hitmsg(mtmp, mattk);
            break;
        }
        /* this condition must match the one in sounds.c for MS_NURSE */
        if (!(uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep))) &&
            !uarmu && (!uarm || uskin()) && !uarmh && !uarms && !uarmg &&
            !uarmc && !uarmf) {
            boolean goaway = FALSE;

            pline("%s hits!  (I hope you don't mind.)", Monnam(mtmp));
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
                    if (u.uhpmax < 5 * u.ulevel + dice(2 * u.ulevel, 10))
                        u.uhpmax++;
                    if (!rn2(13))
                        goaway = TRUE;
                }
                if (u.uhp > u.uhpmax)
                    u.uhp = u.uhpmax;
            }
            if (!rn2(3))
                exercise(A_STR, TRUE);
            if (!rn2(3))
                exercise(A_CON, TRUE);
            if (Sick)
                make_sick(0L, NULL, FALSE, SICK_ALL);
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
                    verbalize("Doc, I can't help you unless you cooperate.");
                dmg = 0;
            } else
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_CURS:
        hitmsg(mtmp, mattk);
        if (!night() && mdat == &mons[PM_GREMLIN])
            break;
        if (!mtmp->mcan && !rn2(10)) {
            if (canhear()) {
                if (Blind)
                    You_hear("laughter.");
                else
                    pline("%s chuckles.", Monnam(mtmp));
            }
            if (u.umonnum == PM_CLAY_GOLEM) {
                pline("Some writing vanishes from your head!");
                rehumanize(DIED, msgcat("deactivated by ", k_monnam(mtmp)));
                break;
            }
            attrcurse();
        }
        break;
    case AD_STUN:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4)) {
            make_stunned(HStun + dmg, TRUE);
            dmg /= 2;
        }
        break;
    case AD_ACID:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(3))
            if (Acid_resistance) {
                pline("You're covered in acid, but it seems harmless.");
                dmg = 0;
            } else {
                pline("You're covered in acid! It burns!");
                exercise(A_STR, FALSE);
        } else
            dmg = 0;
        break;
    case AD_SLOW:
        hitmsg(mtmp, mattk);
        if (uncancelled && HFast && !defends(AD_SLOW, uwep) && !rn2(4))
            u_slow_down();
        break;
    case AD_DREN:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4))
            drain_en(dmg);
        dmg = 0;
        break;
    case AD_CONF:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4) && !mtmp->mspec_used) {
            mtmp->mspec_used = mtmp->mspec_used + (dmg + rn2(6));
            if (Confusion)
                pline("You are getting even more confused.");
            else
                pline("You are getting confused.");
            make_confused(HConfusion + dmg, FALSE);
        }
        dmg = 0;
        break;
    case AD_DETH:
        pline("%s reaches out with its deadly touch.", Monnam(mtmp));
        if (is_undead(youmonst.data)) {
            /* Still does normal damage */
            pline("Was that the touch of death?");
            break;
        }
        switch (rn2_on_rng(20, rng_deathtouch)) {
        case 19:
        case 18:
        case 17:
            if (!Antimagic) {
                done(DIED, killer_msg(DIED, "the touch of Death"));
                dmg = 0;
                break;
            }   /* else FALLTHRU */
        default:       /* case 16: ... case 5: */
            pline("You feel your life force draining away...");
            permdmg = 1;        /* actual damage done below */
            break;
        case 4:
        case 3:
        case 2:
        case 1:
        case 0:
            if (Antimagic)
                shieldeff(u.ux, u.uy);
            pline("Lucky for you, it didn't work!");
            dmg = 0;
            break;
        }
        break;
    case AD_PEST:
        pline("%s reaches out, and you feel fever and chills.", Monnam(mtmp));
        diseasemu(mdat);        /* plus the normal damage */
        break;
    case AD_FAMN:
        pline("%s reaches out, and your body shrivels.", Monnam(mtmp));
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
            pline("The slime burns away!");
            dmg = 0;
        } else if (Unchanging || unsolid(youmonst.data) ||
                   youmonst.data == &mons[PM_GREEN_SLIME]) {
            pline("You are unaffected.");
            dmg = 0;
        } else if (!Slimed) {
            pline("You don't feel very well.");
            Slimed = 10L;
            set_delayed_killer(TURNED_SLIME,
                               killer_msg_mon(TURNED_SLIME, mtmp));
        } else
            pline("Yuck!");
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        hitmsg(mtmp, mattk);
        /* uncancelled is sufficient enough; please don't make this attack less
           frequent */
        if (uncancelled) {
            struct obj *obj = some_armor(&youmonst);

            if (drain_item(obj)) {
                pline("Your %s less effective.", aobjnam(obj, "seem"));
            }
        }
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
    if (dmg && get_player_ac() < 0) {
        dmg -= rnd(-get_player_ac());
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
        remove_monster(level, mtmp->mx, mtmp->my);
        mtmp->mtrapped = 0;     /* no longer on old trap */
        place_monster(mtmp, u.ux, u.uy);
        u.ustuck = mtmp;
        newsym(mtmp->mx, mtmp->my);

        if (is_animal(mtmp->data) && u.usteed) {
            /* Too many quirks presently if hero and steed are swallowed.
               Pretend purple worms don't like horses for now :-) */
            pline("%s lunges forward and plucks you off %s!",
                  Monnam(mtmp), mon_nam(u.usteed));
            dismount_steed(DISMOUNT_ENGULFED);
        } else
            pline("%s engulfs you!", Monnam(mtmp));
        action_interrupted();
        reset_occupations(TRUE);    /* behave as if you had moved */

        if (u.utrap) {
            pline("You are released from the %s!",
                  u.utraptype == TT_WEB ? "web" : "trap");
            u.utrap = 0;
        }

        i = number_leashed();
        if (i > 0) {
            const char *s = (i > 1) ? "leashes" : "leash";

            pline("The %s %s loose.", s, vtense(s, "snap"));
            unleash_all();
        }

        if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
            expels(mtmp, mtmp->data, FALSE);
            remove_monster(level, mtmp->mx, mtmp->my);
            place_monster(mtmp, u.ux, u.uy);
            if (Punished)
                placebc();
            minstapetrify(mtmp, TRUE);
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
        tim_tmp += -get_player_ac() + 10;
        u.uswldtim = (unsigned)((tim_tmp < 2) ? 2 : tim_tmp);
        swallowed(1);
        for (otmp2 = invent; otmp2; otmp2 = otmp2->nobj)
            snuff_lit(otmp2);
    }

    if (mtmp != u.ustuck)
        return 0;
    if (u.uswldtim > 0)
        u.uswldtim -= 1;

    switch (mattk->adtyp) {

    case AD_DGST:
        if (Slow_digestion) {
            /* Messages are handled below */
            u.uswldtim = 0;
            tmp = 0;
        } else if (u.uswldtim == 0) {
            pline("%s totally digests you!", Monnam(mtmp));
            tmp = u.uhp;
            if (Half_physical_damage)
                tmp *= 2;       /* sorry */
        } else {
            pline("%s%s digests you!", Monnam(mtmp),
                  (u.uswldtim == 2) ? " thoroughly" :
                  (u.uswldtim == 1) ? " utterly" : "");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_PHYS:
        if (mtmp->data == &mons[PM_FOG_CLOUD]) {
            pline("You are laden with moisture and %s",
                  flaming(youmonst.data) ? "are smoldering out!" :
                  Breathless ? "find it mildly uncomfortable." :
                  amphibious(youmonst.data) ? "feel comforted." :
                  "can barely breathe!");
            /* NB: Amphibious includes Breathless */
            if (Amphibious && !flaming(youmonst.data))
                tmp = 0;
        } else {
            pline("You are pummeled with debris!");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_ACID:
        if (Acid_resistance) {
            pline("You are covered with a seemingly harmless goo.");
            tmp = 0;
        } else {
            if (Hallucination)
                pline("Ouch!  You've been slimed!");
            else
                pline("You are covered in slime!  It burns!");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, NULL)) {
            if (!Blind) {
                pline("You can't see in here!");
                make_blinded((long)tmp, FALSE);
                if (!Blind)
                    pline("Your vision quickly clears.");
            } else
                /* keep him blind until disgorged */
                make_blinded(Blinded + 1, FALSE);
        }
        tmp = 0;
        break;
    case AD_ELEC:
        if (!mtmp->mcan && rn2(2)) {
            pline("The air around you crackles with electricity.");
            if (Shock_resistance) {
                shieldeff(u.ux, u.uy);
                pline("You seem unhurt.");
                ugolemeffects(AD_ELEC, tmp);
                tmp = 0;
            }
        } else
            tmp = 0;
        break;
    case AD_COLD:
        if (!mtmp->mcan && rn2(2)) {
            if (Cold_resistance) {
                shieldeff(u.ux, u.uy);
                pline("You feel mildly chilly.");
                ugolemeffects(AD_COLD, tmp);
                tmp = 0;
            } else
                pline("You are freezing to death!");
        } else
            tmp = 0;
        break;
    case AD_FIRE:
        if (!mtmp->mcan && rn2(2)) {
            if (Fire_resistance) {
                shieldeff(u.ux, u.uy);
                pline("You feel mildly hot.");
                ugolemeffects(AD_FIRE, tmp);
                tmp = 0;
            } else
                pline("You are burning to a crisp!");
            burn_away_slime();
        } else
            tmp = 0;
        break;
    case AD_DISE:
        if (!diseasemu(mtmp->data))
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
        pline("%s very hurriedly %s you!", Monnam(mtmp),
              is_animal(mtmp->data) ? "regurgitates" : "expels");
        expels(mtmp, mtmp->data, FALSE);
    } else if (!u.uswldtim || youmonst.data->msize >= MZ_HUGE) {
        pline("You get %s!",
              is_animal(mtmp->data) ? "regurgitated" : "expelled");
        if (flags.verbose &&
            (is_animal(mtmp->data) ||
             (dmgtype(mtmp->data, AD_DGST) && Slow_digestion)))
            pline("Obviously %s doesn't like your taste.", mon_nam(mtmp));
        expels(mtmp, mtmp->data, FALSE);
    }
    return 1;
}

/* monster explodes in your face */
static int
explmu(struct monst *mtmp, const struct attack *mattk)
{
    if (mtmp->mcan)
        return 0;

    int tmp = dice((int)mattk->damn, (int)mattk->damd);
    boolean not_affected = defends((int)mattk->adtyp, uwep);

    hitmsg(mtmp, mattk);
    remove_monster(level, mtmp->mx, mtmp->my);
    newsym(mtmp->mx, mtmp->my);

    switch (mattk->adtyp) {
    case AD_COLD:
        not_affected |= Cold_resistance;
        goto common;
    case AD_FIRE:
        not_affected |= Fire_resistance;
        goto common;
    case AD_ELEC:
        not_affected |= Shock_resistance;
    common:

        if (!not_affected) {
            if (ACURR(A_DEX) > rnd(20)) {
                pline("You duck some of the blast.");
                tmp = (tmp + 1) / 2;
            } else {
                if (flags.verbose)
                    pline("You get blasted!");
            }
            if (mattk->adtyp == AD_FIRE)
                burn_away_slime();
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
                pline("You are blinded by a blast of light!");
                make_blinded((long)tmp, FALSE);
                if (!Blind)
                    pline("Your vision quickly clears.");
            } else if (flags.verbose)
                pline("You get the impression it was not terribly bright.");
        }
        break;

    case AD_HALU:
        not_affected |= Blind || (u.umonnum == PM_BLACK_LIGHT ||
                                  u.umonnum == PM_VIOLET_FUNGUS ||
                                  dmgtype(youmonst.data, AD_STUN));
        if (!not_affected) {
            boolean chg;

            if (!Hallucination)
                pline("You are caught in a blast of kaleidoscopic light!");
            chg = make_hallucinated(HHallucination + (long)tmp, FALSE);
            pline("You %s.", chg ? "are freaked out" : "seem unaffected");
        }
        break;

    default:
        break;
    }
    if (not_affected) {
        pline("You seem unaffected by it.");
        ugolemeffects((int)mattk->adtyp, tmp);
    }

    place_monster(mtmp, mtmp->mx, mtmp->my);
    mondead(mtmp);
    wake_nearto(mtmp->mx, mtmp->my, 7 * 7);
    if (!DEADMONSTER(mtmp))
        return 0;
    return 2;   /* it dies */
}


/* monster gazes at you */
int
gazemu(struct monst *mtmp, const struct attack *mattk)
{
    switch (mattk->adtyp) {
    case AD_STON:
        if (mtmp->mcan || !mtmp->mcansee) {
            if (!canseemon(mtmp))
                break;  /* silently */
            pline("%s %s.", Monnam(mtmp),
                  (mtmp->data == &mons[PM_MEDUSA] &&
                   mtmp->mcan) ? "doesn't look all that ugly" :
                  "gazes ineffectually");
            break;
        }
        if (Reflecting && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->data == &mons[PM_MEDUSA]) {
            /* hero has line of sight to Medusa and she's not blind */
            boolean useeit = canseemon(mtmp);

            if (useeit)
                ureflects("%s gaze is reflected by your %s.",
                          s_suffix(Monnam(mtmp)));
            if (mon_reflects
                (mtmp, !useeit ? NULL : "The gaze is reflected away by %s %s!"))
                break;
            if (!m_canseeu(mtmp)) {     /* probably you're invisible */
                if (useeit)
                    pline("%s doesn't seem to notice that %s gaze was "
                          "reflected.", Monnam(mtmp), mhis(mtmp));
                break;
            }
            if (useeit)
                pline("%s is turned to stone!", Monnam(mtmp));
            stoned = TRUE;
            killed(mtmp);

            if (!DEADMONSTER(mtmp))
                break;
            return 2;
        }
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            !Stone_resistance) {
            pline("You meet %s gaze.", s_suffix(mon_nam(mtmp)));
            action_interrupted();
            instapetrify(killer_msg(
                             STONING, msgprintf(
                                 "catching the eye of %s", k_monnam(mtmp))));
        }
        break;
    case AD_CONF:
        if (!mtmp->mcan && canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            int conf = dice(3, 4);

            mtmp->mspec_used = mtmp->mspec_used + (conf + rn2(6));
            if (!Confusion)
                pline("%s gaze confuses you!", s_suffix(Monnam(mtmp)));
            else
                pline("You are getting more and more confused.");
            make_confused(HConfusion + conf, FALSE);
            action_interrupted();
        }
        break;
    case AD_STUN:
        if (!mtmp->mcan && canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            int stun = dice(2, 6);

            mtmp->mspec_used = mtmp->mspec_used + (stun + rn2(6));
            pline("%s stares piercingly at you!", Monnam(mtmp));
            make_stunned(HStun + stun, TRUE);
            action_interrupted();
        }
        break;
    case AD_BLND:
        if (!mtmp->mcan && canseemon(mtmp) && !resists_blnd(&youmonst)
            && distu(mtmp->mx, mtmp->my) <= BOLT_LIM * BOLT_LIM) {
            int blnd = dice((int)mattk->damn, (int)mattk->damd);

            pline("You are blinded by %s radiance!", s_suffix(mon_nam(mtmp)));
            make_blinded((long)blnd, FALSE);
            action_interrupted();
            /* not blind at this point implies you're wearing the Eyes of the
               Overworld; make them block this particular stun attack too */
            if (!Blind)
                pline("Your vision quickly clears.");
            else
                make_stunned((long)dice(1, 3), TRUE);
        }
        break;
    case AD_FIRE:
        if (!mtmp->mcan && canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            int dmg = dice(2, 6);

            pline("%s attacks you with a fiery gaze!", Monnam(mtmp));
            action_interrupted();
            if (Fire_resistance) {
                pline("The fire doesn't feel hot!");
                dmg = 0;
            }
            burn_away_slime();
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(SCROLL_CLASS, AD_FIRE);
            if ((int)mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_FIRE);
            if ((int)mtmp->m_lev > rn2(25))
                destroy_item(SPBOOK_CLASS, AD_FIRE);
            if (dmg)
                mdamageu(mtmp, dmg);
        }
        break;
#ifdef PM_BEHOLDER      /* work in progress */
    case AD_SLEE:
        if (!mtmp->mcan && canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !u_helpless(hm_all) && !rn2(5) &&
            !Sleep_resistance) {
            helpless(rnd(10), hr_asleep, "sleeping", NULL);
            pline("%s gaze makes you very sleepy...", s_suffix(Monnam(mtmp)));
        }
        break;
    case AD_SLOW:
        if (!mtmp->mcan && canseemon(mtmp) && mtmp->mcansee &&
            (HFast & (INTRINSIC | TIMEOUT)) && !defends(AD_SLOW, uwep) &&
            !rn2(4))

            u_slow_down();
        action_interrupted();
        break;
#endif
    default:
        impossible("Gaze attack %d?", mattk->adtyp);
        break;
    }
    return 0;
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
        agrinvis = magr->minvis;
        genagr = gender(magr);
    }
    if (mdef == &youmonst) {
        defperc = (See_invisible != 0);
        gendef = poly_gender();
    } else {
        defperc = perceives(mdef->data);
        gendef = gender(mdef);
    }

    if (agrinvis && !defperc && mattk && mattk->adtyp != AD_SSEX)
        return 0;

    if (pagr->mlet != S_NYMPH &&
        ((pagr != &mons[PM_INCUBUS] && pagr != &mons[PM_SUCCUBUS])
         || (mattk && mattk->adtyp != AD_SSEX)))
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
    boolean fem = (mon->data == &mons[PM_SUCCUBUS]);    /* otherwise incubus */
    const char *qbuf;

    if (mon->mcan || mon->mspec_used) {
        pline("%s acts as though %s has got a %sheadache.", Monnam(mon),
              mhe(mon), mon->mcan ? "severe " : "");
        return 0;
    }

    if (u_helpless(hm_all)) {
        pline("%s seems dismayed at your lack of response.", Monnam(mon));
        return 0;
    }

    if (Blind)
        pline("It caresses you...");
    else
        pline("You feel very attracted to %s.", mon_nam(mon));

    for (ring = invent; ring; ring = nring) {
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
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n')
                    continue;
            } else
                pline("%s decides she'd like your %s, and takes it.",
                      Blind ? "She" : Monnam(mon), xname(ring));
            makeknown(RIN_ADORNMENT);
            unwield_silently(ring);
            setunequip(ring);
            freeinv(ring);
            mpickobj(mon, ring);
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
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n')
                    continue;
            } else {
                pline("%s decides you'd look prettier wearing your %s,",
                      Blind ? "He" : Monnam(mon), xname(ring));
                pline("and puts it on your finger.");
            }
            makeknown(RIN_ADORNMENT);
            if (!uright) {
                pline("%s puts %s on your right %s.",
                      Blind ? "He" : Monnam(mon), the(xname(ring)),
                      body_part(HAND));
                slot = os_ringr;
            } else if (!uleft) {
                pline("%s puts %s on your left %s.", Blind ? "He" : Monnam(mon),
                      the(xname(ring)), body_part(HAND));
                slot = os_ringl;
            } else if (uright && uright->otyp != RIN_ADORNMENT) {
                pline("%s replaces your %s with your %s.",
                      Blind ? "He" : Monnam(mon), xname(uright), xname(ring));
                setunequip(uright);
                slot = os_ringr;
            } else if (uleft && uleft->otyp != RIN_ADORNMENT) {
                pline("%s replaces your %s with your %s.",
                      Blind ? "He" : Monnam(mon), xname(uleft), xname(ring));
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
        pline("%s murmurs sweet nothings into your ear.",
              Blind ? (fem ? "She" : "He") : Monnam(mon));
    else
        pline("%s murmurs in your ear, while helping you undress.",
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
        verbalize("You're such a %s; I wish...",
                  u.ufemale ? "sweet lady" : "nice guy");
        if (!tele_restrict(mon))
            rloc(mon, TRUE);
        return 1;
    }
    if (u.ualign.type == A_CHAOTIC)
        adjalign(1);

    /* by this point you have discovered mon's identity, blind or not... */
    pline("Time stands still while you and %s lie in each other's arms...",
          noit_mon_nam(mon));
    if (rn2_on_rng(35, rng_foocubus_results) > ACURR(A_CHA) + ACURR(A_INT)) {
        /* Don't bother with mspec_used here... it didn't get tired! */
        pline("%s seems to have enjoyed it more than you...", noit_Monnam(mon));
        switch (rn2_on_rng(5, rng_foocubus_results)) {
        case 0:
            pline("You feel drained of energy.");
            u.uen = 0;
            u.uenmax -= rnd(Half_physical_damage ? 5 : 10);
            exercise(A_CON, FALSE);
            if (u.uenmax < 0)
                u.uenmax = 0;
            break;
        case 1:
            pline("You are down in the dumps.");
            adjattrib(A_CON, -1, TRUE);
            exercise(A_CON, FALSE);
            break;
        case 2:
            pline("Your senses are dulled.");
            adjattrib(A_WIS, -1, TRUE);
            exercise(A_WIS, FALSE);
            break;
        case 3:
            if (!resists_drli(&youmonst)) {
                pline("You feel out of shape.");
                losexp(killer_msg(DIED, "overexertion"), FALSE);
            } else {
                pline("You have a curious feeling...");
            }
            break;
        case 4:{
                int tmp;

                pline("You feel exhausted.");
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
        pline("You seem to have enjoyed it more than %s...", noit_mon_nam(mon));
        switch (rn2_on_rng(5, rng_foocubus_results)) {
        case 0:
            pline("You feel raised to your full potential.");
            exercise(A_CON, TRUE);
            u.uen = (u.uenmax += rnd(5));
            break;
        case 1:
            pline("You feel good enough to do it again.");
            adjattrib(A_CON, 1, TRUE);
            exercise(A_CON, TRUE);
            break;
        case 2:
            pline("You will always remember %s...", noit_mon_nam(mon));
            adjattrib(A_WIS, 1, TRUE);
            exercise(A_WIS, TRUE);
            break;
        case 3:
            pline("That was a very educational experience.");
            pluslvl(FALSE);
            exercise(A_WIS, TRUE);
            break;
        case 4:
            pline("You feel restored to health!");
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
        pline("%s demands that you pay %s, but you refuse...", noit_Monnam(mon),
              Blind ? (fem ? "her" : "him") : mhim(mon));
    } else if (u.umonnum == PM_LEPRECHAUN)
        pline("%s tries to take your money, but fails...", noit_Monnam(mon));
    else {
        long cost;
        long umoney = money_cnt(invent);

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
            verbalize("It's on the house!");
        else {
            pline("%s takes %ld %s for services rendered!", noit_Monnam(mon),
                  cost, currency(cost));
            money2mon(mon, cost);
        }
    }
    if (!rn2_on_rng(25, rng_foocubus_results))
        mon->mcan = 1;  /* monster is worn out */
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
        verbalize("Take off your %s; %s.", str,
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
            pline("%s is splashed by your acid!", Monnam(mtmp));
            if (resists_acid(mtmp)) {
                pline("%s is not affected.", Monnam(mtmp));
                tmp = 0;
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
                pline("%s turns to stone!", Monnam(mtmp));
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
                pline("You explode!");
                u.mh = -1;
                rehumanize(EXPLODED, killer_msg_mon(EXPLODED, mtmp));
                goto assess_dmg;
            }
            break;
        case AD_PLYS:  /* Floating eye */
            if (tmp > 127)
                tmp = 127;
            if (u.umonnum == PM_FLOATING_EYE) {
                if (!rn2(4))
                    tmp = 127;
                if (mtmp->mcansee && haseyes(mtmp->data) && rn2(3) &&
                    (perceives(mtmp->data) || !Invis)) {
                    if (Blind)
                        pline("As a blind %s, you cannot defend yourself.",
                              youmonst.data->mname);
                    else {
                        if (mon_reflects
                            (mtmp, "Your gaze is reflected by %s %s."))
                            return 1;
                        pline("%s is frozen by your gaze!", Monnam(mtmp));
                        mtmp->mcanmove = 0;
                        mtmp->mfrozen = tmp;
                        return 3;
                    }
                }
            } else {    /* gelatinous cube */
                pline("%s is frozen by you.", Monnam(mtmp));
                mtmp->mcanmove = 0;
                mtmp->mfrozen = tmp;
                return 3;
            }
            return 1;
        case AD_COLD:  /* Brown mold or blue jelly */
            if (resists_cold(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is mildly chilly.", Monnam(mtmp));
                golemeffects(mtmp, AD_COLD, tmp);
                tmp = 0;
                break;
            }
            pline("%s is suddenly very cold!", Monnam(mtmp));
            u.mh += tmp / 2;
            if (u.mhmax < u.mh)
                u.mhmax = u.mh;
            if (u.mhmax > ((youmonst.data->mlevel + 1) * 8))
                split_mon(&youmonst, mtmp);
            break;
        case AD_STUN:  /* Yellow mold */
            if (!mtmp->mstun) {
                mtmp->mstun = 1;
                pline("%s %s.", Monnam(mtmp),
                      makeplural(stagger(mtmp->data, "stagger")));
            }
            tmp = 0;
            break;
        case AD_FIRE:  /* Red mold */
            if (resists_fire(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is mildly warm.", Monnam(mtmp));
                golemeffects(mtmp, AD_FIRE, tmp);
                tmp = 0;
                break;
            }
            pline("%s is suddenly very hot!", Monnam(mtmp));
            break;
        case AD_ELEC:
            if (resists_elec(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is slightly tingled.", Monnam(mtmp));
                golemeffects(mtmp, AD_ELEC, tmp);
                tmp = 0;
                break;
            }
            pline("%s is jolted with your electricity!", Monnam(mtmp));
            break;
        default:
            tmp = 0;
            break;
    } else
        tmp = 0;

assess_dmg:
    if ((mtmp->mhp -= tmp) <= 0) {
        pline("%s dies!", Monnam(mtmp));
        xkilled(mtmp, 0);
        if (!DEADMONSTER(mtmp))
            return 1;
        return 2;
    }
    return 1;
}


#include "edog.h"
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
        mon = christen_monst(mon, u.uplname);
        initedog(mon);
        mon->m_lev = youmonst.data->mlevel;
        mon->mhpmax = u.mhmax;
        mon->mhp = u.mh / 2;
        u.mh -= mon->mhp;
    }
    return mon;
}


/*mhitu.c*/
