/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"
#include "edog.h"
#include <ctype.h>

static boolean restrap(struct monst *);
static int pick_animal(void);
static int select_newcham_form(struct monst *);
static void kill_eggs(struct obj *);

#define LEVEL_SPECIFIC_NOCORPSE(mdat) \
         (Is_rogue_level(&u.uz) || \
           (level->flags.graveyard && is_undead(mdat) && rn2(3)))


static struct obj *make_corpse(struct monst *);
static void m_detach(struct monst *, const struct permonst *);
static void lifesaved_monster(struct monst *);

/* convert the monster index of an undead to its living counterpart */
int
undead_to_corpse(int mndx)
{
    switch (mndx) {
    case PM_KOBOLD_ZOMBIE:
    case PM_KOBOLD_MUMMY:
        mndx = PM_KOBOLD;
        break;
    case PM_DWARF_ZOMBIE:
    case PM_DWARF_MUMMY:
        mndx = PM_DWARF;
        break;
    case PM_GNOME_ZOMBIE:
    case PM_GNOME_MUMMY:
        mndx = PM_GNOME;
        break;
    case PM_ORC_ZOMBIE:
    case PM_ORC_MUMMY:
        mndx = PM_ORC;
        break;
    case PM_ELF_ZOMBIE:
    case PM_ELF_MUMMY:
        mndx = PM_ELF;
        break;
    case PM_VAMPIRE:
    case PM_VAMPIRE_LORD:
    case PM_HUMAN_ZOMBIE:
    case PM_HUMAN_MUMMY:
        mndx = PM_HUMAN;
        break;
    case PM_GIANT_ZOMBIE:
    case PM_GIANT_MUMMY:
        mndx = PM_GIANT;
        break;
    case PM_ETTIN_ZOMBIE:
    case PM_ETTIN_MUMMY:
        mndx = PM_ETTIN;
        break;
    default:
        break;
    }
    return mndx;
}

/* Convert the monster index of some monsters (such as quest guardians)
 * to their generic species type.
 *
 * Return associated character class monster, rather than species
 * if mode is 1.
 */
int
genus(int mndx, int mode)
{
    switch (mndx) {
/* Quest guardians */
    case PM_STUDENT:
        mndx = mode ? PM_ARCHEOLOGIST : PM_HUMAN;
        break;
    case PM_CHIEFTAIN:
        mndx = mode ? PM_BARBARIAN : PM_HUMAN;
        break;
    case PM_NEANDERTHAL:
        mndx = mode ? PM_CAVEMAN : PM_HUMAN;
        break;
    case PM_ATTENDANT:
        mndx = mode ? PM_HEALER : PM_HUMAN;
        break;
    case PM_PAGE:
        mndx = mode ? PM_KNIGHT : PM_HUMAN;
        break;
    case PM_ABBOT:
        mndx = mode ? PM_MONK : PM_HUMAN;
        break;
    case PM_ACOLYTE:
        mndx = mode ? PM_PRIEST : PM_HUMAN;
        break;
    case PM_HUNTER:
        mndx = mode ? PM_RANGER : PM_HUMAN;
        break;
    case PM_THUG:
        mndx = mode ? PM_ROGUE : PM_HUMAN;
        break;
    case PM_ROSHI:
        mndx = mode ? PM_SAMURAI : PM_HUMAN;
        break;
    case PM_GUIDE:
        mndx = mode ? PM_TOURIST : PM_HUMAN;
        break;
    case PM_APPRENTICE:
        mndx = mode ? PM_WIZARD : PM_HUMAN;
        break;
    case PM_WARRIOR:
        mndx = mode ? PM_VALKYRIE : PM_HUMAN;
        break;
    default:
        if (mndx >= LOW_PM && mndx < NUMMONS) {
            const struct permonst *ptr = &mons[mndx];

            if (is_human(ptr))
                mndx = PM_HUMAN;
            else if (is_elf(ptr))
                mndx = PM_ELF;
            else if (is_dwarf(ptr))
                mndx = PM_DWARF;
            else if (is_gnome(ptr))
                mndx = PM_GNOME;
            else if (is_orc(ptr))
                mndx = PM_ORC;
        }
        break;
    }
    return mndx;
}

/* convert monster index to chameleon index */
int
pm_to_cham(int mndx)
{
    int mcham;

    switch (mndx) {
    case PM_CHAMELEON:
        mcham = CHAM_CHAMELEON;
        break;
    case PM_DOPPELGANGER:
        mcham = CHAM_DOPPELGANGER;
        break;
    case PM_SANDESTIN:
        mcham = CHAM_SANDESTIN;
        break;
    default:
        mcham = CHAM_ORDINARY;
        break;
    }
    return mcham;
}

/* convert chameleon index to monster index */
static const short cham_to_pm[] = {
    NON_PM,     /* placeholder for CHAM_ORDINARY */
    PM_CHAMELEON,
    PM_DOPPELGANGER,
    PM_SANDESTIN,
};

/* for deciding whether corpse or statue will carry along full monster data */
#define KEEPTRAITS(mon) ((mon)->isshk || (mon)->mtame ||                \
                         ((mon)->data->geno & G_UNIQ) ||                \
                         is_reviver((mon)->data) ||                     \
                         /* normally leader the will be unique, */      \
                         /* but he might have been polymorphed  */      \
                         (mon)->m_id == u.quest_status.leader_m_id ||     \
                         /* special cancellation handling for these */  \
                         (dmgtype((mon)->data, AD_SEDU) ||              \
                          dmgtype((mon)->data, AD_SSEX)))

/* Creates a monster corpse, a "special" corpse, or nothing if it doesn't
 * leave corpses.  Monsters which leave "special" corpses should have
 * G_NOCORPSE set in order to prevent wishing for one, finding tins of one,
 * etc....
 */
static struct obj *
make_corpse(struct monst *mtmp)
{
    const struct permonst *mdat = mtmp->data;
    int num;
    struct obj *obj = NULL;
    int x = mtmp->mx, y = mtmp->my;
    int mndx = monsndx(mdat);

    switch (mndx) {
    case PM_GRAY_DRAGON:
    case PM_SILVER_DRAGON:
    case PM_RED_DRAGON:
    case PM_ORANGE_DRAGON:
    case PM_WHITE_DRAGON:
    case PM_BLACK_DRAGON:
    case PM_BLUE_DRAGON:
    case PM_GREEN_DRAGON:
    case PM_YELLOW_DRAGON:
        /* Make dragon scales.  This assumes that the order of the */
        /* dragons is the same as the order of the scales.  */
        if ((!mtmp->mrevived || !rn2(7)) && !rn2_on_rng(3, rng_dragonscales)) {
            num = GRAY_DRAGON_SCALES + mndx - PM_GRAY_DRAGON;
            obj = mksobj_at(num, level, x, y, FALSE, FALSE, rng_main);
            obj->spe = 0;
            obj->cursed = obj->blessed = FALSE;
        }
        goto default_1;

    case PM_WHITE_UNICORN:
    case PM_GRAY_UNICORN:
    case PM_BLACK_UNICORN:
        if (mtmp->mrevived && rn2(20)) {
            if (canseemon(mtmp))
                pline(msgc_failrandom,
                      "%s recently regrown horn crumbles to dust.",
                      s_suffix(Monnam(mtmp)));
        } else
            mksobj_at(UNICORN_HORN, level, x, y, TRUE, FALSE, rng_main);
        goto default_1;
    case PM_LONG_WORM:
        mksobj_at(WORM_TOOTH, level, x, y, TRUE, FALSE, rng_main);
        goto default_1;
    case PM_VAMPIRE:
    case PM_VAMPIRE_LORD:
        /* include mtmp in the mkcorpstat() call */
        num = undead_to_corpse(mndx);
        obj = mkcorpstat(CORPSE, mtmp, &mons[num], level, x, y, TRUE, rng_main);
        obj->age -= 100;        /* this is an *OLD* corpse */
        break;
    case PM_KOBOLD_MUMMY:
    case PM_DWARF_MUMMY:
    case PM_GNOME_MUMMY:
    case PM_ORC_MUMMY:
    case PM_ELF_MUMMY:
    case PM_HUMAN_MUMMY:
    case PM_GIANT_MUMMY:
    case PM_ETTIN_MUMMY:
    case PM_KOBOLD_ZOMBIE:
    case PM_DWARF_ZOMBIE:
    case PM_GNOME_ZOMBIE:
    case PM_ORC_ZOMBIE:
    case PM_ELF_ZOMBIE:
    case PM_HUMAN_ZOMBIE:
    case PM_GIANT_ZOMBIE:
    case PM_ETTIN_ZOMBIE:
        num = undead_to_corpse(mndx);
        obj = mkcorpstat(CORPSE, mtmp, &mons[num], level, x, y, TRUE,
                         rng_main);
        obj->age -= 100;        /* this is an *OLD* corpse */
        break;
    case PM_IRON_GOLEM:
        num = dice(2, 6);
        while (num--)
            obj = mksobj_at(IRON_CHAIN, level, x, y, TRUE, FALSE, rng_main);
        mtmp->mnamelth = 0;
        break;
    case PM_GLASS_GOLEM:
        num = dice(2, 4);       /* very low chance of creating all glass gems */
        while (num--)
            obj = mksobj_at((LAST_GEM + rnd(9)), level, x, y, TRUE, FALSE,
                            rng_main);
        mtmp->mnamelth = 0;
        break;
    case PM_CLAY_GOLEM:
        obj = mksobj_at(ROCK, level, x, y, FALSE, FALSE, rng_main);
        obj->quan = (long)(rn2(20) + 50);
        obj->owt = weight(obj);
        mtmp->mnamelth = 0;
        break;
    case PM_STONE_GOLEM:
        obj = mkcorpstat(STATUE, NULL, mdat, level, x, y, FALSE, rng_main);
        break;
    case PM_WOOD_GOLEM:
        num = dice(2, 4);
        while (num--) {
            obj = mksobj_at(QUARTERSTAFF, level, x, y, TRUE, FALSE, rng_main);
        }
        mtmp->mnamelth = 0;
        break;
    case PM_LEATHER_GOLEM:
        num = dice(2, 4);
        while (num--)
            obj = mksobj_at(LEATHER_ARMOR, level, x, y, TRUE, FALSE, rng_main);
        mtmp->mnamelth = 0;
        break;
    case PM_GOLD_GOLEM:
        /* Good luck gives more coins */
        obj = mkgold((long)(200 - rnl(101)), level, x, y, rng_main);
        mtmp->mnamelth = 0;
        break;
    case PM_PAPER_GOLEM:
        num = rnd(4);
        while (num--)
            obj = mksobj_at(SCR_BLANK_PAPER, level, x, y, TRUE, FALSE, rng_main);
        mtmp->mnamelth = 0;
        break;
    default_1:
    default:
        if (mvitals[mndx].mvflags & G_NOCORPSE)
            return NULL;
        else    /* preserve the unique traits of some creatures */
            obj =
                mkcorpstat(CORPSE, KEEPTRAITS(mtmp) ? mtmp : 0, mdat, level, x,
                           y, TRUE, rng_main);
        break;
    }
    /* All special cases should precede the G_NOCORPSE check */

    /* if polymorph or undead turning has killed this monster, prevent the same
       attack beam from hitting its corpse */
    if (flags.bypasses)
        bypass_obj(obj);

    if (mtmp->mnamelth)
        obj = oname(obj, NAME(mtmp));

    /* Avoid "It was hidden under a green mold corpse!" during Blind combat. An
       unseen monster referred to as "it" could be killed and leave a corpse.
       If a hider then hid underneath it, you could be told the corpse type of a
       monster that you never knew was there without this.  The code in hitmu()
       substitutes the word "something" if the corpse's obj->dknown is 0. */
    if (Blind && !sensemon(mtmp))
        obj->dknown = 0;

#ifdef INVISIBLE_OBJECTS
    /* Invisible monster ==> invisible corpse */
    obj->oinvis = mtmp->minvis;
#endif

    stackobj(obj);
    if (mtmp->dlevel == level)
        newsym(x, y);
    return obj;
}


/* check mtmp and water/lava for compatibility, 0 (survived), 1 (died) */
int
minliquid(struct monst *mtmp)
{
    boolean inpool, inlava, infountain;

    inpool = is_pool(level, mtmp->mx, mtmp->my) && !is_flyer(mtmp->data) &&
        !is_floater(mtmp->data);
    inlava = is_lava(level, mtmp->mx, mtmp->my) && !is_flyer(mtmp->data) &&
        !is_floater(mtmp->data);
    infountain = IS_FOUNTAIN(level->locations[mtmp->mx][mtmp->my].typ);

    /* Flying and levitation keeps our steed out of the liquid */
    /* (but not water-walking or swimming) */
    if (mtmp == u.usteed && (Flying || Levitation))
        return 0;

    /* Gremlin multiplying won't go on forever since the hit points keep going
       down, and when it gets to 1 hit point the clone function will fail. */
    if (mtmp->data == &mons[PM_GREMLIN] && (inpool || infountain) && rn2(3)) {
        if (split_mon(mtmp, NULL))
            dryup(mtmp->mx, mtmp->my, FALSE);
        if (inpool)
            water_damage_chain(mtmp->minvent, FALSE);
        return 0;
    } else if (mtmp->data == &mons[PM_IRON_GOLEM] && inpool && !rn2(5)) {
        int dam = dice(2, 6);

        if (cansee(mtmp->mx, mtmp->my))
            pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                  "%s rusts.", Monnam(mtmp));
        mtmp->mhp -= dam;
        if (mtmp->mhpmax > dam)
            mtmp->mhpmax -= dam;
        if (mtmp->mhp < 1) {
            mondead(mtmp);
            if (DEADMONSTER(mtmp))
                return 1;
        }
        water_damage_chain(mtmp->minvent, FALSE);
        return 0;
    }

    if (inlava) {
        boolean alive_means_lifesaved = TRUE;
        /*
         * Lava effects much as water effects. Lava likers are able to
         * protect their stuff. Fire resistant monsters can only protect
         * themselves  --ALI
         */
        if (!is_clinger(mtmp->data) && !likes_lava(mtmp->data)) {
            if (!resists_fire(mtmp)) {
                if (cansee(mtmp->mx, mtmp->my))
                    pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                          "%s %s.", Monnam(mtmp),
                          mtmp->data == &mons[PM_WATER_ELEMENTAL] ?
                          "boils away" : "burns to a crisp");
                mondead(mtmp);
            } else {
                if (--mtmp->mhp < 1) {
                    if (cansee(mtmp->mx, mtmp->my))
                        pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                              "%s surrenders to the fire.", Monnam(mtmp));
                    mondead(mtmp);
                } else {
                    alive_means_lifesaved = FALSE;
                    if (cansee(mtmp->mx, mtmp->my))
                        pline(mtmp->mtame ? msgc_petwarning : msgc_monneutral,
                              "%s burns slightly.", Monnam(mtmp));
                }
            }
            if (!DEADMONSTER(mtmp)) {
                fire_damage(mtmp->minvent, FALSE, FALSE, mtmp->mx, mtmp->my);
                if (alive_means_lifesaved) {
                    rloc(mtmp, TRUE);
                    /* Analogous to player case: if we have nowhere to place the
                       monster, it ends up back in the lava, and dies again */
                    minliquid(mtmp);
                }
                return 0;
            }
            return 1;
        }
    } else if (inpool) {
        /* Most monsters drown in pools.  flooreffects() will take care of
           water damage to dead monsters' inventory, but survivors need to be
           handled here.  Swimmers are able to protect their stuff... */
        if (!is_clinger(mtmp->data)
            && !is_swimmer(mtmp->data) && !amphibious(mtmp->data)) {
            if (cansee(mtmp->mx, mtmp->my)) {
                pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                      "%s drowns.", Monnam(mtmp));
            }
            if (u.ustuck && Engulfed && u.ustuck == mtmp) {
                /* This can happen after a purple worm plucks you off a flying
                   steed while you are over water. */
                pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                      "%s sinks as water rushes in and flushes you out.",
                      Monnam(mtmp));
            }
            mondead(mtmp);
            if (!DEADMONSTER(mtmp)) {
                rloc(mtmp, TRUE);
                water_damage_chain(mtmp->minvent, FALSE);
                minliquid(mtmp);
                return 0;
            }
            return 1;
        }
    } else {
        /* but eels have a difficult time outside */
        if (mtmp->data->mlet == S_EEL && !Is_waterlevel(&u.uz)) {
            if (mtmp->mhp >= 2)
                mtmp->mhp--;
            monflee(mtmp, 2, FALSE, FALSE);
        }
    }
    return 0;
}

