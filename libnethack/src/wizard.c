/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* wizard code - inspired by rogue code from Merlyn Leroy (digi-g!brian) */
/*             - heavily modified to give the wiz balls.  (genat!mike)   */
/*             - dewimped and given some maledictions. -3. */
/*             - generalized for 3.1 (mike@bullns.on01.bull.ca) */

#include "hack.h"
#include "qtext.h"

extern const int monstr[];

static short which_arti(int);
static struct monst *other_mon_has_arti(struct monst *, short);
static struct obj *on_ground(short, xchar, xchar, const struct monst *);
static boolean you_have(int);

static const int nasties[] = {
    PM_COCKATRICE, PM_ETTIN, PM_STALKER, PM_MINOTAUR, PM_RED_DRAGON,
    PM_BLACK_DRAGON, PM_GREEN_DRAGON, PM_OWLBEAR, PM_PURPLE_WORM,
    PM_ROCK_TROLL, PM_XAN, PM_GREMLIN, PM_UMBER_HULK, PM_VAMPIRE_LORD,
    PM_XORN, PM_ZRUTY, PM_ELF_LORD, PM_ELVENKING, PM_YELLOW_DRAGON,
    PM_LEOCROTTA, PM_BALUCHITHERIUM, PM_CARNIVOROUS_APE, PM_FIRE_GIANT,
    PM_COUATL, PM_CAPTAIN, PM_WINGED_GARGOYLE, PM_MASTER_MIND_FLAYER,
    PM_FIRE_ELEMENTAL, PM_JABBERWOCK, PM_ARCH_LICH, PM_OGRE_KING,
    PM_OLOG_HAI, PM_IRON_GOLEM, PM_OCHRE_JELLY, PM_GREEN_SLIME,
    PM_DISENCHANTER
};

static const unsigned wizapp[] = {
    PM_HUMAN, PM_WATER_DEMON, PM_VAMPIRE,
    PM_RED_DRAGON, PM_TROLL, PM_UMBER_HULK,
    PM_XORN, PM_XAN, PM_COCKATRICE,
    PM_FLOATING_EYE,
    PM_GUARDIAN_NAGA,
    PM_TRAPPER
};


/* If you've found the Amulet, make the Wizard appear after some time */
/* Also, give hints about portal locations, if amulet is worn/wielded -dlc */
/* pre condition: Uhave_amulet == 1 */
void
amulet(void)
{
    struct monst *mtmp;
    struct trap *ttmp;
    struct obj *amu;

    if ((((amu = uamul) != 0 && amu->otyp == AMULET_OF_YENDOR) ||
         ((amu = uwep) != 0 && amu->otyp == AMULET_OF_YENDOR))
        && !rn2(15)) {
        for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap) {
            if (ttmp->ttyp == MAGIC_PORTAL) {
                int du = distu(ttmp->tx, ttmp->ty);

                if (du <= 9)
                    pline(msgc_hint, "%s hot!", Tobjnam(amu, "feel"));
                else if (du <= 64)
                    pline(msgc_hint, "%s very warm.", Tobjnam(amu, "feel"));
                else if (du <= 144)
                    pline(msgc_hint, "%s warm.", Tobjnam(amu, "feel"));
                /* else, the amulet feels normal */
                break;
            }
        }
    }

    if (!flags.no_of_wizards)
        return;
    /* find Wizard, and wake him if necessary */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp) && mtmp->iswiz && mtmp->msleeping && !rn2(40)) {
            mtmp->msleeping = 0;
            if (distu(mtmp->mx, mtmp->my) > 2)
                pline(msgc_levelwarning, "You get the creepy feeling that "
                      "somebody noticed you taking the Amulet.");
            return;
        }
}


int
mon_has_amulet(const struct monst *mtmp)
{
    struct obj *otmp;

    for (otmp = m_minvent(mtmp); otmp; otmp = otmp->nobj)
        if (otmp->otyp == AMULET_OF_YENDOR)
            return 1;
    return 0;
}

int
mon_has_special(const struct monst *mtmp)
{
    struct obj *otmp;

    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == AMULET_OF_YENDOR || is_quest_artifact(otmp) ||
            otmp->otyp == BELL_OF_OPENING ||
            otmp->otyp == CANDELABRUM_OF_INVOCATION ||
            otmp->otyp == SPE_BOOK_OF_THE_DEAD)
            return 1;
    return 0;
}

/*
 * New for 3.1  Strategy / Tactics for the wiz, as well as other
 * monsters that are "after" something (defined via mflag3).
 *
 * The strategy section decides *what* the monster is going
 * to attempt, the tactics section implements the decision.
 */
