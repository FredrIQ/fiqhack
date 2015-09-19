/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-09-19 */
/* Copyright (C) 1990 by Ken Arromdee                              */
/* NetHack may be freely redistributed.  See license for details.  */

/*
 * Monster item usage routines.
 */

#include "hack.h"
#include "edog.h"

extern const int monstr[];

boolean m_using = FALSE;

/* Let monsters use magic items.  Arbitrary assumptions: Monsters only use
 * scrolls when they can see, monsters know when wands have 0 charges, monsters
 * cannot recognize if items are cursed are not, monsters which are confused
 * don't know not to read scrolls, etc....
 */

static const struct permonst *muse_newcham_mon(struct monst *);
static int precheck(struct monst *mon, struct obj *obj, struct musable *m);
static void mzapmsg(struct monst *, struct obj *, boolean);
static void mreadmsg(struct monst *, struct obj *);
static void mquaffmsg(struct monst *, struct obj *);
static boolean mon_makewish(struct monst *);
static void mon_break_wand(struct monst *, struct obj *);
static void mon_consume_unstone(struct monst *, struct obj *, boolean, boolean);

static int trapx, trapy;

/* Any preliminary checks which may result in the monster being unable to use
 * the item.  Returns 0 if nothing happened, 2 if the monster can't do anything
 * (i.e. it teleported) and 1 if it's dead.
 */
static int
precheck(struct monst *mon, struct obj *obj, struct musable *m)
{
    boolean vis;
    int wandlevel;

    if (!obj)
        return 0;
    vis = cansee(mon->mx, mon->my);

    if (obj->oclass == POTION_CLASS) {
        coord cc;
        static const char *const empty = "The potion turns out to be empty.";
        const char *potion_descr;
        struct monst *mtmp;

#define POTION_OCCUPANT_CHANCE(n) (13 + 2*(n))  /* also in potion.c */

        potion_descr = OBJ_DESCR(objects[obj->otyp]);
        if (potion_descr && !strcmp(potion_descr, "milky")) {
            if (flags.ghost_count < MAXMONNO &&
                !rn2(POTION_OCCUPANT_CHANCE(flags.ghost_count))) {
                if (!enexto(&cc, level, mon->mx, mon->my, &mons[PM_GHOST]))
                    return 0;
                mquaffmsg(mon, obj);
                m_useup(mon, obj);
                mtmp = makemon(&mons[PM_GHOST], level, cc.x, cc.y, NO_MM_FLAGS);
                if (!mtmp) {
                    if (vis)
                        pline("%s", empty);
                } else {
                    if (vis) {
                        if (Hallucination) {
                            int idx = rndmonidx();

                            pline("As %s opens the bottle, %s emerges!",
                                  mon_nam(mon), monnam_is_pname(idx)
                                  ? monnam_for_index(idx)
                                  : (idx < SPECIAL_PM &&
                                     (mons[idx].geno & G_UNIQ))
                                  ? the(monnam_for_index(idx))
                                  : an(monnam_for_index(idx)));
                        } else {
                            pline("As %s opens the bottle, an enormous"
                                  " ghost emerges!", mon_nam(mon));
                        }
                        pline("%s is frightened to death, and unable to"
                              " move.", Monnam(mon));
                    }
                    mon->mcanmove = 0;
                    mon->mfrozen = 3;
                }
                return 2;
            }
        }
        /* not rng_smoky_potion; that's for wishes that players will get */
        if (potion_descr && !strcmp(potion_descr, "smoky") &&
            flags.djinni_count < MAXMONNO &&
            !rn2(POTION_OCCUPANT_CHANCE(flags.djinni_count))) {
            int blessed = obj->blessed, cursed = obj->cursed;
            if (!enexto(&cc, level, mon->mx, mon->my, &mons[PM_DJINNI]))
                return 0;
            mquaffmsg(mon, obj);
            m_useup(mon, obj);
            mtmp = makemon(&mons[PM_DJINNI], level, cc.x, cc.y, NO_MM_FLAGS);
            if (!mtmp) {
                if (vis)
                    pline("%s", empty);
            } else {
                /* GruntHack used rnl() here. I don't know why, but smoky djinnis
                   aren't affected by luck if players do it, so it shouldn't be
                   if monsters do it either. */
                int what = rn2(5);
                if (blessed && rn2(5))
                    what = 0;
                else if (cursed && rn2(5))
                    what = 4;

                if (vis)
                    pline("In a cloud of smoke, %s emerges!", a_monnam(mtmp));
                if (canhear())
                    pline("%s speaks.", vis ? Monnam(mtmp) : "Something");
                switch (what) {
                case 0:
                    verbalize("I am in your debt.  I will grant one wish!");
                    mon_makewish(mon);
                    mongone(mtmp);
                    break;
                case 1:
                    verbalize("Thank you for freeing me!");
                    mtmp->mpeaceful = mon->mpeaceful;
                    if (mon->mtame)
                        tamedog(mtmp, NULL);
                    break;
                case 2:
                    verbalize("You freed me!");
                    mtmp->mpeaceful = 1;
                    break;
                case 3:
                    verbalize("It is about time!");
                    if (vis)
                        pline("%s vanishes.", Monnam(mtmp));
                    mongone(mtmp);
                    break;
                case 4:
                    verbalize("You disturbed me, fool!");
                    mtmp->mpeaceful = !mtmp->mpeaceful;
                    /* TODO: allow monster specific grudges */
                    if (!mon->mpeaceful)
                        tamedog(mtmp, NULL);
                    break;
                }
            }
            return 2;
        }
    }
    if (obj->oclass == WAND_CLASS) {
        wandlevel = getwandlevel(mon, obj);
        if (wandlevel == P_FAILURE) {
            /* critical failure */
            if (canhear()) {
                if (vis)
                    pline("%s zaps %s, which suddenly explodes!", Monnam(mon),
                          an(xname(obj)));
                else
                    You_hear("a zap and an explosion in the distance.");
            }
            mon_break_wand(mon, obj);
            m_useup(mon, obj);
            m->has_defense = m->has_offense = m->has_misc = MUSE_NONE;
            /* Only one needed to be set to MUSE_NONE but the others are harmless */
            return (mon->mhp <= 0) ? 1 : 2;
        }
    }
    return 0;
}

static void
mzapmsg(struct monst *mtmp, struct obj *otmp, boolean self)
{
    if (!mon_visible(mtmp)) {
        You_hear("a %s zap.",
                    (distu(mtmp->mx, mtmp->my) <=
                    (BOLT_LIM + 1) * (BOLT_LIM + 1)) ? "nearby" : "distant");
    } else if (self)
        pline("%s zaps %sself with %s!", Monnam(mtmp), mhim(mtmp),
              doname(otmp));
    else {
        pline("%s zaps %s!", Monnam(mtmp), an(xname(otmp)));
        action_interrupted();
    }
}

static void
mreadmsg(struct monst *mtmp, struct obj *otmp)
{
    boolean vismon = mon_visible(mtmp);
    short saverole;
    const char *onambuf;
    unsigned savebknown;

    if (!vismon && !canhear())
        return; /* no feedback */

    otmp->dknown = 1;   /* seeing or hearing it read reveals its label */
    /* shouldn't be able to hear curse/bless status of unseen scrolls; for
       priest characters, bknown will always be set during naming */
    savebknown = otmp->bknown;
    saverole = Role_switch;
    if (!vismon) {
        otmp->bknown = 0;
        if (Role_if(PM_PRIEST))
            Role_switch = 0;
    }
    onambuf = singular(otmp, doname);
    Role_switch = saverole;
    otmp->bknown = savebknown;

    if (vismon)
        pline("%s reads %s!", Monnam(mtmp), onambuf);
    else
        You_hear("%s reading %s.",
                 x_monnam(mtmp, ARTICLE_A, NULL,
                          (SUPPRESS_IT | SUPPRESS_INVISIBLE | SUPPRESS_SADDLE),
                          FALSE), onambuf);

    if (confused(mtmp))
        pline("Being confused, %s mispronounces the magic words...",
              vismon ? mon_nam(mtmp) : mhe(mtmp));
}

static void
mquaffmsg(struct monst *mtmp, struct obj *otmp)
{
    if (mon_visible(mtmp)) {
        otmp->dknown = 1;
        pline("%s drinks %s!", Monnam(mtmp), singular(otmp, doname));
    } else
        You_hear("a chugging sound.");
}


/* Monster wishes (currently only from smoky potions).
   Balance considerations: Undead wishing can only be performed latergame
   since only V and L are undead *and* able to wish. Player monsters are
   either undead turned, reverse-genocided or on Astral. Mines inhabitants
   wishing for gems should be rarer than finding *actual* gems in there
   and can be a nice bonus and makes flavour sense as well. As arti-wishing
   makes flavour sense, and they are occasionally generated with said
   artifacts anyway. Rodney is Rodney, and guards knowing what they're doing
   makes sense. The general wishes are neither overpowered early or too harsh. */
