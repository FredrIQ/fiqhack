/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "emin.h"
#include "epri.h"

/* mon summons a monster

   TODO: It'd be nice to have custom RNGs for this, but it's unclear exactly
   what we'd be trying to keep consistent between games. */
void
msummon(struct monst *mon, const d_level *dlev)
{
    const struct permonst *ptr;
    int dtype = NON_PM, cnt = 0;
    aligntyp atyp;
    struct monst *mtmp;

    if (mon) {
        ptr = mon->data;
        if (dlev->dnum != mon->dlevel->z.dnum ||
            dlev->dlevel != mon->dlevel->z.dlevel)
            impossible("dlev mismatch for monster in msummon");
        atyp = (ptr->maligntyp == A_NONE) ? A_NONE : sgn(ptr->maligntyp);
        if (mon->ispriest || roamer_type(mon->data))
            atyp = CONST_EPRI(mon)->shralign;
    } else {
        ptr = &mons[PM_WIZARD_OF_YENDOR];
        atyp = (ptr->maligntyp == A_NONE) ? A_NONE : sgn(ptr->maligntyp);
        if (dlev->dnum != u.uz.dnum || dlev->dlevel != u.uz.dlevel)
            impossible("dlev mismatch for player in msummon");
    }

    if (is_dprince(ptr) || (ptr == &mons[PM_WIZARD_OF_YENDOR])) {
        dtype = (!rn2(20)) ? dprince(atyp) :
                (!rn2(4)) ? dlord(atyp) : ndemon(dlev, atyp);
        cnt = (!rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (is_dlord(ptr)) {
        dtype = (!rn2(50)) ? dprince(atyp) :
                (!rn2(20)) ? dlord(atyp) : ndemon(dlev, atyp);
        cnt = (!rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (is_ndemon(ptr)) {
        dtype = (!rn2(20)) ? dlord(atyp) :
                (!rn2(6)) ? ndemon(dlev, atyp) : monsndx(ptr);
        cnt = 1;
    } else if (mon && is_lminion(mon)) {
        dtype = (is_lord(ptr) && !rn2(20)) ? llord() :
                (is_lord(ptr) || !rn2(6)) ? lminion() : monsndx(ptr);
        cnt = (!rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    } else if (ptr == &mons[PM_ANGEL]) {
        /* non-lawful angels can also summon */
        if (!rn2(6)) {
            switch (atyp) {     /* see summon_minion */
            case A_NEUTRAL:
                dtype = PM_AIR_ELEMENTAL + rn2(4);
                break;
            case A_CHAOTIC:
            case A_NONE:
                dtype = ndemon(dlev, atyp);
                break;
            }
        } else {
            dtype = PM_ANGEL;
        }
        cnt = (!rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    }

    if (dtype == NON_PM)
        return;

    /* sanity checks */
    if (cnt > 1 && (mons[dtype].geno & G_UNIQ))
        cnt = 1;
    /* 
     * If this daemon is unique and being re-summoned (the only way we
     * could get this far with an extinct dtype), try another.
     */
    if (mvitals[dtype].mvflags & G_GONE) {
        dtype = ndemon(dlev, atyp);
        if (dtype == NON_PM)
            return;
    }

    while (cnt > 0) {
        mtmp = makemon(&mons[dtype], level, u.ux, u.uy,
                       MM_CREATEMONSTER | MM_CMONSTER_M);
        if (mtmp && roamer_type(&mons[dtype])) {
            /* alignment should match the summoner */
            EPRI(mtmp)->shralign = atyp;
        }
        cnt--;
    }
}

void
summon_minion(aligntyp alignment, boolean talk)
{
    struct monst *mon;
    int mnum;

    switch ((int)alignment) {
    case A_LAWFUL:
        mnum = lminion();
        break;
    case A_NEUTRAL:
        mnum = PM_AIR_ELEMENTAL + rn2(4);
        break;
    case A_CHAOTIC:
    case A_NONE:
        mnum = ndemon(&u.uz, alignment);
        break;
    default:
        impossible("unaligned player?");
        mnum = ndemon(&u.uz, A_NONE);
        break;
    }
    if (mnum == NON_PM) {
        mon = 0;
    } else if (mons[mnum].pxtyp == MX_NONE) {
        const struct permonst *pm = &mons[mnum];

        mon = makemon(pm, level, u.ux, u.uy, MM_EMIN);
        if (mon) {
            mon->isminion = TRUE;
            EMIN(mon)->min_align = alignment;
        }
    } else if (roamer_type(&mons[mnum])) {
        mon = makemon(&mons[mnum], level, u.ux, u.uy, NO_MM_FLAGS);
        if (mon) {
            mon->isminion = TRUE;
            EPRI(mon)->shralign = alignment;
        }
    } else
        mon = makemon(&mons[mnum], level, u.ux, u.uy, NO_MM_FLAGS);
    if (mon) {
        if (talk) {
            pline(msgc_npcvoice, "The voice of %s booms:",
                  align_gname(alignment));
            verbalize(msgc_alignbad, "Thou shalt pay for thy indiscretion!");
            if (!Blind)
                pline(msgc_levelwarning, "%s appears before you.",
                      Amonnam(mon));
        }
        msethostility(mon, TRUE, FALSE);
        /* don't call set_malign(); player was naughty */
    }
}

/* Returns "mortal", "creature" (if allow_creature), or a more specific term.
   This is mostly used by immortal/unalive entities when talking to other
   entities. If they're talking /down/, "creature" is normally inappropriate
   (e.g. "laughing at cowardly creatures"). If they're merely addressing the
   permonst in question, "creature" is typically fine (e.g. "Hark, creature!").
   Undead can be addressed as "creature"; constructs and demons won't be. */
const char *
mortal_or_creature(const struct permonst *data, boolean allow_creature)
{
    if (data->mlet == S_HUMAN)
        return "mortal";
    else if (allow_creature && (!nonliving(data) || is_undead(data)) &&
             !is_demon(data))
        return "creature";
    else if (!nonliving(data) && !is_demon(data))
        return "mortal";
    else if (is_demon(data) || monsndx(data) == PM_MANES)
        return "demon";
    else if (nonliving(data) && !is_undead(data))
        return "construct"; /* golems and vortices */
    else if (data->mlet == S_LICH)
        return "lich";
    else if (data->mlet == S_MUMMY)
        return "mummy";
    else if (data->mlet == S_VAMPIRE)
        return "vampire";
    else if (data->mlet == S_ZOMBIE &&
             monsndx(data) != PM_GHOUL && monsndx(data) != PM_SKELETON)
        return "zombie";
    else
        return data->mname;
}


#define Athome (Inhell && !mtmp->cham)

/* returns 1 if it won't attack. */
int
demon_talk(struct monst *mtmp)
{
    long cash, demand, offer;

    if (uwep && uwep->oartifact == ART_EXCALIBUR) {
        pline(msgc_npcanger, "%s looks very angry.", Amonnam(mtmp));
        msethostility(mtmp, TRUE, TRUE);
        return 0;
    }

    /* Slight advantage given. */
    if (is_dprince(mtmp->data) && mtmp->minvis) {
        mtmp->minvis = mtmp->perminvis = 0;
        if (!Blind)
            pline(msgc_levelwarning, "%s appears before you.", Amonnam(mtmp));
        newsym(mtmp->mx, mtmp->my);
    }
    if (youmonst.data->mlet == S_DEMON) {       /* Won't blackmail their own. */
        pline(msgc_npcvoice, "%s says, \"Good hunting, %s.\"", Amonnam(mtmp),
              u.ufemale ? "Sister" : "Brother");
        if (!tele_restrict(mtmp))
            rloc(mtmp, TRUE);
        return 1;
    }
    cash = money_cnt(invent);
    /* don't bother with a custom RNG here, too much unpredictability is
       involved */
    demand = (cash * (rnd(80) + 20 * Athome)) /
        (100 * (1 + (sgn(u.ualign.type) == sgn(mtmp->data->maligntyp))));

    if (!demand) {      /* you have no gold */
        msethostility(mtmp, TRUE, TRUE);
        return 0;
    } else {
        /* make sure that the demand is unmeetable if the monster has the
           Amulet, preventing monster from being satisified and removed from
           the game (along with said Amulet...) */
        if (mon_has_amulet(mtmp))
            demand = cash + (long)rn1(1000, 40);

        pline(msgc_unpaid, "%s demands %ld %s for safe passage.",
              Amonnam(mtmp), demand, currency(demand));

        if ((offer = bribe(mtmp)) >= demand) {
            pline(msgc_moncombatgood,
                  "%s vanishes, laughing about cowardly %s.", Amonnam(mtmp),
                  makeplural(mortal_or_creature(youmonst.data, FALSE)));
        } else if (offer > 0L && (long)rnd(40) > (demand - offer)) {
            pline(msgc_moncombatgood,
                  "%s scowls at you menacingly, then vanishes.", Amonnam(mtmp));
        } else {
            pline(msgc_npcanger, "%s gets angry...", Amonnam(mtmp));
            msethostility(mtmp, TRUE, TRUE);
            return 0;
        }
    }
    mongone(mtmp);
    return 1;
}


long
bribe(struct monst *mtmp)
{
    const char *buf;
    long offer;
    long umoney = money_cnt(invent);

    buf = getlin("How much will you offer?", FALSE);
    if (sscanf(buf, "%ld", &offer) != 1)
        offer = 0L;

    /* Michael Paddon -- fix for negative offer to monster */
    /* JAR880815 - */
    if (offer < 0L) {
        pline(msgc_substitute, "You try to shortchange %s, but fumble.",
              mon_nam(mtmp));
        return 0L;
    } else if (offer == 0L) {
        pline(msgc_yafm, "You refuse.");
        return 0L;
    } else if (umoney == 0L) {
        pline(msgc_yafm, "You open your purse, but realize you have no gold.");
        return 0L;
    } else if (offer >= umoney) {
        pline(msgc_actionok, "You give %s all your gold.", mon_nam(mtmp));
        offer = umoney;
    } else {
        pline(msgc_actionok, "You give %s %ld %s.", mon_nam(mtmp), offer,
              currency(offer));
    }
    money2mon(mtmp, offer);

    return offer;
}


int
dprince(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = 0; tryct < 20; tryct++) {
        pm = rn1(PM_DEMOGORGON + 1 - PM_ORCUS, PM_ORCUS);
        if (!(mvitals[pm].mvflags & G_GONE) &&
            (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return dlord(atyp); /* approximate */
}

int
dlord(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = 0; tryct < 20; tryct++) {
        pm = rn1(PM_YEENOGHU + 1 - PM_JUIBLEX, PM_JUIBLEX);
        if (!(mvitals[pm].mvflags & G_GONE) &&
            (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return ndemon(&u.uz, atyp); /* approximate */
}

/* create lawful (good) lord */
int
llord(void)
{
    if (!(mvitals[PM_ARCHON].mvflags & G_GONE))
        return PM_ARCHON;

    return lminion();   /* approximate */
}

int
lminion(void)
{
    int tryct;
    const struct permonst *ptr;

    for (tryct = 0; tryct < 20; tryct++) {
        ptr = mkclass(&u.uz, S_ANGEL, G_HELL | G_NOHELL, rng_main);
        if (ptr && !is_lord(ptr))
            return monsndx(ptr);
    }

    return NON_PM;
}

int
ndemon(const d_level * dlev, aligntyp atyp)
{
    int tryct;
    const struct permonst *ptr;

    for (tryct = 0; tryct < 20; tryct++) {
        ptr = mkclass(dlev, S_DEMON, G_HELL | G_NOHELL, rng_main);
        if (ptr && is_ndemon(ptr) &&
            (atyp == A_NONE || sgn(ptr->maligntyp) == sgn(atyp)))
            return monsndx(ptr);
    }

    return NON_PM;
}

/*minion.c*/

