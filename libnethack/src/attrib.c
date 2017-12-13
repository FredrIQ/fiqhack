/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright 1988, 1989, 1990, 1992, M. Stephenson                */
/* NetHack may be freely redistributed.  See license for details. */

/*  attribute modification routines. */

#include "hack.h"
#include "hungerstatus.h"

/* #define DEBUG *//* uncomment for debugging info */

/* part of the output on gain or loss of attribute */
static
const char *const plusattr[] = {
    "strong", "smart", "wise", "agile", "tough", "charismatic"
}, *const minusattr[] = {
    "weak", "stupid", "foolish", "clumsy", "fragile", "repulsive"
};


struct innate {
    schar ulevel;
    unsigned int *ability;
    const char *gainstr, *losestr;
};

static const struct innate arc_abil[] = {
    {1, &(HStealth), "", ""},
    {1, &(HFast), "", ""},
    {10, &(HSearching), "perceptive", ""},
    {0, 0, 0, 0}
};

static const struct innate bar_abil[] = {
    {1, &(HPoison_resistance), "", ""},
    {7, &(HFast), "quick", "slow"},
    {15, &(HStealth), "stealthy", ""},
    {0, 0, 0, 0}
};

static const struct innate cav_abil[] = {
    {7, &(HFast), "quick", "slow"},
    {15, &(HWarning), "sensitive", ""},
    {0, 0, 0, 0}
};

static const struct innate hea_abil[] = {
    {1, &(HPoison_resistance), "", ""},
    {15, &(HWarning), "sensitive", ""},
    {0, 0, 0, 0}
};

static const struct innate kni_abil[] = {
    {7, &(HFast), "quick", "slow"},
    {0, 0, 0, 0}
};

static const struct innate mon_abil[] = {
    {1, &(HFast), "", ""},
    {1, &(HSleep_resistance), "", ""},
    {1, &(HSee_invisible), "", ""},
    {3, &(HPoison_resistance), "healthy", ""},
    {5, &(HStealth), "stealthy", ""},
    {7, &(HWarning), "sensitive", ""},
    {9, &(HSearching), "perceptive", "unaware"},
    {11, &(HFire_resistance), "cool", "warmer"},
    {13, &(HCold_resistance), "warm", "cooler"},
    {15, &(HShock_resistance), "insulated", "conductive"},
    {17, &(HTeleport_control), "controlled", "uncontrolled"},
    {0, 0, 0, 0}
};

static const struct innate pri_abil[] = {
    {15, &(HWarning), "sensitive", ""},
    {20, &(HFire_resistance), "cool", "warmer"},
    {0, 0, 0, 0}
};

static const struct innate ran_abil[] = {
    {1, &(HSearching), "", ""},
    {7, &(HStealth), "stealthy", ""},
    {15, &(HSee_invisible), "", ""},
    {0, 0, 0, 0}
};

static const struct innate rog_abil[] = {
    {1, &(HStealth), "", ""},
    {10, &(HSearching), "perceptive", ""},
    {0, 0, 0, 0}
};

static const struct innate sam_abil[] = {
    {1, &(HFast), "", ""},
    {15, &(HStealth), "stealthy", ""},
    {0, 0, 0, 0}
};

static const struct innate tou_abil[] = {
    {10, &(HSearching), "perceptive", ""},
    {20, &(HPoison_resistance), "hardy", ""},
    {0, 0, 0, 0}
};

static const struct innate val_abil[] = {
    {1, &(HCold_resistance), "", ""},
    {1, &(HStealth), "", ""},
    {7, &(HFast), "quick", "slow"},
    {0, 0, 0, 0}
};

static const struct innate wiz_abil[] = {
    {15, &(HWarning), "sensitive", ""},
    {17, &(HTeleport_control), "controlled", "uncontrolled"},
    {0, 0, 0, 0}
};

/* Intrinsics conferred by race */
static const struct innate elf_abil[] = {
    {4, &(HSleep_resistance), "awake", "tired"},
    {0, 0, 0, 0}
};

static const struct innate orc_abil[] = {
    {1, &(HPoison_resistance), "", ""},
    {0, 0, 0, 0}
};