#define M_Wants(mask)   (mtmp->data->mflags3 & (mask))

static short
which_arti(int mask)
{
    switch (mask) {
    case M3_WANTSAMUL:
        return AMULET_OF_YENDOR;
    case M3_WANTSBELL:
        return BELL_OF_OPENING;
    case M3_WANTSCAND:
        return CANDELABRUM_OF_INVOCATION;
    case M3_WANTSBOOK:
        return SPE_BOOK_OF_THE_DEAD;
    default:
        break;  /* 0 signifies quest artifact */
    }
    return 0;
}

/*
 * If "otyp" is zero, it triggers a check for the quest_artifact,
 * since bell, book, candle, and amulet are all objects, not really
 * artifacts right now.    [MRS]
 */
boolean
mon_has_arti(const struct monst *mtmp, short otyp)
{
    struct obj *otmp;

    for (otmp = m_minvent(mtmp); otmp; otmp = otmp->nobj) {
        if (otyp) {
            if (otmp->otyp == otyp)
                return 1;
        } else if (is_quest_artifact(otmp))
            return 1;
    }
    return 0;

}

/* Used by msensem: can the viewer detect the viewee via being covetous for an
   item the viewee holds? */
boolean
covetous_sense(const struct monst *viewer, const struct monst *viewee)
{
    if (viewer->data->mflags3 & M3_WANTSAMUL &&
        mon_has_arti(viewee, AMULET_OF_YENDOR))
        return TRUE;
    if (viewer->data->mflags3 & M3_WANTSBELL &&
        mon_has_arti(viewee, BELL_OF_OPENING))
        return TRUE;
    if (viewer->data->mflags3 & M3_WANTSCAND &&
        mon_has_arti(viewee, CANDELABRUM_OF_INVOCATION))
        return TRUE;
    if (viewer->data->mflags3 & M3_WANTSBOOK &&
        mon_has_arti(viewee, SPE_BOOK_OF_THE_DEAD))
        return TRUE;
    if (viewer->data->mflags3 & M3_WANTSARTI && mon_has_arti(viewee, 0))
        return TRUE;

    return FALSE;
}

static struct monst *
other_mon_has_arti(struct monst *mtmp, short otyp)
{
    struct monst *mtmp2;

    for (mtmp2 = level->monlist; mtmp2; mtmp2 = mtmp2->nmon)
        /* no need for !DEADMONSTER check here since they have no inventory */
        if (mtmp2 != mtmp)
            if (mon_has_arti(mtmp2, otyp))
                return mtmp2;

    return NULL;
}

static struct obj *
on_ground(short otyp, xchar x, xchar y, const struct monst *mtmp)
{
    struct obj *otmp;

    if (otyp) {
        for (otmp = level->objlist; otmp; otmp = otmp->nobj)
            if (otyp) {
                if (otmp->otyp == otyp)
                    return otmp;
            } else if (is_quest_artifact(otmp))
                return otmp;
        return NULL;
    }
    for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere) {
        if ((otmp->otyp == AMULET_OF_YENDOR && M_Wants(M3_WANTSAMUL)) ||
            (otmp->otyp == BELL_OF_OPENING && M_Wants(M3_WANTSBELL)) ||
            (otmp->otyp == CANDELABRUM_OF_INVOCATION && M_Wants(M3_WANTSCAND)) ||
            (otmp->otyp == SPE_BOOK_OF_THE_DEAD && M_Wants(M3_WANTSBOOK)) ||
            (is_quest_artifact(otmp) && M_Wants(M3_WANTSARTI)))
            return otmp;
    }
    return NULL;
}

static boolean
you_have(int mask)
{
    switch (mask) {
    case M3_WANTSAMUL:
        return !!Uhave_amulet;
    case M3_WANTSBELL:
        return !!Uhave_bell;
    case M3_WANTSCAND:
        return !!Uhave_menorah;
    case M3_WANTSBOOK:
        return !!Uhave_book;
    case M3_WANTSARTI:
        return !!Uhave_questart;
    default:
        break;
    }
    return 0;
}

