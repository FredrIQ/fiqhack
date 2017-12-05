/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-05 */
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

    int hp_loss = rnd(8);
    hp_loss = min(mdef->mhpmax - 1, hp_loss);
    mdef->mhpmax -= hp_loss;
    mdef->mhp -= min(mdef->mhp - 1, hp_loss);

    int pw_loss = mon_pw_gain(mdef);
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

/* Gives monster appropriate Pw for its level.
   Yes, a monster at XL0 has 0 Pw. */
void
initialize_mon_pw(struct monst *mon)
{
    int xl = mon->m_lev;
    int pw_gain = 0;
    while (xl--) {
        pw_gain = mon_pw_gain(mon);
        mon->pw += pw_gain;
        mon->pwmax += pw_gain;
    }
}

/* Returns an energy gain (or loss) for when a monster gains or loses
   a level. Player equavilents:
   potential is generally wis/2 + 1, but can be + 2 for casting roles,
   might be different based on a special level cutoff value.
   min is the same as below (except no M3_SPELLCASTER/nymph/iswiz case) */
static int
mon_pw_gain(const struct monst *mon)
{
    int potential, min, res;
    res = 0;

    potential = acurr(mon, A_WIS) / 2;
    potential++;
    if (mon->data->mflags3 & M3_SPELLCASTER)
        potential++;

    min = 1;

    if (is_dwarf(mon->data))
        min = 0;
    else if (is_human(mon->data) || is_gnome(mon->data) ||
             (mon->data->mflags3 & M3_SPELLCASTER) ||
             mon->data->mlet == S_NYMPH)
        min = 2;
    else if (is_elf(mon->data) || mon->iswiz ||
             mon->data->mlet == S_DRAGON)
        min = 3;

    res += rn1(potential, min);
    res = menermod(mon, res);
    return res;
}

const struct permonst *
grow_up(struct monst *mtmp,   /* `mtmp' might "grow up" into a bigger version */
        struct monst *victim)
{
    int oldtype, newtype, max_increase, cur_increase, lev_limit, hp_threshold;
    const struct permonst *ptr = mtmp->data;

    /* monster died after killing enemy but before calling this function */
    /* currently possible if killing a gas spore */
    if (DEADMONSTER(mtmp))
        return NULL;

    /* note: none of the monsters with special hit point calculations have both
       little and big forms */
    oldtype = monsndx(ptr);
    newtype = little_to_big(oldtype);

    /* growth limits differ depending on method of advancement */
    if (victim) {       /* killed a monster */
        /*
         * The HP threshold is the maximum number of hit points for the
         * current level; once exceeded, a level will be gained.
         * Possible bug: if somehow the hit points are already higher
         * than that, monster will gain a level without any increase in HP.
         */
        hp_threshold = mtmp->m_lev * 8; /* normal limit */
        if (!mtmp->m_lev)
            hp_threshold = 4;
        else if (is_golem(ptr)) /* strange creatures */
            hp_threshold = ((mtmp->mhpmax / 10) + 1) * 10 - 1;
        else if (is_home_elemental(&mtmp->dlevel->z, ptr))
            hp_threshold *= 3;
        lev_limit = 3 * (int)ptr->mlevel / 2;   /* same as adj_lev() */
        /* If they can grow up, be sure the level is high enough for that */
        if (oldtype != newtype && mons[newtype].mlevel > lev_limit)
            lev_limit = (int)mons[newtype].mlevel;
        /* number of hit points to gain; unlike for the player, we put the
           limit at the bottom of the next level rather than the top */
        max_increase = rnd((int)victim->m_lev + 1);
        if (mtmp->mhpmax + max_increase > hp_threshold + 1)
            max_increase = max((hp_threshold + 1) - mtmp->mhpmax, 0);
        cur_increase = (max_increase > 1) ? rn2(max_increase) : 0;
    } else {
        /* a gain level potion or wraith corpse; always go up a level unless
           already at maximum (49 is hard upper limit except for demon lords,
           who start at 50 and can't go any higher) */
        max_increase = cur_increase = rnd(8);
        hp_threshold = 0;       /* smaller than `mhpmax + max_increase' */
        lev_limit = 50; /* recalc below */
    }

    mtmp->mhpmax += max_increase;
    mtmp->mhp += cur_increase;
    if (mtmp->mhpmax <= hp_threshold)
        return ptr;     /* doesn't gain a level */

    if (is_mplayer(ptr))
        lev_limit = 30; /* same as player */
    else if (lev_limit < 5)
        lev_limit = 5;  /* arbitrary */
    else if (lev_limit > 49)
        lev_limit = (ptr->mlevel > 49 ? 50 : 49);

    if ((int)++mtmp->m_lev >= mons[newtype].mlevel && newtype != oldtype) {
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
        lev_limit = (int)mtmp->m_lev;   /* never undo increment */

        int pw_gain = mon_pw_gain(mtmp);
        mtmp->pw += pw_gain;
        mtmp->pwmax += pw_gain;
    }
    /* sanity checks */
    if ((int)mtmp->m_lev > lev_limit) {
        mtmp->m_lev--;  /* undo increment */
        /* HP might have been allowed to grow when it shouldn't */
        if (mtmp->mhpmax == hp_threshold + 1)
            mtmp->mhpmax--;
    }
    if (mtmp->mhpmax > 50 * 8)
        mtmp->mhpmax = 50 * 8;  /* absolute limit */
    if (mtmp->mhp > mtmp->mhpmax)
        mtmp->mhp = mtmp->mhpmax;

    return ptr;
}

/*exper.c*/
