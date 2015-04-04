/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-13 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int enermod(int);

long
newuexp(int lev)
{
    /* keep this synced with the status-drawing code in the clients */
    if (lev < 10)
        return 10L * (1L << lev);
    if (lev < 20)
        return 10000L * (1L << (lev - 10));
    return 10000000L * ((long)(lev - 19));
}

static int
enermod(int en)
{
    switch (Role_switch) {
    case PM_PRIEST:
    case PM_WIZARD:
        return 2 * en;
    case PM_HEALER:
    case PM_KNIGHT:
        return (3 * en) / 2;
    case PM_BARBARIAN:
    case PM_VALKYRIE:
        return (3 * en) / 4;
    default:
        return en;
    }
}

/* return # of exp points for mtmp after nk killed */
int
experience(struct monst *mtmp, int nk)
{
    const struct permonst *ptr = mtmp->data;
    int i, tmp, tmp2;

    tmp = 1 + mtmp->m_lev * mtmp->m_lev;

/* For higher ac values, give extra experience */
    if ((i = find_mac(mtmp)) < 3)
        tmp += (7 - i) * ((i < 0) ? 2 : 1);

/* For very fast monsters, give extra experience */
    if (ptr->mmove > NORMAL_SPEED)
        tmp += (ptr->mmove > (3 * NORMAL_SPEED / 2)) ? 5 : 3;

/* For each "special" attack type give extra experience */
    for (i = 0; i < NATTK; i++) {

        tmp2 = ptr->mattk[i].aatyp;
        if (tmp2 > AT_BUTT) {

            if (tmp2 == AT_WEAP)
                tmp += 5;
            else if (tmp2 == AT_MAGC)
                tmp += 10;
            else
                tmp += 3;
        }
    }

/* For each "special" damage type give extra experience */
    for (i = 0; i < NATTK; i++) {
        tmp2 = ptr->mattk[i].adtyp;
        if (tmp2 > AD_PHYS && tmp2 < AD_BLND)
            tmp += 2 * mtmp->m_lev;
        else if ((tmp2 == AD_DRLI) || (tmp2 == AD_STON) || (tmp2 == AD_SLIM))
            tmp += 50;
        else if (tmp2 != AD_PHYS)
            tmp += mtmp->m_lev;
        /* extra heavy damage bonus */
        if ((int)(ptr->mattk[i].damd * ptr->mattk[i].damn) > 23)
            tmp += mtmp->m_lev;
        if (tmp2 == AD_WRAP && ptr->mlet == S_EEL && !Amphibious)
            tmp += 1000;
    }

/* For certain "extra nasty" monsters, give even more */
    if (extra_nasty(ptr))
        tmp += (7 * mtmp->m_lev);

/* For higher level monsters, an additional bonus is given */
    if (mtmp->m_lev > 8)
        tmp += 50;

    return tmp;
}

void
more_experienced(int exp, int rexp)
{
    u.uexp += exp;
    if (u.uexp >= (Role_if(PM_WIZARD) ? 250 : 500))
        flags.beginner = 0;
}

/* e.g., hit by drain life attack

   TODO: We really ought to be driving the rng_charstats_* RNGs /backwards/.
   That's left out for now for sanity reasons, but is definitely possible to
   implement. For now, this is left as main RNG. */
void
losexp(const char *killer, boolean override_res)
{
    int num;

    if (!override_res && resists_drli(&youmonst))
        return;

    if (u.ulevel > 1) {
        pline("%s level %d.", Goodbye(), u.ulevel--);
        /* remove intrinsic abilities */
        adjabil(u.ulevel + 1, u.ulevel);
        reset_rndmonst(NON_PM); /* new monster selection */
    } else {
        if (killer)
            done(DIED, killer);

        /* no drainer or lifesaved */
        u.uexp = 0;
    }
    num = newhp();
    u.uhpmax -= num;
    if (u.uhpmax < 1)
        u.uhpmax = 1;
    u.uhp -= num;
    if (u.uhp < 1)
        u.uhp = 1;
    else if (u.uhp > u.uhpmax)
        u.uhp = u.uhpmax;

    if (u.ulevel < urole.xlev)
        num =
            rn1((int)ACURR(A_WIS) / 2 + urole.enadv.lornd + urace.enadv.lornd,
                urole.enadv.lofix + urace.enadv.lofix);
    else
        num =
            rn1((int)ACURR(A_WIS) / 2 + urole.enadv.hirnd + urace.enadv.hirnd,
                urole.enadv.hifix + urace.enadv.hifix);
    num = enermod(num); /* M. Stephenson */
    u.uenmax -= num;
    if (u.uenmax < 0)
        u.uenmax = 0;
    u.uen -= num;
    if (u.uen < 0)
        u.uen = 0;
    else if (u.uen > u.uenmax)
        u.uen = u.uenmax;

    if (u.uexp > 0)
        u.uexp = newuexp(u.ulevel) - 1;
}