boolean
target_on(int mask, struct monst *mtmp)
{
    short otyp;
    struct obj *otmp;
    struct monst *mtmp2;

    if (!M_Wants(mask))
        return FALSE;

    otyp = which_arti(mask);
    if (!mon_has_arti(mtmp, otyp)) {
        if (you_have(mask)) {
            mtmp->mstrategy = st_mon;
            mtmp->sx = u.ux;
            mtmp->sy = u.uy;
        } else if ((otmp = on_ground(otyp, COLNO, ROWNO, NULL))) {
            /* Special case: if a meditating monster is standing on the item,
               act like that item hasn't spawned. Otherwise randomly generated
               liches will try to beat up the Wizard of Yendor. */
            mtmp2 = m_at(level, otmp->ox, otmp->oy);
            if (!mtmp2 || !idle(mtmp2)) {
                mtmp->mstrategy = st_obj;
                mtmp->sx = otmp->ox;
                mtmp->sy = otmp->oy;
            }
        } else if ((mtmp2 = other_mon_has_arti(mtmp, otyp))) {
            mtmp->mstrategy = st_mon;
            mtmp->sx = mtmp2->mx;
            mtmp->sy = mtmp2->my;
        } else
            return FALSE;
        return TRUE;
    }
    return FALSE;
}

/* Work out what this monster wants to be doing, and set its mstrategy field
   appropriately. magical_target is set if the monster should be magically aware
   of your position (typically due to someone casting "aggravate").

   Covetous monsters are automatically aware of the location of any items that
   they specifically want. They aren't necessarily automatically able to reach
   that location, though.

   For NetHack 4.3, this routine now applies to /all/ monsters. Monsters which
   are unaware of you (i.e. mux, muy are more than one square from your actual
   location) will typically keep doing whatever they were doing beforehand,
   unless it becomes clearly impossible, or they covetous-sense an artifact or
   invocation item.

   This function is about what the monster wants to be doing in general terms,
   not necessarily what it wants to do right now (i.e. long-term strategy). Its
   main purpose is so that monsters that are unaware of the player keep doing
   what they were doing beforehand, rather than milling around randomly. (For
   instance, if a monster starts chasing the player, and the player runs away
   out of LOS, the monster will aim for the last square it saw the player
   on.) */