#define FIRST_GEM    DILITHIUM_CRYSTAL
static boolean
mon_makewish(struct monst *mtmp)
{
    if (canseemon(mtmp) && (msensem(mtmp, &youmonst) & MSENSE_ANYVISION) &&
        mtmp->mtame) {
        /* for tame monsters, redirect the wish if hero is in view */
        pline("%s looks at you curiously.", Monnam(mtmp));
        makewish();
        return TRUE;
    }
    const struct permonst *mdat = mtmp->data;
    int pm = monsndx(mdat);
    struct obj *wishobj;
    short wishtyp = 0;
    short wisharti = 0;

    /* Fleeing monsters wish for the best escape item there is */
    if (mtmp->mflee)
        wishtyp = POT_GAIN_LEVEL;
    else if (is_gnome(mdat) || is_dwarf(mdat))
        wishtyp = rn1((LAST_GEM - FIRST_GEM) + 1, FIRST_GEM);
    else if (is_undead(mdat))
        wishtyp = WAN_DEATH;
    else if (is_mplayer(mdat) && !Role_if(monsndx(mdat)))
        wisharti = role_quest_artifact(pm);
    else if (mdat->mlet == S_ANGEL && (!mtmp->mw || !mtmp->mw->oartifact))
        wisharti = rn2(2) ? ART_SUNSWORD : ART_DEMONBANE;
    else if (mtmp->iswiz) {
        /* magic resistance */
        if (!resists_magm(mtmp)) {
            if (!exist_artifact(AMULET_OF_ESP, artiname(ART_EYE_OF_THE_AETHIOPICA)))
                wisharti = ART_EYE_OF_THE_AETHIOPICA;
            else if (!exist_artifact(CREDIT_CARD, artiname(ART_YENDORIAN_EXPRESS_CARD)))
                wisharti = ART_YENDORIAN_EXPRESS_CARD;
            else if (!exist_artifact(LENSES, artiname(ART_EYES_OF_THE_OVERWORLD)))
                wisharti = ART_EYES_OF_THE_OVERWORLD;
            else if (!exist_artifact(MIRROR, artiname(ART_MAGIC_MIRROR_OF_MERLIN)))
                wisharti = ART_MAGIC_MIRROR_OF_MERLIN;
            else if (!exist_artifact(MACE, artiname(ART_SCEPTRE_OF_MIGHT)))
                wisharti = ART_SCEPTRE_OF_MIGHT;
            else if (!exist_artifact(CRYSTAL_BALL, artiname(ART_ORB_OF_DETECTION)))
                wisharti = ART_ORB_OF_DETECTION;
        }
        if (!wisharti)
            wishtyp = WAN_DEATH;
        else if (wisharti == role_quest_artifact(monsndx((&youmonst)->data))) {
            /* rare occurence, since it only happens if the wizard is messed with before
               you did your quest */
            wisharti = 0;
            wishtyp = POT_FULL_HEALING;
        }
    } else if (is_mercenary(mdat)) {
        /* mercenaries, being experienced warriors, knows the good stuff
           (Inspired from GruntHack's monster wishlist) */
        switch (rnd(6)) {
        case 1:
            /* don't replace DSM */
            if (!((reflecting(mtmp) | resists_magm(mtmp)) & W_MASK(os_arm))) {
                if (!reflecting(mtmp))
                    wishtyp = SILVER_DRAGON_SCALE_MAIL;
                else
                    wishtyp = GRAY_DRAGON_SCALE_MAIL;
                break;
            }
            /* fallthrough */
        case 2:
            if (!very_fast(mtmp)) {
                if (!(mtmp->misc_worn_check & W_MASK(os_armf)))
                    wishtyp = SPEED_BOOTS;
                else
                    wishtyp = WAN_SPEED_MONSTER;
                break;
            }
            /* fallthrough */
        case 3:
            if (!(mons[PM_CHICKATRICE].geno & G_UNIQ) &&
                !(mvitals[PM_CHICKATRICE].mvflags & G_NOCORPSE) &&
                !(mons[PM_COCKATRICE].geno & G_UNIQ) &&
                !(mvitals[PM_COCKATRICE].mvflags & G_NOCORPSE) &&
                !(mtmp->mw && mtmp->mw->otyp == CORPSE) &&
                (mtmp->misc_worn_check & W_MASK(os_armg))) {
                wishtyp = CORPSE;
                break;
            }
            /* fallthrough */
        case 4:
            if (!resists_magm(mtmp))
                wishtyp = CLOAK_OF_MAGIC_RESISTANCE;
            else if (!((protected(mtmp) | resists_magm(mtmp)) & W_MASK(os_armc)))
                wishtyp = CLOAK_OF_PROTECTION;
            if (wishtyp)
                break;
            /* fallthrough */
        case 5:
            if (!(mtmp->misc_worn_check & W_MASK(os_amul))) {
                /* 50% of the time, give "oLS anyway */
                if (!reflecting(mtmp))
                    wishtyp = AMULET_OF_LIFE_SAVING;
                else
                    wishtyp = (rn2(2) ? AMULET_OF_REFLECTION :
                               AMULET_OF_LIFE_SAVING);
                break;
            }
            /* fallthrough */
        case 6:
            wishtyp = WAN_DEATH;
        }
    } else if ((likes_gold(mdat) && !rn2(5)) || mtmp->isshk)
        wishtyp = GOLD_PIECE;

    if (wisharti) {
        wishtyp = artityp(wisharti);
        /* 1/5 of the time, try anyway! */
        if (rn2(5) && exist_artifact(wishtyp, artiname(wisharti))) {
            wisharti = 0;
            /* wand of death is OK because only player monsters
               or As will end up here */
            wishtyp = rn2(2) ? WAN_DEATH : AMULET_OF_LIFE_SAVING;
        }
    }

    /* Generic wishing for everything else */
    if (!wishtyp) {
        if (!(mtmp->misc_worn_check & W_MASK(os_amul)))
            wishtyp = AMULET_OF_LIFE_SAVING;
        else /* Not wand of death, that might be too harsh */
            wishtyp = WAN_CREATE_MONSTER;
    }

    /* Wish decided -- perform the wish
       TODO: check luck */
    wishobj = mksobj(mtmp->dlevel, wishtyp, TRUE, FALSE, rng_main);

    /* I kind of want to allow the wizard to cheat the artifact counter.
       However, this could lead to cases where the players deliberately
       donate a stack of smoky potions to the wizard, waiting until he
       eventually performs a guranteed artiwish... */
    if (wisharti) {
        wishobj = oname(wishobj, artiname(wisharti));
        wishobj->quan = 1L;
        if (is_quest_artifact(wishobj) ||
             (wishobj->oartifact &&
              rn2(nartifact_exist()) > 1)) {
            artifact_exists(wishobj, ONAME(wishobj), FALSE);
            obfree(wishobj, NULL);
            wishobj = &zeroobj;
            if (canseemon(mtmp))
                pline("For a moment, you see something in %s %s, but it disappears!",
                      s_suffix(mon_nam(mtmp)), makeplural(mbodypart(mtmp, HAND)));
            return FALSE;
        }
    }

    if (wishtyp == CORPSE) {
        /* trice */
        wishobj->corpsenm = PM_CHICKATRICE;
        if ((mons[wishobj->corpsenm].geno & G_UNIQ) ||
            (mvitals[wishobj->corpsenm].mvflags & G_NOCORPSE))
            wishobj->corpsenm = PM_COCKATRICE;
        /* partly eaten */
        wishobj->oeaten = mons[wishobj->corpsenm].cnutrit;
    }
    if (wishtyp == GOLD_PIECE) {
        /* 1-5000 gold, shopkeepers always wish for 5000 */
        if (!mtmp->isshk)
            wishobj->quan = rnd(5000);
        else
            wishobj->quan = 5000;
    } else {
        /* greased partly eaten very holy fireproof burnt boots of speed of spinach */
        wishobj->blessed = 1;
        /* Undead/demons prefer nonblessed objects. However, wands get significantly
           more potent blessed */
        if (wishobj->oclass != WAND_CLASS && (is_demon(mdat) || is_undead(mdat)))
            wishobj->blessed = 0;
        wishobj->cursed = 0;
        if (wishtyp == POT_GAIN_LEVEL) {
            /* only fleeing monsters do this */
            wishobj->blessed = 0;
            wishobj->cursed = 1;
        }
        wishobj->greased = 1;
        wishobj->oerodeproof = 1;
        if (wishobj->oclass == ARMOR_CLASS ||
             wishobj->oclass == WEAPON_CLASS) {
            /* greedy monsters go for +5, others go for +2 or +3 */
            if (likes_gold(mdat))
                wishobj->spe = 5;
            else
                wishobj->spe = rn2(2) ? 2 : 3;
            /* convert to +0 when applicable */
            if (rnd(5) < wishobj->spe)
                wishobj->spe = 0;
        }
        if (!wishobj->oartifact &&
            (wishobj->oclass == POTION_CLASS ||
             wishobj->oclass == SCROLL_CLASS ||
             wishobj->oclass == GEM_CLASS)) {
            if (likes_gold(mdat))
                wishobj->quan = 5L;
            else
                wishobj->quan = rn2(2) ? 2L : 3L;
            if (rnd(5) < wishobj->quan)
                wishobj->quan = 1L;
        }
    }

    if (wishobj && canseemon(mtmp))
        pline("%s appears in %s %s!",
              distant_name(wishobj, Doname2),
              s_suffix(mon_nam(mtmp)),
              makeplural(mbodypart(mtmp, HAND)));
    if (mpickobj(mtmp, wishobj))
        wishobj = m_carrying(mtmp, wishtyp);
    if (!wishobj) {
        impossible("monster wished-for object disappeared?");
        return FALSE;
    }
    /* wear new equipment */
    if (wishobj->oclass == ARMOR_CLASS ||
        wishobj->oclass == AMULET_CLASS)
        m_dowear(mtmp, FALSE);

    return TRUE;
}

/* Select a defensive item/action for a monster.  Returns TRUE iff one is
   found. */