/*
 * Make experience gaining similar to AD&D(tm), whereby you can at most go
 * up by one level at a time, extra expr possibly helping you along.
 * After all, how much real experience does one get shooting a wand of death
 * at a dragon created with a wand of polymorph??
 */
void
newexplevel(void)
{
    if (u.ulevel < MAXULEV && u.uexp >= newuexp(u.ulevel))
        pluslvl(TRUE);
}

/* incr: true iff via incremental experience growth, false for potion of gain
   level; TODO: place this onto a separate RNG (probably requires changes to
   the energy gain formula) */
void
pluslvl(boolean incr)
{
    int num;

    if (!incr)
        pline("You feel more experienced.");
    num = newhp();
    u.uhpmax += num;
    u.uhp += num;
    if (Upolyd) {
        num = rnd(8);
        u.mhmax += num;
        u.mh += num;
    }
    if (u.ulevel < urole.xlev)
        num =
            rn1((int)ACURR(A_WIS) / 2 + urole.enadv.lornd + urace.enadv.lornd,
                urole.enadv.lofix + urace.enadv.lofix);
    else
        num =
            rn1((int)ACURR(A_WIS) / 2 + urole.enadv.hirnd + urace.enadv.hirnd,
                urole.enadv.hifix + urace.enadv.hifix);
    num = enermod(num); /* M. Stephenson */
    u.uenmax += num;
    u.uen += num;
    if (u.ulevel < MAXULEV) {
        if (incr) {
            long tmp = newuexp(u.ulevel + 1);

            if (u.uexp >= tmp)
                u.uexp = tmp - 1;
        } else {
            u.uexp = newuexp(u.ulevel);
        }
        ++u.ulevel;
        if (u.ulevelmax < u.ulevel) {
            u.ulevelmax = u.ulevel;
            historic_event(FALSE, FALSE,
                           "advanced to experience level %d.", u.ulevel);
        }
        pline("Welcome to experience level %d.", u.ulevel);
        adjabil(u.ulevel - 1, u.ulevel);        /* give new intrinsics */
        reset_rndmonst(NON_PM); /* new monster selection */
    }
}

/* compute a random amount of experience points suitable for the hero's
   experience level:  base number of points needed to reach the current
   level plus a random portion of what it takes to get to the next level */
/* gaining: gaining XP via potion vs setting XP for polyself */
long
rndexp(boolean gaining)
{
    long minexp, maxexp, diff, factor, result;

    minexp = (u.ulevel == 1) ? 0L : newuexp(u.ulevel - 1);
    maxexp = newuexp(u.ulevel);
    diff = maxexp - minexp, factor = 1L;
    /* make sure that `diff' is an argument which rn2() can handle */
    while (diff >= (long)LARGEST_INT)
        diff /= 2L, factor *= 2L;
    result = minexp + factor * (long)rn2((int)diff);
    /* 3.4.1: if already at level 30, add to current experience points rather
       than to threshold needed to reach the current level; otherwise blessed
       potions of gain level can result in lowering the experience points
       instead of raising them */
    if (u.ulevel == MAXULEV && gaining) {
        result += (u.uexp - minexp);
        /* avoid wrapping (over 400 blessed potions needed for that...) */
        if (result < u.uexp)
            result = u.uexp;
    }
    return result;
}

/*exper.c*/

