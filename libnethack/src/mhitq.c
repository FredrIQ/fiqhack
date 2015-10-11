/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
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
                pline("%s reaches towards your distorted image.", Monnam(mtmp));
            else
                pline("%s is fooled by water reflections and misses!",
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
            pline("%s tries to touch you and misses!", Monnam(mtmp));
        else
            switch (rn2(3)) {
            case 0:
                pline("%s %s wildly and misses!", Monnam(mtmp), swings);
                break;
            case 1:
                pline("%s attacks a spot beside you.", Monnam(mtmp));
                break;
            case 2:
                /* Note: mar_guessing_invis implies that the muxy is a) valid,
                   and b) wrong (from the monster's point of view). So we can
                   print this without needing any further sanity checks. */
                pline("%s strikes at %s!", Monnam(mtmp),
                      level->locations[mtmp->mux][mtmp->muy].typ ==
                      WATER ? "empty water" : "thin air");
                break;
            default:
                pline("%s %s wildly!", Monnam(mtmp), swings);
                break;
            }

        break;

    case mar_guessing_displaced:

        if (compat)
            pline("%s smiles %s at your %sdisplaced image...", Monnam(mtmp),
                  compat == 2 ? "engagingly" : "seductively",
                  Invis ? "invisible " : "");
        else
            pline("%s strikes at your %sdisplaced image and misses you!",
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

    mpreattack(mtmp, distmin(mtmp->mx, mtmp->my, x, y) > 1);

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
                pline("%s gazes into the distance.", Monnam(mtmp));
            break;

        case AT_EXPL:
            pline("%s explodes at a spot in %s!",
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
                pline("%s gulps some air!", Monnam(mtmp));
            } else {
                if (canseemon(mtmp))
                    pline("%s lunges forward and recoils!", Monnam(mtmp));
                else
                    You_hear("a %s nearby.",
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

        case AT_MAGC:
            castmu(mtmp, mattk, -1);
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
    if (mtmp->mstrategy & STRAT_PLAYER &&
        STRAT_GOALX(mtmp->mstrategy) == x && STRAT_GOALY(mtmp->mstrategy) == y)
        mtmp->mstrategy = STRAT_NONE;

    return DEADMONSTER(mtmp);
}

/* Called when a monster starts to intentionally attack something, regardless
   of whether that's the player, a monster, or empty space. Returns FALSE if
   the attack should be cancelled.

   The code here was moved here from mattacku (which ran in both the "player"
   and "empty space" codepaths in 4.3, but not the "monster" codepath). */
boolean
mpreattack(struct monst *mtmp, boolean range2)
{
    const struct permonst *mdat = mtmp->data;

    if (DEADMONSTER(mtmp) || (Underwater && !is_swimmer(mtmp->data)))
        return FALSE;

    /* Special demon handling code */
    if (!mtmp->cham && is_demon(mdat) && !range2 && !mtmp->mtame &&
        mtmp->data != &mons[PM_BALROG]
        && mtmp->data != &mons[PM_SUCCUBUS]
        && mtmp->data != &mons[PM_INCUBUS])
        if (!mtmp->mcan && !rn2(13))
            msummon(mtmp, &mtmp->dlevel->z);

    /* Special lycanthrope handling code */
    if (!mtmp->cham && is_were(mdat) && !range2) {

        if (is_human(mdat)) {
            if (!rn2(5 - (night() * 2)) && !mtmp->mcan)
                new_were(mtmp);
        } else if (!rn2(30) && !mtmp->mcan)
            new_were(mtmp);

        if (!rn2(10) && !mtmp->mcan && !mtmp->mtame) {
            int numseen, numhelp;
            const char *buf, *genericwere;

            genericwere = "creature";

            numhelp = were_summon(mtmp, &numseen, &genericwere);
            if (canseemon(mtmp)) {
                pline("%s summons help!", Monnam(mtmp));
                if (numhelp > 0) {
                    if (numseen == 0)
                        pline("You feel hemmed in.");
                } else
                    pline("But none comes.");
            } else {
                const char *from_nowhere;

                if (canhear()) {
                    pline("Something %s!", makeplural(growl_sound(mtmp)));
                    from_nowhere = "";
                } else
                    from_nowhere = " from nowhere";

                if (numhelp > 0) {
                    if (numseen < 1)
                        pline("You feel hemmed in.");
                    else {
                        if (numseen == 1)
                            buf = msgprintf("%s appears", an(genericwere));
                        else
                            buf = msgprintf("%s appear",
                                            makeplural(genericwere));
                        pline("%s%s!", msgupcasefirst(buf), from_nowhere);
                    }
                }
                /* else no help came; but you didn't know it tried */
            }
        }
    }

    return TRUE;
}

/* mhitq.c */