void
strategy(struct monst *mtmp, boolean magical_target)
{
    boolean chases_player = !mtmp->mpeaceful || mx_eshk(mtmp) || mtmp->mtame;

    set_apparxy(mtmp);

    /* Is the monster waiting for something? */
    if (idle(mtmp))
        return;

    /* If the monster is heading for ascension, don't change the mind for anything
       but losing the amulet */
    if (mtmp->mstrategy == st_ascend) {
        if (mon_has_amulet(mtmp)) {
            /* regain courage immediately */
            mtmp->mflee = 0;
            mtmp->mfleetim = 0;
            return;
        } else {
            /* reset strategy */
            mtmp->mstrategy = st_none;
        }
    }

    /* Leprechaun special AI considerations, moved here from m_move.  These give
       an "escaping" status; like fleeing but it wears off the instant the
       conditions that caused it aren't met. */
    struct obj *lepgold, *ygold;
    int lepescape = FALSE;
    if (monsndx(mtmp->data) == PM_LEPRECHAUN &&
        ((lepgold = findgold(mtmp->minvent)) &&
         (lepgold->quan > ((ygold = findgold(invent)) ? ygold->quan : 0L))))
        lepescape = TRUE;

    /* Below half health, or when fleeing, we ignore what we were previously
       doing, and change to escape mode instead. Unless we were escaping, in
       which case, continue doing that. Note that in this case, the strategy
       goal coordinates are the square that the monster is trying to get away
       from, not the square it's running towards.

       st_escape (escaping) is not the same as mflee (fleeing). Escaping is
       basically a case of "heal up, or get out of here"; it's a rational
       approach to being on low HP. Fleeing is running away in a blind panic,
       e.g. as a result of being hit by a scare monster spell, although it can
       also happen by nonmagical means (some monsters just like panicking).

       Note that a monster's reaction to st_escape is not necessarily to run
       away from the target square; it might also heal up, use escape items,
       etc.. */
    if (mtmp->mhp * (mtmp->data == &mons[PM_WIZARD_OF_YENDOR] ? 3 : 2) <
        mtmp->mhpmax || mtmp->mflee || lepescape) {

        /* An escaping pet instead moves towards the player, if it can.
           (dog_move will reverse this direction if fleeing in a panic.) */
        if (mtmp->mtame && aware_of_u(mtmp) && !engulfing_u(mtmp)) {
            mtmp->mstrategy = st_mon;
            mtmp->sx = mtmp->mux;
            mtmp->sy = mtmp->muy;
            return;
        }

        /* If the monster thinks it can see the player, it escapes from the
           location it believes the player to be standing at. Because we just
           called set_apparxy, the monster thinks it knows where the player is
           iff mux/muy differ from mx/my. Exception: a peaceful monster has no
           reason to be afraid of the player in particular (if the player had
           taken an aggressive action, they'd no longer be flagged as
           peaceful). */
        if (!mtmp->mpeaceful && aware_of_u(mtmp) && !engulfing_u(mtmp)) {
            mtmp->mstrategy = st_escape;
            mtmp->sx = mtmp->mux;
            mtmp->sy = mtmp->muy;
        }

        /* Does the monster already have a valid square to escape from? */
        if (mtmp->mstrategy == st_escape)
            return;

        /* The monster has just started to escape; however, it doesn't know what
           it's escaping from. This is because the player sniped it from out of
           its vision range with a ranged attack, because it was attacked by
           another monster, because it stepped on a trap, and possibly other
           reasons. Escaping from its current location makes sense in all those
           cases. */
        mtmp->mstrategy = st_escape;
        mtmp->sx = mtmp->mx;
        mtmp->sy = mtmp->my;
        return;
    }

    /* Covetous monster checks. */

    if (flags.made_amulet)
        if (target_on(M3_WANTSAMUL, mtmp))
            return;

    if (u.uevent.invoked) {     /* priorities change once gate opened */

        if (target_on(M3_WANTSARTI, mtmp))
            return;
        if (target_on(M3_WANTSBOOK, mtmp))
            return;
        if (target_on(M3_WANTSBELL, mtmp))
            return;
        if (target_on(M3_WANTSCAND, mtmp))
            return;

    } else {

        if (target_on(M3_WANTSBOOK, mtmp))
            return;
        if (target_on(M3_WANTSBELL, mtmp))
            return;
        if (target_on(M3_WANTSCAND, mtmp))
            return;
        if (target_on(M3_WANTSARTI, mtmp))
            return;

    }

    /* Aggravated monsters are all attracted to the player's real location. */
    if (magical_target && chases_player) {
        mtmp->mstrategy = st_mon;
        mtmp->mstrategy = u.ux;
        mtmp->mstrategy = u.uy;
        return;
    }

    /* Monsters in the middle of a swarm don't run a full AI for efficiency
       reasons: they attack, but that's about it. (They don't really have room
       to move.) So don't bother messing about with their strategy either. */
    int adjacent_nonmonsters = 0;
    int dx, dy;
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            if (adjacent_nonmonsters > 2)
                break;
            if (!isok(mtmp->mx + dx, mtmp->my + dy))
                continue;
            if (!MON_AT(mtmp->dlevel, mtmp->mx + dx, mtmp->my + dy))
                adjacent_nonmonsters++;
        }
    if (adjacent_nonmonsters <= 2)
        return;

    /* If the monster is hostile, aware of your current location (or thinks it
       is), and not escaping, it's going to hunt you down. Likewise, tame
       monsters will try to follow. Shopkeeper strategy is determined as if the
       shopkeeper is angry; it won't be used in other situations. */
    if (aware_of_u(mtmp) && !engulfing_u(mtmp) && chases_player) {
        mtmp->mstrategy = st_mon;
        mtmp->sx = mtmp->mux;
        mtmp->sy = mtmp->muy;
        return;
    }

    /* If the monster is one that's naturally social (determined using the
       G_SGROUP and G_LGROUP flags), if it sees another monster of the same mlet
       nearby, the monster with fewer max HP changes strategy to match the
       monster with more. This makes it likely (but not guaranteed) that
       monsters that generate as a group will stay as a group. Exception: a
       monster won't choose a goal square near its current square via this
       method. Note that we're using mlet matches, not species matches, so
       (e.g.) different species of elf may decide to band together if they meet
       each other in a corridor. Likewise, a healthy monster that can't see the
       player may decide to escape along with a wounded one. This is likely to
       lead to emergent behaviour; it may or may not need changing, depending on
       what that emergent behaviour is. */
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            if (!isok(mtmp->mx + dx, mtmp->my + dy))
                continue;
            struct monst *mtmp2 = m_at(mtmp->dlevel,
                                       mtmp->mx + dx, mtmp->my + dy);
            if (mtmp2 && mtmp2 != mtmp && mtmp2->mstrategy != mtmp->mstrategy &&
                mtmp2->data->mlet == mtmp->data->mlet &&
                mtmp->mpeaceful == mtmp2->mpeaceful &&
                mtmp->data->geno & (G_SGROUP | G_LGROUP) &&
                mtmp2->data->geno & (G_SGROUP | G_LGROUP) &&
                (mtmp->mhpmax < mtmp2->mhpmax ||
                 (mtmp->m_id < mtmp2->m_id && mtmp->mhpmax == mtmp2->mhpmax))) {
                mtmp->mstrategy = mtmp2->mstrategy;
                return;
            }
        }

    /* If the monster finds your tracks and wants to follow you, it follows the
       tracks. */
    if (can_track(mtmp->data) && chases_player) {
        coord *cp;
        
        cp = gettrack(mtmp->mx, mtmp->my);
        if (cp) {
            mtmp->mstrategy = st_mon;
            mtmp->sx = cp->x;
            mtmp->sy = cp->y;
            return;
        }
    }

    boolean randcheck = !rn2(100);
    /* If the monster is escaping, had nothing to do or reached its' goal but nothing
       interesting happened, pick a new strategy. However, if it is wandering aimlessy
       (st_wander), look for nearby objects to pickup since that is significantly more
       interesting than wandering around for no reason. */
    if (mtmp->mstrategy == st_escape || mtmp->mstrategy == st_none ||
        mtmp->mstrategy == st_wander || randcheck) {
        struct distmap_state ds;
        distmap_init(&ds, mtmp->mx, mtmp->my, mtmp);
        
        /* Check to see if there are any items around that the monster might
           want. (This code was moved from monmove.c, and slightly edited;
           previously, monsters would interrupt chasing the player to look for
           an item through rock.) */
#define SQSRCHRADIUS    8
        int minr = SQSRCHRADIUS;        /* not too far away */
        struct obj *otmp;
        struct monst *mtoo;
        int gx = COLNO, gy = ROWNO;

        /* guards shouldn't get too distracted */
        if (!mtmp->mpeaceful && is_mercenary(mtmp->data))
            minr = 1;

        if ((!mtmp->mpeaceful || mtmp->mtame) &&
            (!*in_rooms(mtmp->dlevel, mtmp->mx, mtmp->my, SHOPBASE) ||
             (!rn2(25) && !mx_eshk(mtmp)))) {
            for (otmp = mtmp->dlevel->objlist; otmp; otmp = otmp->nobj) {
                /* monsters may pick rocks up, but won't go out of their way
                   to grab them; this might hamper sling wielders, but it
                   cuts down on move overhead by filtering out most common
                   item */
                if (otmp->otyp == ROCK)
                    continue;
                if (distmap(&ds, otmp->ox, otmp->oy) <= minr) {
                    /* don't get stuck circling around an object that's
                       underneath an immobile or hidden monster; paralysis
                       victims excluded */
                    if ((mtoo = m_at(mtmp->dlevel, otmp->ox, otmp->oy)) &&
                        (mtoo->msleeping || mtoo->mundetected ||
                         (mtoo->mappearance && !mtoo->iswiz) ||
                         !mtoo->data->mmove))
                        continue;

                    if (((monster_would_take_item(mtmp, otmp) &&
                          can_carry(mtmp, otmp)) ||
                         ((Is_box(otmp) || otmp->otyp == ICE_BOX) &&
                          !otmp->mknown && !nohands(mtmp->data) &&
                          !is_animal(mtmp->data) &&
                          !mindless(mtmp->data))) &&
                        (throws_rocks(mtmp->data) ||
                         !sobj_at(BOULDER, level, otmp->ox, otmp->oy)) &&
                        !(onscary(otmp->ox, otmp->oy, mtmp))) {
                        minr = distmap(&ds, otmp->ox, otmp->oy) - 1;
                        gx = otmp->ox;
                        gy = otmp->oy;
                    }
                }
            }
        }

        if (gx != COLNO) {
            mtmp->mstrategy = st_obj;
            mtmp->sx = gx;
            mtmp->sy = gy;
            return;
        }

        /* If we are actually heading somewhere, don't pick a new target, unless
           we reached it with nothing happenening or passed a 1% check... */
        if (!st_target(mtmp) || randcheck ||
            (mtmp->mx == mtmp->sx && mtmp->my == mtmp->sy)) {
            /* Try up to ten locations on the level, and pick the most distant
               reachable one. If we haven't found one by then, try another 20. */
            int trycount = 0;
            int dist = 0;
            mtmp->mstrategy = st_none;
            do {
                int x = rn2(COLNO);
                int y = rn2(ROWNO);
                if (goodpos(mtmp->dlevel, x, y, mtmp, 0)) {
                    int distm = distmap(&ds, x, y);
                    if (distm > dist && distm < COLNO * ROWNO) {
                        dist = distm;
                        mtmp->mstrategy = st_wander;
                        mtmp->sx = x;
                        mtmp->sy = y;
                    }
                }
            } while (++trycount < (dist ? 10 : 30));
            return;
        }
    }

    /* And otherwise, we just stick with the previous strategy, whatever it
       happened to be. */
}