static void exerper(void);
static void postadjabil(unsigned int *);

/* adjust an attribute; return TRUE if change is made, FALSE otherwise

   msgflag is positive for no message, zero for message, negative to print a
   message when returning TRUE */
boolean
adjattrib(int ndx, int incr, int msgflg)
{
    if (Fixed_abil || !incr)
        return FALSE;

    if ((ndx == A_INT || ndx == A_WIS)
        && uarmh && uarmh->otyp == DUNCE_CAP) {
        if (msgflg == 0)
            pline(msgc_playerimmune,
                  "Your cap constricts briefly, then relaxes again.");
        return FALSE;
    }

    if (incr > 0) {
        if ((AMAX(ndx) >= ATTRMAX(ndx)) && (ACURR(ndx) >= AMAX(ndx))) {
            if (msgflg == 0 && flags.verbose)
                pline(msgc_playerimmune, "You're already as %s as you can get.",
                      plusattr[ndx]);
            ABASE(ndx) = AMAX(ndx) = ATTRMAX(ndx);      /* just in case */
            return FALSE;
        }

        if (ABASE(ndx) == ATTRMAX(ndx)) {
            if (msgflg == 0 && flags.verbose)
                pline(msgc_playerimmune,
                      "You're as %s as you can be right now.", plusattr[ndx]);
            return FALSE;
        }

        ABASE(ndx) += incr;
        if (ABASE(ndx) > AMAX(ndx)) {
            incr = ABASE(ndx) - AMAX(ndx);
            AMAX(ndx) += incr;
            if (AMAX(ndx) > ATTRMAX(ndx))
                AMAX(ndx) = ATTRMAX(ndx);
            ABASE(ndx) = AMAX(ndx);
        }
    } else {
        if (ABASE(ndx) <= ATTRMIN(ndx)) {
            if (msgflg == 0 && flags.verbose)
                pline(msgc_playerimmune, "You're already as %s as you can get.",
                      minusattr[ndx]);
            ABASE(ndx) = ATTRMIN(ndx);  /* just in case */
            return FALSE;
        }

        if (ABASE(ndx) == ATTRMIN(ndx)) {
            if (msgflg == 0 && flags.verbose)
                pline(msgc_playerimmune,
                      "You're as %s as you can be right now.", minusattr[ndx]);
            return FALSE;
        }

        ABASE(ndx) += incr;
        if (ABASE(ndx) < ATTRMIN(ndx)) {
            incr = ABASE(ndx) - ATTRMIN(ndx);
            ABASE(ndx) = ATTRMIN(ndx);
            AMAX(ndx) += incr;
            if (AMAX(ndx) < ATTRMIN(ndx))
                AMAX(ndx) = ATTRMIN(ndx);
        }
    }
    if (msgflg <= 0)
        pline((incr > 0) ? msgc_intrgain : msgc_intrloss,
              "You feel %s%s!", (incr > 1 || incr < -1) ? "very " : "",
              (incr > 0) ? plusattr[ndx] : minusattr[ndx]);
    if (moves > 1 && (ndx == A_STR || ndx == A_CON))
        encumber_msg();
    return TRUE;
}

void
gainstr(struct obj *otmp, int incr)
{
    int num = 1;
    boolean cursed = otmp && otmp->cursed;
    enum rng rng = cursed ? rng_main : rng_strength_gain;

    if (incr)
        num = incr;
    else {
        boolean gain_is_small = !!rn2_on_rng(4, rng);
        int large_gain_amount = rn2_on_rng(6, rng);
        if (ABASE(A_STR) < 18)
            num = (gain_is_small ? 1 : large_gain_amount + 1);
        else if (ABASE(A_STR) < STR18(85)) {
            num = large_gain_amount + 3;
        }
    }
    adjattrib(A_STR, cursed ? -num : num, TRUE);
}