boolean
find_defensive(struct monst *mtmp, struct musable *m)
{
    struct obj *obj = 0;
    struct trap *t;
    int x = mtmp->mx, y = mtmp->my;
    struct level *lev = mtmp->dlevel;
    boolean stuck = (mtmp == u.ustuck);
    boolean immobile = (mtmp->data->mmove == 0);
    boolean ranged_stuff = FALSE;
    int fraction;
    
    struct monst *target = mfind_target(mtmp, FALSE);

    if (target)
        ranged_stuff = TRUE;

    if (is_animal(mtmp->data) || mindless(mtmp->data))
        return FALSE;
    if (engulfing_u(mtmp) || !aware_of_u(mtmp) ||
        dist2(x, y, mtmp->mux, mtmp->muy) > 25)
        return FALSE;
    if (Engulfed && stuck)
        return FALSE;

    m->defensive = NULL;
    m->has_defense = 0;

    /* since unicorn horns don't get used up, the monster would look silly
       trying to use the same cursed horn round after round */
    if (confused(mtmp) || stunned(mtmp) || blind(mtmp)) {
        if (!is_unicorn(mtmp->data) && !nohands(mtmp->data)) {
            for (obj = mtmp->minvent; obj; obj = obj->nobj)
                if (obj->otyp == UNICORN_HORN && !obj->cursed)
                    break;
        }
        if (obj || is_unicorn(mtmp->data)) {
            m->defensive = obj;
            m->has_defense = MUSE_UNICORN_HORN;
            return TRUE;
        }
    }

    if (confused(mtmp)) {
        for (obj = mtmp->minvent; obj; obj = obj->nobj) {
            if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD) {
                m->defensive = obj;
                m->has_defense = MUSE_LIZARD_CORPSE;
                return TRUE;
            }
        }
    }

    /* It so happens there are two unrelated cases when we might want to check
       specifically for healing alone.  The first is when the monster is blind
       (healing cures blindness).  The second is when the monster is peaceful;
       then we don't want to flee the player, and by coincidence healing is all 
       there is that doesn't involve fleeing. These would be hard to combine
       because of the control flow. Pestilence won't use healing even when
       blind. */
    if (blind(mtmp) && !nohands(mtmp->data) &&
        mtmp->data != &mons[PM_PESTILENCE]) {
        if ((obj = m_carrying(mtmp, POT_FULL_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_FULL_HEALING;
            return TRUE;
        }
        if ((obj = m_carrying(mtmp, POT_EXTRA_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_EXTRA_HEALING;
            return TRUE;
        }
        if ((obj = m_carrying(mtmp, POT_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_HEALING;
            return TRUE;
        }
    }

    fraction = u.ulevel < 10 ? 5 : u.ulevel < 14 ? 4 : 3;
    if (mtmp->mhp >= mtmp->mhpmax ||
        (mtmp->mhp >= 10 && mtmp->mhp * fraction >= mtmp->mhpmax))
        return FALSE;

    if (mtmp->mpeaceful) {
        if (!nohands(mtmp->data)) {
            if ((obj = m_carrying(mtmp, POT_FULL_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
                return TRUE;
            }
            if ((obj = m_carrying(mtmp, POT_EXTRA_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_EXTRA_HEALING;
                return TRUE;
            }
            if ((obj = m_carrying(mtmp, POT_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_HEALING;
                return TRUE;
            }
        }
        return FALSE;
    }


    if (lev->locations[x][y].typ == STAIRS && !stuck && !immobile) {
        if (x == lev->dnstair.sx && y == lev->dnstair.sy &&
            !levitates(mtmp))
            m->has_defense = MUSE_DOWNSTAIRS;
        if (x == lev->upstair.sx && y == lev->upstair.sy &&
            ledger_no(&u.uz) != 1)
            /* Unfair to let the monsters leave the dungeon with the Amulet
               (or go to the endlevel since you also need it, to get there) */
            m->has_defense = MUSE_UPSTAIRS;
    } else if (lev->locations[x][y].typ == LADDER && !stuck && !immobile) {
        if (x == lev->upladder.sx && y == lev->upladder.sy)
            m->has_defense = MUSE_UP_LADDER;
        if (x == lev->dnladder.sx && y == lev->dnladder.sy &&
            !levitates(mtmp))
            m->has_defense = MUSE_DN_LADDER;
    } else if (lev->sstairs.sx == x && lev->sstairs.sy == y) {
        m->has_defense = MUSE_SSTAIRS;
    } else if (!stuck && !immobile) {
        /* Note: trap doors take precedence over teleport traps. */
        int xx, yy;

        for (xx = x - 1; xx <= x + 1; xx++)
            for (yy = y - 1; yy <= y + 1; yy++)
                if (isok(xx, yy))
                    if (xx != u.ux || yy != u.uy)
                        if (mtmp->data != &mons[PM_GRID_BUG] || xx == x ||
                            yy == y)
                            if ((xx == x && yy == y) || !lev->monsters[xx][yy])
                                if ((t = t_at(lev, xx, yy)) != 0)
                                    if ((verysmall(mtmp->data) ||
                                         throws_rocks(mtmp->data) ||
                                         phasing(mtmp)) ||
                                        !sobj_at(BOULDER, lev, xx, yy))
                                        if (!onscary(xx, yy, mtmp)) {
                                            if ((t->ttyp == TRAPDOOR ||
                                                 t->ttyp == HOLE)
                                                && !levitates(mtmp)
                                                && !mtmp->isshk && !mtmp->isgd
                                                && !mtmp->ispriest &&
                                                can_fall_thru(lev)) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->has_defense = MUSE_TRAPDOOR;
                                            } else if (t->ttyp == TELEP_TRAP &&
                                                       m->has_defense !=
                                                       MUSE_TRAPDOOR) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->has_defense =
                                                    MUSE_TELEPORT_TRAP;
                                            }
                                        }
    }

    if (nohands(mtmp->data))    /* can't use objects */
        goto botm;

    if (is_mercenary(mtmp->data) && (obj = m_carrying(mtmp, BUGLE))) {
        int xx, yy;
        struct monst *mon;

        /* Distance is arbitrary.  What we really want to do is have the
           soldier play the bugle when it sees or remembers soldiers nearby...
           */
        for (xx = x - 3; xx <= x + 3; xx++)
            for (yy = y - 3; yy <= y + 3; yy++)
                if (isok(xx, yy))
                    if ((mon = m_at(lev, xx, yy)) && is_mercenary(mon->data) &&
                        mon->data != &mons[PM_GUARD] && (mon->msleeping ||
                                                         (!mon->mcanmove))) {
                        m->defensive = obj;
                        m->has_defense = MUSE_BUGLE;
                    }
    }

    /* use immediate physical escape prior to attempting magic */
    if (m->has_defense) /* stairs, trap door or tele-trap, bugle alert */
        goto botm;

    /* kludge to cut down on trap destruction (particularly portals) */
    t = t_at(lev, x, y);
    if (t &&
        (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == WEB ||
         t->ttyp == BEAR_TRAP))
        t = 0;  /* ok for monster to dig here */

#define nomore(x) if (m->has_defense==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        /* don't always use the same selection pattern */
        if (m->has_defense && !rn2(3))
            break;

        /* nomore(MUSE_WAN_DIGGING); */
        if (m->has_defense == MUSE_WAN_DIGGING)
            break;
        if (obj->otyp == WAN_DIGGING && obj->spe > 0 && !stuck && !t &&
            !mtmp->isshk && !mtmp->isgd && !mtmp->ispriest &&
            !levitates(mtmp)
            /* monsters digging in Sokoban can ruin things */
            && !In_sokoban(&u.uz)
            /* digging wouldn't be effective; assume they know that */
            && !(lev->locations[x][y].wall_info & W_NONDIGGABLE)
            && !(Is_botlevel(&u.uz) || In_endgame(&u.uz))
            && !(is_ice(lev, x, y) || is_pool(lev, x, y) || is_lava(lev, x, y))
            && !(mtmp->data == &mons[PM_VLAD_THE_IMPALER]
                 && In_V_tower(&u.uz))) {
            m->defensive = obj;
            m->has_defense = MUSE_WAN_DIGGING;
        }
        nomore(MUSE_WAN_TELEPORTATION_SELF);
        nomore(MUSE_WAN_TELEPORTATION);
        if (obj->otyp == WAN_TELEPORTATION && obj->spe > 0) {
            /* use the TELEP_TRAP bit to determine if they know about
               noteleport on this level or not.  Avoids ineffective re-use of
               teleportation.  This does mean if the monster leaves the level,
               they'll know about teleport traps. */
            if (!lev->flags.noteleport ||
                !(mtmp->mtrapseen & (1 << (TELEP_TRAP - 1)))) {
                if (mon_has_amulet(mtmp) && !ranged_stuff)
                    continue;
                m->defensive = obj;
                m->has_defense = (mon_has_amulet(mtmp))
                    ? MUSE_WAN_TELEPORTATION : MUSE_WAN_TELEPORTATION_SELF;
            }
        }
        nomore(MUSE_SCR_TELEPORTATION);
        if (obj->otyp == SCR_TELEPORTATION && !blind(mtmp) &&
            haseyes(mtmp->data)
            && (!obj->cursed || (!(mtmp->isshk && inhishop(mtmp))
                                 && !mtmp->isgd && !mtmp->ispriest))) {
            /* see WAN_TELEPORTATION case above */
            if (!lev->flags.noteleport ||
                !(mtmp->mtrapseen & (1 << (TELEP_TRAP - 1)))) {
                m->defensive = obj;
                m->has_defense = MUSE_SCR_TELEPORTATION;
            }
        }

        if (mtmp->data != &mons[PM_PESTILENCE]) {
            nomore(MUSE_POT_FULL_HEALING);
            if (obj->otyp == POT_FULL_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
            }
            nomore(MUSE_POT_EXTRA_HEALING);
            if (obj->otyp == POT_EXTRA_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_EXTRA_HEALING;
            }
            nomore(MUSE_WAN_CREATE_MONSTER);
            if (obj->otyp == WAN_CREATE_MONSTER && obj->spe > 0) {
                m->defensive = obj;
                m->has_defense = MUSE_WAN_CREATE_MONSTER;
            }
            nomore(MUSE_POT_HEALING);
            if (obj->otyp == POT_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_HEALING;
            }
        } else {        /* Pestilence */
            nomore(MUSE_POT_FULL_HEALING);
            if (obj->otyp == POT_SICKNESS) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
            }
            nomore(MUSE_WAN_CREATE_MONSTER);
            if (obj->otyp == WAN_CREATE_MONSTER && obj->spe > 0) {
                m->defensive = obj;
                m->has_defense = MUSE_WAN_CREATE_MONSTER;
            }
        }
        nomore(MUSE_SCR_CREATE_MONSTER);
        if (obj->otyp == SCR_CREATE_MONSTER) {
            m->defensive = obj;
            m->has_defense = MUSE_SCR_CREATE_MONSTER;
        }
    }
botm:return (boolean) (! !m->has_defense);
#undef nomore
}

/* Perform a defensive action for a monster.  Must be called immediately
 * after find_defensive().  Return values are 0: did something, 1: died,
 * 2: did something and can't attack again (i.e. teleported).
 */
int
use_defensive(struct monst *mtmp, struct musable *m)
{
    int i, fleetim, how = 0;
    int wandlevel = 0;
    struct obj *otmp = m->defensive;
    if (otmp && otmp->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mtmp, otmp);
    boolean vis, vismon, oseen;

    if ((i = precheck(mtmp, otmp, m)) != 0)
        return i;
    vis = cansee(mtmp->mx, mtmp->my);
    vismon = mon_visible(mtmp);
    oseen = otmp && vismon;

    /* when using defensive choice to run away, we want monster to avoid
       rushing right straight back; don't override if already scared */
    fleetim = !mtmp->mflee ? (33 - (30 * mtmp->mhp / mtmp->mhpmax)) : 0;
#define m_flee(m)       if (fleetim && !m->iswiz) \
                        { monflee(m, fleetim, FALSE, FALSE); }
    if (oseen)
        examine_object(otmp);

    switch (m->has_defense) {
    case MUSE_UNICORN_HORN:
        if (vismon) {
            if (otmp)
                pline("%s uses a unicorn horn!", Monnam(mtmp));
            else
                pline("The tip of %s's horn glows!", mon_nam(mtmp));
        }
        set_property(mtmp, BLINDED, -2, FALSE);
        set_property(mtmp, CONFUSION, -2, FALSE);
        set_property(mtmp, STUNNED, -2, FALSE);
        return 2;
    case MUSE_BUGLE:
        if (vismon)
            pline("%s plays %s!", Monnam(mtmp), doname(otmp));
        else
            You_hear("a bugle playing reveille!");
        awaken_soldiers();
        return 2;
    case MUSE_WAN_TELEPORTATION_SELF:
        if ((mtmp->isshk && inhishop(mtmp))
            || mtmp->isgd || mtmp->ispriest)
            return 2;
        m_flee(mtmp);
        mzapmsg(mtmp, otmp, TRUE);
        otmp->spe--;
        how = WAN_TELEPORTATION;
    mon_tele:
        if (tele_restrict(mtmp)) {      /* mysterious force... */
            if (vismon && how)  /* mentions 'teleport' */
                makeknown(how);
            /* monster learns that teleportation isn't useful here */
            if (level->flags.noteleport)
                mtmp->mtrapseen |= (1 << (TELEP_TRAP - 1));
            return 2;
        }
        if ((On_W_tower_level(&u.uz)) && !rn2(3)) {
            if (vismon)
                pline("%s seems disoriented for a moment.", Monnam(mtmp));
            return 2;
        }
        if (oseen && how)
            makeknown(how);
        rloc(mtmp, TRUE);
        return 2;
    case MUSE_WAN_TELEPORTATION:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        m_using = TRUE;
        mbhit(mtmp, rn1(8, 6), otmp);
        /* monster learns that teleportation isn't useful here */
        if (level->flags.noteleport)
            mtmp->mtrapseen |= (1 << (TELEP_TRAP - 1));
        m_using = FALSE;
        return 2;
    case MUSE_SCR_TELEPORTATION:
        {
            int obj_is_cursed = otmp->cursed;

            if (mtmp->isshk || mtmp->isgd || mtmp->ispriest)
                return 2;
            m_flee(mtmp);
            mreadmsg(mtmp, otmp);
            m_useup(mtmp, otmp);        /* otmp might be free'ed */
            how = SCR_TELEPORTATION;
            if (obj_is_cursed || confused(mtmp)) {
                int nlev;
                d_level flev;

                if (mon_has_amulet(mtmp) || In_endgame(&u.uz)) {
                    if (vismon)
                        pline("%s seems very disoriented for a moment.",
                              Monnam(mtmp));
                    return 2;
                }
                nlev = random_teleport_level();
                if (nlev == depth(&u.uz)) {
                    if (vismon)
                        pline("%s shudders for a moment.", Monnam(mtmp));
                    return 2;
                }
                get_level(&flev, nlev);
                migrate_to_level(mtmp, ledger_no(&flev), MIGR_RANDOM, NULL);
                if (oseen)
                    makeknown(SCR_TELEPORTATION);
            } else
                goto mon_tele;
            return 2;
        }
    case MUSE_WAN_DIGGING:
        {
            struct trap *ttmp;

            m_flee(mtmp);
            mzapmsg(mtmp, otmp, FALSE);
            otmp->spe--;
            if (oseen)
                makeknown(WAN_DIGGING);
            if (IS_FURNITURE(level->locations[mtmp->mx][mtmp->my].typ) ||
                IS_DRAWBRIDGE(level->locations[mtmp->mx][mtmp->my].typ) ||
                (is_drawbridge_wall(mtmp->mx, mtmp->my) >= 0) ||
                (level->sstairs.sx == mtmp->mx &&
                 level->sstairs.sy == mtmp->my)) {
                pline("The digging ray is ineffective.");
                return 2;
            }
            if (!can_dig_down(level)) {
                if (mon_visible(mtmp))
                    pline("The %s here is too hard to dig in.",
                          surface(mtmp->mx, mtmp->my));
                return 2;
            }
            ttmp = maketrap(level, mtmp->mx, mtmp->my, HOLE, rng_main);
            if (!ttmp)
                return 2;
            seetrap(ttmp);
            if (vis) {
                pline("%s has made a hole in the %s.", Monnam(mtmp),
                      surface(mtmp->mx, mtmp->my));
                pline("%s %s through...", Monnam(mtmp),
                      flying(mtmp) ? "dives" : "falls");
            } else
                You_hear("something crash through the %s.",
                         surface(mtmp->mx, mtmp->my));
            /* we made sure that there is a level for mtmp to go to */
            migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
            return 2;
        }
    case MUSE_WAN_CREATE_MONSTER:
        {
            coord cc;
            const struct permonst *pm = 0, *fish = 0;
            int cnt = wandlevel;
            struct monst *mon;

            if (!rn2(23) || wandlevel == P_MASTER)
                cnt += rnd(7);
            if (is_pool(level, mtmp->mx, mtmp->my))
                fish = &mons[u.uinwater ? PM_GIANT_EEL : PM_CROCODILE];
            mzapmsg(mtmp, otmp, FALSE);
            otmp->spe--;
            while (cnt--) {
                /* `fish' potentially gives bias towards water locations; `pm'
                   is what to actually create (0 => random) */
                if (!enexto(&cc, level, mtmp->mx, mtmp->my, fish))
                    break;
                mon = makemon(pm, level, cc.x, cc.y,
                              MM_CREATEMONSTER | MM_CMONSTER_M);
                if (mon && cansuspectmon(mon))
                makeknown(WAN_CREATE_MONSTER);
            }
            return 2;
        }
    case MUSE_SCR_CREATE_MONSTER:
        {
            coord cc;
            const struct permonst *pm = 0, *fish = 0;
            int cnt = 1;
            struct monst *mon;
            boolean known = FALSE;

            if (!rn2(73))
                cnt += rnd(4);
            if (confused(mtmp) || otmp->cursed)
                cnt += 12;
            if (confused(mtmp))
                pm = fish = &mons[PM_ACID_BLOB];
            else if (is_pool(level, mtmp->mx, mtmp->my))
                fish = &mons[u.uinwater ? PM_GIANT_EEL : PM_CROCODILE];
            mreadmsg(mtmp, otmp);
            while (cnt--) {
                /* `fish' potentially gives bias towards water locations; `pm'
                   is what to actually create (0 => random) */
                if (!enexto(&cc, level, mtmp->mx, mtmp->my, fish))
                    break;
                mon = makemon(pm, level, cc.x, cc.y,
                              MM_CREATEMONSTER | MM_CMONSTER_M);
                if (mon && cansuspectmon(mon))
                    known = TRUE;
            }
            /* The only case where we don't use oseen.  For wands, you have to
               be able to see the monster zap the wand to know what type it is. 
               For teleport scrolls, you have to see the monster to know it
               teleported. */
            if (known)
                makeknown(SCR_CREATE_MONSTER);
            else if (!objects[SCR_CREATE_MONSTER].oc_name_known &&
                     !objects[SCR_CREATE_MONSTER].oc_uname)
                docall(otmp);
            m_useup(mtmp, otmp);
            return 2;
        }
    case MUSE_TRAPDOOR:
        /* trap doors on "bottom" levels of dungeons are rock-drop trap doors,
           not holes in the floor.  We check here for safety. */
        if (Is_botlevel(&u.uz))
            return 0;
        m_flee(mtmp);
        if (vis) {
            struct trap *t;

            t = t_at(level, trapx, trapy);
            pline("%s %s into a %s!", Monnam(mtmp),
                  makeplural(locomotion(mtmp->data, "jump")),
                  t->ttyp == TRAPDOOR ? "trap door" : "hole");
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);     /* update old location */
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
        return 2;
    case MUSE_UPSTAIRS:
        /* Monsters without amulets escape the dungeon and are gone for good
           when they leave up the up stairs. Monsters with amulets would reach
           the endlevel, which we cannot allow since that would leave the
           player stranded. */
        if (ledger_no(&u.uz) == 1) {
            if (mon_has_special(mtmp))
                return 0;
            if (vismon)
                pline("%s escapes the dungeon!", Monnam(mtmp));
            mongone(mtmp);
            return 2;
        }
        m_flee(mtmp);
        if (Inhell && mon_has_amulet(mtmp) && !rn2(4) &&
            (dunlev(&u.uz) < dunlevs_in_dungeon(&u.uz) - 3)) {
            if (vismon)
                pline("As %s climbs the stairs, a mysterious force momentarily "
                      "surrounds %s...", mon_nam(mtmp), mhim(mtmp));
            /* simpler than for the player; this will usually be the Wizard and 
               he'll immediately go right to the upstairs, so there's not much
               point in having any chance for a random position on the current
               level */
            migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
        } else {
            if (vismon)
                pline("%s escapes upstairs!", Monnam(mtmp));
            migrate_to_level(mtmp, ledger_no(&u.uz) - 1, MIGR_STAIRS_DOWN,
                             NULL);
        }
        return 2;
    case MUSE_DOWNSTAIRS:
        m_flee(mtmp);
        if (vismon)
            pline("%s escapes downstairs!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_STAIRS_UP, NULL);
        return 2;
    case MUSE_UP_LADDER:
        m_flee(mtmp);
        if (vismon)
            pline("%s escapes up the ladder!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) - 1, MIGR_LADDER_DOWN, NULL);
        return 2;
    case MUSE_DN_LADDER:
        m_flee(mtmp);
        if (vismon)
            pline("%s escapes down the ladder!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_LADDER_UP, NULL);
        return 2;
    case MUSE_SSTAIRS:
        m_flee(mtmp);
        /* the stairs leading up from the 1st level are */
        /* regular stairs, not sstairs.  */
        if (level->sstairs.up) {
            if (vismon)
                pline("%s escapes upstairs!", Monnam(mtmp));
            if (Inhell) {
                migrate_to_level(mtmp, ledger_no(&level->sstairs.tolev),
                                 MIGR_RANDOM, NULL);
                return 2;
            }
        } else if (vismon)
            pline("%s escapes downstairs!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&level->sstairs.tolev), MIGR_SSTAIRS,
                         NULL);
        return 2;
    case MUSE_TELEPORT_TRAP:
        m_flee(mtmp);
        if (vis) {
            pline("%s %s onto a teleport trap!", Monnam(mtmp),
                  makeplural(locomotion(mtmp->data, "jump")));
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }
        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);     /* update old location */
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        goto mon_tele;
    case MUSE_POT_HEALING:
        mquaffmsg(mtmp, otmp);
        i = dice(6 + 2 * bcsign(otmp), 4);
        mtmp->mhp += i;
        if (mtmp->mhp > mtmp->mhpmax)
            mtmp->mhp = ++mtmp->mhpmax;
        if (!otmp->cursed)
            set_property(mtmp, BLINDED, -2, FALSE);
        if (vismon)
            pline("%s looks better.", Monnam(mtmp));
        if (oseen)
            makeknown(POT_HEALING);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_EXTRA_HEALING:
        mquaffmsg(mtmp, otmp);
        i = dice(6 + 2 * bcsign(otmp), 8);
        mtmp->mhp += i;
        if (mtmp->mhp > mtmp->mhpmax)
            mtmp->mhp = (mtmp->mhpmax += (otmp->blessed ? 5 : 2));
        set_property(mtmp, BLINDED, -2, FALSE);
        if (vismon)
            pline("%s looks much better.", Monnam(mtmp));
        if (oseen)
            makeknown(POT_EXTRA_HEALING);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_FULL_HEALING:
        mquaffmsg(mtmp, otmp);
        if (otmp->otyp == POT_SICKNESS)
            unbless(otmp);      /* Pestilence */
        mtmp->mhp = (mtmp->mhpmax += (otmp->blessed ? 8 : 4));
        set_property(mtmp, BLINDED, -2, FALSE);
        if (vismon)
            pline("%s looks completely healed.", Monnam(mtmp));
        if (oseen)
            makeknown(otmp->otyp);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_LIZARD_CORPSE:
        /* not actually called for its unstoning effect */
        mon_consume_unstone(mtmp, otmp, FALSE, FALSE);
        return 2;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_defense);
        break;
    }
    return 0;
#undef m_flee
}

int
rnd_defensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = monstr[monsndx(pm)];
    int trycnt = 0;

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;
try_again:
    switch (rn2_on_rng(8 + (difficulty > 3) + (difficulty > 6) +
                       (difficulty > 8), rng)) {
    case 6:
    case 9:
        if (mtmp->dlevel->flags.noteleport && ++trycnt < 2)
            goto try_again;
        if (!rn2_on_rng(3, rng))
            return WAN_TELEPORTATION;
        /* else FALLTHRU */
    case 0:
    case 1:
        return SCR_TELEPORTATION;
    case 8:
    case 10:
        if (!rn2_on_rng(3, rng))
            return WAN_CREATE_MONSTER;
        /* else FALLTHRU */
    case 2:
        return SCR_CREATE_MONSTER;
    case 3:
        return POT_HEALING;
    case 4:
        return POT_EXTRA_HEALING;
    case 5:
        return (mtmp->data !=
                &mons[PM_PESTILENCE]) ? POT_FULL_HEALING : POT_SICKNESS;
    case 7:
        if (levitates(mtmp) || mtmp->isshk || mtmp->isgd || mtmp->ispriest)
            return 0;
        else
            return WAN_DIGGING;
    }
     /*NOTREACHED*/ return 0;
}


/* Select an offensive item/action for a monster.  Returns TRUE iff one is
   found. */
boolean
find_offensive(struct monst *mtmp, struct musable *m)
{
    struct obj *obj;
    boolean ranged_stuff = FALSE;
    boolean reflection_skip = FALSE;
    struct obj *helmet = which_armor(mtmp, os_armh);

    struct monst *target = mfind_target(mtmp, FALSE);

    if (target) {
        ranged_stuff = TRUE;
        if (target == &youmonst)
            reflection_skip = Reflecting && rn2(2);
        else
            reflection_skip = mon_reflects(target, NULL) && rn2(2);
        /* Skilled wand users bypass reflection. Cursed wands reduce skill,
           but monsters don't recognize BUC at the moment. */
        if (mprof(mtmp, MP_WANDS) >= P_SKILLED)
            reflection_skip = FALSE;
    }

    m->offensive = NULL;
    m->has_offense = 0;
    if (is_animal(mtmp->data) || mindless(mtmp->data) || nohands(mtmp->data))
        return FALSE;
    if (target == &youmonst) {
        if (Engulfed)
            return FALSE;
        if (in_your_sanctuary(mtmp, 0, 0))
            return FALSE;
        if (dmgtype(mtmp->data, AD_HEAL) && !uwep && !uarmu && !uarmh
            && !uarms && !uarmg && !uarmc && !uarmf && (!uarm || uskin()))
            return FALSE;
    }

    if (!ranged_stuff)
        return FALSE; /* nothing to do */
#define nomore(x) if (m->has_offense==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (obj->oclass == WAND_CLASS && obj->spe < 1)
            continue;
        if (!reflection_skip) {
            nomore(MUSE_WAN_DEATH);
            if (obj->otyp == WAN_DEATH) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_DEATH;
            }
            nomore(MUSE_WAN_SLEEP);
            if (obj->otyp == WAN_SLEEP &&
               ((target == &youmonst && !u_helpless(hm_paralyzed | hm_unconscious)) ||
                (target != &youmonst && target->mcanmove))) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_SLEEP;
            }
            nomore(MUSE_WAN_FIRE);
            if (obj->otyp == WAN_FIRE) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_FIRE;
            }
            nomore(MUSE_FIRE_HORN);
            if (obj->otyp == FIRE_HORN &&
                can_blow_instrument(mtmp->data)) {
                m->offensive = obj;
                m->has_offense = MUSE_FIRE_HORN;
            }
            nomore(MUSE_WAN_COLD);
            if (obj->otyp == WAN_COLD) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_COLD;
            }
            nomore(MUSE_FROST_HORN);
            if (obj->otyp == FROST_HORN &&
                can_blow_instrument(mtmp->data)) {
                m->offensive = obj;
                m->has_offense = MUSE_FROST_HORN;
            }
            nomore(MUSE_WAN_LIGHTNING);
            if (obj->otyp == WAN_LIGHTNING) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_LIGHTNING;
            }
            nomore(MUSE_WAN_MAGIC_MISSILE);
            if (obj->otyp == WAN_MAGIC_MISSILE) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_MAGIC_MISSILE;
            }
        }
        nomore(MUSE_WAN_STRIKING);
        if (obj->otyp == WAN_STRIKING) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_STRIKING;
        }
        nomore(MUSE_WAN_UNDEAD_TURNING);
        if (obj->otyp == WAN_UNDEAD_TURNING && is_undead(target->data)) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_UNDEAD_TURNING;
        }
        nomore(MUSE_WAN_SLOW_MONSTER);
        if (obj->otyp == MUSE_WAN_SLOW_MONSTER && !slow(target)) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_SLOW_MONSTER;
        }
        nomore(MUSE_POT_PARALYSIS);
        if (obj->otyp == POT_PARALYSIS &&
            !u_helpless(hm_paralyzed | hm_unconscious)) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_PARALYSIS;
        }
        nomore(MUSE_POT_BLINDNESS);
        if (obj->otyp == POT_BLINDNESS && !attacktype(mtmp->data, AT_GAZE)) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_BLINDNESS;
        }
        nomore(MUSE_POT_CONFUSION);
        if (obj->otyp == POT_CONFUSION) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_CONFUSION;
        }
        nomore(MUSE_POT_SLEEPING);
        if (obj->otyp == POT_SLEEPING) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_SLEEPING;
        }
        nomore(MUSE_POT_ACID);
        if (obj->otyp == POT_ACID) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_ACID;
        }
        /* we can safely put this scroll here since the locations that are in a 
           1 square radius are a subset of the locations that are in wand range 
         */
        nomore(MUSE_SCR_EARTH);
        if (obj->otyp == SCR_EARTH &&
            ((helmet && is_metallic(helmet)) || confused(mtmp) ||
             amorphous(mtmp->data) || phasing(mtmp) ||
             noncorporeal(mtmp->data) || unsolid(mtmp->data) || !rn2(10))
            && aware_of_u(mtmp) && !engulfing_u(mtmp)
            && dist2(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <= 2
            && !blind(mtmp) && haseyes(mtmp->data)
            && !Is_rogue_level(&u.uz)
            && (!In_endgame(&u.uz) || Is_earthlevel(&u.uz))) {
            m->offensive = obj;
            m->has_offense = MUSE_SCR_EARTH;
        }
    }
    return (boolean) (!!m->has_offense);