/*
 * How a covetous monster interprets the strategy fields.
 *
 * Return values:
 * 0 Fall through to the regular AI
 * 1 End the turn here
 * 2 mtmp died
 */
int
tactics(struct monst *mtmp)
{
    if (idle(mtmp))
        panic("Covetous AI running on an idle monster?");

    long strat = mtmp->mstrategy;

    switch (strat) {
    case st_escape:   /* hide and recover */
        /* if wounded, hole up on or near the stairs (to block them) */
        /* unless, of course, there are no stairs (e.g. endlevel) */
        mtmp->mavenge = 1;      /* covetous monsters attack while fleeing */
        if (In_W_tower(mtmp->mx, mtmp->my, &u.uz) ||
            (mtmp->iswiz && isok(level->upstair.sx, level->upstair.sy) &&
             !mon_has_amulet(mtmp))) {
            if (!rn2(3 + mtmp->mhp / 10))
                rloc(mtmp, TRUE);
        } else if (isok(level->upstair.sx, level->upstair.sy) &&
                   (mtmp->mx != level->upstair.sx ||
                    mtmp->my != level->upstair.sy)) {
            mnearto(mtmp, level->upstair.sx, level->upstair.sy, TRUE);
        }
        if (distu(mtmp->mx, mtmp->my) > (BOLT_LIM * BOLT_LIM)) {
            /* if you're not around, cast healing spells

               TODO: experience shows that this is probably too weak with the
               new covetous monster AI */
            if (mtmp->mhp <= mtmp->mhpmax - 8) {
                mtmp->mhp += rnd(8);
                return 1;
            }
        } else {
            /* if you are around, fall through to the normal AI (which
               includes running up the stairs, if necessary) */
            return 0;
        }

    case st_none:   /* harrass */
        if (!rn2(!mtmp->mflee ? 5 : 33))
            mnexto(mtmp);
        return 0;

    default:   /* kill, maim, pillage! */
        {
            xchar tx = mtmp->sx, ty = mtmp->sy;
            struct obj *otmp;

            if (!st_target(mtmp)) /* simply wants you to close */
                return 0;

            /* teleport as near to the desired square as possible */
            mnearto(mtmp, tx, ty, FALSE);

            /* if the player's blocking our target square, use the regular AI
               to attack them (so that we use items, request bribes, etc.) */
            if (u.ux == tx && u.uy == ty)
                return 0;

            if (strat == st_obj) {
                if (!MON_AT(level, tx, ty) ||
                    (mtmp->mx == tx && mtmp->my == ty)) {
                    /* teleport to it and pick it up */
                    rloc_to(mtmp, tx, ty);      /* clean old pos */

                    if ((otmp = on_ground(0, tx, ty, mtmp)) != 0) {
                        if (cansee(mtmp->mx, mtmp->my))
                            pline(msgc_monneutral, "%s picks up %s.",
                                  Monnam(mtmp),
                                  (distu(mtmp->mx, mtmp->my) <= 5) ?
                                  doname(otmp) : distant_name(otmp, doname));
                        obj_extract_self(otmp);
                        mpickobj(mtmp, otmp, NULL);
                        return 1;
                    } else
                        return 0;
                } else {
                    /* a monster is standing on it - go after it */
                    return mattackq(mtmp, tx, ty) ? 2 : 1;
                }
            } else if (mtmp->mx != tx || mtmp->my != ty) {
                /* something is blocking our square; attack it */
                return mattackq(mtmp, tx, ty) ? 2 : 1;
            } else {
                /* fall through to the normal AI */
                return 0;
            }
        }
    }
}