/* Called whenever a monster changes speed, and ensures that the number of
   partial movement points the monster had changes appropriately. Also called
   for the player at turn boundary (we cache the player's speed within a
   turn). */
void
adjust_move_offset(struct monst *mon, int oldspeed, int newspeed)
{
    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2)

       If the offset wasn't initialized, leave it alone, so that we know it's
       still uninitialized. */
    if (mon->moveoffset >= NORMAL_SPEED)
        return;

    /* Increasing the speed value by 1 effectively increases the offset by 1 for
       each turn on the turn counter, based on the formula used. We thus need to
       decrease the offset by the speed increase times the turn counter. All
       this is modulo 12. */
    int speedinc = newspeed - oldspeed;
    if (speedinc < 0)
        speedinc = NORMAL_SPEED - (-speedinc % NORMAL_SPEED);
    else
        speedinc %= NORMAL_SPEED;

    int offsetdec = ((long long)moves * (long long)speedinc) % NORMAL_SPEED;

    mon->moveoffset = (mon->moveoffset + NORMAL_SPEED - offsetdec) %
        NORMAL_SPEED;
}

boolean
can_act_this_turn(struct monst *mon)
{
    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2)

       The movement structures might not have been set correctly for this turn,
       if we're loading an old save. Detect that this has happened using
       youmonst.moveoffset >= 12, and give the player 1 action and monsters no
       actions this turn. */
    if (youmonst.moveoffset >= NORMAL_SPEED)
        return mon == &youmonst && flags.actions == 0;

    /* Work out how many actions the player has this turn. Each monster has a
       range of 12 possible values for movement points that turn. The low end
       of the range is given by mcalcmove... */
    int movement = mcalcmove(mon);

    /* ...and the position modulo 12 is given by (speed * turn counter) +
       move offset. */
    int modulo = (((long long)moves * (long long)movement) +
                  mon->moveoffset) % NORMAL_SPEED;

    /* C sucks at modular arithmetic :-( */
    movement += (modulo + NORMAL_SPEED - (movement % NORMAL_SPEED)) %
        NORMAL_SPEED;

    /* It costs 12 movement points to perform an action. */
    int actions_this_turn = movement / NORMAL_SPEED;

    /* We can move if we've done fewer actions this turn than we have
       available. */
    return flags.actions < actions_this_turn;
}

int
mcalcmove(struct monst *mon)
{
    int mmove = mon->data->mmove;

    if (mon == &youmonst) {
        /* The player has different, and somewhat randomized, movment rules. */
        return u.moveamt;
    }

    /* Note: MSLOW's `+ 1' prevents slowed speed 1 getting reduced to 0;
       MFAST's `+ 2' prevents hasted speed 1 from becoming a no-op; both
       adjustments have negligible effect on higher speeds. */
    if (mon->mspeed == MSLOW)
        mmove = (2 * mmove + 1) / 3;
    else if (mon->mspeed == MFAST)
        mmove = (4 * mmove + 2) / 3;

    if (mon == u.usteed) {
        /* This used to have a flags.mv check, but that has been conclusively
           been shown to be a) abusable, and b) really confusing in practice.
           (flags.mv no longer exists, but the same effect could be achieved
           using flags.occupation. It's just that this is no longer an effect
           that's worth achieving.) */
        if (u.ugallop) {
            /* movement is 1.50 times normal; randomization has been removed
               because mcalcmove now needs to be deterministic */
            mmove = (3 * mmove) / 2;
        }
    }

    return mmove;
}

/* actions that happen once per ``turn'', regardless of each
   individual monster's metabolism; some of these might need to
   be reclassified to occur more in proportion with movement rate */
void
mcalcdistress(void)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;

        /* must check non-moving monsters once/turn in case they managed to end
           up in liquid */
        if (mtmp->data->mmove == 0) {
            if (turnstate.vision_full_recalc)
                vision_recalc(0);
            if (minliquid(mtmp))
                continue;
        }

        /* regenerate hit points */
        mon_regen(mtmp, FALSE);

        /* possibly polymorph shapechangers and lycanthropes */
        if (mtmp->cham && !rn2(6))
            newcham(mtmp, NULL, FALSE, FALSE);
        were_change(mtmp);

        /* gradually time out temporary problems */
        if (mtmp->mblinded && !--mtmp->mblinded)
            mtmp->mcansee = 1;
        if (mtmp->mfrozen && !--mtmp->mfrozen)
            mtmp->mcanmove = 1;
        if (mtmp->mfleetim && !--mtmp->mfleetim)
            mtmp->mflee = 0;

        /* FIXME: mtmp->mlstmv ought to be updated here */
    }
}

static struct monst *nmtmp = (struct monst *)0;

int
movemon(void)
{
    struct monst *mtmp;
    boolean somebody_can_move = FALSE;

    /*
       Some of you may remember the former assertion here that because of
       deaths and other actions, a simple one-pass algorithm wasn't possible
       for movemon.  Deaths are no longer removed to the separate list fdmon;
       they are simply left in the chain with hit points <= 0, to be cleaned up
       at the end of the pass.

       The only other actions which cause monsters to be removed from the chain
       are level migrations and losedogs().  I believe losedogs() is a cleanup
       routine not associated with monster movements, and monsters can only
       affect level migrations on themselves, not others (hence the fetching of
       nmon before moving the monster).  Currently, monsters can jump into
       traps, read cursed scrolls of teleportation, and drink cursed potions of
       raise level to change levels.  These are all reflexive at this point.
       Should one monster be able to level teleport another, this scheme would
       have problems. */

    for (mtmp = level->monlist; mtmp; mtmp = nmtmp) {
        nmtmp = mtmp->nmon;

        /* Find a monster that we have not treated yet.  */
        if (DEADMONSTER(mtmp))
            continue;
        if (!can_act_this_turn(mtmp))
            continue;

        somebody_can_move = TRUE;

        if (turnstate.vision_full_recalc)
            vision_recalc(0);   /* vision! */

        if (minliquid(mtmp))
            continue;

        if (is_hider(mtmp->data)) {
            /* unwatched mimics and piercers may hide again [MRS] */
            if (restrap(mtmp))
                continue;
            if (mtmp->m_ap_type == M_AP_FURNITURE ||
                mtmp->m_ap_type == M_AP_OBJECT)
                continue;
            if (mtmp->mundetected)
                continue;
        }

        /* continue if the monster died fighting */
        if (Conflict && !mtmp->iswiz && mtmp->mcansee) {
            /* Note: Conflict does not take effect in the first round.
               Therefore, A monster when stepping into the area will get to
               swing at you.  The call to fightm() must be _last_.  The monster
               might have died if it returns 1. */
            if (couldsee(mtmp->mx, mtmp->my) &&
                (distu(mtmp->mx, mtmp->my) <= BOLT_LIM * BOLT_LIM) &&
                fightm(mtmp))
                continue;       /* mon might have died */
        }
        if (dochugw(mtmp))      /* otherwise just move the monster */
            continue;
    }

    if (any_light_source())
        /* in case a mon moved with a light source */
        turnstate.vision_full_recalc = TRUE;
    dmonsfree(level);   /* remove all dead monsters */

    /* a monster may have levteleported player -dlc */
    if (deferred_goto())
        /* changed levels, so these monsters are dormant */
        somebody_can_move = FALSE;

    return somebody_can_move;
}


#define mstoning(obj) (ofood(obj) && \
                       (touch_petrifies(&mons[(obj)->corpsenm]) || \
                        (obj)->corpsenm == PM_MEDUSA))

/*
 * Maybe eat a metallic object (not just gold).
 * Return value: 0 => nothing happened, 1 => monster ate something,
 * 2 => monster died (it must have grown into a genocided form, but
 * that can't happen at present because nothing which eats objects
 * has young and old forms).
 */
int
meatmetal(struct monst *mtmp)
{
    struct obj *otmp;
    const struct permonst *ptr;
    int poly, grow, heal, mstone;

    /* If a pet, eating is handled separately, in dog.c */
    if (mtmp->mtame)
        return 0;

    /* Eats topmost metal object if it is there */
    for (otmp = level->objects[mtmp->mx][mtmp->my]; otmp;
         otmp = otmp->nexthere) {
        if (mtmp->data == &mons[PM_RUST_MONSTER] && !is_rustprone(otmp))
            continue;
        if (is_metallic(otmp) && !obj_resists(otmp, 5, 95) &&
            touch_artifact(otmp, mtmp)) {
            if (mtmp->data == &mons[PM_RUST_MONSTER] && otmp->oerodeproof) {
                if (canseemon(mtmp)) {
                    pline(msgc_itemloss, "%s eats %s!", Monnam(mtmp),
                          distant_name(otmp, doname));
                }
                /* The object's rustproofing is gone now */
                otmp->oerodeproof = 0;
                mtmp->mstun = 1;
                if (canseemon(mtmp))
                    pline_implied(
                        mtmp->mtame ? msgc_petneutral : msgc_monneutral,
                        "%s spits %s out in disgust!", Monnam(mtmp),
                        distant_name(otmp, doname));
                /* KMH -- Don't eat indigestible/choking objects */
            } else if (otmp->otyp != AMULET_OF_STRANGULATION &&
                       otmp->otyp != RIN_SLOW_DIGESTION) {
                if (cansee(mtmp->mx, mtmp->my))
                    pline(msgc_itemloss, "%s eats %s!", Monnam(mtmp),
                          distant_name(otmp, doname));
                else
                    You_hear(msgc_itemloss, "a crunching sound.");
                mtmp->meating = otmp->owt / 2 + 1;
                /* Heal up to the object's weight in hp */
                if (mtmp->mhp < mtmp->mhpmax) {
                    mtmp->mhp += objects[otmp->otyp].oc_weight;
                    if (mtmp->mhp > mtmp->mhpmax)
                        mtmp->mhp = mtmp->mhpmax;
                }
                if (otmp == uball) {
                    unpunish();
                    delobj(otmp);
                } else if (otmp == uchain) {
                    unpunish(); /* frees uchain */
                } else {
                    poly = polyfodder(otmp);
                    grow = mlevelgain(otmp);
                    heal = mhealup(otmp);
                    mstone = mstoning(otmp);
                    delobj(otmp);
                    ptr = mtmp->data;
                    if (poly) {
                        if (newcham(mtmp, NULL, FALSE, FALSE))
                            ptr = mtmp->data;
                    } else if (grow) {
                        ptr = grow_up(mtmp, NULL);
                    } else if (mstone) {
                        if (poly_when_stoned(ptr)) {
                            mon_to_stone(mtmp);
                            ptr = mtmp->data;
                        } else if (!resists_ston(mtmp)) {
                            if (canseemon(mtmp))
                                pline(mtmp->mtame ? msgc_petfatal :
                                      msgc_monneutral,
                                      "%s turns to stone!", Monnam(mtmp));
                            monstone(mtmp);
                            ptr = NULL;
                        }
                    } else if (heal) {
                        mtmp->mhp = mtmp->mhpmax;
                    }
                    if (!ptr)
                        return 2;       /* it died */
                }
                /* Left behind a pile? */
                if (rnd(25) < 3)
                    mksobj_at(ROCK, level, mtmp->mx, mtmp->my, TRUE, FALSE,
                              rng_main);
                if (mtmp->dlevel == level)
                    newsym(mtmp->mx, mtmp->my);
                return 1;
            }
        }
    }
    return 0;
}