#undef nomore
}

/* Used for critical failures with wand use. Might also see
   use if monsters learn to break wands intelligently.
   FIXME: merge this with do_break_wand() */

/* what? (investigate do_break_wand for this...) */
#define BY_OBJECT       (NULL)

static void
mon_break_wand(struct monst *mtmp, struct obj *otmp) {
    int i, x, y;
    struct monst *mon;
    int damage;
    int expltype;
    int otyp;
    boolean oseen = mon_visible(mtmp);
    boolean affects_objects;

    otyp = otmp->otyp;
    affects_objects = FALSE;
    otmp->ox = mtmp->mx;
    otmp->oy = mtmp->my;

    /* damage */
    damage = otmp->spe * 4;
    if (otyp != WAN_MAGIC_MISSILE)
        damage *= 2;
    if (otyp == WAN_DEATH || otyp == WAN_LIGHTNING)
        damage *= 2;

    /* explosion color */
    if (otyp == WAN_FIRE)
        expltype = EXPL_FIERY;
    else if (otyp == WAN_COLD)
        expltype = EXPL_FROSTY; 
    else
        expltype = EXPL_MAGICAL;

    /* (non-sleep) ray explosions */
    if (otyp == WAN_DEATH
     || otyp == WAN_FIRE
     || otyp == WAN_COLD
     || otyp == WAN_LIGHTNING
     || otyp == WAN_MAGIC_MISSILE)
        explode(otmp->ox, otmp->oy, (otyp - WAN_MAGIC_MISSILE), damage, WAND_CLASS,
                expltype, NULL, 0);
    else {
        if (otyp == WAN_STRIKING) {
            if (oseen)
                pline("A wall of force smashes down around %s!",
                      mon_nam(mtmp));
            damage = dice(1 + otmp->spe, 6);
        }
        if (otyp == WAN_STRIKING
         || otyp == WAN_CANCELLATION
         || otyp == WAN_POLYMORPH
         || otyp == WAN_TELEPORTATION
         || otyp == WAN_UNDEAD_TURNING)
            affects_objects = TRUE;

        explode(otmp->ox, otmp->oy, 0, rnd(damage), WAND_CLASS, expltype, NULL, 0);

        /* affect all tiles around the monster */
        for (i = 0; i <= 8; i++) {
            bhitpos.x = x = otmp->ox + xdir[i];
            bhitpos.y = y = otmp->oy + ydir[i];
            if (!isok(x, y))
                continue;

            if (otyp == WAN_DIGGING && dig_check(BY_OBJECT, FALSE, x, y)) {
                if (IS_WALL(level->locations[x][y].typ) ||
                    IS_DOOR(level->locations[x][y].typ)) {
                    /* add potential shop damage for fixing */
                    if (*in_rooms(level, x, y, SHOPBASE))
                        add_damage(bhitpos.x, bhitpos.y, 0L);
                }
                digactualhole(x, y, BY_OBJECT,
                              (rn2(otmp->spe) < 3 ||
                               !can_dig_down(level)) ? PIT : HOLE);
            } else if (otyp == WAN_CREATE_MONSTER) {
                makemon(NULL, level, otmp->ox, otmp->oy, MM_CREATEMONSTER | MM_CMONSTER_U);
            } else {
                /* avoid telecontrol/autopickup shenanigans */
                if (x == u.ux && y == u.uy) {
                    if (otyp == WAN_TELEPORTATION &&
                        level->objects[x][y]) {
                        bhitpile(otmp, bhito, x, y);
                        bot();  /* potion effects */
                    }
                    damage = zapyourself(otmp, FALSE);
                    if (damage) {
                        losehp(damage, "killed by a wand's explosion");
                    }
                    bot();      /* blindness */
                } else if ((mon = m_at(level, x, y)) != 0) {
                    bhitm(mtmp, mon, otmp);
                }
                if (affects_objects && level->objects[x][y]) {
                    bhitpile(otmp, bhito, x, y);
                    bot();      /* potion effects */
                }
            }
        }
    }
    if (otyp == WAN_LIGHT)
        litroom(mtmp, TRUE, otmp);     /* only needs to be done once */
}


