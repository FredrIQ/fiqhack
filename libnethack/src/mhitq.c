/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-28 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static void wildmiss(struct monst *, const struct attack *);

/* monster attacked your displaced image */
static void
wildmiss(struct monst *mtmp, const struct attack *mattk)
{
    int compat;

    /* no map_invisible() -- no way to tell where _this_ is coming from */

    if (!flags.verbose)
        return;
    if (!cansee(mtmp->mx, mtmp->my))
        return;
    /* maybe it's attacking an image around the corner? */

    compat = (mattk->adtyp == AD_SEDU || mattk->adtyp == AD_SSEX) &&
        could_seduce(mtmp, &youmonst, NULL);

    switch(awareness_reason(mtmp)) {

    case mar_guessing_other:

        if (Underwater) {
            /* monsters may miss especially on water level where bubbles shake
               the player here and there */
            if (compat)
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s reaches towards your distorted image.", Monnam(mtmp));
            else
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s is fooled by water reflections and misses!",
                      Monnam(mtmp));
            break;
        }
        /* otherwise fall through */
 
    case mar_guessing_invis:

        ;
        const char *swings =
            mattk->aatyp == AT_BITE ? "snaps" : mattk->aatyp ==
            AT_KICK ? "kicks" : (mattk->aatyp == AT_STNG ||
                                 mattk->aatyp == AT_BUTT ||
                                 nolimbs(mtmp->data)) ? "lunges" : "swings";

        if (compat)
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s tries to touch you and misses!", Monnam(mtmp));
        else
            switch (rn2(3)) {
            case 0:
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s %s wildly and misses!", Monnam(mtmp), swings);
                break;
            case 1:
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s attacks a spot beside you.", Monnam(mtmp));
                break;
            case 2:
                /* Note: mar_guessing_invis implies that the muxy is a) valid,
                   and b) wrong (from the monster's point of view). So we can
                   print this without needing any further sanity checks. */
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s strikes at %s!", Monnam(mtmp),
                      level->locations[mtmp->mux][mtmp->muy].typ ==
                      WATER ? "empty water" : "thin air");
                break;
            default:
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s %s wildly!", Monnam(mtmp), swings);
                break;
            }

        break;

    case mar_guessing_displaced:

        if (compat)
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s smiles %s at your %sdisplaced image...", Monnam(mtmp),
                  compat == 2 ? "engagingly" : "seductively",
                  Invis ? "invisible " : "");
        else
            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s strikes at your %sdisplaced image and misses you!",
                  /* Note: if you're both invisible and displaced, only
                     monsters which see invisible will attack your displaced
                     image, since the displaced image is also invisible. */
                  Monnam(mtmp), Invis ? "invisible " : "");

        break;

    case mar_unaware:
        impossible("%s attacks you without knowing your location?",
                   Monnam(mtmp));
        break;

    case mar_aware:
        impossible("%s misses wildly for no reason?", Monnam(mtmp));
        break;
    }
}


/* mattackq: monster attacks a square
   
   This function is called when the monster believes it's attacking something,
   but might potentially be mistaken about what's actually there. Returns 1 if
   the monster dies during the attack, 0 otherwise.

   The function handles the "empty space" case itself, and farms the other two
   possibilities out to mattacku and mattackm.

   The monster must be on the current level. Additionally, this function
   currently assumes that the monster believes it's attacking the player. */