void
aggravate(void)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp)) {
            mtmp->msleeping = 0;
            if (!mtmp->mcanmove && !(property_timeout(mtmp, STONED) <= 3)) {
                mtmp->mfrozen = 0;
                mtmp->mcanmove = 1;
            }
            strategy(mtmp, TRUE);
        }
}

void
clonewiz(void)
{
    struct monst *mtmp2;

    if ((mtmp2 =
         makemon(&mons[PM_WIZARD_OF_YENDOR], level, u.ux, u.uy,
                 NO_MM_FLAGS)) != 0) {
        mtmp2->msleeping = 0;
        msethostility(mtmp2, TRUE, FALSE); /* TODO: reset alignment? */
        if (!Uhave_amulet && rn2(2)) {        /* give clone a fake */
            add_to_minv(mtmp2, mksobj(level, FAKE_AMULET_OF_YENDOR,
                                      TRUE, FALSE, rng_main), NULL);
        }
        mtmp2->m_ap_type = M_AP_MONSTER;
        mtmp2->mappearance = wizapp[rn2(SIZE(wizapp))];
        newsym(mtmp2->mx, mtmp2->my);
    }
}

/* also used by newcham() */
int
pick_nasty(void)
{
    /* To do? Possibly should filter for appropriate forms when in the
       elemental planes or surrounded by water or lava. */
    return nasties[rn2(SIZE(nasties))];
}

