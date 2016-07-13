/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright 1988, 1989, 1990, 1992, M. Stephenson                */
/* NetHack may be freely redistributed.  See license for details. */

/*  attribute modification routines. */

#include "hack.h"
#include "hungerstatus.h"

/* part of the output on gain or loss of attribute */
static const char *const plusattr[] = {
    "strong", "smart", "wise", "agile", "tough", "charismatic"
}, *const minusattr[] = {
    "weak", "stupid", "foolish", "clumsy", "fragile", "repulsive"
};
static const char *const plusattr_more[] = {
    "stronger", "smarter", "wiser", "more agile", "tougher", "more charismatic"
}, *const minusattr_more[] = {
    "weaker", "more stupid", "more foolish", "clumsier", "more fragile", "more repulsive"
};
static const char *const plusattr_exe[] = {
    "You must have been exercising.",
    "You must have been thinking a lot lately.", /* unused */
    "You must have been very observant.",
    "You must have been working on your reflexes.",
    "You must be leading a healthy life-style.",
    "You must have been working on your appearance." /* unused */
}, *const minusattr_exe[] = {
    "You must have been abusing your body.",
    "You haven't been thinking things through.", /* unused */
    "You haven't been paying attention.",
    "You haven't been working on your reflexes lately.",
    "You haven't been watching your health.",
    "You haven't been working on your appearance lately." /* unused */
};

static boolean poly_attrib(const struct monst *, boolean);
static struct eattr *set_attrib(struct monst *);
static int attr_bonus(const struct monst *, int);
static void exerper(void);

/* Returns TRUE if the attribute should be checked for in polyform data */
static boolean
poly_attrib(const struct monst *mon, int attr)
{
    /* If monster is polymorphed, physical attributes are part of the polyform */
    if (attr != A_INT && attr != A_WIS && poly(mon))
        return TRUE;
    return FALSE;
}

/* Initialize an mx_eattr() for given monster with preset values based on what they are at
   the moment without eattr. Returns the eattr.
   The reason it is implemented as a mextra is to save save space in general cases. */
static struct eattr *
set_attrib(struct monst *mon)
{
    if (mx_eattr(mon))
        return mx_eattr(mon);

    /* Get the current attributes */
    struct attribs cur;
    int i;
    for (i = 0; i < A_MAX; i++)
        cur.a[i] = abase(mon, i);

    mx_eattr_new(mon);
    struct eattr *eattr = mx_eattr(mon);
    eattr->abase = cur;
    eattr->amax = cur;
    memset(&(eattr->aexe), 0, sizeof (struct attribs));
    eattr->exercise_time = rn1(400, 600); /* start an exercise timer */
    return eattr;
}

/* Adjust an attribute. Returns TRUE if the attribute was changed, FALSE otherwise.
   msg = FALSE avoids messages, forced = TRUE bypasses sustain ability. */