/* for gelatinous cubes */
int
meatobj(struct monst *mtmp)
{
    struct obj *otmp, *otmp2;
    const struct permonst *ptr;
    int poly, grow, heal, count = 0, ecount = 0;
    const char *buf = "";

    /* If a pet, eating is handled separately, in dog.c */
    if (mtmp->mtame)
        return 0;

    /* Eats organic objects, including cloth and wood, if there */
    /* Engulfs others, except huge rocks and metal attached to player */
    for (otmp = level->objects[mtmp->mx][mtmp->my]; otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        if (is_organic(otmp) && !obj_resists(otmp, 5, 95) &&
            touch_artifact(otmp, mtmp)) {
            if (otmp->otyp == CORPSE && touch_petrifies(&mons[otmp->corpsenm])
                && !resists_ston(mtmp))
                continue;
            if (otmp->otyp == AMULET_OF_STRANGULATION ||
                otmp->otyp == RIN_SLOW_DIGESTION)
                continue;
            ++count;
            if (cansee(mtmp->mx, mtmp->my))
                pline(msgc_itemloss, "%s eats %s!", Monnam(mtmp),
                      distant_name(otmp, doname));
            else
                You_hear(msgc_itemloss, "a slurping sound.");
            /* Heal up to the object's weight in hp */
            if (mtmp->mhp < mtmp->mhpmax) {
                mtmp->mhp += objects[otmp->otyp].oc_weight;
                if (mtmp->mhp > mtmp->mhpmax)
                    mtmp->mhp = mtmp->mhpmax;
            }
            if (Has_contents(otmp)) {
                struct obj *otmp3;

                /* contents of eaten containers become engulfed; this is
                   arbitrary, but otherwise g.cubes are too powerful */
                while ((otmp3 = otmp->cobj) != 0) {
                    obj_extract_self(otmp3);
                    if (otmp->otyp == ICE_BOX && otmp3->otyp == CORPSE) {
                        otmp3->age = moves - otmp3->age;
                        start_corpse_timeout(otmp3);
                    }
                    mpickobj(mtmp, otmp3);
                }
            }
            poly = polyfodder(otmp);
            grow = mlevelgain(otmp);
            heal = mhealup(otmp);
            delobj(otmp);       /* munch */
            ptr = mtmp->data;
            if (poly) {
                if (newcham(mtmp, NULL, FALSE, FALSE))
                    ptr = mtmp->data;
            } else if (grow) {
                ptr = grow_up(mtmp, NULL);
            } else if (heal) {
                mtmp->mhp = mtmp->mhpmax;
            }
            /* in case it polymorphed or died */
            if (ptr != &mons[PM_GELATINOUS_CUBE])
                return !ptr ? 2 : 1;
        } else if (otmp->oclass != ROCK_CLASS && otmp != uball &&
                   otmp != uchain) {
            if ((otmp->otyp == CORPSE) && is_rider(&mons[otmp->corpsenm])) {
                buf = "";
                if (cansee(mtmp->mx, mtmp->my)) {
                    pline(mtmp->mtame ? msgc_petwarning : msgc_monneutral,
                          "%s attempts to engulf %s.", Monnam(mtmp),
                          distant_name(otmp, doname));
                    pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                          "%s dies!", Monnam(mtmp));
                } else {
                    You_hear(msgc_monneutral,
                             "a slurping sound abruptly stop.");
                    if (mtmp->mtame) {
                        pline(msgc_petfatal, "You have a queasy feeling for a "
                              "moment, then it passes.");
                    }
                }
                mondied(mtmp);
                (void)revive_corpse(otmp);
                return 2;
            }
            ++ecount;
            if (ecount == 1) {
                buf = msgprintf("%s engulfs %s.", Monnam(mtmp),
                                distant_name(otmp, doname));
            } else if (ecount == 2)
                buf = msgprintf("%s engulfs several objects.", Monnam(mtmp));
            obj_extract_self(otmp);
            mpickobj(mtmp, otmp);       /* slurp */
        }
        /* Engulf & devour is instant, so don't set meating */
        if (mtmp->minvis && mtmp->dlevel == level)
            newsym(mtmp->mx, mtmp->my);
    }
    if (ecount > 0) {
        if (cansee(mtmp->mx, mtmp->my) && buf[0])
            pline(msgc_itemloss, "%s", buf);
        else
            You_hear(msgc_itemloss, "%s slurping sound%s.",
                     ecount == 1 ? "a" : "several", ecount == 1 ? "" : "s");
    }
    return ((count > 0) || (ecount > 0)) ? 1 : 0;
}


void
mpickgold(struct monst *mtmp)
{
    struct obj *gold;
    int mat_idx;

    if ((gold = gold_at(level, mtmp->mx, mtmp->my)) != 0) {
        mat_idx = objects[gold->otyp].oc_material;
        obj_extract_self(gold);
        add_to_minv(mtmp, gold);
        if (cansee(mtmp->mx, mtmp->my)) {
            if (!mtmp->isgd)
                pline(mtmp->mtame ? msgc_petneutral : msgc_monneutral,
                      "%s picks up some %s.", Monnam(mtmp),
                      mat_idx == GOLD ? "gold" : "money");
            newsym(mtmp->mx, mtmp->my);
        }
    }
}


boolean
mpickstuff(struct monst *mtmp)
{
    struct obj *otmp, *otmp2;

    /* prevent shopkeepers from leaving the door of their shop */
    if (mtmp->isshk && inhishop(mtmp))
        return FALSE;

    /* non-tame monsters normally don't go shopping */
    if (*in_rooms(mtmp->dlevel, mtmp->mx, mtmp->my, SHOPBASE) && rn2(25))
        return FALSE;

    for (otmp = level->objects[mtmp->mx][mtmp->my]; otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        /* Nymphs take everything.  Most monsters don't pick up corpses. */
        if (monster_would_take_item(mtmp, otmp)) {
            if (otmp->otyp == CORPSE && mtmp->data->mlet != S_NYMPH &&
                /* let a handful of corpse types thru to can_carry() */
                !touch_petrifies(&mons[otmp->corpsenm]) &&
                otmp->corpsenm != PM_LIZARD && !acidic(&mons[otmp->corpsenm]))
                continue;
            if (!touch_artifact(otmp, mtmp))
                continue;
            if (!can_carry(mtmp, otmp))
                continue;
            if (is_pool(level, mtmp->mx, mtmp->my))
                continue;
#ifdef INVISIBLE_OBJECTS
            if (otmp->oinvis && !perceives(mtmp->data))
                continue;
#endif
            if (cansee(mtmp->mx, mtmp->my) && flags.verbose)
                pline(mtmp->mtame ? msgc_petneutral : msgc_monneutral,
                      "%s picks up %s.", Monnam(mtmp),
                      (distu(mtmp->mx, mtmp->my) <=
                       5) ? doname(otmp) : distant_name(otmp, doname));
            obj_extract_self(otmp);
            /* unblock point after extract, before pickup */
            if (otmp->otyp == BOULDER)
                unblock_point(otmp->ox, otmp->oy);      /* vision */
            mpickobj(mtmp, otmp);       /* may merge and free otmp */
            m_dowear(mtmp, FALSE);
            newsym(mtmp->mx, mtmp->my);
            return TRUE;        /* pick only one object */
        }
    }
    return FALSE;
}


int
curr_mon_load(struct monst *mtmp)
{
    int curload = 0;
    struct obj *obj;

    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (obj->otyp != BOULDER || !throws_rocks(mtmp->data))
            curload += obj->owt;
    }

    return curload;
}

int
max_mon_load(struct monst *mtmp)
{
    long maxload;

    /* Base monster carrying capacity is equal to human maximum carrying
       capacity, or half human maximum if not strong. (for a polymorphed
       player, the value used would be the non-polymorphed carrying capacity
       instead of max/half max). This is then modified by the ratio between the
       monster weights and human weights.  Corpseless monsters are given a
       capacity proportional to their size instead of weight. */
    if (!mtmp->data->cwt)
        maxload = (MAX_CARR_CAP * (long)mtmp->data->msize) / MZ_HUMAN;
    else if (!strongmonst(mtmp->data)
             || (strongmonst(mtmp->data) && (mtmp->data->cwt > WT_HUMAN)))
        maxload = (MAX_CARR_CAP * (long)mtmp->data->cwt) / WT_HUMAN;
    else
        maxload = MAX_CARR_CAP; /* strong monsters w/cwt <= WT_HUMAN */

    if (!strongmonst(mtmp->data))
        maxload /= 2;

    if (maxload < 1)
        maxload = 1;

    return (int)maxload;
}

/* for restricting monsters' object-pickup */
boolean
can_carry(struct monst *mtmp, struct obj *otmp)
{
    int otyp = otmp->otyp, newload = otmp->owt;
    const struct permonst *mdat = mtmp->data;

    if (notake(mdat))
        return FALSE;   /* can't carry anything */

    if (otyp == CORPSE && touch_petrifies(&mons[otmp->corpsenm]) &&
        !(mtmp->misc_worn_check & W_MASK(os_armg)) && !resists_ston(mtmp))
        return FALSE;
    if (otyp == CORPSE && is_rider(&mons[otmp->corpsenm]))
        return FALSE;
    if (objects[otyp].oc_material == SILVER && hates_silver(mdat) &&
        (otyp != BELL_OF_OPENING || !is_covetous(mdat)))
        return FALSE;

    /* Steeds don't pick up stuff (to avoid shop abuse) */
    if (mtmp == u.usteed)
        return FALSE;
    if (mtmp->isshk)
        return TRUE;    /* no limit */
    if (mtmp->mpeaceful && !mtmp->mtame)
        return FALSE;
    /* otherwise players might find themselves obligated to violate their
       alignment if the monster takes something they need */

    /* special--boulder throwers carry unlimited amounts of boulders */
    if (throws_rocks(mdat) && otyp == BOULDER)
        return TRUE;

    /* nymphs deal in stolen merchandise, but not boulders or statues */
    if (mdat->mlet == S_NYMPH)
        return (boolean) (otmp->oclass != ROCK_CLASS);

    if (curr_mon_load(mtmp) + newload > max_mon_load(mtmp))
        return FALSE;

    return TRUE;
}

