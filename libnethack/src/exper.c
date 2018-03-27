/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-03-27 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int enermod(int);
static int menermod(const struct monst *, int);
static int mon_pw_gain(const struct monst *);

long
newuexp(int lev)
{
    /* keep this synced with the status-drawing code in the clients */
    switch (lev) {
    case  0: return      0;
    case  1: return     20; /* n^2 */
    case  2: return     40;
    case  3: return     80;
    case  4: return    160;
    case  5: return    320;
    case  6: return    640;
    case  7: return   1280;
    case  8: return   2560;
    case  9: return   5120;
    case 10: return  10000; /* triangle numbers */
    case 11: return  15000;
    case 12: return  21000;
    case 13: return  28000;
    case 14: return  36000;
    case 15: return  45000;
    case 16: return  55000;
    case 17: return  66000;
    case 18: return  81000; /* n*n series */
    case 19: return 100000;
    case 20: return 142000;
    case 21: return 188000;
    case 22: return 238000;
    case 23: return 292000;
    case 24: return 350000;
    case 25: return 412000;
    case 26: return 478000;
    case 27: return 548000;
    case 28: return 622000;
    case 29: return 700000;
    case 30: return 800000; /* 100k per additional !oGL */
    }
    impossible("unknown level: %d", lev);
    return 10000000;
}

static int
enermod(int en)
{
    return menermod(&youmonst, en);
}

static int
menermod(const struct monst *mon, int en)
{
    int pm = monsndx(mon->data);
    if (mon == &youmonst)
        pm = Role_switch;

    switch (pm) {
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
        if (mon != &youmonst) {
            if (mon->iswiz)
                return (5 * en) / 2;
            if (mon->data->mflags3 & M3_SPELLCASTER)
                return 2 * en;
            if (mon->data->mflags1 & M1_MINDLESS)
                return en / 2;
            if (mon->data->mflags1 & M1_ANIMAL)
                return (3 * en) / 4;
        }
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
        if (tmp2 == AD_WRAP && ptr->mlet == S_EEL && !Breathless)
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
    if (u.ulevel < u.ulevelmax) {
        /* We are drained. Boost EXP gain rate proportional to the levels lost */
        int lev = u.ulevel;
        while (lev++ < u.ulevelmax)
            exp *= 2;

        /* Only double up to each level threshould */
        if ((u.uexp + exp) > newuexp(u.ulevel)) {
            exp -= (newuexp(u.ulevel) - u.uexp);
            u.uexp = newuexp(u.ulevel);
            exp /= 2;
        }
    }

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
        pline(msgc_intrloss, "%s level %d.", Goodbye(), u.ulevel--);
        /* remove intrinsic abilities */
        adjabil(u.ulevel + 1, u.ulevel);
    } else {
        if (killer)
            done(DIED, killer);

        /* no drainer or lifesaved */
        u.uexp = 0;
    }
    num = mnewadv(&youmonst, FALSE);
    u.uhpmax -= num;
    u.mhmax -= num;
    if (u.uhpmax < 1)
        u.uhpmax = 1;
    if (u.mhmax < 1)
        u.mhmax = 1;
    u.uhp -= num;
    u.mh -= num;
    if (u.uhp < 1)
        u.uhp = 1;
    else if (u.uhp > u.uhpmax)
        u.uhp = u.uhpmax;
    if (u.mh < 1)
        u.mh = 1;
    else if (u.mh > u.mhmax)
        u.mh = u.mhmax;

    num = mnewadv(&youmonst, TRUE);
    youmonst.pwmax -= num;
    if (youmonst.pwmax < 0)
        youmonst.pwmax = 0;
    youmonst.pw -= num;
    if (youmonst.pw < 0)
        youmonst.pw = 0;
    else if (youmonst.pw > youmonst.pwmax)
        youmonst.pw = youmonst.pwmax;

    /* Compute the progress the hero did towards the next level. Scale back one level,
       but keep the same progress. */
    if (u.uexp) {
        int expdiff_high = newuexp(u.ulevel + 1) - newuexp(u.ulevel);
        int expdiff_low = newuexp(u.ulevel) - newuexp(u.ulevel - 1);
        int pct = (u.uexp - newuexp(u.ulevel)) * 100 / expdiff_high;
        u.uexp = newuexp(u.ulevel - 1);
        u.uexp += pct * expdiff_low / 100;
    }
}

