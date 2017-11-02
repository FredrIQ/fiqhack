/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-02 */
/* Copyright 1988, 1989, 1990, 1992, M. Stephenson                */
/* NetHack may be freely redistributed.  See license for details. */

/*  attribute modification routines. */

#include "hack.h"
#include "hungerstatus.h"

/* part of the output on gain or loss of attribute */
static
const char *const plusattr[] = {
    "strong", "smart", "wise", "agile", "tough", "charismatic"
}, *const minusattr[] = {
    "weak", "stupid", "foolish", "clumsy", "fragile", "repulsive"
};

static int attr_bonus(const struct monst *, int);
static void exerper(void);

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

    int oldcap = near_capacity();

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
        encumber_msg(oldcap);
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

    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj)
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
    int oldcap = near_capacity();

    for (i = 0; i < A_MAX; i++) {       /* all temporary losses/gains */

        if (ATEMP(i) && ATIME(i)) {
            if (!(--(ATIME(i)))) {      /* countdown for change */
                ATEMP(i) += ATEMP(i) > 0 ? -1 : 1;

                if (ATEMP(i))   /* reset timer */
                    ATIME(i) = 100 / ACURR(A_CON);
            }
        }
    }
    encumber_msg(oldcap);
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

    int oldcap = near_capacity();
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
        encumber_msg(oldcap);
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
        if (clairvoyant(&youmonst))
            exercise(A_WIS, TRUE);
        if (regenerates(&youmonst))
            exercise(A_STR, TRUE);
        if (sick(&youmonst) || vomiting(&youmonst))
            exercise(A_CON, FALSE);
        if (confused(&youmonst) || hallucinating(&youmonst))
            exercise(A_WIS, FALSE);
        if (leg_hurt(&youmonst) || fumbling(&youmonst) ||
            stunned(&youmonst))
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
    int oldcap = near_capacity();

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
    encumber_msg(oldcap);
}

void
adjabil(int oldlevel, int newlevel)
{
    /* Level-based extrinsics */
    if (oldlevel)
        update_xl_properties(&youmonst, oldlevel);

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

/* This works on monsters, but since monsters lack proper attributes,
   should be used sparingly. */
schar
acurr(const struct monst *mon, int x)
{
    boolean you = (mon == &youmonst);
    int tmp = 0;
    if (you) {
        tmp += u.atemp.a[x];
        tmp += u.acurr.a[x];
    } else {
        tmp = 11;
        if (x == A_STR && strongmonst(mon->data))
            tmp = STR18(100);
        if (x == A_CON)
            tmp++; /* more HP regeneration */
        if (x == A_INT || x == A_WIS) {
            if (spellcaster(mon->data))
                tmp = 18;
            if (mon->iswiz)
                tmp = 20;
        }
    }
    tmp += attr_bonus(mon, x);

    if (x == A_STR) {
        struct obj *obj;

        obj = which_armor(mon, os_armg);
        if (obj && obj->otyp == GAUNTLETS_OF_POWER)
            return 125;

        /* check for the "power" obj property, only functions on
           worn armor. */
        for (obj = mon->minvent; obj; obj = obj->nobj)
            if ((obj->owornmask & W_ARMOR) &&
                (obj_properties(obj) & opm_power))
                return 125;

        return (schar) ((tmp >= 125) ? 125 : (tmp <= 3) ? 3 : tmp);
    } else if (x == A_CHA) {
        if (tmp < 18 &&
            (mon->data->mlet == S_NYMPH || monsndx(mon->data) == PM_INCUBUS))
            return 18;
    } else if (x == A_INT || x == A_WIS) {
        struct obj *armh = which_armor(mon, os_armh);
        /* yes, this may raise int/wis if player is sufficiently stupid.  there
           are lower levels of cognition than "dunce". */
        if (armh && armh->otyp == DUNCE_CAP)
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

/* Avoid possible problems with alignment overflow, and provide a centralized
   location for any future alignment limits. */
void
adjalign(int n)
{
    int cnt; /* for loop initial declarations are only allowed in C99 mode */
    int oldalign = u.ualign.record;
    int newalign = oldalign;
    if (n < 0 && oldalign > AR_TRANSGRESSED)
        pline(msgc_alignbad, "Your conscience bothers you...");

    newalign += n;
    if (newalign > ALIGNLIM)
        newalign = ALIGNLIM;
    u.ualign.record = newalign;

    /* conduct */
    if (n < 0 && newalign < oldalign)
        for (cnt = newalign; cnt < oldalign; cnt++)
            break_conduct(conduct_lostalign);
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

/* Calculate attribute bonus from worn armor/rings/etc. */
static int
attr_bonus(const struct monst *mon, int attrib)
{
    boolean you = (mon == &youmonst);
    int ascore = 0;
    struct obj *obj;
    int otyp;
    uint64_t props;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        /* is it worn properly */
        if (!(obj->owornmask & W_WORN))
            continue;
        otyp = obj->otyp;
        props = obj_properties(obj);
        /* check if the equipment grants any bonus */
        if ((attrib == A_STR &&
             (otyp == RIN_GAIN_STRENGTH)) ||
            (attrib == A_CON &&
             (otyp == RIN_GAIN_CONSTITUTION)) ||
            (attrib == A_CHA &&
             (otyp == RIN_ADORNMENT)) ||
            (attrib == A_DEX &&
             (otyp == GAUNTLETS_OF_DEXTERITY ||
              (props & opm_dexterity))) ||
            (attrib == A_INT &&
             (otyp == HELM_OF_BRILLIANCE ||
              (props & opm_brilliance))) ||
            (attrib == A_WIS &&
             (otyp == HELM_OF_BRILLIANCE ||
              (props & opm_brilliance))))
            ascore += obj->spe;
        /* cornuthaums give +1/-1 cha depending on
           if you're a spellcaster or not */
        if (attrib == A_CHA && otyp == CORNUTHAUM)
            ascore += ((you && Role_if(PM_WIZARD)) ||
                       (!you && spellcaster(mon->data))) ?
                1 : -1;
    }
    return ascore;
}

/*attrib.c*/