/* Perform an offensive action for a monster.  Must be called immediately
 * after find_offensive().  Return values are same as use_defensive().
 */
int
use_offensive(struct monst *mtmp, struct musable *m)
{
    int i;
    int wandlevel = 0;
    struct obj *otmp = m->offensive;
    if (otmp && otmp->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mtmp, otmp);
    boolean oseen;
    boolean isray = FALSE;

    /* offensive potions are not drunk, they're thrown */
    if (otmp->oclass != POTION_CLASS && (i = precheck(mtmp, otmp, m)) != 0)
        return i;
    oseen = mon_visible(mtmp);
    if (oseen)
        examine_object(otmp);

    /* wand efficiency is determined by a monster's proficiency
       with wands and adjusted by +/-1 based on BUC.
       cursed + unskilled results in an effect similar to breaking
       wands. */
    switch (m->has_offense) {
    case MUSE_WAN_DEATH:
    case MUSE_WAN_SLEEP:
    case MUSE_WAN_FIRE:
    case MUSE_WAN_COLD:
    case MUSE_WAN_LIGHTNING:
    case MUSE_WAN_MAGIC_MISSILE:
        isray = TRUE;
    case MUSE_WAN_TELEPORTATION:
    case MUSE_WAN_STRIKING:
    case MUSE_WAN_UNDEAD_TURNING:
    case MUSE_WAN_SLOW_MONSTER:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        if (oseen)
            makeknown(otmp->otyp);
        m_using = TRUE;

        /* FIXME: make buzz() handle any zap */
        if (!isray) {
            mbhit(mtmp, rn1(8, 6), otmp);
            m_using = FALSE;
            return 2;
        } else {
            buzz((int)(-30 - (otmp->otyp - WAN_MAGIC_MISSILE)),
                 (wandlevel == P_UNSKILLED) ? 3 : 6, mtmp->mx, mtmp->my,
                 sgn(tbx), sgn(tby), wandlevel);
    
            m_using = FALSE;
            return (mtmp->mhp <= 0) ? 1 : 2;
        }

    case MUSE_FIRE_HORN:
    case MUSE_FROST_HORN:
        if (oseen) {
            makeknown(otmp->otyp);
            pline("%s plays a %s!", Monnam(mtmp), xname(otmp));
        } else
            You_hear("a horn being played.");
        otmp->spe--;
        m_using = TRUE;
        buzz(-30 - ((otmp->otyp == FROST_HORN) ? AD_COLD - 1 : AD_FIRE - 1),
             rn1(6, 6), mtmp->mx, mtmp->my, sgn(tbx), sgn(tby), 0);
        m_using = FALSE;
        return (mtmp->mhp <= 0) ? 1 : 2;
    case MUSE_SCR_EARTH:
        {
            /* TODO: handle steeds */
            int x, y;

            /* don't use monster fields after killing it */
            boolean confused = (confused(mtmp) ? TRUE : FALSE);
            int mmx = mtmp->mx, mmy = mtmp->my;

            mreadmsg(mtmp, otmp);
            /* Identify the scroll */
            if (canspotmon(mtmp)) {
                pline("The %s rumbles %s %s!", ceiling(mtmp->mx, mtmp->my),
                      otmp->blessed ? "around" : "above", mon_nam(mtmp));
                if (oseen)
                    makeknown(otmp->otyp);
            } else if (cansee(mtmp->mx, mtmp->my)) {
                pline("The %s rumbles in the middle of nowhere!",
                      ceiling(mtmp->mx, mtmp->my));
                map_invisible(mtmp->mx, mtmp->my);
                if (oseen)
                    makeknown(otmp->otyp);
            }

            /* Loop through the surrounding squares */
            for (x = mmx - 1; x <= mmx + 1; x++) {
                for (y = mmy - 1; y <= mmy + 1; y++) {
                    /* Is this a suitable spot? */
                    if (isok(x, y) && !closed_door(level, x, y) &&
                        !IS_ROCK(level->locations[x][y].typ) &&
                        !IS_AIR(level->locations[x][y].typ) &&
                        (((x == mmx) &&
                          (y == mmy)) ? !otmp->blessed : !otmp->cursed) &&
                        (x != u.ux || y != u.uy)) {
                        struct obj *otmp2;
                        struct monst *mtmp2;

                        /* Make the object(s) */
                        otmp2 = mksobj(level, confused ? ROCK : BOULDER,
                                       FALSE, FALSE, rng_main);
                        if (!otmp2)
                            continue;   /* Shouldn't happen */
                        otmp2->quan = confused ? rn1(5, 2) : 1;
                        otmp2->owt = weight(otmp2);

                        /* Find the monster here (might be same as mtmp) */
                        mtmp2 = m_at(level, x, y);
                        if (mtmp2 && !amorphous(mtmp2->data) &&
                            !phasing(mtmp2) &&
                            !noncorporeal(mtmp2->data) &&
                            !unsolid(mtmp2->data)) {
                            struct obj *helmet = which_armor(mtmp2, os_armh);
                            int mdmg;

                            if (cansee(mtmp2->mx, mtmp2->my)) {
                                pline("%s is hit by %s!", Monnam(mtmp2),
                                      doname(otmp2));
                                if (!canspotmon(mtmp2))
                                    map_invisible(mtmp2->mx, mtmp2->my);
                            }
                            mdmg = dmgval(otmp2, mtmp2) * otmp2->quan;
                            if (helmet) {
                                if (is_metallic(helmet)) {
                                    if (canseemon(mtmp2))
                                        pline("Fortunately, %s is wearing "
                                              "a hard %s.", mon_nam(mtmp2),
                                              helmet_name(helmet));
                                    else
                                        You_hear("a clanging sound.");
                                    if (mdmg > 2)
                                        mdmg = 2;
                                } else {
                                    if (canseemon(mtmp2))
                                        pline("%s's %s does not protect %s.",
                                              Monnam(mtmp2), xname(helmet),
                                              mhim(mtmp2));
                                }
                            }
                            mtmp2->mhp -= mdmg;
                            if (mtmp2->mhp <= 0) {
                                pline("%s is killed.", Monnam(mtmp2));
                                mondied(mtmp2);
                            }
                        }
                        /* Drop the rock/boulder to the floor */
                        if (!flooreffects(otmp2, x, y, "fall")) {
                            place_object(otmp2, level, x, y);
                            stackobj(otmp2);
                            newsym(x, y);       /* map the rock */
                        }
                    }
                }
            }
            m_useup(mtmp, otmp);
            /* Attack the player */
            if (distmin(mmx, mmy, u.ux, u.uy) == 1 && !otmp->cursed) {
                int dmg;
                struct obj *otmp2;

                /* Okay, _you_ write this without repeating the code */
                otmp2 = mksobj(level, confused ? ROCK : BOULDER, FALSE, FALSE,
                               rng_main);
                if (!otmp2)
                    goto xxx_noobj;     /* Shouldn't happen */
                otmp2->quan = confused ? rn1(5, 2) : 1;
                otmp2->owt = weight(otmp2);
                if (!amorphous(youmonst.data) && !Passes_walls &&
                    !noncorporeal(youmonst.data) && !unsolid(youmonst.data)) {
                    pline("You are hit by %s!", doname(otmp2));
                    dmg = dmgval(otmp2, &youmonst) * otmp2->quan;
                    if (uarmh) {
                        if (is_metallic(uarmh)) {
                            pline("Fortunately, you are wearing a hard %s.",
                                  helmet_name(uarmh));
                            if (dmg > 2)
                                dmg = 2;
                        } else if (flags.verbose) {
                            pline("Your %s does not protect you.",
                                  xname(uarmh));
                        }
                    }
                } else
                    dmg = 0;
                if (!flooreffects(otmp2, u.ux, u.uy, "fall")) {
                    place_object(otmp2, level, u.ux, u.uy);
                    stackobj(otmp2);
                    newsym(u.ux, u.uy);
                }
                if (dmg)
                    losehp(dmg, killer_msg(DIED, "scroll of earth"));
            }
        xxx_noobj:

            return (mtmp->mhp <= 0) ? 1 : 2;
        }
    case MUSE_POT_PARALYSIS:
    case MUSE_POT_BLINDNESS:
    case MUSE_POT_CONFUSION:
    case MUSE_POT_SLEEPING:
    case MUSE_POT_ACID:
        /* Note: this setting of dknown doesn't suffice.  A monster which is
           out of sight might throw and it hits something _in_ sight, a problem 
           not existing with wands because wand rays are not objects.  Also set 
           dknown in mthrowu.c. */
        if (cansee(mtmp->mx, mtmp->my)) {
            otmp->dknown = 1;
            pline("%s hurls %s!", Monnam(mtmp), singular(otmp, doname));
        }
        /* Wow, this is a twisty mess of ugly global variables. */
        if (!aware_of_u(mtmp))
            panic("Monster throws potion while unaware!");
        if (engulfing_u(mtmp))
            panic("Monster throws potion while engulfing you!");
        m_throw(mtmp, mtmp->mx, mtmp->my, sgn(tbx), sgn(tby),
                distmin(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy), otmp, TRUE);
        return 2;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_offense);
        break;
    }
    return 0;
}