void
losestr(int num, int how, const char *killer, struct monst *magr)
{       /* may kill you; cause may be poison or monster like 'a' */
    int ustr = ABASE(A_STR) - num;

    while (ustr < 3) {
        ++ustr;
        --num;
        if (Upolyd) {
            u.mh -= 6;
            u.mhmax -= 6;

            if (u.mh <= 0) {
                /* Would normally be only msgc_statusend, but the player will
                   presumably still be in serious nutrition trouble, so treat
                   it as an extra warning of impending doom */
                if (how == STARVING)
                    pline(msgc_fatal,
                          "You can't go on any more like this.");
                rehumanize(how, killer);
            }
        } else {
            u.uhp -= 6;
            u.uhpmax -= 6;

            if (u.uhp <= 0) {
                if (how == STARVING)
                    pline(msgc_fatal_predone,
                          "You die from hunger and exhaustion.");

                if (magr) /* don't give at the same time as STARVING */
                    done_in_by(magr, killer);
                else
                    done(how, killer);
            }
        }
    }
    adjattrib(A_STR, -num, TRUE);
}

void
change_luck(schar n)
{
    u.uluck += n;
    if (u.uluck < 0 && u.uluck < LUCKMIN)
        u.uluck = LUCKMIN;
    if (u.uluck > 0 && u.uluck > LUCKMAX)
        u.uluck = LUCKMAX;
}

int
stone_luck(boolean parameter)
{
    struct obj *otmp;
    long bonchance = 0;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (confers_luck(otmp)) {
            if (otmp->cursed)
                bonchance -= otmp->quan;
            else if (otmp->blessed)
                bonchance += otmp->quan;
            else if (parameter)
                bonchance += otmp->quan;
        }

    return sgn((int)bonchance);
}

/* there has just been an inventory change affecting a luck-granting item */
void
set_moreluck(void)
{
    int luckbon = stone_luck(TRUE);

    if (!luckbon && !carrying(LUCKSTONE))
        u.moreluck = 0;
    else if (luckbon >= 0)
        u.moreluck = LUCKADD;
    else
        u.moreluck = -LUCKADD;
}


void
restore_attrib(void)
{
    int i;

    for (i = 0; i < A_MAX; i++) {       /* all temporary losses/gains */

        if (ATEMP(i) && ATIME(i)) {
            if (!(--(ATIME(i)))) {      /* countdown for change */
                ATEMP(i) += ATEMP(i) > 0 ? -1 : 1;

                if (ATEMP(i))   /* reset timer */
                    ATIME(i) = 100 / ACURR(A_CON);
            }
        }
    }
    encumber_msg();
}


#define AVAL    50      /* tune value for exercise gains */

void
exercise(int i, boolean inc_or_dec)
{
    if (i == A_INT || i == A_CHA)
        return; /* can't exercise these */

    /* no physical exercise while polymorphed; the body's temporary */
    if (Upolyd && i != A_WIS)
        return;

    if (abs(AEXE(i)) < AVAL) {
        /*
         *      Law of diminishing returns (Part I):
         *
         *      Gain is harder at higher attribute values.
         *      79% at "3" --> 0% at "18"
         *      Loss is even at all levels (50%).
         *
         *      Note: *YES* ACURR is the right one to use.
         */
        AEXE(i) += (inc_or_dec) ? (rn2(19) > ACURR(i)) : -rn2(2);
    }
    if (moves > 0 && (i == A_STR || i == A_CON))
        encumber_msg();
}

static void
exerper(void)
{
    if (!(moves % 10)) {
        /* Hunger Checks */
        int hs =
            (u.uhunger > 1000) ? SATIATED :
            (u.uhunger > 150) ? NOT_HUNGRY :
            (u.uhunger > 50) ? HUNGRY :
            (u.uhunger > 0) ? WEAK : FAINTING;

        switch (hs) {
        case SATIATED:
            exercise(A_DEX, FALSE);
            if (Role_if(PM_MONK))
                exercise(A_WIS, FALSE);
            break;
        case NOT_HUNGRY:
            exercise(A_CON, TRUE);
            break;
        case WEAK:
            exercise(A_STR, FALSE);
            if (Role_if(PM_MONK))       /* fasting */
                exercise(A_WIS, TRUE);
            break;
        case FAINTING:
        case FAINTED:
            exercise(A_CON, FALSE);
            break;
        }

        /* Encumberance Checks */
        switch (near_capacity()) {
        case MOD_ENCUMBER:
            exercise(A_STR, TRUE);
            break;
        case HVY_ENCUMBER:
            exercise(A_STR, TRUE);
            exercise(A_DEX, FALSE);
            break;
        case EXT_ENCUMBER:
            exercise(A_DEX, FALSE);
            exercise(A_CON, FALSE);
            break;
        }

    }

    /* status checks */
    if (!(moves % 5)) {
        if ((HClairvoyant & (INTRINSIC | TIMEOUT)) && !BClairvoyant)
            exercise(A_WIS, TRUE);
        if (HRegeneration)
            exercise(A_STR, TRUE);
        if (Sick || Vomiting)
            exercise(A_CON, FALSE);
        if (Confusion || Hallucination)
            exercise(A_WIS, FALSE);
        if ((Wounded_legs && !u.usteed) || Fumbling || HStun)
            exercise(A_DEX, FALSE);
    }
}