/* return number of acceptable neighbour positions */
int
mfndpos(struct monst *mon, coord * poss,        /* coord poss[9] */
        long *info,     /* long info[9] */
        long flag)
{
    const struct permonst *mdat = mon->data;
    xchar x, y, nx, ny;
    int cnt = 0;
    uchar ntyp;
    uchar nowtyp;
    boolean wantpool, poolok, lavaok, nodiag;
    boolean rockok = FALSE, treeok = FALSE, thrudoor;
    int maxx, maxy;
    int swarmcount = 0;
    struct level *const mlevel = mon->dlevel;

    x = mon->mx;
    y = mon->my;
    nowtyp = mlevel->locations[x][y].typ;

    nodiag = (mdat == &mons[PM_GRID_BUG]);
    wantpool = mdat->mlet == S_EEL;
    poolok = is_flyer(mdat) || is_clinger(mdat) ||
        (is_swimmer(mdat) && !wantpool);
    lavaok = is_flyer(mdat) || is_clinger(mdat) || likes_lava(mdat);
    thrudoor = ((flag & (ALLOW_WALL | BUSTDOOR)) != 0L);
    if (flag & ALLOW_DIG) {
        struct obj *mw_tmp;

        /* need to be specific about what can currently be dug */
        if (!needspick(mdat)) {
            rockok = treeok = TRUE;
        } else if ((mw_tmp = MON_WEP(mon)) && mw_tmp->cursed &&
                   mon->weapon_check == NO_WEAPON_WANTED) {
            rockok = is_pick(mw_tmp);
            treeok = is_axe(mw_tmp);
        } else {
            rockok = (m_carrying(mon, PICK_AXE) ||
                      (m_carrying(mon, DWARVISH_MATTOCK) &&
                       !which_armor(mon, os_arms)));
            treeok = (m_carrying(mon, AXE) ||
                      (m_carrying(mon, BATTLE_AXE) &&
                       !which_armor(mon, os_arms)));
        }
        thrudoor |= rockok || treeok;
    }

nexttry:       /* eels prefer the water, but if there is no water nearby, they
                   will crawl over land */
    if (mon->mconf) {
        flag |= ALLOW_ALL;
        flag &= ~NOTONL;
    }
    if (!mon->mcansee)
        flag |= ALLOW_SSM;
    maxx = min(x + 1, COLNO - 1);
    maxy = min(y + 1, ROWNO - 1);
    for (nx = max(0, x - 1); nx <= maxx; nx++)
        for (ny = max(0, y - 1); ny <= maxy; ny++) {
            if (nx == x && ny == y)
                continue;
            if (IS_ROCK(ntyp = mlevel->locations[nx][ny].typ) &&
                !((flag & ALLOW_WALL) && may_passwall(mlevel, nx, ny)) &&
                !((IS_TREE(ntyp) ? treeok : rockok) && may_dig(mlevel, nx, ny)))
                continue;
            /* KMH -- Added iron bars */
            if (ntyp == IRONBARS && !(flag & ALLOW_BARS))
                continue;
            if (IS_DOOR(ntyp) && !amorphous(mdat) &&
                ((mlevel->locations[nx][ny].doormask & D_CLOSED &&
                  !(flag & OPENDOOR)) ||
                 (mlevel->locations[nx][ny].doormask & D_LOCKED &&
                  !(flag & UNLOCKDOOR))) && !thrudoor)
                continue;
            if (nx != x && ny != y &&
                (nodiag ||
                 ((IS_DOOR(nowtyp) &&
                   ((mlevel->locations[x][y].doormask & ~D_BROKEN) ||
                    Is_rogue_level(&u.uz))) ||
                  (IS_DOOR(ntyp) &&
                   ((mlevel->locations[nx][ny].doormask & ~D_BROKEN) ||
                    Is_rogue_level(&u.uz))))))
                continue;
            if ((is_pool(mlevel, nx, ny) == wantpool || poolok) &&
                (lavaok || !is_lava(mlevel, nx, ny))) {
                int dispx, dispy;
                boolean checkobj = OBJ_AT(nx, ny);
                boolean elbereth_activation = checkobj;

                /* Displacement also displaces the Elbereth/scare monster, as
                   long as you are visible. */
                if (awareness_reason(mon) == mar_guessing_displaced &&
                    mon->mux == nx && mon->muy == ny) {
                    dispx = u.ux;
                    dispy = u.uy;

                    /* The code previously checked for (checkobj || Displaced),
                       but that's wrong if the mu[xy] == n[xy] check fails;
                       it'll incorrectly treat you as activating an engraving
                       on n[xy]. Instead, we check for checkobj in the other
                       branch of the if statement, and unconditionally check
                       onscary in this branch. */
                    elbereth_activation = TRUE;
                } else {
                    dispx = nx;
                    dispy = ny;
                }

                info[cnt] = 0;
                if (elbereth_activation && onscary(dispx, dispy, mon)) {
                    if (!(flag & ALLOW_SSM))
                        continue;
                    info[cnt] |= ALLOW_SSM;
                }

                /* This codepath previously automatically set the monster to
                   detect you if it was actually adjacent to you, regardless of
                   anything else (unless you were on an Elbereth, because that
                   case has already been checked). This behaviour is debatable
                   from a balance point of view, but more importantly, this is
                   the wrong place in the AI to implement it. It's been moved to
                   set_apparxy so that it is at least in the right place; the
                   balance has been left unchanged pending more discussion.

                   We also need to check for being engulfed; monsters should be
                   willing to attack a monster that's engulfing the player. */
                if (nx == mon->mux && ny == mon->muy &&
                    (!Engulfed || mon == u.ustuck)) {
                    if (!(flag & ALLOW_MUXY))
                        continue;
                    info[cnt] |= ALLOW_MUXY;
                } else {
                    if (MON_AT(mlevel, nx, ny)) {
                        struct monst *mtmp2 = m_at(mlevel, nx, ny);
                        long mmflag = flag | mm_aggression(mon, mtmp2);

                        swarmcount++;

                        if (!(mmflag & ALLOW_M))
                            continue;
                        info[cnt] |= ALLOW_M;
                        if (mtmp2->mtame) {
                            if (!(mmflag & ALLOW_TM))
                                continue;
                            info[cnt] |= ALLOW_TM;
                        }
                    }
                    /* Note: ALLOW_SANCT only prevents movement, not attack,
                       into a temple. */
                    if (*in_rooms(mlevel, nx, ny, TEMPLE) &&
                        !*in_rooms(mlevel, x, y, TEMPLE) &&
                        in_your_sanctuary(NULL, nx, ny)) {
                        if (!(flag & ALLOW_SANCT))
                            continue;
                        info[cnt] |= ALLOW_SANCT;
                    }
                }
                if (checkobj && sobj_at(CLOVE_OF_GARLIC, mlevel, nx, ny)) {
                    if (flag & NOGARLIC)
                        continue;
                    info[cnt] |= NOGARLIC;
                }
                if (checkobj && sobj_at(BOULDER, mlevel, nx, ny)) {
                    if (!(flag & ALLOW_ROCK))
                        continue;
                    info[cnt] |= ALLOW_ROCK;
                }
                if (mon->mcansee && (!Invis || perceives(mdat)) &&
                    onlineu(nx, ny)) {
                    if (flag & NOTONL)
                        continue;
                    info[cnt] |= NOTONL;
                }
                if (nx != x && ny != y && bad_rock(mdat, x, ny)
                    && bad_rock(mdat, nx, y)
                    && ((bigmonst(mdat) && !can_ooze(mon)) ||
                        (curr_mon_load(mon) > 600)))
                    continue;
                /* The monster avoids a particular type of trap if it's familiar
                   with the trap type. Pets get ALLOW_TRAPS and checking is done
                   in dogmove.c. In either case, "harmless" traps are neither
                   avoided nor marked in info[]. Quest leaders avoid traps even
                   if they aren't familiar with them, because they're being
                   careful or something. */
                {
                    struct trap *ttmp = t_at(mlevel, nx, ny);

                    if (ttmp) {
                        if (ttmp->ttyp >= TRAPNUM || ttmp->ttyp == 0) {
                            impossible("A monster looked at a very strange "
                                       "trap of type %d.", ttmp->ttyp);
                            continue;
                        }
                        if ((ttmp->ttyp != RUST_TRAP ||
                             mdat == &mons[PM_IRON_GOLEM])
                            && ttmp->ttyp != STATUE_TRAP &&
                            ((ttmp->ttyp != PIT && ttmp->ttyp != SPIKED_PIT &&
                              ttmp->ttyp != TRAPDOOR && ttmp->ttyp != HOLE)
                             || (!is_flyer(mdat)
                                 && !is_floater(mdat)
                                 && !is_clinger(mdat))
                             || In_sokoban(&u.uz))
                            && (ttmp->ttyp != SLP_GAS_TRAP ||
                                !resists_sleep(mon))
                            && (ttmp->ttyp != BEAR_TRAP ||
                                (mdat->msize > MZ_SMALL && !amorphous(mdat) &&
                                 !is_flyer(mdat)))
                            && (ttmp->ttyp != FIRE_TRAP || !resists_fire(mon))
                            && (ttmp->ttyp != SQKY_BOARD || !is_flyer(mdat))
                            && (ttmp->ttyp != WEB ||
                                (!amorphous(mdat) && !webmaker(mdat)))
                            ) {
                            if (!(flag & ALLOW_TRAPS)) {
                                if ((mon->mtrapseen & (1L << (ttmp->ttyp - 1))) ||
                                    mon->data == &pm_leader)
                                    continue;
                            }
                            info[cnt] |= ALLOW_TRAPS;
                        }
                    }
                }
                poss[cnt].x = nx;
                poss[cnt].y = ny;
                cnt++;
            }
        }
    if (!cnt && wantpool && !is_pool(mlevel, x, y)) {
        wantpool = FALSE;
        goto nexttry;
    }

    /* Special case for optimizing the behaviour of large groups of monsters:
       those on the inside (6 or more neighbours) do not move horizontally
       (although they can still attack). This behaviour mostly looks the same as
       if all the monsters could move, but makes the save file much smaller, and
       also makes pudding farming a little more difficult because you can't rely
       on a single square to drop the items (the puddings will tend to spread
       back a little from the square you're farming from; although there will
       always or nearly always be a few adjacent, they won't be in the same
       places). This also means that in large mobs, enemies with ranged weapons
       will get more of a chance to use them. */
    if (swarmcount >= 6) {
        long infocopy[9];
        coord posscopy[9];
        int oldcnt;

        memcpy(infocopy, info, sizeof infocopy);
        memcpy(posscopy, poss, sizeof posscopy);
        oldcnt = cnt;
        cnt = 0;

        int i;
        for (i = 0; i < oldcnt; i++) {
            if ((infocopy[i] & (ALLOW_MUXY | ALLOW_M))) {
                info[cnt] = infocopy[i];
                poss[cnt] = posscopy[i];
                cnt++;
            }
        }
    }

    return cnt;
}

/* Pets do not take on difficult opponents. Especially not when wounded. This is
   also symmetrised, for balance reasons; difficult monsters do not take on pets
   that wouldn't attack them. (This is mostly unrealistic, but necessary to
   prevent the pet being torn apart by a more powerful opponent.)

   This function returns the highest difficulty the pet is willing to attack.

   See also find_mac in worn.c, which gives pets a large AC boost in situations
   where they shouldn't be attacked, just in case there are codepaths that
   don't go through mm_aggression. */
static int
pet_attacks_up_to_difficulty(const struct monst *mtmp)
{
    if (mtmp->mhp * 3 <= mtmp->mhpmax)
        return 0; /* pets below 1/3 health do not attack */
    /* Fully healthy pets will attack up to their own level + 3; this goes
       down linearly as they get wounded. */
    return ((mtmp->m_lev + 3) * mtmp->mhp) / mtmp->mhpmax;
}


/* Monster against monster special attacks; for the specified monster
   combinations, this allows one monster to attack another adjacent one in the
   absence of Conflict. There is no provision for targeting other monsters;
   just hand to hand fighting when they happen to be next to each other. The
   purpose of the function is to check whether magr would want to attack mdef.

   This also handles monster vs. player and player vs. monster for the purposes
   of Warning (which doesn't show monsters that are peaceful towards you), so
   that Warning works correctly on monsters. The latter is naturally has to be a
   guess, because we have no way to predict the player's actions in advance; we
   assume that for the purposes of Warning, the player would attack any monster
   that would attack them, unless pacifist. */
long
mm_aggression(const struct monst *magr, /* monster that might attack */
              const struct monst *mdef) /* the monster it might attack  */
{
    const struct permonst *ma = magr->data;
    const struct permonst *md = mdef->data;

    /* magr or mdef as the player is a special case; not checking Conflict is
       correct, because it shouldn't suddenly warn you of peacefuls */
    if (magr == &youmonst)
        return (mdef->mpeaceful || !u.uconduct[conduct_killer])
            ? 0 : (ALLOW_M | ALLOW_TM);
    if (mdef == &youmonst)
        return magr->mpeaceful ? 0 : (ALLOW_M | ALLOW_TM);

    /* anti-stupidity checks moved here from dog_move, so that hostile monsters
       benefit from the improved AI when attacking pets too: */

    if (!Conflict) {
        /* monsters have a 9/10 chance of rejecting an attack on a monster that
           would paralyze them; in a change from 3.4.3, they don't check whether
           a floating eye they're attacking is blind (because it's not obvious
           that they know whether the floating eye is blind), nor whether it can
           see invisible (can /you/ determine whether a floating eye can see
           invisible by looking at it?) */
        if (md == &mons[PM_FLOATING_EYE] && rn2(10) &&
            magr->mcansee && haseyes(ma))
            return 0;
        if (md == &mons[PM_GELATINOUS_CUBE] && rn2(10))
            return 0;

        /* monsters won't make an attack that would kill them with the passive
           damage they'd take in response */
        if (max_passive_dmg(mdef, magr) >= magr->mhp)
            return 0;

        /* monsters won't make an attack that would petrify them */
        if (touch_petrifies(md) && !resists_ston(magr))
            return 0;
        /* and for balance, the reverse */
        if (touch_petrifies(ma) && !resists_ston(mdef))
            return 0;

        /* tame monsters won't attack peaceful guardians or leaders, unless
           conflicted */
        if (magr->mtame && mdef->mpeaceful && !Conflict &&
            (md->msound == MS_GUARDIAN || md->msound == MS_LEADER))
            return 0;

        /* monsters won't attack enemies that are out of their league */
        if (magr->mtame && mdef->m_lev > pet_attacks_up_to_difficulty(magr))
            return 0;
        /* and for balance, hostiles won't attack pets that wouldn't attack
           back */
        if (mdef->mtame && magr->m_lev > pet_attacks_up_to_difficulty(mdef))
            return 0;
    }
    /* end anti-stupidity checks */

    /* supposedly purple worms are attracted to shrieking because they like to
       eat shriekers, so attack the latter when feasible */
    if (ma == &mons[PM_PURPLE_WORM] && md == &mons[PM_SHRIEKER])
        return ALLOW_M | ALLOW_TM;

    /* pets attack hostile monsters */
    if (magr->mtame && !mdef->mpeaceful)
        return ALLOW_M | ALLOW_TM;

    /* and vice versa */
    if (mdef->mtame && !magr->mpeaceful)
        return ALLOW_M | ALLOW_TM;

    /* Since the quest guardians are under siege, it makes sense to have them
       fight hostiles.  (But don't put the quest leader in danger.) */
    if (ma->msound == MS_GUARDIAN && mdef->mpeaceful == FALSE)
        return ALLOW_M | ALLOW_TM;
    /* ... and vice versa */
    if (md->msound == MS_GUARDIAN && magr->mpeaceful == FALSE)
        return ALLOW_M | ALLOW_TM;

    /* elves vs. orcs */
    if (is_elf(ma) && is_orc(md))
        return ALLOW_M | ALLOW_TM;
    /* ... and vice versa */
    if (is_elf(md) && is_orc(ma))
        return ALLOW_M | ALLOW_TM;

    /* angels vs. demons */
    if (ma->mlet == S_ANGEL && is_demon(md))
        return ALLOW_M | ALLOW_TM;
    /* ... and vice versa */
    if (md->mlet == S_ANGEL && is_demon(ma))
        return ALLOW_M | ALLOW_TM;

    /* domestic dogs vs. domestic cats, unless both are tame */
    if (!(magr->mtame && mdef->mtame) && is_domestic(ma) && is_domestic(md)) {
        if (ma->mlet == S_DOG && md->mlet == S_FELINE)
            return ALLOW_M | ALLOW_TM;
        /* ... and vice versa, for dog/cat symmetry reasons */
        if (md->mlet == S_DOG && ma->mlet == S_FELINE)
            return ALLOW_M | ALLOW_TM;
    }

    /* woodchucks vs. The Oracle */
    if (ma == &mons[PM_WOODCHUCK] && md == &mons[PM_ORACLE])
        return ALLOW_M | ALLOW_TM;

    /* ravens like eyes */
    if (ma == &mons[PM_RAVEN] && md == &mons[PM_FLOATING_EYE])
        return ALLOW_M | ALLOW_TM;

    return 0L;
}

boolean
monnear(struct monst * mon, int x, int y)
/* Is the square close enough for the monster to move or attack into? */
{
    int distance = dist2(mon->mx, mon->my, x, y);

    if (distance == 2 && mon->data == &mons[PM_GRID_BUG])
        return 0;
    return (boolean) (distance < 3);
}


/* really free dead monsters */
void
dmonsfree(struct level *lev)
{
    struct monst **mtmp;
    int count = 0;

    for (mtmp = &lev->monlist; *mtmp;) {
        if (DEADMONSTER(*mtmp)) {
            struct monst *freetmp = *mtmp;

            *mtmp = (*mtmp)->nmon;
            dealloc_monst(freetmp);
            count++;
        } else
            mtmp = &(*mtmp)->nmon;
    }

    if (count != lev->flags.purge_monsters)
        impossible("dmonsfree: %d removed doesn't match %d pending", count,
                   lev->flags.purge_monsters);
    lev->flags.purge_monsters = 0;
}


/* called when monster is moved to larger structure */
void
replmon(struct monst *mtmp, struct monst *mtmp2)
{
    struct obj *otmp;

    /* transfer the monster's inventory */
    for (otmp = mtmp2->minvent; otmp; otmp = otmp->nobj)
        otmp->ocarry = mtmp2;

    mtmp->minvent = 0;

    /* remove the old monster from the map and from `level->monlist' list */
    relmon(mtmp);

    /* finish adding its replacement */
    if (mtmp != u.usteed)       /* don't place steed onto the map */
        place_monster(mtmp2, mtmp2->mx, mtmp2->my);
    if (mtmp2->wormno)  /* update level->monsters[wseg->wx][wseg->wy] */
        place_wsegs(mtmp2);     /* locations to mtmp2 not mtmp. */
    if (emits_light(mtmp2->data)) {
        /* since this is so rare, we don't have any `mon_move_light_source' */
        new_light_source(mtmp2->dlevel, mtmp2->mx, mtmp2->my,
                         emits_light(mtmp2->data), LS_MONSTER, mtmp2);
        /* here we rely on the fact that `mtmp' hasn't actually been deleted */
        del_light_source(mtmp->dlevel, LS_MONSTER, mtmp);
    }
    mtmp2->nmon = mtmp2->dlevel->monlist;
    mtmp2->dlevel->monlist = mtmp2;
    if (u.ustuck == mtmp)
        u.ustuck = mtmp2;
    if (u.usteed == mtmp)
        u.usteed = mtmp2;
    if (mtmp2->isshk)
        replshk(mtmp, mtmp2);

    if (nmtmp == mtmp)
        nmtmp = mtmp2;

    /* discard the old monster */
    dealloc_monst(mtmp);
}