boolean
adj_attrib(struct monst *mon, int attr, int change, boolean msg, boolean forced)
{
    /* Not allowed for intelligence below 3 (brainlessness, mindlessness or animal),
       not even by forced (since it's intended only to bypass sustain ability). If you
       need to bypass this, set the attribute by hand (e.g. wizmode and brainlessness). */
    if (abase(mon, attr) < 3 && attr == A_INT)
        return FALSE;

    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    if ((fixed_abilities(mon) && !forced) || !change)
        return FALSE;
    struct obj *armh = which_armor(mon, os_armh);
    if ((attr == A_INT || attr == A_WIS) && armh && armh->otyp == DUNCE_CAP) {
        if (vis && msg)
            pline(combat_msgc(NULL, mon, cr_immune),
                  "%s cap constricts briefly, then relaxes again.",
                  s_suffix(Monnam(mon)));
        return FALSE;
    }

    int oldcap = near_capacity(); /* if player encumbrance changes */

    /* If items/hunger status/etc is messing with attributes, your current "cap" might be
       lower than usual. If it is lower and you are hitting the upper cap, or vice versa,
       add "right now" to the message to clarify. */
    boolean rightnow = FALSE;
    if ((change > 0 && abase(mon, attr) < acurr(mon, attr)) ||
        (change < 0 && abase(mon, attr) > acurr(mon, attr)))
        rightnow = TRUE;

    if ((change > 0 && abase(mon, attr) >= amaxcap(mon, attr)) ||
        (change < 0 && abase(mon, attr) <= amincap(mon, attr))) {
        if (you && msg)
            pline(msgc_playerimmune, "You're already as %s as you can get%s.",
                  change > 0 ? plusattr[attr] : minusattr[attr],
                  rightnow ? " right now" : "");
        return FALSE;
    }

    int abase_attr = abase(mon, attr);
    int amax_attr = amax(mon, attr); /* highest reached */
    abase_attr += change;

    /* Clamp to caps */
    if (abase_attr > amaxcap(mon, attr))
        abase_attr = amaxcap(mon, attr);
    if (abase_attr < amincap(mon, attr))
        abase_attr = amincap(mon, attr);
    /* We might have reached a new upper limit */
    if (amax_attr < abase_attr)
        amax_attr = abase_attr;

    if (poly_attrib(mon, attr)) {
        struct epoly *epoly = mx_epoly(mon);
        epoly->abase.a[attr] = abase_attr;
        epoly->amax.a[attr] = amax_attr;
    } else {
        struct eattr *eattr = set_attrib(mon);
        eattr->abase.a[attr] = abase_attr;
        eattr->amax.a[attr] = amax_attr;
    }
    if (vis && msg)
        pline(!you ? msgc_monneutral : change > 0 ? msgc_intrgain : msgc_intrloss,
              "%s %s%s!", M_verbs(mon, you ? "feel" : "look"),
              abs(change) > 1 ? "much " : "",
              change > 0 ? plusattr_more[attr] : minusattr_more[attr]);
    return TRUE;
}

/* Returns the base attribute */
int
abase(const struct monst *mon, int attr)
{
    /* If mon has proper attributes, return those */
    if (poly_attrib(mon, attr))
        return mx_poly(mon)->abase.a[attr];
    if (mx_eattr(mon))
        return mx_eattr(mon)->abase.a[attr];

    /* calculate based on type and level */
    int ret = pm_abase(mon->data, attr);

    /* Simulate exercise. For every 4th (str/wis), 3th (con) or 5th (dex) level, increase
       the attribute compared to the base with 1. For strength in particular, if it's
       18 or more, give more strength gain (from spinach/giants), slowing down at 18/85+,
       similar to how it usually ends up for players. */
    if (attr == A_INT || attr == A_CHA)
        return ret; /* can't be exercised */

    int xl = mon->m_lev;
    while (xl-- > 0) {
        if (attr == A_STR && ret >= 18) {
            if (ret > STR18(85))
                ret++; /* somewhat replicating gainstr() limitation */
            else
                ret += 5;
        } else if (!(xl % (attr == A_STR ? 4 :
                           attr == A_DEX ? 5 :
                           attr == A_CON ? 3 :
                           attr == A_WIS ? 4 :
                           5)) && attr != A_CHA && attr != A_INT)
            ret++;
    }

    /* Ensure that it doesn't go above/below caps. */
    if (ret > amaxcap(mon, attr))
        ret = amaxcap(mon, attr);
    if (ret < amincap(mon, attr))
        ret = amincap(mon, attr);
    return ret;
}

/* Returns the peak attribute reached */
int
amax(const struct monst *mon, int attr)
{
    if (poly_attrib(mon, attr))
        return mx_poly(mon)->amax.a[attr];
    if (mx_eattr(mon))
        return mx_eattr(mon)->amax.a[attr];
    return abase(mon, attr);
}

/* Returns base attributes for monster types */
int
pm_abase(const struct permonst *pm, int attr)
{
    int ret = 11; /* the general case */
    if (attr == A_CON)
        ret++; /* major impact on HP regeneration */
    if (strongmonst(pm) && attr = A_STR)
        ret = 18;
    if (pm == &mons[PM_SUCCUBUS] || pm == &mons[PM_INCUBUS] || pm->mlet == S_NYMPH)
        ret = 18;
    if (spellcaster(pm) && (attr == A_INT || attr == A_WIS))
        ret = 18;
    if (ret > pm_amaxcap(pm, attr))
        ret = pm_amaxcap(pm, attr);
    if (ret < pm_amincap(pm, attr))
        ret = pm_amincap(pm, attr);
    return ret;
}