void
exerchk(void)
{
    int i, mod_val;

    /* Check out the periodic accumulations */
    exerper();

    /* Are we ready for a test? */
    if (moves >= u.next_attr_check && !u_helpless(hm_all)) {
        /*
         *  Law of diminishing returns (Part II):
         *
         *  The effects of "exercise" and "abuse" wear
         *  off over time.  Even if you *don't* get an
         *  increase/decrease, you lose some of the
         *  accumulated effects.
         */
        for (i = 0; i < A_MAX; AEXE(i++) /= 2) {

            if (ABASE(i) >= 18 || !AEXE(i))
                continue;
            if (i == A_INT || i == A_CHA)
                continue;       /* can't exercise these */

            /*
             *      Law of diminishing returns (Part III):
             *
             *      You don't *always* gain by exercising.
             *      [MRS 92/10/28 - Treat Wisdom specially for balance.]
             */
            if (rn2(AVAL) >
                ((i != A_WIS) ? abs(AEXE(i) * 2 / 3) : abs(AEXE(i))))
                continue;
            mod_val = sgn(AEXE(i));

            if (adjattrib(i, mod_val, -1)) {
                /* if you actually changed an attrib - zero accumulation */
                AEXE(i) = 0;
                /* then print an explanation */
                switch (i) {
                case A_STR:
                    pline_implied(msgc_hint, (mod_val > 0) ?
                                  "You must have been exercising." :
                                  "You must have been abusing your body.");
                    break;
                case A_WIS:
                    pline_implied(msgc_hint, (mod_val > 0) ?
                                  "You must have been very observant." :
                                  "You haven't been paying attention.");
                    break;
                case A_DEX:
                    pline_implied(
                        msgc_hint, (mod_val > 0) ?
                        "You must have been working on your reflexes." :
                        "You haven't been working on reflexes lately.");
                    break;
                case A_CON:
                    pline_implied(msgc_hint, (mod_val > 0) ?
                                  "You must be leading a healthy life-style." :
                                  "You haven't been watching your health.");
                    break;
                }
            }
        }
        u.next_attr_check += rn1(200, 800);
    }
}


void
init_attr(int np)
{
    int i, x, tryct;

    for (i = 0; i < A_MAX; i++) {
        ABASE(i) = AMAX(i) = urole.attrbase[i];
        ATEMP(i) = ATIME(i) = 0;
        np -= urole.attrbase[i];
    }

    /* The starting ability distribution has changed slightly since 3.4.3 so
       that players with different races but the same role will have the same
       stats, as far as is possible. Instead of capping scores at the racial
       maximum, we cap them at STR18(100) for Strength, or 20 for other
       stats. Then, if any stats end up over the racial cap, we reduce them to
       the cap and redistribute them on rng_main.  The result is that the number
       of seeds consumed from rng_charstats_role depends purely on role.

       Note: there were previously two loops here, one to top up to np points,
       one to cut down to np points. The latter was dead code, and has been
       removed. */
    int pass;
    for (pass = 1; pass < 3; pass++) {
        tryct = 0;
        while (np > 0 && tryct < 100) {

            x = rn2_on_rng(100, pass == 1 ? rng_charstats_role : rng_main);

            for (i = 0; (i < A_MAX) && ((x -= urole.attrdist[i]) > 0); i++)
                ;
            if (i >= A_MAX)
                continue;   /* impossible */

            int current_max = (pass == 1 ? ATTRMAX(i) :
                               i == A_STR ? STR18(100) : 20);

            if (ABASE(i) >= current_max) {
                tryct++;
                continue;
            }

            tryct = 0;
            ABASE(i)++;
            AMAX(i)++;
            np--;
        }

        for (i = 0; i < A_MAX; i++) {
            if (ABASE(i) > ATTRMAX(i)) {
                np += ABASE(i) - ATTRMAX(i);
                AMAX(i) -= ABASE(i) - ATTRMAX(i);
                ABASE(i) = ATTRMAX(i);
            }
        }
    }
}