/* release mon from display and monster list */
void
relmon(struct monst *mon)
{
    struct monst *mtmp;

    if (mon->dlevel->monlist == NULL)
        panic("relmon: no level->monlist available.");

    mon->dlevel->monsters[mon->mx][mon->my] = NULL;

    if (mon == mon->dlevel->monlist)
        mon->dlevel->monlist = mon->dlevel->monlist->nmon;
    else {
        for (mtmp = mon->dlevel->monlist; mtmp && mtmp->nmon != mon;
             mtmp = mtmp->nmon)
            ;
        if (mtmp)
            mtmp->nmon = mon->nmon;
        else
            panic("relmon: mon not in list.");
    }
}

/* remove effects of mtmp from other data structures */
static void
m_detach(struct monst *mtmp, const struct permonst *mptr)
{       /* reflects mtmp->data _prior_ to mtmp's death */
    if (mtmp->mleashed)
        m_unleash(mtmp, FALSE);
    /* to prevent an infinite relobj-flooreffects-hmon-killed loop */
    mtmp->mtrapped = 0;
    mtmp->mhp = 0;      /* simplify some tests: force mhp to 0 */
    relobj(mtmp, 0, FALSE);
    if (isok(mtmp->mx, mtmp->my))
        mtmp->dlevel->monsters[mtmp->mx][mtmp->my] = NULL;
    if (emits_light(mptr))
        del_light_source(mtmp->dlevel, LS_MONSTER, mtmp);
    if (mtmp->dlevel == level && isok(mtmp->mx, mtmp->my))
        newsym(mtmp->mx, mtmp->my);
    unstuck(mtmp);
    if (isok(mtmp->mx, mtmp->my))
        fill_pit(mtmp->dlevel, mtmp->mx, mtmp->my);

    if (mtmp->isshk)
        shkgone(mtmp);
    if (mtmp->wormno)
        wormgone(mtmp);

    if (!DEADMONSTER(mtmp)) {
        impossible("Monster detached without dying?");
        mtmp->deadmonster = 1;
    }
    mtmp->dlevel->flags.purge_monsters++;
}

/* find the worn amulet of life saving which will save a monster */
struct obj *
mlifesaver(struct monst *mon)
{
    if (!nonliving(mon->data)) {
        struct obj *otmp = which_armor(mon, os_amul);

        if (otmp && otmp->otyp == AMULET_OF_LIFE_SAVING)
            return otmp;
    }
    return NULL;
}

static void
lifesaved_monster(struct monst *mtmp)
{
    struct obj *lifesave = mlifesaver(mtmp);

    if (lifesave) {
        /* Not canseemon; amulets are on the head, so you don't want to show
           this for a long worm with only a tail visible.  Not mon_visible
           (which only checks the head), because that checks invisibility;
           glowing and disintegrating amulets are always visible.

           TODO: Maybe this should be couldsee; it makes sense that the player
           could see a glowing amulet on an unlit square. If that change is
           made, it'll also be important to check player blindness. */
        if (cansee(mtmp->mx, mtmp->my)) {
            /* the lifesave itself is treated as a self-buff for channelization
               purposes; all the messages about the consequences are neutral */
            enum msg_channel msgc =
                mtmp->mtame ? msgc_petneutral : msgc_monneutral;
            pline(combat_msgc(mtmp, NULL, cr_hit),
                  "But wait...  %s medallion begins to glow!",
                  s_suffix(Monnam(mtmp)));
            makeknown(AMULET_OF_LIFE_SAVING);
            if (attacktype(mtmp->data, AT_EXPL)
                || attacktype(mtmp->data, AT_BOOM))
                pline_implied(msgc, "%s reconstitutes!", Monnam(mtmp));
            else if (canseemon(mtmp))
                pline_implied(msgc, "%s looks much better!", Monnam(mtmp));
            else
                pline_implied(msgc, "%s seems much better!", Monnam(mtmp));
            pline_implied(msgc, "The medallion crumbles to dust!");
        }
        m_useup(mtmp, lifesave);
        mtmp->mcanmove = 1;
        mtmp->mfrozen = 0;
        if (mtmp->mtame && !mtmp->isminion) {
            wary_dog(mtmp, FALSE);
        }
        if (mtmp->mhpmax <= 0)
            mtmp->mhpmax = 10;
        mtmp->mhp = mtmp->mhpmax;
        if (mvitals[monsndx(mtmp->data)].mvflags & G_GENOD) {
            if (cansee(mtmp->mx, mtmp->my))
                pline(mtmp->mtame ? msgc_petfatal : msgc_monneutral,
                      "Unfortunately %s is still genocided...", mon_nam(mtmp));
        } else {
            mtmp->deadmonster = 0; /* paranoia */
            return;
        }
    }
    mtmp->mhp = 0;
    mtmp->deadmonster = 1; /* flag as dead in the monster list */
}

void
mondead(struct monst *mtmp)
{
    const struct permonst *mptr;
    int tmp;
    boolean player_aware = cansuspectmon(mtmp);

    if (mtmp->isgd) {
        /* if we're going to abort the death, it *must* be before the m_detach
           or there will be relmon problems later */
        if (!grddead(mtmp))
            return;
    }
    lifesaved_monster(mtmp);
    if (!DEADMONSTER(mtmp))
        return;

    /* Player is thrown from his steed when it dies */
    if (mtmp == u.usteed)
        dismount_steed(DISMOUNT_GENERIC);

    /* monster should no longer block vision */
    if ((!mtmp->minvis || See_invisible) &&
        ((mtmp->m_ap_type == M_AP_FURNITURE &&
          (mtmp->mappearance == S_vcdoor || mtmp->mappearance == S_hcdoor)) ||
         (mtmp->m_ap_type == M_AP_OBJECT && mtmp->mappearance == BOULDER)))
        unblock_point(mtmp->mx, mtmp->my);

    mptr = mtmp->data;  /* save this for m_detach() */
    /* restore chameleon, lycanthropes to true form at death */
    if (mtmp->cham)
        set_mon_data(mtmp, &mons[cham_to_pm[mtmp->cham]], -1);
    else if (mtmp->data == &mons[PM_WEREJACKAL])
        set_mon_data(mtmp, &mons[PM_HUMAN_WEREJACKAL], -1);
    else if (mtmp->data == &mons[PM_WEREWOLF])
        set_mon_data(mtmp, &mons[PM_HUMAN_WEREWOLF], -1);
    else if (mtmp->data == &mons[PM_WERERAT])
        set_mon_data(mtmp, &mons[PM_HUMAN_WERERAT], -1);

    /* if MAXMONNO monsters of a given type have died, and it can be done,
       extinguish that monster. mvitals[].died does double duty as total number
       of dead monsters and as experience factor for the player killing more
       monsters. this means that a dragon dying by other means reduces the
       experience the player gets for killing a dragon directly; this is
       probably not too bad, since the player likely finagled the first dead
       dragon via ring of conflict or pets, and extinguishing based on only
       player kills probably opens more avenues of abuse for rings of conflict
       and such. */
    tmp = monsndx(mtmp->data);
    if (mvitals[tmp].died < 255)
        mvitals[tmp].died++;

    /* if it's a (possibly polymorphed) quest leader, mark him as dead */
    if (mtmp->m_id == u.quest_status.leader_m_id)
        u.quest_status.leader_is_dead = TRUE;

    if (mtmp->data->mlet == S_KOP) {
        /* Dead Kops may come back. */
        switch (rnd(5)) {
        case 1:        /* returns near the stairs */
            makemon(mtmp->data, level, level->dnstair.sx, level->dnstair.sy,
                    NO_MM_FLAGS);
            break;
        case 2:        /* randomly */
            makemon(mtmp->data, level, COLNO, ROWNO, NO_MM_FLAGS);
            break;
        default:
            break;
        }
    }

    /* TODO: this is probably dead code */
    if (player_aware)
        level->locations[mtmp->mx][mtmp->my].mem_invis = FALSE;

    if (mtmp->iswiz)
        wizdead();
    if (mtmp->data->msound == MS_NEMESIS)
        nemdead();
    m_detach(mtmp, mptr);
}

/* TRUE if corpse might be dropped, magr may die if mon was swallowed */
boolean
corpse_chance(struct monst *mon,
              struct monst *magr,    /* killer, if swallowed */
              boolean was_swallowed) /* digestion */
{
    const struct permonst *mdat = mon->data;
    int i, tmp;

    if (mdat == &mons[PM_VLAD_THE_IMPALER] || mdat->mlet == S_LICH) {
        if (cansee(mon->mx, mon->my) && !was_swallowed)
            pline_implied(msgc_monneutral, "%s body crumbles into dust.",
                          s_suffix(Monnam(mon)));
        return FALSE;
    }

    /* Gas spores always explode upon death */
    for (i = 0; i < NATTK; i++) {
        if (mdat->mattk[i].aatyp == AT_BOOM) {
            if (mdat->mattk[i].damn)
                tmp = dice((int)mdat->mattk[i].damn, (int)mdat->mattk[i].damd);
            else if (mdat->mattk[i].damd)
                tmp = dice((int)mdat->mlevel + 1, (int)mdat->mattk[i].damd);
            else
                tmp = 0;
            if (was_swallowed && magr) {
                if (magr == &youmonst) {
                    pline(combat_msgc(mon, magr, cr_hit),
                          "There is an explosion in your %s!",
                          body_part(STOMACH));
                    if (Half_physical_damage)
                        tmp = (tmp + 1) / 2;
                    losehp(tmp, msgprintf("%s explosion", s_suffix(mdat->mname)));
                } else {
                    You_hear(msgc_levelsound, "an explosion.");
                    magr->mhp -= tmp;
                    if (magr->mhp < 1)
                        mondied(magr);
                    if (DEADMONSTER(magr)) {        /* i.e. not lifesaved */
                        if (canseemon(magr))
                            pline(combat_msgc(mon, magr, cr_kill),
                                  "%s rips open!", Monnam(magr));
                    } else if (canseemon(magr))
                        pline(combat_msgc(mon, magr, cr_hit),
                              "%s seems to have indigestion.", Monnam(magr));
                }

                return FALSE;
            }

            explode(mon->mx, mon->my, -1, tmp, MON_EXPLODE, EXPL_NOXIOUS,
                    msgcat(s_suffix(mdat->mname), " explosion"));
            return FALSE;
        }
    }

    /* must duplicate this below check in xkilled() since it results in
       creating no objects as well as no corpse */
    if (LEVEL_SPECIFIC_NOCORPSE(mdat))
        return FALSE;

    if (bigmonst(mdat) || mdat == &mons[PM_LIZARD]
        || is_golem(mdat)
        || is_mplayer(mdat)
        || is_rider(mdat))
        return TRUE;
    return (boolean) (!rn2((int) (2 + ((int)(mdat->geno & G_FREQ) < 2) +
                                  verysmall(mdat))));
}

/* drop (perhaps) a cadaver and remove monster */
void
mondied(struct monst *mdef)
{
    mondead(mdef);
    if (!DEADMONSTER(mdef))
        return; /* lifesaved */

    if (corpse_chance(mdef, NULL, FALSE) &&
        (accessible(mdef->mx, mdef->my) || is_pool(level, mdef->mx, mdef->my)))
        make_corpse(mdef);
}

/* monster disappears, not dies */
void
mongone(struct monst *mdef)
{
    /* Player is thrown from his steed when it disappears */
    if (mdef == u.usteed)
        dismount_steed(DISMOUNT_GENERIC);

    mdef->mhp = 0;         /* can skip some inventory bookkeeping */

    /* The monster death code is somewhat spaghetti and could do with being
       merged into fewer functions. For now, it's worth noting that m_detach
       must be called if and only if deadmonster is set to 1.

       mdrop_special_objs also looks at this flag to know where it's being
       called from, which is potentially quite dubious; that's something to
       look at in the future. */
    mdef->deadmonster = 1;

    /* monster should no longer block vision */
    if ((!mdef->minvis || See_invisible) &&
        ((mdef->m_ap_type == M_AP_FURNITURE &&
          (mdef->mappearance == S_vcdoor || mdef->mappearance == S_hcdoor)) ||
         (mdef->m_ap_type == M_AP_OBJECT && mdef->mappearance == BOULDER)))
        unblock_point(mdef->mx, mdef->my);

    /* drop special items like the Amulet so that a dismissed Kop or nurse
       can't remove them from the game */
    mdrop_special_objs(mdef);
    /* release rest of monster's inventory--it is removed from game */
    discard_minvent(mdef);
    m_detach(mdef, mdef->data);
}

/* drop a statue or rock and remove monster */
void
monstone(struct monst *mdef)
{
    struct obj *otmp, *obj, *oldminvent;
    xchar x = mdef->mx, y = mdef->my;
    boolean wasinside = FALSE;

    /* we have to make the statue before calling mondead, to be able to put
       inventory in it, and we have to check for lifesaving before making the
       statue.... */
    lifesaved_monster(mdef);
    if (!DEADMONSTER(mdef))
        return;

    mdef->mtrapped = 0; /* (see m_detach) */

    if ((int)mdef->data->msize > MZ_TINY ||
        !rn2(2 + ((int)(mdef->data->geno & G_FREQ) > 2))) {
        oldminvent = 0;
        /* some objects may end up outside the statue */
        while ((obj = mdef->minvent) != 0) {
            obj_extract_self(obj);
            if (obj->owornmask)
                update_mon_intrinsics(mdef, obj, FALSE, TRUE);
            obj_no_longer_held(obj);
            if (obj->owornmask & W_MASK(os_wep))
                setmnotwielded(mdef, obj);
            obj->owornmask = 0L;
            if (obj->otyp == BOULDER || obj_resists(obj, 0, 0)) {
                if (flooreffects(obj, x, y, "fall"))
                    continue;
                place_object(obj, level, x, y);
            } else {
                if (obj->lamplit)
                    end_burn(obj, TRUE);
                obj->nobj = oldminvent;
                oldminvent = obj;
            }
        }

        /* defer statue creation until after inventory removal so that saved
           monster traits won't retain any stale item-conferred attributes */
        otmp = mkcorpstat(STATUE, KEEPTRAITS(mdef) ? mdef : 0, mdef->data,
                          level, x, y, FALSE, rng_main);
        if (mdef->mnamelth)
            otmp = oname(otmp, NAME(mdef));
        while ((obj = oldminvent) != 0) {
            oldminvent = obj->nobj;
            add_to_container(otmp, obj);
        }
        /* Archeologists should not break unique statues */
        if (mdef->data->geno & G_UNIQ)
            otmp->spe = 1;
        otmp->owt = weight(otmp);
    } else
        otmp = mksobj_at(ROCK, level, x, y, TRUE, FALSE, rng_main);

    stackobj(otmp);
    /* assume that a statue appearing within vision range lets the player know
       that there isn't an invisible monster there any more, and overwrites
       detected items; either seeing the monster disappear or the statue appear
       is enough information */
    if (cansee(x, y) || cansuspectmon(mdef)) {
        unmap_object(x, y);
        newsym(x, y);
    }
    /* We don't currently trap the hero in the statue in this case, but we
       could */
    if (Engulfed && u.ustuck == mdef)
        wasinside = TRUE;
    mondead(mdef);
    if (wasinside) {
        if (is_animal(mdef->data))
            pline(msgc_nonmongood, "You %s through an opening in the new %s.",
                  locomotion(youmonst.data, "jump"), xname(otmp));
    }
}