/* Returns the max attribute cap of the monster */
int
amaxcap(const struct monst *mon, int attr)
{
    if (poly_attrib(mon, attr))
        return pm_amaxcap(&mons[mx_epoly(mon)->mnum], attr);
    return pm_amaxcap(mon->data, attr);
}

/* Returns the max attribute cap of the monster type */
int
pm_amaxcap(const struct permonst *pm, int attr)
{
    if (attr == A_STR) {
        if (pm->mlet == S_OGRE)
            return STR19(20);
        if (pm->mlet == S_GIANT)
            return STR19(25);
        if (is_elf(pm))
            return 18;
        if (pm->mlet == S_GNOME || pm->mlet == S_ORC)
            return STR18(50);
        if (strongmonst(pm))
            return STR18(100);
    }

    if (attr == A_DEX || attr == A_CON) {
        if (attr == A_DEX && is_elf(pm))
            return 16;
        if (pm->mlet == S_HUMANOID && pm != &mons[PM_MIND_FLAYER] &&
            pm != &mons[PM_MASTER_MIND_FLAYER])
            return 20;
    }

    if (attr = A_INT || attr == A_WIS) {
        if (mindless(pm))
            return 0; /* no mind, no intelligence */
        if (is_animal(pm))
            return 1;
        if (pm->mlet == S_DRAGON)
            return 20; /* dragons, while lacking hands, are very intelligent */
        if (spellcaster(pm))
            return 20;
        if (nohands(pm))
            return 2;
        if (pm == &mons[PM_MIND_FLAYER] || pm == &mons[PM_MASTER_MIND_FLAYER])
            return 20; /* highest potential cap from brain eating */
        if (pm->mlet == S_TROLL)
            return 14;
        if (pm->mlet == S_HUMANOID || pm->mlet == S_ORC ||
            pm->mlet == S_OGRE || pm->mlet == S_GIANT)
            return 16;
        if (is_elf(pm))
            return 20;
        if (is_gnome(pm))
            return (attr == A_INT ? 19 : 18);
    }

    if (attr == A_CHA) {
        if (pm == &mons[PM_SUCCUBUS] || pm == &mons[PM_INCUBUS] || pm->mlet == S_NYMPH)
            return 20;
        if (pm->mlet == S_TROLL)
            return 12;
        if (pm->mlet == S_OGRE)
            return 14;
        if (pm->mlet == S_HUMANOID || pm->mlet == S_ORC || pm->mlet == S_GIANT)
            return 16;
    }

    return 18; /* everything else */
}

/* Returns the min attribute cap of the monster */
int
amincap(const struct monst *mon, int attr)
{
    if (poly_attrib(mon, attr))
        return pm_amincap(&mons[mx_epoly(mon)->mnum], attr);
    return pm_amincap(mon->data, attr);
}

/* Returns the min attribute cap of the monster type */
int
pm_amincap(const struct permonst *pm, int attr)
{
    /* for int/wis, the cap can be below 3 */
    return min(pm_amaxcap(pm, attr), 3);
}