void
mlosexp(struct monst *magr, struct monst *mdef, const char *killer,
        boolean override_res)
{
    if (mdef == &youmonst) {
        losexp(killer, override_res);
        return;
    }

    if (!override_res && resists_drli(mdef))
        return;

    if (!mdef->m_lev) {
        monkilled(magr, mdef, "", AD_DRLI);
        return;
    }

    if (canseemon(mdef))
        pline(combat_msgc(magr, mdef, cr_hit),
              "%s suddenly seems weaker!", Monnam(mdef));

    int hp_loss = mnewadv(mdef, FALSE);
    hp_loss = min(mdef->mhpmax - 1, hp_loss);
    mdef->mhpmax -= hp_loss;
    mdef->mhp -= min(mdef->mhp - 1, hp_loss);

    int pw_loss = mnewadv(mdef, TRUE);
    pw_loss = min(mdef->pwmax, pw_loss);
    mdef->pwmax -= pw_loss;
    mdef->pw -= min(mdef->pw, pw_loss);

    mdef->m_lev--;
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
        pline(msgc_intrgain, "You feel more experienced.");
    num = mnewadv(&youmonst, FALSE);
    u.uhpmax += num;
    u.uhp += num;
    if (Upolyd) {
        u.mhmax += num;
        u.mh += num;
    }
    num = mnewadv(&youmonst, TRUE);
    youmonst.pwmax += num;
    youmonst.pw += num;
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
        pline(msgc_intrgain, "Welcome to experience level %d.", u.ulevel);
        adjabil(u.ulevel - 1, u.ulevel);        /* give new intrinsics */
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

/* Gives monster appropriate HP for its level. */
void
initialize_mon_hp(struct monst *mon, enum rng rng)
{
    int hp_gain = get_advmod_total(acurr(mon, A_CON), mon, FALSE);
    if (mon->m_lev == 0)
        hp_gain /= 2;

    /* Fuzz between 0.75x and 1.25x */
    hp_gain += rn2_on_rng(max(hp_gain / 2, 1), rng) - (hp_gain / 4);
    hp_gain++;

    if (hp_gain < 4)
        hp_gain = 4;

    /* "Special" HP values */
    if (is_golem(mon->data))
        hp_gain = golemhp(monsndx(mon->data));
    else if (mon->data->mlevel > 49) {
        /* "special" fixed hp monster the hit points are encoded in the mlevel
           in a somewhat strange way to fit in the 50..127 positive range of a
           signed character above the 1..49 that indicate "normal" monster
           levels */
        hp_gain = 2 * (mon->data->mlevel - 6);
        mon->m_lev = hp_gain / 4;    /* approximation */
    } else if (is_home_elemental(&mon->dlevel->z, mon->data))
        hp_gain *= 2;

    mon->mhp += hp_gain;
    mon->mhpmax += hp_gain;
}

/* Gives monster appropriate Pw for its level. */
void
initialize_mon_pw(struct monst *mon, enum rng rng)
{
    int pw_gain = get_advmod_total(acurr(mon, A_WIS), mon, TRUE);
    if (mon->m_lev == 0)
        pw_gain /= 2;

    /* Fuzz between 0.75x and 1.25x */
    pw_gain += rn2_on_rng(max(pw_gain / 2, 1), rng) - (pw_gain / 4);
    pw_gain++;
    if (pw_gain < 0)
        pw_gain = 0;
    mon->pw += pw_gain;
    mon->pwmax += pw_gain;
}

const struct permonst *
grow_up(struct monst *mtmp,   /* `mtmp' might "grow up" into a bigger version */
        struct monst *victim)
{
    int oldtype, newtype, lev_limit, hpincr;
    boolean pluslvl = TRUE;
    const struct permonst *ptr = mtmp->data;

    /* monster died after killing enemy but before calling this function */
    /* currently possible if killing a gas spore */
    if (DEADMONSTER(mtmp))
        return NULL;

    /* note: none of the monsters with special hit point calculations have both
       little and big forms */
    oldtype = monsndx(ptr);
    newtype = little_to_big(oldtype);

    hpincr = mnewadv(mtmp, FALSE);
    lev_limit = 50;

    int hpmod = mtmp->mhpmax - get_advmod_total(acurr(mtmp, A_CON),
                                                    mtmp, FALSE);

    /* Experience from killing stuff */
    if (victim) {       /* killed a monster */
        hpincr = rnd(max(victim->m_lev - mtmp->m_lev, 1));
        int threshold = !mtmp->m_lev ? 4 : mnewadv(mtmp, FALSE);

        /* Don't allow gaining enough HP for more than a
           single level, unless that would lead to a decrease
           in max HP */
        if ((hpmod + hpincr) < threshold)
            pluslvl = FALSE;
        else if ((hpmod + hpincr) < (threshold * 2))
            hpincr = threshold - hpmod;
        if (hpincr < 0)
            hpincr = 0;
        lev_limit = (mtmp->m_lev * 3) / 2;
    }

    if (is_mplayer(ptr))
        lev_limit = 30; /* same as player */
    else if (lev_limit < 5)
        lev_limit = 5;  /* arbitrary */
    else if (lev_limit > 49)
        lev_limit = (ptr->mlevel > 49 ? 50 : 49);

    /* If we can't gain levels anymore, just increase max HP
       up to a limit of 1.5x what it would be from base HP
       growth alone */
    if (mtmp->m_lev >= lev_limit) {
        int hp_limit = get_advmod_total(acurr(mtmp, A_WIS), mtmp, FALSE);
        hp_limit *= 3;
        hp_limit /= 2;
        while (hpincr && mtmp->mhpmax < hp_limit) {
            mtmp->mhpmax++;
            mtmp->mhp++;
            hpincr--;
        }

        return ptr;
    }

    mtmp->mhpmax += hpincr;
    mtmp->mhp += hpincr;

    if (!pluslvl)
        return ptr; /* we're done here */

    mtmp->m_lev++;

    /* Pw gain */
    int pwincr = mnewadv(mtmp, TRUE);
    mtmp->pwmax += pwincr;
    mtmp->pw += pwincr;

    if (mtmp->m_lev >= mons[newtype].mlevel && newtype != oldtype) {
        ptr = &mons[newtype];
        if (mvitals[newtype].mvflags & G_GENOD) {       /* allow G_EXTINCT */
            if (sensemon(mtmp))
                pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                      "As %s grows up into %s, %s %s!", mon_nam(mtmp),
                      an(mtmp->female ? ptr->fname : ptr->mname), mhe(mtmp),
                      nonliving(ptr) ? "expires" : "dies");
            set_mon_data(mtmp, ptr);        /* keep mvitals[] accurate */
            mondied(mtmp);
            return NULL;
        }
        set_mon_data(mtmp, ptr);      /* preserve intrinsics */
        if (mtmp->dlevel == level)
            newsym(mtmp->mx, mtmp->my);     /* color may change */
    }

    return ptr;
}

/*exper.c*/