void
redist_attr(void)
{
    int i, tmp;

    for (i = 0; i < A_MAX; i++) {
        if (i == A_INT || i == A_WIS)
            continue;
        /* Polymorphing doesn't change your mind */
        tmp = AMAX(i);
        AMAX(i) += (rn2(5) - 2);
        if (AMAX(i) > ATTRMAX(i))
            AMAX(i) = ATTRMAX(i);
        if (AMAX(i) < ATTRMIN(i))
            AMAX(i) = ATTRMIN(i);
        ABASE(i) = ABASE(i) * AMAX(i) / tmp;
        /* ABASE(i) > ATTRMAX(i) is impossible */
        if (ABASE(i) < ATTRMIN(i))
            ABASE(i) = ATTRMIN(i);
    }
    encumber_msg();
}

static void
postadjabil(unsigned int *ability)
{
    if (!ability)
        return;
    if (ability == &(HWarning) || ability == &(HSee_invisible))
        see_monsters(FALSE);
}

void
adjabil(int oldlevel, int newlevel)
{
    const struct innate *abil, *rabil;
    long mask = FROMEXPER;

    switch (Role_switch) {
    case PM_ARCHEOLOGIST:
        abil = arc_abil;
        break;
    case PM_BARBARIAN:
        abil = bar_abil;
        break;
    case PM_CAVEMAN:
        abil = cav_abil;
        break;
    case PM_HEALER:
        abil = hea_abil;
        break;
    case PM_KNIGHT:
        abil = kni_abil;
        break;
    case PM_MONK:
        abil = mon_abil;
        break;
    case PM_PRIEST:
        abil = pri_abil;
        break;
    case PM_RANGER:
        abil = ran_abil;
        break;
    case PM_ROGUE:
        abil = rog_abil;
        break;
    case PM_SAMURAI:
        abil = sam_abil;
        break;
    case PM_TOURIST:
        abil = tou_abil;
        break;
    case PM_VALKYRIE:
        abil = val_abil;
        break;
    case PM_WIZARD:
        abil = wiz_abil;
        break;
    default:
        abil = 0;
        break;
    }

    switch (Race_switch) {
    case PM_ELF:
        rabil = elf_abil;
        break;
    case PM_ORC:
        rabil = orc_abil;
        break;
    case PM_HUMAN:
    case PM_DWARF:
    case PM_GNOME:
    default:
        rabil = 0;
        break;
    }

    while (abil || rabil) {
        long prevabil;

        /* Have we finished with the intrinsics list? */
        if (!abil || !abil->ability) {
            /* Try the race intrinsics */
            if (!rabil || !rabil->ability)
                break;
            abil = rabil;
            rabil = 0;
            mask = FROMRACE;
        }
        prevabil = *(abil->ability);
        if (oldlevel < abil->ulevel && newlevel >= abil->ulevel) {
            /* Abilities gained at level 1 can never be lost via level loss,
               only via means that remove _any_ sort of ability.  A "gain" of
               such an ability from an outside source is devoid of meaning, so
               we set FROMOUTSIDE to avoid such gains. */
            if (abil->ulevel == 1)
                *(abil->ability) |= (mask | FROMOUTSIDE);
            else
                *(abil->ability) |= mask;
            if (!(*(abil->ability) & INTRINSIC & ~mask)) {
                if (*(abil->gainstr))
                    pline(msgc_intrgain_level, "You feel %s!", abil->gainstr);
            }
        } else if (oldlevel >= abil->ulevel && newlevel < abil->ulevel) {
            *(abil->ability) &= ~mask;
            if (!(*(abil->ability) & INTRINSIC)) {
                if (*(abil->losestr))
                    pline(msgc_intrloss_level, "You feel %s!", abil->losestr);
                else if (*(abil->gainstr))
                    pline(msgc_intrloss_level, "You feel less %s!",
                          abil->gainstr);
            }
        }
        if (prevabil != *(abil->ability))       /* it changed */
            postadjabil(abil->ability);
        abil++;
    }

    if (oldlevel > 0) {
        if (newlevel > oldlevel)
            add_weapon_skill(newlevel - oldlevel);
        else
            lose_weapon_skill(oldlevel - newlevel);

        update_supernatural_abilities();
    }
}


