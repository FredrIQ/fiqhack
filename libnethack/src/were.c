/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

void
were_change(struct monst *mon)
{
    if (!is_were(mon->data))
        return;

    if (is_human(mon->data)) {
        if (!Protection_from_shape_changers &&
            !rn2(night()? (flags.moonphase == FULL_MOON ? 3 : 30)
                 : (flags.moonphase == FULL_MOON ? 10 : 50))) {
            new_were(mon);      /* change into animal form */
            if (!canseemon(mon)) {
                const char *howler;

                switch (monsndx(mon->data)) {
                case PM_WEREWOLF:
                    howler = "wolf";
                    break;
                case PM_WEREJACKAL:
                    howler = "jackal";
                    break;
                default:
                    howler = NULL;
                    break;
                }
                if (howler)
                    You_hear(msgc_levelsound, "a %s howling at the moon.",
                             howler);
            }
        }
    } else if (!rn2(30) || Protection_from_shape_changers) {
        new_were(mon);  /* change back into human form */
    }
}


static int counter_were(int);

static int
counter_were(int pm)
{
    switch (pm) {
    case PM_WEREWOLF:
        return PM_HUMAN_WEREWOLF;
    case PM_HUMAN_WEREWOLF:
        return PM_WEREWOLF;
    case PM_WEREJACKAL:
        return PM_HUMAN_WEREJACKAL;
    case PM_HUMAN_WEREJACKAL:
        return PM_WEREJACKAL;
    case PM_WERERAT:
        return PM_HUMAN_WERERAT;
    case PM_HUMAN_WERERAT:
        return PM_WERERAT;
    default:
        return 0;
    }
}

void
new_were(struct monst *mon)
{
    int pm;

    pm = counter_were(monsndx(mon->data));
    if (!pm) {
        impossible("unknown lycanthrope %s.", mon->data->mname);
        return;
    }

    if (canseemon(mon) && !Hallucination)
        pline(msgc_monneutral, "%s changes into a %s.", Monnam(mon),
              is_human(&mons[pm]) ? "human" : mons[pm].mname + 4);

    set_mon_data(mon, &mons[pm], 0);
    if (mon->msleeping || !mon->mcanmove) {
        /* transformation wakens and/or revitalizes */
        mon->msleeping = 0;
        mon->mfrozen = 0;       /* not asleep or paralyzed */
        mon->mcanmove = 1;
    }
    /* regenerate by 1/4 of the lost hit points */
    mon->mhp += (mon->mhpmax - mon->mhp) / 4;
    newsym(mon->mx, mon->my);
    mon_break_armor(mon, FALSE);
    possibly_unwield(mon, FALSE);
}

/* were-creature (even you) summons a horde */
int
were_summon(struct monst *msummoner,
            int *visible,    /* number of visible helpers created */
            const char **genbuf)
{
    int i, typ, pm = monsndx(msummoner->data);
    struct monst *mtmp;
    int total = 0;

    *visible = 0;
    if (Protection_from_shape_changers && msummoner != &youmonst)
        return 0;
    for (i = rnd(5); i > 0; i--) {
        switch (pm) {

        case PM_WERERAT:
        case PM_HUMAN_WERERAT:
            typ = rn2(3) ? PM_SEWER_RAT : rn2(3) ? PM_GIANT_RAT : PM_RABID_RAT;
            if (genbuf)
                *genbuf = "rat";
            break;
        case PM_WEREJACKAL:
        case PM_HUMAN_WEREJACKAL:
            typ = PM_JACKAL;
            if (genbuf)
                *genbuf = "jackal";
            break;
        case PM_WEREWOLF:
        case PM_HUMAN_WEREWOLF:
            typ = rn2(5) ? PM_WOLF : PM_WINTER_WOLF;
            if (genbuf)
                *genbuf = "wolf";
            break;
        default:
            continue;
        }
        mtmp = makemon(&mons[typ], msummoner == &youmonst ? 
                       level : msummoner->dlevel,
                       m_mx(msummoner), m_my(msummoner), MM_ADJACENTOK);
        if (mtmp) {
            total++;
            if (cansuspectmon(mtmp))
                *visible += 1;
        }
        if (msummoner == &youmonst && mtmp)
            tamedog(mtmp, NULL);
    }
    return total;
}

void
you_were(void)
{
    const char *qbuf;

    if (Unchanging || (u.umonnum == u.ulycn))
        return;
    if (Polymorph_control) {
        /* `+4' => skip "were" prefix to get name of beast */
        qbuf = msgprintf("Do you want to change into %s? ",
                         an(mons[u.ulycn].mname + 4));
        if (yn(qbuf) == 'n')
            return;
    }
    polymon(u.ulycn, TRUE);
}

void
you_unwere(boolean purify)
{
    if (purify) {
        pline(msgc_statusheal, "You feel purified.");
        u.ulycn = NON_PM;       /* cure lycanthropy */
    }
    if (!Unchanging && is_were(youmonst.data) &&
        (!Polymorph_control || yn("Remain in beast form?") == 'n'))
        rehumanize(DIED, NULL);
}

/*were.c*/