int
rnd_offensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = monstr[monsndx(pm)];

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;
    if (difficulty > 7 && !rn2_on_rng(35, rng))
        return WAN_DEATH;
    switch (rn2_on_rng(9 - (difficulty < 4) + 4 * (difficulty > 6), rng)) {
    case 0:{
            struct obj *helmet = which_armor(mtmp, os_armh);

            if ((helmet && is_metallic(helmet)) || amorphous(pm) ||
                phasing(mtmp) || noncorporeal(pm) || unsolid(pm))
                return SCR_EARTH;
        }       /* fall through */
    case 1:
        return WAN_STRIKING;
    case 2:
        return POT_ACID;
    case 3:
        return POT_CONFUSION;
    case 4:
        return POT_BLINDNESS;
    case 5:
        return POT_SLEEPING;
    case 6:
        return POT_PARALYSIS;
    case 7:
    case 8:
        return WAN_MAGIC_MISSILE;
    case 9:
        return WAN_SLEEP;
    case 10:
        return WAN_FIRE;
    case 11:
        return WAN_COLD;
    case 12:
        return WAN_LIGHTNING;
    }
     /*NOTREACHED*/ return 0;
}

boolean
find_misc(struct monst * mtmp, struct musable * m)
{
    struct obj *obj;
    const struct permonst *mdat = mtmp->data;
    int x = mtmp->mx, y = mtmp->my;
    struct trap *t;
    int xx, yy;
    boolean immobile = (mdat->mmove == 0);
    boolean stuck = (mtmp == u.ustuck);
    boolean ranged_stuff = FALSE;

    struct monst *target = mfind_target(mtmp, TRUE); /* zaps here is helpful */
    const struct permonst *tdat = NULL;
    if (target) {
        tdat = target->data;
        ranged_stuff = TRUE;
    }

    m->misc = NULL;
    m->has_misc = 0;
    if (is_animal(mdat) || mindless(mdat))
        return 0;
    if (Engulfed && stuck)
        return FALSE;

    /* We arbitrarily limit to times when a player is nearby for the same
       reason as Junior Pac-Man doesn't have energizers eaten until you can see 
       them... */
    if (!aware_of_u(mtmp) || engulfing_u(mtmp) ||
        dist2(x, y, mtmp->mux, mtmp->muy) > 36)
        return FALSE;

    if (!stuck && !immobile && !mtmp->cham && monstr[monsndx(mdat)] < 6) {
        boolean ignore_boulders = (verysmall(mdat) || throws_rocks(mdat) ||
                                   phasing(mtmp));
        for (xx = x - 1; xx <= x + 1; xx++)
            for (yy = y - 1; yy <= y + 1; yy++)
                if (isok(xx, yy) && (xx != u.ux || yy != u.uy))
                    if (mdat != &mons[PM_GRID_BUG] || xx == x || yy == y)
                        if ( /* (xx==x && yy==y) || */ !level->monsters[xx][yy])
                            if ((t = t_at(level, xx, yy)) != 0 &&
                                (ignore_boulders ||
                                 !sobj_at(BOULDER, level, xx, yy))
                                && !onscary(xx, yy, mtmp)) {
                                if (t->ttyp == POLY_TRAP) {
                                    trapx = xx;
                                    trapy = yy;
                                    m->has_misc = MUSE_POLY_TRAP;
                                    return TRUE;
                                }
                            }
    }
    if (nohands(mdat))
        return 0;

#define nomore(x) if (m->has_misc==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (obj && obj->oclass == WAND_CLASS && obj->spe < 1)
            continue;
        /* Monsters shouldn't recognize cursed items; this kludge is */
        /* necessary to prevent serious problems though... */
        if (obj->otyp == POT_GAIN_LEVEL &&
            (!obj->cursed ||
             (!mtmp->isgd && !mtmp->isshk && !mtmp->ispriest))) {
            m->misc = obj;
            m->has_misc = MUSE_POT_GAIN_LEVEL;
        }
        nomore(MUSE_BULLWHIP);
        if (obj->otyp == BULLWHIP && (MON_WEP(mtmp) == obj) &&
            distu(mtmp->mx, mtmp->my) == 1 && uwep && !mtmp->mpeaceful) {
            m->misc = obj;
            m->has_misc = MUSE_BULLWHIP;
        }
        /* Note: peaceful/tame monsters won't make themselves invisible unless
           you can see them.  Not really right, but... */
        nomore(MUSE_WAN_MAKE_INVISIBLE_SELF);
        if (obj->otyp == WAN_MAKE_INVISIBLE &&
            !m_has_property(mtmp, INVIS, os_outside, TRUE) &&
            (!mtmp->mpeaceful || see_invisible(&youmonst)) &&
            (!attacktype(mtmp->data, AT_GAZE) || cancelled(mtmp))) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_MAKE_INVISIBLE_SELF;
            continue;
        }
        nomore(MUSE_POT_INVISIBILITY);
        if (obj->otyp == POT_INVISIBILITY &&
            !m_has_property(mtmp, INVIS, os_outside, TRUE) &&
            (!mtmp->mpeaceful || see_invisible(&youmonst)) &&
            (!attacktype(mtmp->data, AT_GAZE) || cancelled(mtmp))) {
            m->misc = obj;
            m->has_misc = MUSE_POT_INVISIBILITY;
        }
        nomore(MUSE_WAN_MAKE_INVISIBLE);
        if (ranged_stuff && target != &youmonst &&
            obj->otyp == WAN_MAKE_INVISIBLE &&
            !m_has_property(target, INVIS, os_outside, TRUE) &&
            (!mtmp->mpeaceful || see_invisible(&youmonst)) &&
            (!attacktype(target->data, AT_GAZE) || cancelled(target))) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_MAKE_INVISIBLE;
        }
        nomore(MUSE_WAN_SPEED_MONSTER_SELF);
        if (obj->otyp == WAN_SPEED_MONSTER &&
            !very_fast(mtmp) && !mtmp->isgd) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_SPEED_MONSTER_SELF;
            continue;
        }
        nomore(MUSE_POT_SPEED);
        if (obj->otyp == POT_SPEED && !very_fast(mtmp) && !mtmp->isgd) {
            m->misc = obj;
            m->has_misc = MUSE_POT_SPEED;
        }
        nomore(MUSE_WAN_SPEED_MONSTER);
        if (ranged_stuff && obj->otyp == WAN_SPEED_MONSTER &&
            !very_fast(target) && !target->isgd) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_SPEED_MONSTER;
        }
        nomore(MUSE_WAN_POLYMORPH_SELF);
        if (obj->otyp == WAN_POLYMORPH && !mtmp->cham &&
            monstr[monsndx(mdat)] < 6) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_POLYMORPH_SELF;
            continue;
        }
        nomore(MUSE_POT_POLYMORPH);
        if (obj->otyp == POT_POLYMORPH && !mtmp->cham &&
            monstr[monsndx(mdat)] < 6) {
            m->misc = obj;
            m->has_misc = MUSE_POT_POLYMORPH;
        }
        nomore(MUSE_WAN_POLYMORPH);
        if (ranged_stuff && target != &youmonst &&
            obj->otyp == WAN_POLYMORPH && !target->cham && !resists_magm(target) &&
            (monstr[monsndx(tdat)] < 6 || mprof(mtmp, MP_WANDS) == P_EXPERT)) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_POLYMORPH;
        }
        nomore(MUSE_SCR_REMOVE_CURSE);
        if (obj->otyp == SCR_REMOVE_CURSE) {
            struct obj *otmp;

            for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
                if (otmp->cursed &&
                    (otmp->otyp == LOADSTONE || otmp->owornmask)) {
                    m->misc = obj;
                    m->has_misc = MUSE_SCR_REMOVE_CURSE;
                }
            }
        }
    }
    return (boolean) (! !m->has_misc);