/* Returns the current attribute point (with bonuses/penalties/etc) for given mon. */
int
acurr(const struct monst *mon, int attr)
{
    boolean you = (mon == &youmonst);
    int ret = abase(mon, attr);

    /* If the base attribute is lower than 3, we're dealing with monsters lacking notable
       mental capabilities, and those aren't allowed bonuses/penalties. */
    if (ret < 3)
        return ret;

    /* attribute penalties from hunger/wounded legs, will only proc if base is above 3 */
    if (ret > 3 &&
        (attr == A_STR && you && u.uhs >= WEAK) ||
        (attr == A_DEX && leg_hurt(mon)))
        ret--;

    /* equipment bonuses */
    struct obj *obj;
    int otyp;
    for (obj = m_minvent(mon); obj; obj = obj->nobj) {
        /* is it worn properly */
        if (!(obj->owornmask & W_WORN))
            continue;
        otyp = obj->otyp;
        /* check if the equipment grants any bonus */
        if ((attrib == A_STR &&
             (otyp == RIN_GAIN_STRENGTH)) ||
            (attrib == A_CON &&
             (otyp == RIN_GAIN_CONSTITUTION)) ||
            (attrib == A_CHA &&
             (otyp == RIN_ADORNMENT)) ||
            (attrib == A_DEX &&
             (otyp == GAUNTLETS_OF_DEXTERITY)) ||
            (attrib == A_INT &&
             (otyp == HELM_OF_BRILLIANCE)) ||
            (attrib == A_WIS &&
             (otyp == HELM_OF_BRILLIANCE)))
            ret += obj->spe;
        /* people think marked wizards know what they're talking about, but it
           takes trained arrogance to pull it off, and the actual enchantment
           of the hat is irrelevant. */
        if (attrib == A_CHA && otyp == CORNUTHAUM)
            ret += ((you && Role_if(PM_WIZARD)) ||
                    (!you && spellcaster(mon->data))) ?
                1 : -1;
    }

    /* check for gauntlets of power/dunce cap seperately, since they override everything
       else */
    if ((obj = which_armor(mon, os_armg)) && obj->otyp == GAUNTLETS_OF_POWER)
        ret = STR19(25);

    /* yes, this may raise int/wis if player is sufficiently stupid.  there
       are lower levels of cognition than "dunce". */
    if ((obj = which_armor(mon, os_armh)) && obj->otyp == DUNCE_CAP)
        ret = 6;
    return ret;
}

/* Potentially adds a point to exercise attributes to be checked during next update */
void
exercise(struct monst *mon, int attr, boolean positive)
{
    if (!mon)
        return; /* just in case */

    /* If the monster doesn't have any attribute data, don't add any, exercising isn't
       significant enough to warrant it. */
    if (!mx_eattr(mon))
        return;

    if (attr == A_INT || attr == A_CHA) {
        impossible("%s exercising int or cha?", M_verbs(mon, "are"));
        return;
    }

    if (poly_attrib(mon, attr))
        return; /* no exercise of a temporary body */

    /* exercise values go up to 50 (or -50), with a lower rate the closer to 50 it is */
    struct attribs aexe = mx_eattr(mon)->aexe;
    if (rnd(50) > (positive ? aexe.a[attr] : -aexe.a[attr]))
        mx_eattr(mon)->aexe.a[attr] += (positive ? 1 : -1);
}

/* Exercises/abuses periodically based on hunger/properties, and runs the updating every
   so often. */