int
mattackq(struct monst *mtmp, int x, int y)
{
    struct monst *mdef = m_at(level, x, y);
    const struct attack *mattk;
    int i;

    if (x == u.ux && y == u.uy)
        mdef = &youmonst;

    /* From monmove.c: handle monster counterattacks. */
    if (mdef) {

        notonhead = mdef && (mdef->mx != x || mdef->my != y);
        int mstatus = mattackm(mtmp, mdef) & MM_AGR_DIED;
        notonhead = 0;

        if (mstatus & MM_AGR_DIED)  /* aggressor died */
            return 2;
        
        if ((mstatus & MM_HIT) && !(mstatus & MM_DEF_DIED) && rn2(4)) {
            notonhead = 0;
            mstatus = mattackm(mdef, mtmp);        /* return attack */
            if (mstatus & MM_DEF_DIED)
                return 2;
        }
        return 3;
    }

    int sum[NATTK];
    struct attack alt_attk;

    for (i = 0; i < NATTK; i++) {

        sum[i] = 0;

        mattk = getmattk(mtmp->data, i, sum, &alt_attk);
        switch (mattk->aatyp) {
        case AT_CLAW:  /* "hand to hand" attacks */
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            wildmiss(mtmp, mattk);
            break;

        case AT_HUGS:  /* requires previous attacks to succeed */
            break;

        case AT_GAZE:
            if (canseemon(mtmp))
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s gazes into the distance.", Monnam(mtmp));
            break;

        case AT_EXPL:
            pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                  "%s explodes at a spot in %s!",
                  canseemon(mtmp) ? Monnam(mtmp) : "It",
                  level->locations[x][y].typ == WATER ?
                  "empty water" : "thin air");
            mtmp->mhp = 0;
            mondead(mtmp);

            /* TODO: Shouldn't there be some 'explosion effect on terrain'
               function? Might the player get caught in the explosion anyway? */
            break;

        case AT_ENGL:
            if (is_animal(mtmp->data)) {
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "%s gulps some air!", Monnam(mtmp));
            } else {
                if (canseemon(mtmp))
                    pline(combat_msgc(mtmp, &youmonst, cr_miss),
                          "%s lunges forward and recoils!", Monnam(mtmp));
                else
                    You_hear(msgc_levelsound, "a %s nearby.",
                             is_whirly(mtmp->data) ? "rushing noise" : "splat");
            }
            break;

        /* 3.4.3 aims at your real location regardless in these cases, due to a
           bug. Now we have breamq/spitmq, we can breathe at the believed
           location. And maybe hit anyway! */
        case AT_BREA:
            breamq(mtmp, x, y, mattk);
            break;

        case AT_SPIT:
            spitmq(mtmp, x, y, mattk);
            break;

        case AT_WEAP:
            if (distmin(mtmp->mx, mtmp->my, x, y) > 1) {
                if (!Is_rogue_level(&u.uz))
                    thrwmq(mtmp, x, y);
            } else {
                wildmiss(mtmp, mattk);
            }
            break;

        default:       /* no attack */
            break;
        }
    }

    /* If the monster thought the square was the player's location, it now knows
       it's wrong. This must come last, so as to not clobber the reason it's
       attacking thin air. */
    if (mtmp->mux == x && mtmp->muy == y) {
        mtmp->mux = COLNO;
        mtmp->muy = ROWNO;
    }
    if (mtmp->mstrategy == st_mon && is_goal(mtmp, x, y))
        mtmp->mstrategy = st_none;

    return DEADMONSTER(mtmp);
}

/* Called when a monster starts to intentionally attack something, regardless
   of whether that's the player, a monster, or empty space. Returns FALSE if
   the attack should be cancelled.

   The code here was moved here from mattacku (which ran in both the "player"
   and "empty space" codepaths in 4.3, but not the "monster" codepath). */
boolean
mpreattack(struct monst *magr, struct monst *mdef, boolean range2)
{
    const struct permonst *mdat = magr->data;

    if (DEADMONSTER(magr) || (mdef == &youmonst && Underwater &&
                              !swims(magr)))
        return FALSE;

    /* Special demon handling code */
    if (!magr->cham && is_demon(mdat) && !range2 && !magr->mtame &&
        !magr->mpeaceful &&
        magr->data != &mons[PM_BALROG] &&
        magr->data != &mons[PM_INCUBUS] &&
        !(m_mwep(mdef) && m_mwep(mdef)->oartifact == ART_DEMONBANE))
        if (!cancelled(magr) && !rn2(13))
            msummon(magr, &magr->dlevel->z);

    /* Special lycanthrope handling code */
    if (!magr->cham && is_were(mdat) && !range2) {

        if (is_human(mdat)) {
            if (!rn2(5 - (night() * 2)) && !cancelled(magr))
                new_were(magr);
        } else if (!rn2(30) && !cancelled(magr))
            new_were(magr);

        if (!rn2(10) && !cancelled(magr) && !magr->mtame) {
            int numseen, numhelp;
            const char *buf, *genericwere;

            genericwere = "creature";

            numhelp = were_summon(magr, &numseen, &genericwere);
            if (canseemon(magr)) {
                if (numhelp > 0) {
                    pline(combat_msgc(magr, NULL, cr_hit),
                          "%s summons help!", Monnam(magr));
                    if (numseen == 0)
                        pline(msgc_levelwarning, "You feel hemmed in.");
                } else
                    pline(combat_msgc(magr, NULL, cr_miss),
                          "%s summons help, but none comes.", Monnam(magr));
            } else {
                const char *from_nowhere;

                if (canhear()) {
                    pline(msgc_levelwarning, "Something %s!",
                          makeplural(growl_sound(magr)));
                    from_nowhere = "";
                } else
                    from_nowhere = " from nowhere";

                if (numhelp > 0) {
                    if (numseen < 1)
                        pline(msgc_levelwarning, "You feel hemmed in.");
                    else {
                        if (numseen == 1)
                            buf = msgprintf("%s appears", an(genericwere));
                        else
                            buf = msgprintf("%s appear",
                                            makeplural(genericwere));
                        pline(msgc_levelwarning, "%s%s!",
                              msgupcasefirst(buf), from_nowhere);
                    }
                }
                /* else no help came; but you didn't know it tried */
            }
        }
    }

    return TRUE;
}

/* mhitq.c */