#undef nomore
}

/* type of monster to polymorph into; defaults to one suitable for the
   current level rather than the totally arbitrary choice of newcham() */
static const struct permonst *
muse_newcham_mon(struct monst *mon)
{
    struct obj *m_armr;

    if ((m_armr = which_armor(mon, os_arm)) != 0) {
        if (Is_dragon_scales(m_armr))
            return Dragon_scales_to_pm(m_armr);
        else if (Is_dragon_mail(m_armr))
            return Dragon_mail_to_pm(m_armr);
    }
    return rndmonst(&mon->dlevel->z, rng_main);
}

int
use_misc(struct monst *mtmp, struct musable *m)
{
    int i;
    struct obj *otmp = m->misc;
    boolean vismon, oseen;

    if ((i = precheck(mtmp, otmp, m)) != 0)
        return i;
    vismon = mon_visible(mtmp);
    oseen = otmp && vismon;

    switch (m->has_misc) {
    case MUSE_WAN_MAKE_INVISIBLE:
    case MUSE_WAN_SPEED_MONSTER:
    case MUSE_WAN_POLYMORPH:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        m_using = TRUE;
        mbhit(mtmp, rn1(8, 6), otmp);
        m_using = FALSE;
        return 2;
    case MUSE_WAN_MAKE_INVISIBLE_SELF:
    case MUSE_WAN_SPEED_MONSTER_SELF:
    case MUSE_WAN_POLYMORPH_SELF:
        mzapmsg(mtmp, otmp, TRUE);
        otmp->spe--;
        m_using = TRUE;
        bhitm(mtmp, mtmp, otmp);
        m_using = FALSE;
        return 2;
    case MUSE_POT_GAIN_LEVEL:
        mquaffmsg(mtmp, otmp);
        if (otmp->cursed) {
            if (Can_rise_up(mtmp->mx, mtmp->my, &u.uz)) {
                int tolev = depth(&u.uz) - 1;
                d_level tolevel;

                get_level(&tolevel, tolev);
                /* insurance against future changes... */
                if (on_level(&tolevel, &u.uz))
                    goto skipmsg;
                if (vismon) {
                    pline("%s rises up, through the %s!", Monnam(mtmp),
                          ceiling(mtmp->mx, mtmp->my));
                    if (!objects[POT_GAIN_LEVEL].oc_name_known &&
                        !objects[POT_GAIN_LEVEL].oc_uname)
                        docall(otmp);
                }
                m_useup(mtmp, otmp);
                migrate_to_level(mtmp, ledger_no(&tolevel), MIGR_RANDOM, NULL);
                return 2;
            } else {
            skipmsg:
                if (vismon) {
                    pline("%s looks uneasy.", Monnam(mtmp));
                    if (!objects[POT_GAIN_LEVEL].oc_name_known &&
                        !objects[POT_GAIN_LEVEL].oc_uname)
                        docall(otmp);
                }
                m_useup(mtmp, otmp);
                return 2;
            }
        }
        if (vismon)
            pline("%s seems more experienced.", Monnam(mtmp));
        if (oseen)
            makeknown(POT_GAIN_LEVEL);
        m_useup(mtmp, otmp);
        if (!grow_up(mtmp, NULL))
            return 1;
        /* grew into genocided monster */
        return 2;
    case MUSE_POT_INVISIBILITY:
        mquaffmsg(mtmp, otmp);
        boolean effect;
        if (otmp->blessed)
            effect = set_property(mtmp, INVIS, 0, FALSE);
        else
            effect = set_property(mtmp, INVIS, rn1(15, 31), FALSE);
        newsym(mtmp->mx, mtmp->my);  /* update position */
        if (otmp->cursed) {
            you_aggravate(mtmp);
        }
        if (effect)
            makeknown(otmp->otyp);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_SPEED:
        mquaffmsg(mtmp, otmp);
        set_property(mtmp, FAST, rn1(10, 100 + 60 * bcsign(otmp)), FALSE);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_POLYMORPH:
        mquaffmsg(mtmp, otmp);
        if (vismon)
            pline("%s suddenly mutates!", Monnam(mtmp));
        newcham(mtmp, muse_newcham_mon(mtmp), FALSE, FALSE);
        if (oseen)
            makeknown(POT_POLYMORPH);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POLY_TRAP:
        if (vismon) {
            /* If the player can see the monster jump onto a square and
               polymorph, they'll know there's a trap there even if they can't
               see the square the trap's on (e.g. infravisible monster). */
            pline("%s deliberately %s onto a polymorph trap!", Monnam(mtmp),
                  makeplural(locomotion(mtmp->data, "jump")));
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc() due to worms */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        newcham(mtmp, NULL, FALSE, FALSE);
        return 2;
    case MUSE_BULLWHIP:
        /* attempt to disarm hero */
        if (uwep && !rn2(5)) {
            const char *The_whip = vismon ? "The bullwhip" : "A whip";
            int where_to = rn2(4);
            struct obj *obj = uwep;
            const char *hand;
            const char *the_weapon = the(xname(obj));

            hand = body_part(HAND);
            if (bimanual(obj))
                hand = makeplural(hand);

            if (vismon)
                pline("%s flicks a bullwhip towards your %s!", Monnam(mtmp),
                      hand);
            if (obj->otyp == HEAVY_IRON_BALL) {
                pline("%s fails to wrap around %s.", The_whip, the_weapon);
                return 1;
            }
            pline("%s wraps around %s you're wielding!", The_whip, the_weapon);
            if (welded(obj)) {
                pline("%s welded to your %s%c",
                      !is_plural(obj) ? "It is" : "They are", hand,
                      !obj->bknown ? '!' : '.');
                /* obj->bknown = 1; *//* welded() takes care of this */
                where_to = 0;
            }
            if (!where_to) {
                pline("The whip slips free.");  /* not `The_whip' */
                return 1;
            } else if (where_to == 3 && hates_silver(mtmp->data) &&
                       objects[obj->otyp].oc_material == SILVER) {
                /* this monster won't want to catch a silver weapon; drop it at 
                   hero's feet instead */
                where_to = 2;
            }
            uwepgone();
            freeinv(obj);
            switch (where_to) {
            case 1:    /* onto floor beneath mon */
                pline("%s yanks %s from your %s!", Monnam(mtmp), the_weapon,
                      hand);
                place_object(obj, level, mtmp->mx, mtmp->my);
                break;
            case 2:    /* onto floor beneath you */
                pline("%s yanks %s to the %s!", Monnam(mtmp), the_weapon,
                      surface(u.ux, u.uy));
                dropy(obj);
                break;
            case 3:    /* into mon's inventory */
                pline("%s snatches %s!", Monnam(mtmp), the_weapon);
                mpickobj(mtmp, obj);
                break;
            }
            return 1;
        }
        return 0;
    case MUSE_SCR_REMOVE_CURSE:
        mreadmsg(mtmp, otmp);
        if (canseemon(mtmp)) {
            if (confused(mtmp))
                pline("You feel as if %s needs some help.", mon_nam(mtmp));
            else
                pline("You feel like someone is helping %s.", mon_nam(mtmp));
            if (!objects[SCR_REMOVE_CURSE].oc_name_known &&
                !objects[SCR_REMOVE_CURSE].oc_uname)
                docall(otmp);
        }
        {
            struct obj *obj;

            for (obj = mtmp->minvent; obj; obj = obj->nobj) {
                /* gold isn't subject to cursing and blessing */
                if (obj->oclass == COIN_CLASS)
                    continue;
                if (otmp->blessed || otmp->owornmask ||
                    obj->otyp == LOADSTONE) {
                    if (confused(mtmp))
                        blessorcurse(obj, 2, rng_main);
                    else
                        uncurse(obj);
                }
            }
        }
        m_useup(mtmp, otmp);
        return 0;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_misc);
        break;
    }
    return 0;
}