/* create some nasty monsters, aligned or neutral with the caster */
/* a null caster defaults to a chaotic caster (e.g. the wizard) */
int
nasty(struct monst *mcast, coord bypos)
{
    struct monst *mtmp;
    struct monst *mtarget = m_at(level, bypos.x, bypos.y);
    /* m_at doesn't work on you */
    if (bypos.x == u.ux && bypos.y == u.uy)
        mtarget = &youmonst;
    int i, j, tmp;
    int castalign = (mcast ? mcast->data->maligntyp : -1);
    int count = 0;
    boolean you = (mcast && mcast == &youmonst);
    if (you)
        castalign = u.ualign.type;
    boolean tame = (you || (mcast && mcast->mtame));
    if (mtarget && (!mtarget->mpeaceful || mtarget->mtame)) {
        /* create monsters hostile to the target */
        tame = TRUE;
        if (mtarget == &youmonst || mtarget->mpeaceful)
            tame = FALSE;
    }

    if (!rn2(10) && Inhell) {
        if (tame)
            demonpet();
        else
            msummon(mcast, &level->z);       /* summons like WoY */
        count++;
    } else {
        if (!mcast) /* WoY harassment */
            tmp = (u.ulevel > 3) ? u.ulevel / 3 : 1; /* just in case -- rph */
        else {
            if (you)
                tmp = u.ulevel / 5;
            else
                tmp = mcast->m_lev / 5;
            if (tmp < 1)
                tmp = 1;
        }
        for (i = rnd(tmp); i > 0; --i) {
            int makeindex;

            /* Summon selection choice:
                 - Don't generate higher-level spellcasters to avoid
                   chain summoning
                 - Don't generate lawful if chaotic or vice versa
                 - Also re-roll if the target is genocided
               Only re-roll like this 20 times. If our target still
               fails creation (only possible when genocided), generate
               a random monster instead */
            j = 0;
            do {
                makeindex = pick_nasty();
                j++;
            } while (((mcast && spellcaster(&mons[makeindex]) &&
                       monstr[makeindex] >= monstr[monsndx(mcast->data)]) ||
                      (mons[makeindex].maligntyp &&
                       sgn(mons[makeindex].maligntyp) == -sgn(castalign)) ||
                      (mvitals[makeindex].mvflags & G_GENOD)) && j < 20);
            /* do this after picking the monster to place */
            if (!enexto(&bypos, level, bypos.x, bypos.y,
                        &mons[makeindex]))
                continue;
            mtmp = makemon(&mons[makeindex], level, bypos.x, bypos.y,
                           tame ? MM_EDOG : NO_MM_FLAGS);
            if (!mtmp) {
                /* probably genocided, try a random monster */
                mtmp = makemon(NULL, level, bypos.x, bypos.y,
                               tame ? MM_EDOG : NO_MM_FLAGS);
                if (!mtmp) /* failed again? */
                    continue;
            }
            mtmp->msleeping = 0;
            if (tame) {
                initedog(mtmp);
                set_malign(mtmp);
                /* tame monsters drop items, ensure they don't drop weapon selection */
                if (mtmp->mtame && attacktype(mtmp->data, AT_WEAP)) {
                    mtmp->weapon_check = NEED_HTH_WEAPON;
                    mon_wield_item(mtmp);
                }
            } else
                msethostility(mtmp, TRUE, TRUE);
            newsym(mtmp->mx, mtmp->my);
            /* Give number of seen monsters to return later */
            if (canseemon(mtmp))
                count++;
        }
    }
    return count;
}

/* Let's resurrect the wizard, for some unexpected fun.    */
void
resurrect(void)
{
    struct monst *mtmp, **mmtmp;
    long elapsed;
    const char *verb;

    if (!flags.no_of_wizards) {
        /* make a new Wizard */
        verb = "kill";
        mtmp =
            makemon(&mons[PM_WIZARD_OF_YENDOR], level, u.ux, u.uy, MM_NOWAIT);
    } else {
        /* look for a migrating Wizard */
        verb = "elude";
        mmtmp = &migrating_mons;
        while ((mtmp = *mmtmp) != 0) {
            if (mtmp->iswiz &&
                /* if he has the Amulet, he won't bring it to you */
                !mon_has_amulet(mtmp) &&
                (elapsed = moves - mtmp->mlstmv) > 0L) {
                mon_catchup_elapsed_time(mtmp, elapsed);
                if (elapsed >= LARGEST_INT)
                    elapsed = LARGEST_INT - 1;
                elapsed /= 50L;
                if (mtmp->msleeping && rn2((int)elapsed + 1))
                    mtmp->msleeping = 0;
                if (mtmp->mfrozen == 1) /* would unfreeze on next move */
                    mtmp->mfrozen = 0, mtmp->mcanmove = 1;
                if (mtmp->mcanmove && !mtmp->msleeping) {
                    *mmtmp = mtmp->nmon;
                    mon_arrive(mtmp, TRUE);
                    /* note: there might be a second Wizard; if so, he'll have
                       to wait til the next resurrection */
                    break;
                }
            }
            mmtmp = &mtmp->nmon;
        }
    }

    if (mtmp) {
        mtmp->msleeping = 0;
        msethostility(mtmp, TRUE, TRUE);
        pline(msgc_npcvoice, "A voice booms out...");
        verbalize(msgc_npcanger,
                  "So thou thought thou couldst %s me, fool.", verb);
    }

}