void
apply_exercise(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    if (!mx_eattr(mon))
        return;

    if (you && !(moves % 10)) {
        /* Hunger checks */
        switch (u.uhs) {
        case SATIATED:
            exercise(mon, A_DEX, FALSE);
            if (Role_if(PM_MONK))
                exercise(mon, A_WIS, FALSE);
            break;
        case NOT_HUNGRY:
            exercise(mon, A_CON, TRUE);
            break;
        case FAINTING:
        case FAINTED:
            exercise(A_CON, FALSE);
            /* fallthrough */
        case WEAK:
            exercise(A_STR, FALSE);
            if (Role_if(PM_MONK))
                exercise(A_WIS, TRUE);
            break;
        default:
            break;
        }

        /* Encumbrance checks */
        switch (near_capacity()) {
        case MOD_ENCUMBER:
            exercise(A_STR, TRUE);
            /* fallthrough */
        case HVY_ENCUMBER:
            exercise(A_DEX, FALSE);
            break;
        case EXT_ENCUMBER:
        case OVERLOADED:
            exercise(A_DEX, FALSE);
            exercise(A_CON, FALSE);
            break;
        default:
            break;
        }
    }

    /* property checks */
    if (!(moves % 5)) {
        if (clairvoyant(mon))
            exercise(A_WIS, TRUE);
        if (regenerates(mon))
            exercise(A_STR, TRUE);
        if (sick(mon) || vomiting(mon))
            exercise(A_CON, FALSE);
        if (confused(mon) || hallucinating(mon))
            exercise(A_WIS, FALSE);
        if (leg_hurt(mon) || fumbling(mon) || stunned(mon))
            exercise(A_DEX, FALSE);
    }

    struct eattr *eattr = mx_eattr(mon);
    eattr->exercise_time--;
    if (eattr->exercise_time <= 0) {
        /* Time for a check. The closer you are to the cap (clamped to 3-18), the harder
           it is to gain/lose further points. Stat gains doesn't affect the max, so
           points gained while underattributed (poison/similar) are "pointless". Stat
           *losses* however, *are* permanent. */
        int i;
        for (i = 0; i < A_MAX; i++) {
            int exe = eattr->aexe.a[i];
            if ((exe > 0 && abase(mon, i) >= amax(mon, i)) ||
                (exe < 0 && abase(mon, i) <= amin(mon, i)))
                continue; /* cap reached */

            /* Use the lesser of acurr and abase -- lowering your stats wont help you
               exercise, but increasing *will* make it harder. */
            int base = min(acurr(mon, i), abase(mon, i));

            /* The monster needs to pass both those random checks for it to count */
            int rnd_exe = rnd(50);
            int threshold = rn1(min(amaxcap(mon, i), 18) - 2, 3);

            if (rnd_exe <= (exe > 0 ? exe : -exe) &&
                ((exe < 0 && threshold < exe) ||
                 (exe > 0 && threshold > exe)) &&
                adj_attrib(mon, i, exe > 0 ? 1 : -1, TRUE, FALSE)) {
                /* we have a result! */
                eattr->aexe.a[i] = 0; /* reset for future exercise */
                if (exe < 0)
                    eattr->amax.a[i]--; /* loss is permanent... */
                if (you) /* give a hint where the stat gain came from */
                    pline_implied(msgc_hint, "%s", (exe > 0) ?
                                  plusattr_exe[i] : minusattr_exe[i]);
            } else
                eattr->aexe.a[i] /= 2; /* lower, but keep, potential future exercise */
        }

        eattr->exercise_time = rn1(400, 600);
    }
}

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


    if (youmonst.m_lev == 0) {
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
        if (youmonst.m_lev < urole.xlev) {
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
        tmp += u.acurr.a[x];
        if (u.uhs >= WEAK && x == A_STR && tmp > 3)
            tmp--;
    } else {
        tmp = 11;
        if (x == A_STR && strongmonst(mon->data))
            tmp = STR18(100);
        if (x == A_CON)
            tmp++; /* more HP regeneration */
        if (x == A_INT && spellcaster(mon->data))
            tmp = 18;
        if (mon->iswiz && (x == A_INT || x == A_WIS))
            tmp = 20;
    }
    if (leg_hurt(mon) && x == A_DEX && tmp > 3)
        tmp--;

    tmp += attr_bonus(mon, x);

    if (x == A_STR) {
        struct obj *armg = which_armor(mon, os_armg);
        if (armg && armg->otyp == GAUNTLETS_OF_POWER)
            return 125;
        else
            return (schar) ((tmp >= 125) ? 125 : (tmp <= 3) ? 3 : tmp);
    } else if (x == A_CHA) {
        if (tmp < 18 &&
            (mon->data->mlet == S_NYMPH || monsndx(mon->data) == PM_SUCCUBUS ||
             monsndx(mon->data) == PM_INCUBUS))
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

/* Calculate attribute bonus from worn armor/rings/etc. */
static int
attr_bonus(const struct monst *mon, int attrib)
{
    boolean you = (mon == &youmonst);
    int ascore = 0;
    struct obj *obj;
    int otyp;

    for (obj = m_minvent(mon); obj; obj = obj->nobj) {
        /* is it worn properly */
        if (!(obj->owornmask & W_WORN))
            continue;
        otyp = obj->otyp;
        /* check if the equipment grants any bonus */
        if ((attrib == A_STR &&
             (otyp == RIN_GAIN_STRENGTH)) ||
            (attrib == A_CON &&
             (otyp == RIN_GAIN_CONSTITUTION)) ||
            (attrib == A_CHA &&
             (otyp == RIN_ADORNMENT)) ||
            (attrib == A_DEX &&
             (otyp == GAUNTLETS_OF_DEXTERITY)) ||
            (attrib == A_INT &&
             (otyp == HELM_OF_BRILLIANCE)) ||
            (attrib == A_WIS &&
             (otyp == HELM_OF_BRILLIANCE)))
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