int
newhp(void)
{
    int hp, conplus;


    if (u.ulevel == 0) {
        /* Initialize hit points */
        hp = urole.hpadv.infix + urace.hpadv.infix;
        if (urole.hpadv.inrnd > 0)
            hp += 1 + rn2_on_rng(urole.hpadv.inrnd, rng_charstats_role);
        if (urace.hpadv.inrnd > 0)
            hp += 1 + rn2_on_rng(urace.hpadv.inrnd, rng_charstats_race);

        /* Initialize alignment stuff */
        u.ualign.type = aligns[u.initalign].value;
        u.ualign.record = urole.initrecord;

        return hp;
    } else {
        if (u.ulevel < urole.xlev) {
            hp = urole.hpadv.lofix + urace.hpadv.lofix;
            if (urole.hpadv.lornd > 0)
                hp += 1 + rn2_on_rng(urole.hpadv.lornd, rng_charstats_role);
            if (urace.hpadv.lornd > 0)
                hp += 1 + rn2_on_rng(urace.hpadv.lornd, rng_charstats_race);
        } else {
            hp = urole.hpadv.hifix + urace.hpadv.hifix;
            if (urole.hpadv.hirnd > 0)
                hp += 1 + rn2_on_rng(urole.hpadv.hirnd, rng_charstats_role);
            if (urace.hpadv.hirnd > 0)
                hp += 1 + rn2_on_rng(urace.hpadv.hirnd, rng_charstats_race);
        }
    }

    if (ACURR(A_CON) <= 3)
        conplus = -2;
    else if (ACURR(A_CON) <= 6)
        conplus = -1;
    else if (ACURR(A_CON) <= 14)
        conplus = 0;
    else if (ACURR(A_CON) <= 16)
        conplus = 1;
    else if (ACURR(A_CON) == 17)
        conplus = 2;
    else if (ACURR(A_CON) == 18)
        conplus = 3;
    else
        conplus = 4;

    hp += conplus;
    return (hp <= 0) ? 1 : hp;
}

schar
acurr(int x)
{
    int tmp = (u.abon.a[x] + u.atemp.a[x] + u.acurr.a[x]);

    if (x == A_STR) {
        if (uarmg && uarmg->otyp == GAUNTLETS_OF_POWER)
            return 125;
        else
            return (schar) ((tmp >= 125) ? 125 : (tmp <= 3) ? 3 : tmp);
    } else if (x == A_CHA) {
        if (tmp < 18 &&
            (youmonst.data->mlet == S_NYMPH || u.umonnum == PM_SUCCUBUS ||
             u.umonnum == PM_INCUBUS))
            return 18;
    } else if (x == A_INT || x == A_WIS) {
        /* yes, this may raise int/wis if player is sufficiently stupid.  there
           are lower levels of cognition than "dunce". */
        if (uarmh && uarmh->otyp == DUNCE_CAP)
            return 6;
    }
    return (schar) ((tmp >= 25) ? 25 : (tmp <= 3) ? 3 : tmp);
}

/* condense clumsy ACURR(A_STR) value into value that fits into game formulas
 */
schar
acurrstr(void)
{
    int str = ACURR(A_STR);

    if (str <= 18)
        return (schar) str;
    if (str <= 121)
        return (schar) (19 + str / 50); /* map to 19-21 */
    else
        return (schar) (str - 100);
}