/* The monster magr has killed the monster mdef (with a fltxt, if that's set;
   it can be "" for no implement, or NULL to hide the message altogether).

   Occasionally (for backwards compatibility) magr might be unknown, in which
   case you can pass NULL. (Please try to avoid this case if you can; we hope
   to get rid of it eventually.) In some cases, magr might not exist (e.g.
   because the monster is killed by a trap that generated along with the level),
   or might have died since starting a delayed-action attack that kills another
   monster; NULL is fine in those cases too, and will continue to be so.

   Handles: messages; corpses; dropping inventory; flagging the monster as dead
   Does not (yet) handle: experience; magr or mdef as the player
   Does not happen: deathdrops of new items (those are for player kills only) */
void
monkilled(struct monst *magr, struct monst *mdef, const char *fltxt, int how)
{
    boolean be_sad = FALSE;     /* true if unseen pet is killed */

    if (fltxt && canseemon(mdef))
        pline(magr ? combat_msgc(magr, mdef, cr_kill) : msgc_monneutral,
              "%s is %s%s%s!", Monnam(mdef),
              nonliving(mdef->data) ? "destroyed" : "killed",
              *fltxt ? " by the " : "", fltxt);
    else
        be_sad = !!mdef->mtame;

    /* no corpses if digested or disintegrated */
    if (how == AD_DGST || how == -AD_RBRE)
        mondead(mdef);
    else
        mondied(mdef);

    if (be_sad && DEADMONSTER(mdef))
        pline(msgc_petfatal,
              "You have a sad feeling for a moment, then it passes.");
}

void
unstuck(struct monst *mtmp)
{
    if (u.ustuck == mtmp) {
        if (Engulfed) {
            u.ux = mtmp->mx;
            u.uy = mtmp->my;
            if (Punished) {
                unplacebc();
                placebc();
            }
            Engulfed = 0;
            u.uswldtim = 0;
            turnstate.vision_full_recalc = TRUE;
            doredraw();
        }
        u.ustuck = 0;
    }
}

void
killed(struct monst *mtmp)
{
    xkilled(mtmp, 1);
}

/* the player has killed the monster mtmp */
void
xkilled(struct monst *mtmp, int dest)
/*
 * Dest=1, normal; dest=0, don't print message; dest=2, don't drop corpse
 * either; dest=3, message but no corpse
 */
{
    int tmp, x = mtmp->mx, y = mtmp->my;
    const struct permonst *mdat;
    int mndx;
    struct obj *otmp;
    struct trap *t;
    boolean redisp = FALSE;
    boolean wasinside = Engulfed && (u.ustuck == mtmp);

    mtmp->mhp = -1; /* assumed by this code in 3.4.3; paranoia that some
                       similar assumptions still exist in the code */

    /* KMH, conduct */
    break_conduct(conduct_killer);

    if (dest & 1) {
        const char *verb = nonliving(mtmp->data) ? "destroy" : "kill";

        if (!wasinside && mtmp != u.usteed && !canclassifymon(mtmp))
            pline(msgc_kill, "You %s it!", verb);
        else if (mtmp->mtame)
            /* TODO: If the character can't hear for some reason, and the player
               is turning off spammy messages, they'll get neither this
               petwarning nor the subsequent petfatal. It's unclear how to fix
               this without adding a special case (for canclassifymon but unable
               to hear). */
            pline_implied(msgc_petwarning, "You %s %s!", verb,
                          x_monnam(mtmp, mtmp->mnamelth ? ARTICLE_NONE :
                                   ARTICLE_THE, "poor", mtmp->mnamelth ?
                                   SUPPRESS_SADDLE : 0, FALSE));
        else
            pline(msgc_kill, "You %s %s!", verb, mon_nam(mtmp));
    }

    if (mtmp->mtrapped && (t = t_at(level, x, y)) != 0 &&
        (t->ttyp == PIT || t->ttyp == SPIKED_PIT) &&
        sobj_at(BOULDER, level, x, y))
        dest |= 2;      /*
                         * Prevent corpses/treasure being created "on top"
                         * of the boulder that is about to fall in. This is
                         * out of order, but cannot be helped unless this
                         * whole routine is rearranged.
                         */

    /* your pet knows who just killed it...watch out */
    if (mtmp->mtame && !mtmp->isminion)
        EDOG(mtmp)->killed_by_u = 1;

    /* dispose of monster and make cadaver */
    if (stoned)
        monstone(mtmp);
    else
        mondead(mtmp);

    if (!DEADMONSTER(mtmp)) {        /* monster lifesaved */
        /* Cannot put the non-visible lifesaving message in lifesaved_monster()
           since the message appears only when you kill it (as opposed to
           visible lifesaving which always appears). */
        stoned = FALSE;
        if (!cansee(x, y))
            pline(msgc_substitute, "Maybe not...");
        return;
    }

    /* with lifesaving taken care of, history can record the heroic deed */
    if ((mtmp->data->geno & G_UNIQ)) {
        historic_event(FALSE, "killed %s %s.",
                       x_monnam(mtmp, ARTICLE_NONE, NULL, EXACT_NAME, TRUE),
                       hist_lev_name(&u.uz, TRUE));
    }

    mdat = mtmp->data;  /* note: mondead can change mtmp->data */
    mndx = monsndx(mdat);

    if (stoned) {
        stoned = FALSE;
        goto cleanup;
    }

    if ((dest & 2) || LEVEL_SPECIFIC_NOCORPSE(mdat))
        goto cleanup;

    /* might be here after swallowed */
    if (((x != u.ux) || (y != u.uy)) && !rn2(6) &&
        !(mvitals[mndx].mvflags & G_NOCORPSE) && mdat->mlet != S_KOP) {
        int typ;

        otmp = mkobj_at(RANDOM_CLASS, level, x, y, TRUE, mdat->msize < MZ_HUMAN
                        ? rng_death_drop_s : rng_death_drop_l);
        /* Don't create large objects from small monsters */
        typ = otmp->otyp;
        if (mdat->msize < MZ_HUMAN && typ != FOOD_RATION &&
            typ != LEASH && typ != FIGURINE &&
            (otmp->owt > 3 || objects[typ].oc_big || /* oc_bimanual/oc_bulky */
             is_spear (otmp) || is_pole (otmp) || typ == MORNING_STAR)) {
            delobj(otmp);
        } else
            redisp = TRUE;
    }
    /* Whether or not it always makes a corpse is, in theory, different from
       whether or not the corpse is "special"; if we want both, we have to
       specify it explicitly. */
    if (corpse_chance(mtmp, NULL, FALSE))
        make_corpse(mtmp);

    if (!accessible(x, y) && !is_pool(level, x, y)) {
        /* might be mimic in wall or corpse in lava */
        redisp = TRUE;
        if (wasinside)
            spoteffects(TRUE);
    }
    if (redisp)
        newsym(x, y);
cleanup:
    /* punish bad behaviour */
    if (is_human(mdat) && (!always_hostile(mdat) && mtmp->malign <= 0) &&
        (mndx < PM_ARCHEOLOGIST || mndx > PM_WIZARD) &&
        u.ualign.type != A_CHAOTIC) {
        HTelepat &= ~INTRINSIC;
        change_luck(-2);
        pline(msgc_alignbad, "You murderer!");
        if (Blind && !Blind_telepat)
            see_monsters(FALSE);     /* Can't sense monsters any more. */
    }
    if ((mtmp->mpeaceful && !rn2(2)) || mtmp->mtame)
        change_luck(-1);
    if (is_unicorn(mdat) && sgn(u.ualign.type) == sgn(mdat->maligntyp)) {
        change_luck(-5);
        pline(msgc_alignbad, "You feel guilty...");
    }

    /* give experience points */
    tmp = experience(mtmp, (int)mvitals[mndx].died + 1);
    more_experienced(tmp, 0);
    newexplevel();      /* will decide if you go up */

    /* adjust alignment points */
    if (mtmp->m_id == u.quest_status.leader_m_id) {       /* REAL BAD! */
        adjalign(-(u.ualign.record + (int)ALIGNLIM / 2));
        /* Technically just a msgc_alignbad, but bad enough that we want to use
           a higher-priority channel */
        pline(msgc_intrloss, "That was %sa bad idea...",
              u.uevent.qcompleted ? "probably " : "");
    } else if (mdat->msound == MS_NEMESIS)      /* Real good! */
        adjalign((int)(ALIGNLIM / 4));
    else if (mdat->msound == MS_GUARDIAN) {     /* Bad */
        adjalign(-(int)(ALIGNLIM / 8));
        if (!Hallucination)
            pline(msgc_alignbad, "That was probably a bad idea...");
        else
            pline(msgc_alignbad, "Whoopsie-daisy!");
    } else if (mtmp->ispriest) {
        adjalign((p_coaligned(mtmp)) ? -2 : 2);
        /* cancel divine protection for killing your priest */
        if (p_coaligned(mtmp))
            u.ublessed = 0;
        if (mdat->maligntyp == A_NONE)
            adjalign((int)(ALIGNLIM / 4));      /* BIG bonus */
    } else if (mtmp->mtame) {
        adjalign(-15);  /* bad!! */
        /* your god is mighty displeased... */
        if (!Hallucination)
            You_hear(msgc_petfatal, "the rumble of distant thunder...");
        else
            You_hear(msgc_petfatal, "the studio audience applaud!");
    } else if (mtmp->mpeaceful)
        adjalign(-5);

    /* malign was already adjusted for u.ualign.type and randomization */
    adjalign(mtmp->malign);
}

/* changes the monster into a stone monster of the same type */
/* this should only be called when poly_when_stoned() is true */
void
mon_to_stone(struct monst *mtmp)
{
    if (mtmp->data->mlet == S_GOLEM) {
        /* it's a golem, and not a stone golem */
        if (canseemon(mtmp))
            pline(mtmp->mtame ? msgc_petneutral : msgc_monneutral,
                  "%s solidifies...", Monnam(mtmp));
        if (newcham(mtmp, &mons[PM_STONE_GOLEM], FALSE, FALSE)) {
            if (canseemon(mtmp))
                pline_implied(mtmp->mtame ? msgc_petneutral : msgc_monneutral,
                              "Now it's %s.", an(mtmp->data->mname));
        } else {
            if (canseemon(mtmp))
                pline(msgc_noconsequence, "... and returns to normal.");
        }
    } else
        impossible("Can't polystone %s!", a_monnam(mtmp));
}

/* Make monster mtmp next to you (if possible) */
void
mnexto(struct monst *mtmp)
{
    coord mm;

    if (mtmp == u.usteed) {
        /* Keep your steed in sync with you instead */
        mtmp->mx = u.ux;
        mtmp->my = u.uy;
        return;
    }

    if (!enexto(&mm, level, u.ux, u.uy, mtmp->data))
        return;
    rloc_to(mtmp, mm.x, mm.y);
    return;
}

/* mnearto()
 * Put monster near (or at) location if possible.
 * Returns:
 *      1 - if a monster was moved from x, y to put mtmp at x, y.
 *      0 - in most cases.
 */
boolean
mnearto(struct monst * mtmp, xchar x, xchar y, boolean move_other)
{       /* make sure mtmp gets to x, y! so move m_at(level, x, y) */
    struct monst *othermon = NULL;
    xchar newx, newy;
    coord mm;

    if ((mtmp->mx == x) && (mtmp->my == y))
        return FALSE;

    if (move_other && (othermon = m_at(level, x, y))) {
        if (othermon->wormno)
            remove_worm(othermon);
        else
            remove_monster(level, x, y);
    }

    newx = x;
    newy = y;

    if (!goodpos(level, newx, newy, mtmp, 0)) {
        /* actually we have real problems if enexto ever fails. migrating_mons
           that need to be placed will cause no end of trouble. */
        if (!enexto(&mm, level, newx, newy, mtmp->data))
            panic("Nowhere to place '%s' (at (%d, %d), wanted (%d, %d))",
                  k_monnam(mtmp), mtmp->mx, mtmp->my, x, y);
        newx = mm.x;
        newy = mm.y;
    }

    rloc_to(mtmp, newx, newy);

    if (move_other && othermon) {
        othermon->mx = COLNO;
        othermon->my = ROWNO;
        mnearto(othermon, x, y, FALSE);
        if ((othermon->mx != x) || (othermon->my != y))
            return TRUE;
    }

    return FALSE;
}


static const char *const poiseff[] = {
    " feel weaker", "r brain is on fire",
    "r judgement is impaired", "r muscles won't obey you",
    " feel very sick", " break out in hives"
};

/* Print an alternative message for losing ability to poison; this assumes that
   the normal ability-loss message will be suppressed. */
void
poisontell(int typ)
{
    pline_implied(msgc_intrloss, "You%s.", poiseff[typ]);
}

/* Cause should be a string suitable for passing to killer_msg, because the
   actual death mechanism may vary. It should have an article if appropriate. */