/* Here, we make trouble for the poor shmuck who actually  */
/* managed to do in the Wizard.                            */
void
intervene(void)
{
    int which = Is_astralevel(&u.uz) ?
        1 + rn2_on_rng(4, rng_intervention) : rn2_on_rng(6, rng_intervention);
    coord cc;

    /* cases 0 and 5 don't apply on the Astral level */
    switch (which) {
    case 0:
    case 1:
        pline(msgc_levelsound, "You feel vaguely nervous.");
        break;
    case 2:
        if (!Blind)
            pline(msgc_itemloss, "You notice a %s glow surrounding you.",
                  hcolor("black"));
        rndcurse(&youmonst, NULL);
        break;
    case 3:
        aggravate();
        break;
    case 4:
        cc.x = u.ux;
        cc.y = u.uy;
        nasty(NULL, cc);
        break;
    case 5:
        resurrect();
        break;
    }
}

void
wizdead(void)
{
    flags.no_of_wizards--;
    if (!u.uevent.udemigod) {
        u.uevent.udemigod = TRUE;
        u.udg_cnt = 50 + rn2(250);
    }
}

static const char *const random_insult[] = {
    "antic",
    "blackguard",
    "caitiff",
    "chucklehead",
    "coistrel",
    "craven",
    "cretin",
    "cur",
    "dastard",
    "demon fodder",
    "dimwit",
    "dolt",
    "fool",
    "footpad",
    "imbecile",
    "knave",
    "maledict",
    "miscreant",
    "niddering",
    "poltroon",
    "rattlepate",
    "reprobate",
    "scapegrace",
    "varlet",
    "villein",  /* (sic.) */
    "wittol",
    "worm",
    "wretch",
};

static const char *const random_malediction[] = {
    "Hell shall soon claim thy remains,",
    "I chortle at thee, thou pathetic",
    "Prepare to die, thou",
    "Resistance is useless,",
    "Surrender or die, thou",
    "There shall be no mercy, thou",
    "Thou shalt repent of thy cunning,",
    "Thou art as a flea to me,",
    "Thou art doomed,",
    "Thy fate is sealed,",
    "Verily, thou shalt be one dead"
};

/* Insult or intimidate the player */
void
cuss(struct monst *mtmp)
{
    if (mtmp->iswiz) {
        if (!rn2(5))    /* typical bad guy action */
            pline(msgc_npcvoice, "%s laughs fiendishly.", Monnam(mtmp));
        else if (Uhave_amulet && !rn2(SIZE(random_insult)))
            verbalize(msgc_npcvoice, "Relinquish the amulet, %s!",
                      random_insult[rn2(SIZE(random_insult))]);
        else if (u.uhp < 5 && !rn2(2))  /* Panic */
            verbalize(msgc_npcvoice, rn2(2) ?
                      "Even now thy life force ebbs, %s!" :
                      "Savor thy breath, %s, it be thy last!",
                      random_insult[rn2(SIZE(random_insult))]);
        else if (mtmp->mhp < 5 && !rn2(2))      /* Parthian shot */
            verbalize(msgc_npcvoice,
                      rn2(2) ? "I shall return." : "I'll be back.");
        else
            verbalize(msgc_npcvoice, "%s %s!",
                      random_malediction[rn2(SIZE(random_malediction))],
                      random_insult[rn2(SIZE(random_insult))]);
    } else if (is_lminion(mtmp)) {
        com_pager(rn2(QTN_ANGELIC - 1 + (Hallucination ? 1 : 0)) + QT_ANGELIC);
    } else {
        if (!rn2(5))
            pline(msgc_npcvoice, "%s casts aspersions on your ancestry.",
                  Monnam(mtmp));
        else
            com_pager(rn2(QTN_DEMONIC) + QT_DEMONIC);
    }
}

/*wizard.c*/