void
you_aggravate(struct monst *mtmp)
{
    pline("For some reason, %s presence is known to you.",
          s_suffix(noit_mon_nam(mtmp)));
    cls();
    dbuf_set(mtmp->mx, mtmp->my, S_unexplored, 0, 0, 0, 0,
             dbuf_monid(mtmp, mtmp->mx, mtmp->my, rn2), 0, 0, 0);
    display_self();
    pline("You feel aggravated at %s.", noit_mon_nam(mtmp));
    win_pause_output(P_MAP);
    doredraw();
    cancel_helplessness(hm_unconscious,
                        "Aggravated, you are jolted into full consciousness.");

    newsym(mtmp->mx, mtmp->my);
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);
}

int
rnd_misc_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = monstr[monsndx(pm)];

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;
    /* Unlike other rnd_item functions, we only allow _weak_ monsters to have
       this item; after all, the item will be used to strengthen the monster
       and strong monsters won't use it at all... */
    if (difficulty < 6 && !rn2_on_rng(30, rng))
        return rn2_on_rng(6, rng) ? POT_POLYMORPH : WAN_POLYMORPH;

    if (!rn2_on_rng(40, rng) && !nonliving(pm))
        return AMULET_OF_LIFE_SAVING;

    switch (rn2_on_rng(3, rng)) {
    case 0:
        if (mtmp->isgd)
            return 0;
        return rn2_on_rng(6, rng) ? POT_SPEED : WAN_SPEED_MONSTER;
    case 1:
        return rn2_on_rng(6, rng) ? POT_INVISIBILITY : WAN_MAKE_INVISIBLE;
    case 2:
        return POT_GAIN_LEVEL;
    }
     /*NOTREACHED*/ return 0;
}

boolean
searches_for_item(struct monst *mon, struct obj *obj)
{
    int typ = obj->otyp;

    /* don't loot bones piles */
    if (is_animal(mon->data) || mindless(mon->data) ||
        mon->data == &mons[PM_GHOST])
        return FALSE;

    /* discharged wands/magical horns */
    if ((obj->oclass == WAND_CLASS ||
        typ == FROST_HORN || typ == FIRE_HORN) && obj->spe <= 0)
        return FALSE;
    if (typ == POT_INVISIBILITY)
        return (boolean) (!m_has_property(mon, INVIS, W_MASK(os_outside), TRUE) &&
                          !attacktype(mon->data, AT_GAZE));
    if (typ == POT_SPEED)
        return (boolean) (!ifast(mon));

    switch (obj->oclass) {
    case WAND_CLASS:
        if (typ == WAN_DIGGING)
            return (boolean) (!levitates(mon));
        if (objects[typ].oc_dir == RAY || typ == WAN_STRIKING ||
            typ == WAN_TELEPORTATION || typ == WAN_CREATE_MONSTER ||
            typ == WAN_SLOW_MONSTER || typ == WAN_UNDEAD_TURNING ||
            typ == WAN_MAKE_INVISIBLE || typ == WAN_SPEED_MONSTER ||
            typ == WAN_POLYMORPH)
            return TRUE;
        break;
    case POTION_CLASS:
        if (typ == POT_HEALING || typ == POT_EXTRA_HEALING ||
            typ == POT_FULL_HEALING || typ == POT_POLYMORPH ||
            typ == POT_GAIN_LEVEL || typ == POT_PARALYSIS || typ == POT_SLEEPING
            || typ == POT_ACID || typ == POT_CONFUSION)
            return TRUE;
        if (typ == POT_BLINDNESS && !attacktype(mon->data, AT_GAZE))
            return TRUE;
        break;
    case SCROLL_CLASS:
        if (typ == SCR_TELEPORTATION || typ == SCR_CREATE_MONSTER ||
            typ == SCR_EARTH || typ == SCR_REMOVE_CURSE)
            return TRUE;
        break;
    case AMULET_CLASS:
        if (typ == AMULET_OF_LIFE_SAVING)
            return (boolean) (!nonliving(mon->data));
        if (typ == AMULET_OF_REFLECTION)
            return TRUE;
        break;
    case TOOL_CLASS:
        if (typ == PICK_AXE)
            return (boolean) needspick(mon->data);
        if (typ == UNICORN_HORN)
            return (boolean) (!obj->cursed && !is_unicorn(mon->data));
        if (typ == FROST_HORN || typ == FIRE_HORN)
            return (obj->spe > 0) && can_blow_instrument(mon->data);
        break;
    case FOOD_CLASS:
        if (typ == CORPSE)
            return (boolean) (((mon->misc_worn_check & W_MASK(os_armg)) &&
                               touch_petrifies(&mons[obj->corpsenm])) ||
                              (!resists_ston(mon) &&
                               (obj->corpsenm == PM_LIZARD ||
                                (acidic(&mons[obj->corpsenm]) &&
                                 obj->corpsenm != PM_GREEN_SLIME))));
        if (typ == EGG)
            return (boolean) (touch_petrifies(&mons[obj->corpsenm]));
        break;
    default:
        break;
    }

    return FALSE;
}

boolean
mon_reflects(struct monst * mon, const char *str)
{
    struct obj *orefl = which_armor(mon, os_arms);

    if (orefl && orefl->otyp == SHIELD_OF_REFLECTION) {
        if (str) {
            pline(str, s_suffix(mon_nam(mon)), "shield");
            makeknown(SHIELD_OF_REFLECTION);
        }
        return TRUE;
    } else if (arti_reflects(MON_WEP(mon))) {
        /* due to wielded artifact weapon */
        if (str)
            pline(str, s_suffix(mon_nam(mon)), "weapon");
        return TRUE;
    } else if ((orefl = which_armor(mon, os_amul)) &&
               orefl->otyp == AMULET_OF_REFLECTION) {
        if (str) {
            pline(str, s_suffix(mon_nam(mon)), "amulet");
            makeknown(AMULET_OF_REFLECTION);
        }
        return TRUE;
    } else if ((orefl = which_armor(mon, os_arm)) &&
               (orefl->otyp == SILVER_DRAGON_SCALES ||
                orefl->otyp == SILVER_DRAGON_SCALE_MAIL)) {
        if (str)
            pline(str, s_suffix(mon_nam(mon)), "armor");
        return TRUE;
    } else if (mon->data == &mons[PM_SILVER_DRAGON] ||
               mon->data == &mons[PM_CHROMATIC_DRAGON]) {
        /* Silver dragons only reflect when mature; babies do not */
        if (str)
            pline(str, s_suffix(mon_nam(mon)), "scales");
        return TRUE;
    }
    return FALSE;
}

boolean
ureflects(const char *fmt, const char *str)
{
    /* Check from outermost to innermost objects */
    unsigned reflect_reason = u_have_property(REFLECTING, ANY_PROPERTY, FALSE);
    if (reflect_reason & W_MASK(os_arms)) {
        if (fmt && str) {
            pline(fmt, str, "shield");
            makeknown(SHIELD_OF_REFLECTION);
        }
        return TRUE;
    } else if (reflect_reason & W_MASK(os_wep)) {
        /* Due to wielded artifact weapon */
        if (fmt && str)
            pline(fmt, str, "weapon");
        return TRUE;
    } else if (reflect_reason & W_MASK(os_amul)) {
        if (fmt && str) {
            pline(fmt, str, "medallion");
            makeknown(AMULET_OF_REFLECTION);
        }
        return TRUE;
    } else if (reflect_reason & W_MASK(os_arm)) {
        if (fmt && str)
            pline(fmt, str, "armor");
        return TRUE;
    } else if (reflect_reason & W_MASK(os_polyform)) {
        if (fmt && str)
            pline(fmt, str, "scales");
        return TRUE;
    } else if (reflect_reason) {
        impossible("Reflecting for unknown reason");
        return TRUE;
    }
    return FALSE;
}


/* TRUE if the monster ate something */
boolean
munstone(struct monst * mon, boolean by_you)
{
    struct obj *obj;

    if (resists_ston(mon))
        return FALSE;
    if (mon->meating || !mon->mcanmove || mon->msleeping)
        return FALSE;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        /* Monsters can also use potions of acid */
        if ((obj->otyp == POT_ACID) ||
            (obj->otyp == CORPSE &&
             (obj->corpsenm == PM_LIZARD ||
              (acidic(&mons[obj->corpsenm]) &&
               obj->corpsenm != PM_GREEN_SLIME)))) {
            mon_consume_unstone(mon, obj, by_you, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

static void
mon_consume_unstone(struct monst *mon, struct obj *obj, boolean by_you,
                    boolean stoning)
{
    int nutrit = (obj->otyp == CORPSE) ? dog_nutrition(mon, obj) : 0;

    /* also sets meating */

    /* give a "<mon> is slowing down" message and also remove intrinsic speed
       (comparable to similar effect on the hero) */
    if (stoning) {
        if (canseemon(mon))
            pline("%s is slowing down.", Monnam(mon));
        set_property(mon, FAST, -1, TRUE);
    }

    if (mon_visible(mon)) {
        long save_quan = obj->quan;

        obj->quan = 1L;
        pline("%s %ss %s.", Monnam(mon),
              (obj->otyp == POT_ACID) ? "quaff" : "eat", distant_name(obj,
                                                                      doname));
        obj->quan = save_quan;
    } else
        You_hear("%s.", (obj->otyp == POT_ACID) ? "drinking" : "chewing");
    if (((obj->otyp == POT_ACID) || acidic(&mons[obj->corpsenm])) &&
        !resists_acid(mon)) {
        mon->mhp -= rnd(15);
        pline("%s has a very bad case of stomach acid.", Monnam(mon));
    }
    if (mon->mhp <= 0) {
        pline("%s dies!", Monnam(mon));
        m_useup(mon, obj);
        if (by_you)
            xkilled(mon, 0);
        else
            mondead(mon);
        return;
    }
    if (stoning && canseemon(mon)) {
        if (Hallucination)
            pline("What a pity - %s just ruined a future piece of art!",
                  mon_nam(mon));
        else
            pline("%s seems limber!", Monnam(mon));
    }
    if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD && confused(mon)) {
        if (property_timeout(mon, CONFUSION) >= 2) {
            set_property(mon, CONFUSION, -2, TRUE);
            set_property(mon, CONFUSION, 2, TRUE);
        }
    }
    if (mon->mtame && !mon->isminion && nutrit > 0) {
        struct edog *edog = EDOG(mon);

        if (edog->hungrytime < moves)
            edog->hungrytime = moves;
        edog->hungrytime += nutrit;
    }
    mon->mlstmv = moves;        /* it takes a turn */
    m_useup(mon, obj);
}

/*muse.c*/