void
poisoned(const char *string, int typ, const char *killer, int fatal)
{
    int i, plural;
    boolean thrown_weapon = (fatal < 0);
    boolean resist_message_printed = FALSE;

    if (thrown_weapon)
        fatal = -fatal;

    fatal += 20 * thrown_weapon;
    enum rng rng;
    switch (fatal) {
    case  8: rng = rng_deadlypoison_8;  break;
    case 10: rng = rng_deadlypoison_10; break;
    case 15: rng = rng_deadlypoison_15; break;
    case 30: rng = rng_deadlypoison_30; break;
    default:
        impossible("Unknown poison type %d", fatal);
        rng = rng_main;
        break;
    }

    i = Poison_resistance ? 0 : rn2_on_rng(fatal, rng);

    if (strcmp(string, "blast") && !thrown_weapon) {
        /* 'blast' has already given a 'poison gas' message */
        /* so have "poison arrow", "poison dart", etc... */
        plural = (string[strlen(string) - 1] == 's') ? 1 : 0;
        /* avoid "The" Orcus's sting was poisoned... */
        pline(Poison_resistance ? msgc_playerimmune :
              i == 0 ? msgc_fatal_predone : msgc_fatalavoid,
              "%s%s %s poisoned%s", isupper(*string) ? "" : "The ",
              string, plural ? "were" : "was", Poison_resistance ?
              ", but doesn't affect you." : "!");
        resist_message_printed = TRUE;
    }

    if (Poison_resistance) {
        if (!strcmp(string, "blast"))
            shieldeff(u.ux, u.uy);
        if (!resist_message_printed)
            pline(msgc_playerimmune, "You aren't poisoned.");
        return;
    }

    if (i == 0 && typ != A_CHA) {
        pline(msgc_fatal_predone, "The poison was deadly...");
        done(POISONING, killer);
    } else if (i <= 5) {
        /* Check that a stat change was made */
        if (adjattrib(typ, thrown_weapon ? -1 : -rn1(3, 3), 1))
            pline(msgc_intrloss, "You%s!", poiseff[typ]);
    } else {
        i = thrown_weapon ? rnd(6) : rn1(10, 6);
        if (Half_physical_damage)
            i = (i + 1) / 2;
        losehp(i, killer);
    }

    if (u.uhp < 1) {
        impossible("Survived to the end of poisoned() with negative HP");
        done(DIED, killer);
    }
    encumber_msg();
}

/* monster responds to player action; not the same as a passive attack */
/* assumes reason for response has been tested, and response _must_ be made */
void
m_respond(struct monst *mtmp)
{
    if (mtmp->data->msound == MS_SHRIEK) {
        if (canhear()) {
            pline(msgc_levelsound, "%s shrieks.", Monnam(mtmp));
            action_interrupted();
        }
        if (!rn2(10)) {
            if (!rn2(13))
                makemon(&mons[PM_PURPLE_WORM], level, COLNO, ROWNO,
                        NO_MM_FLAGS);
            else
                makemon(NULL, level, COLNO, ROWNO, NO_MM_FLAGS);

        }
        aggravate();
    }
    if (mtmp->data == &mons[PM_MEDUSA]) {
        int i;

        for (i = 0; i < NATTK; i++)
            if (mtmp->data->mattk[i].aatyp == AT_GAZE) {
                gazemu(mtmp, &mtmp->data->mattk[i]);
                break;
            }
    }
}


/* Marks a monster as peaceful (hostile = FALSE) or hostile (hostile = TRUE),
   while handling appropriate side effects (brandings, etc.); note that this
   will untame tame monsters. Does not print messages; this is a low-level
   function, basically just a setter method for mtmp->mpeaceful. Use this for
   hostility changes on monsters after makemon finishes returning; you don't
   need to use it before the monster is placed on the map. You also don't need
   to use this function during level creation (and shouldn't, really, although
   there's an !in_mklev check to prevent problems if you do anyway).

   If adjust_malign is true, then change the alignment yield of the monster as
   if it had generated in the given hostility status (i.e. bonus for killing
   hostiles, penalty for killing peacefuls). If it's false, leave the alignment
   yield of the monster alone.

   For general angering when it's the player's fault (attacking a monster,
   etc.), see setmangry. */
void
msethostility(struct monst *mtmp, boolean hostile, boolean adjust_malign)
{
    mtmp->mpeaceful = !hostile;
    mtmp->mtame = 0;

    if (mtmp->dlevel == level && !in_mklev)
        newsym(mtmp->mx, mtmp->my);

    if (adjust_malign)
        set_malign(mtmp);
}

void
setmangry(struct monst *mtmp)
{
    mtmp->mstrategy &= ~STRAT_WAITMASK;
    if (!mtmp->mpeaceful)
        return;
    if (mtmp->mtame)
        return;
    msethostility(mtmp, TRUE, FALSE);

    if (mtmp->ispriest) {
        if (p_coaligned(mtmp))
            adjalign(-5);       /* very bad */
        else
            adjalign(2);
    } else
        adjalign(-1);   /* attacking peaceful monsters is bad */

    if (couldsee(mtmp->mx, mtmp->my)) {
        if (humanoid(mtmp->data) || mtmp->isshk || mtmp->isgd)
            pline(always_peaceful(mtmp->data) ? msgc_npcanger :
                  msgc_alignbad, "%s gets angry!", Monnam(mtmp));
        else if (flags.verbose)
            growl(mtmp);
    }

    /* attacking your own quest leader will anger his or her guardians */
    if (!flags.mon_moving &&    /* should always be the case here */
        monsndx(mtmp->data) == quest_info(MS_LEADER)) {
        struct monst *mon;
        int got_mad = 0;

        /* guardians will sense this attack even if they can't see it */
        for (mon = level->monlist; mon; mon = mon->nmon)
            if (!DEADMONSTER(mon) && mon->data == &pm_guardian &&
                mon->mpeaceful) {
                msethostility(mtmp, TRUE, FALSE);
                if (canseemon(mon))
                    ++got_mad;
            }
        if (got_mad && !Hallucination)
            pline(msgc_npcanger, "The %s appear%s to be angry too...",
                  got_mad == 1 ?
                  pm_guardian.mname : makeplural(pm_guardian.mname),
                  got_mad == 1 ? "s" : "");
    }
}


void
wakeup(struct monst *mtmp, boolean force_detected)
{
    mtmp->msleeping = 0;
    mtmp->meating = 0;  /* assume there's no salvagable food left */
    setmangry(mtmp);
    if (mtmp->m_ap_type)
        seemimic(mtmp);
    else if (force_detected && !flags.mon_moving && mtmp->mundetected) {
        mtmp->mundetected = 0;
        if (mtmp->dlevel == level)
            newsym(mtmp->mx, mtmp->my);
    }
}

/* Wake up monsters near the player. If 'intentional' is set, more experienced
   players produce a louder volume. Otherwise, more experienced players produce
   a quieter volume, and Stealth and being a rogue both muffle it. Aggravate
   monster overrides all this, causing even the slightest noise to be heard by
   the entire level. */
void
wake_nearby(boolean intentional)
{
    wake_nearto(u.ux, u.uy, Aggravate_monster ? COLNO * COLNO + ROWNO * ROWNO :
                intentional ? u.ulevel * 20 :
                600 / (u.ulevel * (Role_if(PM_ROGUE) ? 2 : 1) *
                       (Stealth ? 2 : 1)));
}

/* Produce noise at a particular location. Monsters in the given dist2 radius
   will hear the noise, wake up if asleep, and go to investigate. */
void
wake_nearto(int x, int y, int distance)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp) &&
            (distance == 0 || dist2(mtmp->mx, mtmp->my, x, y) < distance)) {
            mtmp->msleeping = 0;
            /* monsters are curious as to what caused the noise, and don't
               necessarily consider it to have been the player */
            if (!(mtmp->mstrategy & STRAT_WAITMASK))
                mtmp->mstrategy = STRAT(STRAT_GROUND, x, y, 0);
        }
}

/* NOTE: we must check for mimicry before calling this routine */
void
seemimic(struct monst *mtmp)
{
    unsigned old_app = mtmp->mappearance;
    uchar old_ap_type = mtmp->m_ap_type;

    mtmp->m_ap_type = M_AP_NOTHING;
    mtmp->mappearance = 0;

    /*
     *  Discovered mimics don't block light.
     */
    if (((old_ap_type == M_AP_FURNITURE &&
          (old_app == S_hcdoor || old_app == S_vcdoor)) ||
         (old_ap_type == M_AP_OBJECT && old_app == BOULDER)) &&
        !does_block(mtmp->dlevel, mtmp->mx, mtmp->my))
        unblock_point(mtmp->mx, mtmp->my);

    if (mtmp->dlevel == level)
        newsym(mtmp->mx, mtmp->my);
}

/* force all chameleons to become normal */
void
resistcham(void)
{
    struct monst *mtmp;
    int mcham;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        mcham = (int)mtmp->cham;
        if (mcham) {
            mtmp->cham = CHAM_ORDINARY;
            newcham(mtmp, &mons[cham_to_pm[mcham]], FALSE, FALSE);
        }
        if (is_were(mtmp->data) && mtmp->data->mlet != S_HUMAN)
            new_were(mtmp);
        if (mtmp->m_ap_type && cansee(mtmp->mx, mtmp->my)) {
            seemimic(mtmp);
            /* we pretend that the mimic doesn't */
            /* know that it has been unmasked.  */
            mtmp->msleeping = 1;
        }
    }
}

/* Let the chameleons change again -dgk */
void
restartcham(void)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        mtmp->cham = pm_to_cham(monsndx(mtmp->data));
        if (mtmp->data->mlet == S_MIMIC && mtmp->msleeping &&
            cansee(mtmp->mx, mtmp->my)) {
            set_mimic_sym(mtmp, mtmp->dlevel, rng_main);
            if (mtmp->dlevel == level)
                newsym(mtmp->mx, mtmp->my);
        }
    }
}

/* called when restoring a monster from a saved level; protection
   against shape-changing might be different now than it was at the
   time the level was saved. */
void
restore_cham(struct monst *mon)
{
    int mcham;

    if (Protection_from_shape_changers) {
        mcham = (int)mon->cham;
        if (mcham) {
            mon->cham = CHAM_ORDINARY;
            newcham(mon, &mons[cham_to_pm[mcham]], FALSE, FALSE);
        } else if (is_were(mon->data) && !is_human(mon->data)) {
            new_were(mon);
        }
    } else if (mon->cham == CHAM_ORDINARY) {
        mon->cham = pm_to_cham(monsndx(mon->data));
    }
}

/* unwatched hiders may hide again; if so, a 1 is returned.  */
static boolean
restrap(struct monst *mtmp)
{
    if (mtmp->cham || mtmp->mcan || mtmp->m_ap_type ||
        cansee(mtmp->mx, mtmp->my) || rn2(3) || (mtmp == u.ustuck) ||
        (sensemon(mtmp) && distu(mtmp->mx, mtmp->my) <= 2))
        return FALSE;

    if (mtmp->data->mlet == S_MIMIC) {
        set_mimic_sym(mtmp, level, rng_main);
        return TRUE;
    } else if (level->locations[mtmp->mx][mtmp->my].typ == ROOM) {
        mtmp->mundetected = 1;
        return TRUE;
    }

    return FALSE;
}

static short *animal_list = 0; /* list of PM values for animal monsters */
static int animal_list_count;

void
mon_animal_list(boolean construct)
{
    if (construct) {
        short animal_temp[SPECIAL_PM];
        int i, n;

        /* if (animal_list) impossible("animal_list already exists"); */

        for (n = 0, i = LOW_PM; i < SPECIAL_PM; i++)
            if (is_animal(&mons[i]))
                animal_temp[n++] = i;
        /* if (n == 0) animal_temp[n++] = NON_PM; */

        animal_list = malloc(n * sizeof *animal_list);
        memcpy(animal_list, animal_temp, n * sizeof *animal_list);
        animal_list_count = n;
    } else {    /* release */
        if (animal_list)
            free(animal_list), animal_list = 0;
        animal_list_count = 0;
    }
}

static int
pick_animal(void)
{
    if (!animal_list)
        mon_animal_list(TRUE);

    return animal_list[rn2(animal_list_count)];
}

static int
select_newcham_form(struct monst *mon)
{
    int mndx = NON_PM;

    switch (mon->cham) {
    case CHAM_SANDESTIN:
        if (rn2(7))
            mndx = pick_nasty();
        break;
    case CHAM_DOPPELGANGER:
        if (!rn2(7))
            mndx = pick_nasty();
        else if (rn2(3))
            mndx = rn1(PM_WIZARD - PM_ARCHEOLOGIST + 1, PM_ARCHEOLOGIST);
        break;
    case CHAM_CHAMELEON:
        if (!rn2(3))
            mndx = pick_animal();
        break;
    case CHAM_ORDINARY:
        {
            struct obj *m_armr = which_armor(mon, os_arm);

            if (m_armr && Is_dragon_scales(m_armr))
                mndx = Dragon_scales_to_pm(m_armr) - mons;
            else if (m_armr && Is_dragon_mail(m_armr))
                mndx = Dragon_mail_to_pm(m_armr) - mons;
        }
        break;
    }

    /* For debugging only: allow control of polymorphed monster */
    if (wizard && flags.mon_polycontrol) {
        const char *buf;
        const char *pprompt;
        int tries = 0;

        do {
            pprompt = msgprintf(
                "Change %s into what kind of monster? [type the name]",
                mon_nam(mon));
            buf = getlin(pprompt, FALSE);
            mndx = name_to_mon(buf);
            if (mndx < LOW_PM)
                pline(msgc_hint, "You cannot polymorph %s into that.",
                      mon_nam(mon));
            else
                break;
        } while (++tries < 5);
        if (tries == 5)
            pline(msgc_yafm, "That's enough tries!");
    }

    if (mndx == NON_PM)
        mndx = LOW_PM + rn2_on_rng(
            SPECIAL_PM - LOW_PM, mon->mtame ? rng_polyform_tame :
            mon->mpeaceful ? rng_main : rng_polyform_hostile);
    return mndx;
}