/* Returns the player's effective AC rating. Use in place of u.uac. */
schar
get_player_ac(void)
{
    /*
     * Do internal AC calculations with int instead of schar to prevent
     * overflow.
     * Return schar because that's what everything expects to see.
     * Start with intrinsic AC, which might not be 10 from eating rings.
     */
    int player_ac = (int)u.uac;

    /* If polymorphed, get the AC bonus from that. */
    player_ac -= (10 - mons[u.umonnum].ac);

    /* If wearing rings of protection, get the AC bonus from them. */
    if (uleft && uleft->otyp == RIN_PROTECTION)
        player_ac -= uleft->spe;
    if (uright && uright->otyp == RIN_PROTECTION)
        player_ac -= uright->spe;

    /* If casting Protection, get the AC bonus from that. */
    player_ac -= u.uspellprot;

    /* If the player has divine protection, get the AC bonus from that. */
    player_ac -= u.ublessed;

    /* Armor transformed into dragon skin gives no AC bonus. TODO: Should it at
       least give a bonus/penalty from its enchantment? */
    if (uarm && !uskin())
        player_ac -= ARM_BONUS(uarm);
    if (uarmc)
        player_ac -= ARM_BONUS(uarmc);
    if (uarmh)
        player_ac -= ARM_BONUS(uarmh);
    if (uarmf)
        player_ac -= ARM_BONUS(uarmf);
    if (uarms)
        player_ac -= ARM_BONUS(uarms);
    if (uarmg)
        player_ac -= ARM_BONUS(uarmg);
    if (uarmu)
        player_ac -= ARM_BONUS(uarmu);

    /* Trim to valid schar range. */
    if (player_ac < -128)
        player_ac = -128;
    if (player_ac > 127)
        player_ac = 127;

    return (schar)player_ac;
}

/* Avoid possible problems with alignment overflow, and provide a centralized
   location for any future alignment limits. */
void
adjalign(int n)
{
    int cnt; /* for loop initial declarations are only allowed in C99 mode */
    int newalign = u.ualign.record + n;

    if (n < 0) {
        if (newalign < u.ualign.record) {
            for (cnt = newalign; cnt < u.ualign.record; cnt++) {
                break_conduct(conduct_lostalign);
            }
            u.ualign.record = newalign;
        }
    } else if (newalign > u.ualign.record) {
        u.ualign.record = newalign;
        if (u.ualign.record > ALIGNLIM)
            u.ualign.record = ALIGNLIM;
    }
}

/* Return "beautiful", "handsome" or "ugly"
 * according to gender and charisma.
 */
const char *
beautiful(void)
{
    return ACURR(A_CHA) > 14 ?
        (poly_gender() == 1 ? "beautiful" : "handsome") : "ugly";
}

/* make sure u.abon is correct; it is dead-reckoned during the move,
 * but this produces some incorrect edge cases. */
void
calc_attr_bonus(void)
{
    int i, spe;

    struct obj *abon_items[] = {        /* item slots that might affect abon */
        uarmh /* helmet */ ,
        uarmg /* gloves */ ,
        uright /* right ring */ ,
        uleft /* left ring */ ,
    };

    memset(u.abon.a, 0, sizeof (u.abon.a));

    for (i = 0; i < SIZE(abon_items); i++) {
        if (!abon_items[i])
            continue;

        spe = abon_items[i]->spe;
        switch (abon_items[i]->otyp) {
        case RIN_GAIN_STRENGTH:
            ABON(A_STR) += spe;
            break;

        case RIN_GAIN_CONSTITUTION:
            ABON(A_CON) += spe;
            break;

        case RIN_ADORNMENT:
            ABON(A_CHA) += spe;
            break;

        case GAUNTLETS_OF_DEXTERITY:
            ABON(A_DEX) += spe;
            break;

        case HELM_OF_BRILLIANCE:
            ABON(A_INT) += spe;
            ABON(A_WIS) += spe;
            break;

        case CORNUTHAUM:
            ABON(A_CHA) += (Role_if(PM_WIZARD) ? 1 : -1);
            break;
        }
    }
}

/*attrib.c*/