/* make a chameleon look like a new monster; returns 1 if it actually changed */
int
newcham(struct monst *mtmp, const struct permonst *mdat,
        boolean polyspot, /* change is the result of wand or spell of
                             polymorph */
        boolean msg)      /* "The oldmon turns into a newmon!" */
{
    int mhp, hpn, hpd;
    int mndx, tryct;
    const struct permonst *olddata = mtmp->data;
    const char *oldname = NULL; /* initialize because gcc can't figure out that
                                   this is unused if !msg */

    if (msg) {
        /* like Monnam() but never mention saddle */
        oldname = x_monnam(mtmp, ARTICLE_THE, NULL, SUPPRESS_SADDLE, FALSE);
        oldname = msgupcasefirst(oldname);
    }

    /* mdat = 0 -> caller wants a random monster shape */
    tryct = 0;
    if (mdat == 0) {
        while (++tryct <= 100) {
            mndx = select_newcham_form(mtmp);
            mdat = &mons[mndx];
            if ((mvitals[mndx].mvflags & G_GENOD) != 0 || is_placeholder(mdat))
                continue;
            /* polyok rules out all M2_PNAME and M2_WERE's; select_newcham_form
               might deliberately pick a player character type, so we can't
               arbitrarily rule out all human forms any more */
            if (is_mplayer(mdat) || (!is_human(mdat) && polyok(mdat)))
                break;
        }
        if (tryct > 100)
            return 0;   /* Should never happen */
    } else if (mvitals[monsndx(mdat)].mvflags & G_GENOD)
        return 0;       /* passed in mdat is genocided */

    if (is_male(mdat)) {
        if (mtmp->female)
            mtmp->female = FALSE;
    } else if (is_female(mdat)) {
        if (!mtmp->female)
            mtmp->female = TRUE;
    } else if (!is_neuter(mdat)) {
        if (!rn2(10))
            mtmp->female = !mtmp->female;
    }

    if (In_endgame(&mtmp->dlevel->z) && is_mplayer(olddata)) {
        /* mplayers start out as "Foo the Bar", but some of the titles are
           inappropriate when polymorphed, particularly into the opposite sex.
           players don't use ranks when polymorphed, so dropping the rank for
           mplayers seems reasonable. */
        char *p = strchr(NAME(mtmp), ' ');

        if (p) {
            *p = '\0';
            mtmp->mnamelth = p - NAME(mtmp) + 1;
        }
    }

    if (mdat == mtmp->data)
        return 0;       /* still the same monster */

    if (mtmp->wormno) { /* throw tail away */
        wormgone(mtmp);
        place_monster(mtmp, mtmp->mx, mtmp->my);
    }

    hpn = mtmp->mhp;
    hpd = (mtmp->m_lev < 50) ? ((int)mtmp->m_lev) * 8 : mdat->mlevel;
    if (!hpd)
        hpd = 4;

    mtmp->m_lev = adj_lev(&mtmp->dlevel->z, mdat);      /* new monster level */

    mhp = (mtmp->m_lev < 50) ? ((int)mtmp->m_lev) * 8 : mdat->mlevel;
    if (!mhp)
        mhp = 4;

    /* new hp: same fraction of max as before */
    mtmp->mhp = (int)(((long)hpn * (long)mhp) / (long)hpd);
    if (mtmp->mhp < 0)
        mtmp->mhp = hpn;        /* overflow */
/* Unlikely but not impossible; a 1HD creature with 1HP that changes into a
   0HD creature will require this statement */
    if (!mtmp->mhp)
        mtmp->mhp = 1;

/* and the same for maximum hit points */
    hpn = mtmp->mhpmax;
    mtmp->mhpmax = (int)(((long)hpn * (long)mhp) / (long)hpd);
    if (mtmp->mhpmax < 0)
        mtmp->mhpmax = hpn;     /* overflow */
    if (!mtmp->mhpmax)
        mtmp->mhpmax = 1;

    /* take on the new form... */
    set_mon_data(mtmp, mdat, 0);

    if (emits_light(olddata) != emits_light(mtmp->data)) {
        /* used to give light, now doesn't, or vice versa, or light's range has
           changed */
        if (emits_light(olddata))
            del_light_source(mtmp->dlevel, LS_MONSTER, mtmp);
        if (emits_light(mtmp->data))
            new_light_source(mtmp->dlevel, mtmp->mx, mtmp->my,
                             emits_light(mtmp->data), LS_MONSTER, mtmp);
    }
    if (!mtmp->perminvis || pm_invisible(olddata))
        mtmp->perminvis = pm_invisible(mdat);
    mtmp->minvis = mtmp->invis_blkd ? 0 : mtmp->perminvis;
    if (!(hides_under(mdat) && OBJ_AT_LEV(mtmp->dlevel, mtmp->mx, mtmp->my)) &&
        !(mdat->mlet == S_EEL && is_pool(level, mtmp->mx, mtmp->my)))
        mtmp->mundetected = 0;
    if (u.ustuck == mtmp) {
        if (Engulfed) {
            if (!attacktype(mdat, AT_ENGL)) {
                /* Does mdat care? */
                if (!noncorporeal(mdat) && !amorphous(mdat) && !is_whirly(mdat)
                    && (mdat != &mons[PM_YELLOW_LIGHT])) {
                    pline(msgc_consequence, "You break out of %s%s!",
                          mon_nam(mtmp), (is_animal(mdat) ? "'s stomach" : ""));
                    mtmp->mhp = 1;      /* almost dead */
                }
                expels(mtmp, olddata, FALSE);
            } else {
                /* update swallow glyphs for new monster */
                swallowed(0);
            }
        } else if (!sticks(mdat) && !sticks(youmonst.data))
            unstuck(mtmp);
    }

    if (mdat == &mons[PM_LONG_WORM] &&
        (mtmp->wormno = get_wormno(mtmp->dlevel)) != 0) {
        /* we can now create worms with tails - 11/91 */
        initworm(mtmp, rn2(5));
        if (count_wsegs(mtmp))
            place_worm_tail_randomly(mtmp, mtmp->mx, mtmp->my, rng_main);
    }

    if (mtmp->dlevel == level)
        newsym(mtmp->mx, mtmp->my);

    if (msg) {
        uchar save_mnamelth = mtmp->mnamelth;

        mtmp->mnamelth = 0;
        pline(mtmp->mtame ? msgc_petneutral : msgc_monneutral,
              "%s turns into %s!", oldname,
              mdat == &mons[PM_GREEN_SLIME] ? "slime" :
              x_monnam(mtmp, ARTICLE_A, NULL, SUPPRESS_SADDLE, FALSE));
        mtmp->mnamelth = save_mnamelth;
    }

    possibly_unwield(mtmp, polyspot);   /* might lose use of weapon */
    mon_break_armor(mtmp, polyspot);
    /* TODO: the use of mtmp here is a guess. */
    if (!(mtmp->misc_worn_check & W_MASK(os_armg)))
        mselftouch(mtmp, "No longer petrify-resistant, ",
                   flags.mon_moving ? mtmp : &youmonst);
    m_dowear(mtmp, FALSE);

    /* This ought to re-test can_carry() on each item in the inventory rather
       than just checking ex-giants & boulders, but that'd be pretty expensive
       to perform.  If implemented, then perhaps minvent should be sorted in
       order to drop heaviest items first. */
    /* former giants can't continue carrying boulders */
    if (mtmp->minvent && !throws_rocks(mdat)) {
        struct obj *otmp, *otmp2;

        for (otmp = mtmp->minvent; otmp; otmp = otmp2) {
            otmp2 = otmp->nobj;
            if (otmp->otyp == BOULDER) {
                /* this keeps otmp from being polymorphed in the same zap that
                   the monster that held it is polymorphed */
                if (polyspot)
                    bypass_obj(otmp);
                obj_extract_self(otmp);
                /* probably ought to give some "drop" message here */
                if (flooreffects(otmp, mtmp->mx, mtmp->my, ""))
                    continue;
                place_object(otmp, level, mtmp->mx, mtmp->my);
            }
        }
    }

    return 1;
}

/* sometimes an egg will be special */
#define BREEDER_EGG (!rn2(77))

/*
 * Determine if the given monster number can be hatched from an egg.
 * Return the monster number to use as the egg's corpsenm.  Return
 * NON_PM if the given monster can't be hatched.
 */
int
can_be_hatched(int mnum)
{
    /* ranger quest nemesis has the oviparous bit set, making it be possible to
       wish for eggs of that unique monster; turn such into ordinary eggs
       rather than forbidding them outright */
    if (mnum == PM_SCORPIUS)
        mnum = PM_SCORPION;

    mnum = little_to_big(mnum);
    /*
     * Queen bees lay killer bee eggs (usually), but killer bees don't
     * grow into queen bees.  Ditto for [winged-]gargoyles.
     */
    if (mnum == PM_KILLER_BEE || mnum == PM_GARGOYLE ||
        (lays_eggs(&mons[mnum]) &&
         (BREEDER_EGG || (mnum != PM_QUEEN_BEE && mnum != PM_WINGED_GARGOYLE))))
        return mnum;
    return NON_PM;
}

/* type of egg laid by #sit; usually matches parent */
int egg_type_from_parent(int mnum,      /* parent monster; caller must handle
                                           lays_eggs() check */
                         boolean force_ordinary) {
    if (force_ordinary || !BREEDER_EGG) {
        if (mnum == PM_QUEEN_BEE)
            mnum = PM_KILLER_BEE;
        else if (mnum == PM_WINGED_GARGOYLE)
            mnum = PM_GARGOYLE;
    }
    return mnum;
}

/* decide whether an egg of the indicated monster type is viable; */
/* also used to determine whether an egg or tin can be created... */
boolean
dead_species(int m_idx, boolean egg)
{
    /*
     * For monsters with both baby and adult forms, genociding either
     * form kills all eggs of that monster.  Monsters with more than
     * two forms (small->large->giant mimics) are more or less ignored;
     * fortunately, none of them have eggs.  Species extinction due to
     * overpopulation does not kill eggs.
     */
    return (boolean)
        (m_idx >= LOW_PM &&
         ((mvitals[m_idx].mvflags & G_GENOD) != 0 ||
          (egg && (mvitals[big_to_little(m_idx)].mvflags & G_GENOD) != 0)));
}

/* kill off any eggs of genocided monsters */
static void
kill_eggs(struct obj *obj_list)
{
    struct obj *otmp;

    for (otmp = obj_list; otmp; otmp = otmp->nobj)
        if (otmp->otyp == EGG) {
            if (dead_species(otmp->corpsenm, TRUE)) {
                /*
                 * It seems we could also just catch this when
                 * it attempted to hatch, so we wouldn't have to
                 * search all of the objlists.. or stop all
                 * hatch timers based on a corpsenm.
                 */
                kill_egg(otmp);
            }
        } else if (Has_contents(otmp)) {
            kill_eggs(otmp->cobj);
        }
}

/* kill all members of genocided species */
void
kill_genocided_monsters(void)
{
    struct monst *mtmp, *mtmp2;
    boolean kill_cham[CHAM_MAX_INDX + 1];
    int mndx;

    kill_cham[CHAM_ORDINARY] = FALSE;   /* (this is mndx==0) */
    for (mndx = 1; mndx <= CHAM_MAX_INDX; mndx++)
        kill_cham[mndx] = (mvitals[cham_to_pm[mndx]].mvflags & G_GENOD) != 0;
    /*
     * Called during genocide, and again upon level change.  The latter
     * catches up with any migrating monsters as they finally arrive at
     * their intended destinations, so possessions get deposited there.
     *
     * Chameleon handling:
     *      1) if chameleons have been genocided, destroy them
     *         regardless of current form;
     *      2) otherwise, force every chameleon which is imitating
     *         any genocided species to take on a new form.
     */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (DEADMONSTER(mtmp))
            continue;
        mndx = monsndx(mtmp->data);
        if ((mvitals[mndx].mvflags & G_GENOD) || kill_cham[mtmp->cham]) {
            if (mtmp->cham && !kill_cham[mtmp->cham])
                newcham(mtmp, NULL, FALSE, FALSE);
            else
                mondead(mtmp);
        }
        if (mtmp->minvent)
            kill_eggs(mtmp->minvent);
    }

    kill_eggs(invent);
    kill_eggs(level->objlist);
    kill_eggs(level->buriedobjlist);
}


void
golemeffects(struct monst *mon, int damtype, int dam)
{
    int heal = 0, slow = 0;

    if (mon->data == &mons[PM_FLESH_GOLEM]) {
        if (damtype == AD_ELEC)
            heal = dam / 6;
        else if (damtype == AD_FIRE || damtype == AD_COLD)
            slow = 1;
    } else if (mon->data == &mons[PM_IRON_GOLEM]) {
        if (damtype == AD_ELEC)
            slow = 1;
        else if (damtype == AD_FIRE)
            heal = dam;
    } else {
        return;
    }
    if (slow) {
        if (mon->mspeed != MSLOW)
            mon_adjust_speed(mon, -1, NULL);
    }
    if (heal) {
        if (mon->mhp < mon->mhpmax) {
            mon->mhp += dam;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
            if (cansee(mon->mx, mon->my))
                pline(combat_msgc(mon, NULL, cr_hit),
                      "%s seems healthier.", Monnam(mon));
        }
    }
}

boolean
angry_guards(boolean silent)
{
    struct monst *mtmp;
    int ct = 0, nct = 0, sct = 0, slct = 0;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if ((mtmp->data == &mons[PM_WATCHMAN] ||
             mtmp->data == &mons[PM_WATCH_CAPTAIN])
            && mtmp->mpeaceful) {
            ct++;
            if (cansee(mtmp->mx, mtmp->my) && mtmp->mcanmove) {
                if (distu(mtmp->mx, mtmp->my) == 2)
                    nct++;
                else
                    sct++;
            }
            if (mtmp->msleeping || mtmp->mfrozen) {
                slct++;
                mtmp->msleeping = mtmp->mfrozen = 0;
            }
            msethostility(mtmp, TRUE, FALSE);
        }
    }
    if (ct) {
        if (!silent) {  /* do we want pline msgs? */
            if (slct)
                pline(msgc_levelsound,
                      "The guard%s wake%s up!", slct > 1 ? "s" : "",
                      slct == 1 ? "s" : "");
            if (nct || sct) {
                if (nct)
                    pline(msgc_npcanger,
                          "The guard%s get%s angry!", nct == 1 ? "" : "s",
                          nct == 1 ? "s" : "");
                else if (!Blind)
                    pline(msgc_npcanger,
                          "You see %sangry guard%s approaching!",
                          sct == 1 ? "an " : "", sct > 1 ? "s" : "");
            } else
                You_hear(msgc_npcanger,
                         "the shrill sound of a guard's whistle.");
        }
        return TRUE;
    }
    return FALSE;
}

void
pacify_guards(void)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (mtmp->data == &mons[PM_WATCHMAN] ||
            mtmp->data == &mons[PM_WATCH_CAPTAIN])
            msethostility(mtmp, FALSE, FALSE);
    }
}

void
mimic_hit_msg(struct monst *mtmp, short otyp)
{
    short ap = mtmp->mappearance;

    switch (mtmp->m_ap_type) {
    case M_AP_NOTHING:
    case M_AP_FURNITURE:
    case M_AP_MONSTER:
        break;
    case M_AP_OBJECT:
        if (otyp == SPE_HEALING || otyp == SPE_EXTRA_HEALING) {
            pline(msgc_monneutral, "%s seems a more vivid %s than before.",
                  The(simple_typename(ap)), c_obj_colors[objects[ap].oc_color]);
        }
        break;
    }
}

/*mon.c*/
