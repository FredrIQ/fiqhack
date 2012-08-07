/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"
#include "emin.h"
#include "edog.h"
#include "eshk.h"
#include "vault.h"
#include <ctype.h>

/* this assumes that a human quest leader or nemesis is an archetype
   of the corresponding role; that isn't so for some roles (tourist
   for instance) but is for the priests and monks we use it for... */
#define quest_mon_represents_role(mptr,role_pm) \
                (mptr->mlet == S_HUMAN && Role_if (role_pm) && \
                  (mptr->msound == MS_LEADER || mptr->msound == MS_NEMESIS))

static boolean uncommon(const d_level * dlev, int mndx);
static int align_shift(const d_level * dlev, const struct permonst *);
static boolean wrong_elem_type(const struct d_level *dlev,
                               const struct permonst *);
static void m_initgrp(struct monst *, struct level *lev, int, int, int);
static void m_initthrow(struct monst *, int, int);
static void m_initweap(struct monst *);
static void m_initinv(struct monst *);

extern const int monstr[];

#define m_initsgrp(mtmp, lev, x, y) m_initgrp(mtmp, lev, x, y, 3)
#define m_initlgrp(mtmp, lev, x, y) m_initgrp(mtmp, lev, x, y, 10)
#define toostrong(monindx, lev)     (monstr[monindx] > lev)
#define tooweak(monindx, lev)       (monstr[monindx] < lev)

struct monst zeromonst; /* only address matters, value is irrelevant */

struct monst *
newmonst(int extyp, int namelen)
{
    struct monst *mon;
    int xlen;

    switch (extyp) {
    case MX_EMIN:
        xlen = sizeof (struct emin);
        break;
    case MX_EPRI:
        xlen = sizeof (struct epri);
        break;
    case MX_EGD:
        xlen = sizeof (struct egd);
        break;
    case MX_ESHK:
        xlen = sizeof (struct eshk);
        break;
    case MX_EDOG:
        xlen = sizeof (struct edog);
        break;
    default:
        xlen = 0;
        break;
    }

    mon = malloc(sizeof (struct monst) + namelen + xlen);
    memset(mon, 0, sizeof (struct monst) + namelen + xlen);
    mon->mxtyp = extyp;
    mon->mxlth = xlen;
    mon->mnamelth = namelen;

    return mon;
}


boolean
is_home_elemental(const struct d_level * dlev, const struct permonst * ptr)
{
    if (ptr->mlet == S_ELEMENTAL)
        switch (monsndx(ptr)) {
        case PM_AIR_ELEMENTAL:
            return Is_airlevel(dlev);
        case PM_FIRE_ELEMENTAL:
            return Is_firelevel(dlev);
        case PM_EARTH_ELEMENTAL:
            return Is_earthlevel(dlev);
        case PM_WATER_ELEMENTAL:
            return Is_waterlevel(dlev);
        }
    return FALSE;
}

/*
 * Return true if the given monster cannot exist on this elemental level->
 */
static boolean
wrong_elem_type(const struct d_level *dlev, const struct permonst *ptr)
{
    if (ptr->mlet == S_ELEMENTAL) {
        return (boolean) (!is_home_elemental(dlev, ptr));
    } else if (Is_earthlevel(dlev)) {
        /* no restrictions? */
    } else if (Is_waterlevel(dlev)) {
        /* just monsters that can swim */
        if (!is_swimmer(ptr))
            return TRUE;
    } else if (Is_firelevel(dlev)) {
        if (!pm_resistance(ptr, MR_FIRE))
            return TRUE;
    } else if (Is_airlevel(dlev)) {
        if (!(is_flyer(ptr) && ptr->mlet != S_TRAPPER) && !is_floater(ptr)
            && !amorphous(ptr) && !noncorporeal(ptr) && !is_whirly(ptr))
            return TRUE;
    }
    return FALSE;
}

/* make a group just like mtmp */
static void
m_initgrp(struct monst *mtmp, struct level *lev, int x, int y, int n)
{
    coord mm;
    int cnt = rnd(n), dl = level_difficulty(&lev->z);
    struct monst *mon;

    /* Tuning: cut down on swarming at low depths */
    if (dl > 0) {
        cnt /= (dl < 3) ? 4 : (dl < 5) ? 2 : 1;
        if (!cnt)
            cnt++;
    }

    mm.x = x;
    mm.y = y;
    while (cnt--) {
        if (peace_minded(mtmp->data))
            continue;
        /* Don't create groups of peaceful monsters since they'll get in our
           way.  If the monster has a percentage chance so some are peaceful
           and some are not, the result will just be a smaller group. */
        if (enexto(&mm, lev, mm.x, mm.y, mtmp->data)) {
            mon = makemon(mtmp->data, lev, mm.x, mm.y, NO_MM_FLAGS);
            mon->mpeaceful = FALSE;
            mon->mavenge = 0;
            set_malign(mon);
            /* Undo the second peace_minded() check in makemon(); if the
               monster turned out to be peaceful the first time we didn't
               create it at all; we don't want a second check. */
        }
    }
}

static void
m_initthrow(struct monst *mtmp, int otyp, int oquan)
{
    struct obj *otmp;

    otmp = mksobj(level, otyp, TRUE, FALSE);
    otmp->quan = (long)rn1(oquan, 3);
    otmp->owt = weight(otmp);
    if (otyp == ORCISH_ARROW)
        otmp->opoisoned = TRUE;
    mpickobj(mtmp, otmp);
}


static void
m_initweap(struct monst *mtmp)
{
    const struct permonst *ptr = mtmp->data;
    int mm = monsndx(ptr);
    struct obj *otmp;

    if (Is_rogue_level(&mtmp->dlevel->z))
        return;
/*
 * first a few special cases:
 *
 *      giants get a boulder to throw sometimes.
 *      ettins get clubs
 *      kobolds get darts to throw
 *      centaurs get some sort of bow & arrows or bolts
 *      soldiers get all sorts of things.
 *      kops get clubs & cream pies.
 */
    switch (ptr->mlet) {
    case S_GIANT:
        if (rn2(2))
            mongets(mtmp, (mm != PM_ETTIN) ? BOULDER : CLUB);
        break;
    case S_HUMAN:
        if (is_mercenary(ptr)) {
            int w1 = 0, w2 = 0;

            switch (mm) {

            case PM_WATCHMAN:
            case PM_SOLDIER:
                if (!rn2(3)) {
                    w1 = rn1(BEC_DE_CORBIN - PARTISAN + 1, PARTISAN);
                    w2 = rn2(2) ? DAGGER : KNIFE;
                } else
                    w1 = rn2(2) ? SPEAR : SHORT_SWORD;
                break;
            case PM_SERGEANT:
                w1 = rn2(2) ? FLAIL : MACE;
                break;
            case PM_LIEUTENANT:
                w1 = rn2(2) ? BROADSWORD : LONG_SWORD;
                break;
            case PM_CAPTAIN:
            case PM_WATCH_CAPTAIN:
                w1 = rn2(2) ? LONG_SWORD : SILVER_SABER;
                break;
            default:
                if (!rn2(4))
                    w1 = DAGGER;
                if (!rn2(7))
                    w2 = SPEAR;
                break;
            }
            if (w1)
                mongets(mtmp, w1);
            if (!w2 && w1 != DAGGER && !rn2(4))
                w2 = KNIFE;
            if (w2)
                mongets(mtmp, w2);
        } else if (is_elf(ptr)) {
            if (rn2(2))
                mongets(mtmp, rn2(2) ? ELVEN_MITHRIL_COAT : ELVEN_CLOAK);
            if (rn2(2))
                mongets(mtmp, ELVEN_LEATHER_HELM);
            else if (!rn2(4))
                mongets(mtmp, ELVEN_BOOTS);
            if (rn2(2))
                mongets(mtmp, ELVEN_DAGGER);
            switch (rn2(3)) {
            case 0:
                if (!rn2(4))
                    mongets(mtmp, ELVEN_SHIELD);
                if (rn2(3))
                    mongets(mtmp, ELVEN_SHORT_SWORD);
                mongets(mtmp, ELVEN_BOW);
                m_initthrow(mtmp, ELVEN_ARROW, 12);
                break;
            case 1:
                mongets(mtmp, ELVEN_BROADSWORD);
                if (rn2(2))
                    mongets(mtmp, ELVEN_SHIELD);
                break;
            case 2:
                if (rn2(2)) {
                    mongets(mtmp, ELVEN_SPEAR);
                    mongets(mtmp, ELVEN_SHIELD);
                }
                break;
            }
            if (mm == PM_ELVENKING) {
                if (rn2(3) || (in_mklev && Is_earthlevel(&u.uz)))
                    mongets(mtmp, PICK_AXE);
                if (!rn2(50))
                    mongets(mtmp, CRYSTAL_BALL);
            }
        } else if (ptr->msound == MS_PRIEST ||
                   quest_mon_represents_role(ptr, PM_PRIEST)) {
            otmp = mksobj(level, MACE, FALSE, FALSE);
            if (otmp) {
                otmp->spe = rnd(3);
                if (!rn2(2))
                    curse(otmp);
                mpickobj(mtmp, otmp);
            }
        }
        break;

    case S_ANGEL:
        {
            int spe2;

            /* create minion stuff; can't use mongets */
            otmp = mksobj(level, LONG_SWORD, FALSE, FALSE);

            /* maybe make it special */
            if (!rn2(20) || is_lord(ptr))
                otmp =
                    oname(otmp,
                          artiname(rn2(2) ? ART_DEMONBANE : ART_SUNSWORD));
            bless(otmp);
            otmp->oerodeproof = TRUE;
            spe2 = rn2(4);
            otmp->spe = max(otmp->spe, spe2);
            mpickobj(mtmp, otmp);

            otmp = mksobj(level, !rn2(4) ||
                          is_lord(ptr) ? SHIELD_OF_REFLECTION : LARGE_SHIELD,
                          FALSE, FALSE);
            otmp->cursed = FALSE;
            otmp->oerodeproof = TRUE;
            otmp->spe = 0;
            mpickobj(mtmp, otmp);
        }
        break;

    case S_HUMANOID:
        if (mm == PM_HOBBIT) {
            switch (rn2(3)) {
            case 0:
                mongets(mtmp, DAGGER);
                break;
            case 1:
                mongets(mtmp, ELVEN_DAGGER);
                break;
            case 2:
                mongets(mtmp, SLING);
                break;
            }
            if (!rn2(10))
                mongets(mtmp, ELVEN_MITHRIL_COAT);
            if (!rn2(10))
                mongets(mtmp, DWARVISH_CLOAK);
        } else if (is_dwarf(ptr)) {
            if (rn2(7))
                mongets(mtmp, DWARVISH_CLOAK);
            if (rn2(7))
                mongets(mtmp, IRON_SHOES);
            if (!rn2(4)) {
                mongets(mtmp, DWARVISH_SHORT_SWORD);
                /* note: you can't use a mattock with a shield */
                if (rn2(2))
                    mongets(mtmp, DWARVISH_MATTOCK);
                else {
                    mongets(mtmp, AXE);
                    mongets(mtmp, DWARVISH_ROUNDSHIELD);
                }
                mongets(mtmp, DWARVISH_IRON_HELM);
                if (!rn2(3))
                    mongets(mtmp, DWARVISH_MITHRIL_COAT);
            } else {
                mongets(mtmp, !rn2(3) ? PICK_AXE : DAGGER);
            }
        }
        break;
    case S_KOP:        /* create Keystone Kops with cream pies to throw. As
                           suggested by KAA.  [MRS] */
        if (!rn2(4))
            m_initthrow(mtmp, CREAM_PIE, 2);
        if (!rn2(3))
            mongets(mtmp, (rn2(2)) ? CLUB : RUBBER_HOSE);
        break;
    case S_ORC:
        if (rn2(2))
            mongets(mtmp, ORCISH_HELM);
        switch (mm !=
                PM_ORC_CAPTAIN ? mm : rn2(2) ? PM_MORDOR_ORC : PM_URUK_HAI) {
        case PM_MORDOR_ORC:
            if (!rn2(3))
                mongets(mtmp, SCIMITAR);
            if (!rn2(3))
                mongets(mtmp, ORCISH_SHIELD);
            if (!rn2(3))
                mongets(mtmp, KNIFE);
            if (!rn2(3))
                mongets(mtmp, ORCISH_CHAIN_MAIL);
            break;
        case PM_URUK_HAI:
            if (!rn2(3))
                mongets(mtmp, ORCISH_CLOAK);
            if (!rn2(3))
                mongets(mtmp, ORCISH_SHORT_SWORD);
            if (!rn2(3))
                mongets(mtmp, IRON_SHOES);
            if (!rn2(3)) {
                mongets(mtmp, ORCISH_BOW);
                m_initthrow(mtmp, ORCISH_ARROW, 12);
            }
            if (!rn2(3))
                mongets(mtmp, URUK_HAI_SHIELD);
            break;
        default:
            if (mm != PM_ORC_SHAMAN && rn2(2))
                mongets(mtmp, (mm == PM_GOBLIN || rn2(2) == 0)
                        ? ORCISH_DAGGER : SCIMITAR);
        }
        break;
    case S_OGRE:
        if (!rn2(mm == PM_OGRE_KING ? 3 : mm == PM_OGRE_LORD ? 6 : 12))
            mongets(mtmp, BATTLE_AXE);
        else
            mongets(mtmp, CLUB);
        break;
    case S_TROLL:
        if (!rn2(2))
            switch (rn2(4)) {
            case 0:
                mongets(mtmp, RANSEUR);
                break;
            case 1:
                mongets(mtmp, PARTISAN);
                break;
            case 2:
                mongets(mtmp, GLAIVE);
                break;
            case 3:
                mongets(mtmp, SPETUM);
                break;
            }
        break;
    case S_KOBOLD:
        if (!rn2(4))
            m_initthrow(mtmp, DART, 12);
        break;

    case S_CENTAUR:
        if (rn2(2)) {
            if (ptr == &mons[PM_FOREST_CENTAUR]) {
                mongets(mtmp, BOW);
                m_initthrow(mtmp, ARROW, 12);
            } else {
                mongets(mtmp, CROSSBOW);
                m_initthrow(mtmp, CROSSBOW_BOLT, 12);
            }
        }
        break;
    case S_WRAITH:
        mongets(mtmp, KNIFE);
        mongets(mtmp, LONG_SWORD);
        break;
    case S_ZOMBIE:
        if (!rn2(4))
            mongets(mtmp, LEATHER_ARMOR);
        if (!rn2(4))
            mongets(mtmp, (rn2(3) ? KNIFE : SHORT_SWORD));
        break;
    case S_LIZARD:
        if (mm == PM_SALAMANDER)
            mongets(mtmp, (rn2(7) ? SPEAR : rn2(3) ? TRIDENT : STILETTO));
        break;
    case S_DEMON:
        switch (mm) {
        case PM_BALROG:
            mongets(mtmp, BULLWHIP);
            mongets(mtmp, BROADSWORD);
            break;
        case PM_ORCUS:
            mongets(mtmp, WAN_DEATH);   /* the Wand of Orcus */
            break;
        case PM_HORNED_DEVIL:
            mongets(mtmp, rn2(4) ? TRIDENT : BULLWHIP);
            break;
        case PM_DISPATER:
            mongets(mtmp, WAN_STRIKING);
            break;
        case PM_YEENOGHU:
            mongets(mtmp, FLAIL);
            break;
        }
        /* prevent djinnis and mail daemons from leaving objects when they
           vanish */
        if (!is_demon(ptr))
            break;
        /* fall thru */
/*
 * Now the general case, Some chance of getting some type
 * of weapon for "normal" monsters.  Certain special types
 * of monsters will get a bonus chance or different selections.
 */
    default:
        {
            int bias;

            bias = is_lord(ptr) + is_prince(ptr) * 2 + extra_nasty(ptr);
            switch (rnd(14 - (2 * bias))) {
            case 1:
                if (strongmonst(ptr))
                    mongets(mtmp, BATTLE_AXE);
                else
                    m_initthrow(mtmp, DART, 12);
                break;
            case 2:
                if (strongmonst(ptr))
                    mongets(mtmp, TWO_HANDED_SWORD);
                else {
                    mongets(mtmp, CROSSBOW);
                    m_initthrow(mtmp, CROSSBOW_BOLT, 12);
                }
                break;
            case 3:
                mongets(mtmp, BOW);
                m_initthrow(mtmp, ARROW, 12);
                break;
            case 4:
                if (strongmonst(ptr))
                    mongets(mtmp, LONG_SWORD);
                else
                    m_initthrow(mtmp, DAGGER, 3);
                break;
            case 5:
                if (strongmonst(ptr))
                    mongets(mtmp, LUCERN_HAMMER);
                else
                    mongets(mtmp, AKLYS);
                break;
            default:
                break;
            }
        }
        break;
    }
    if ((int)mtmp->m_lev > rn2(75))
        mongets(mtmp, rnd_offensive_item(mtmp));
}


/*
 *   Makes up money for monster's inventory.
 *   This will change with silver & copper coins
 */
void
mkmonmoney(struct monst *mtmp, long amount)
{
    struct obj *gold = mksobj(level, GOLD_PIECE, FALSE, FALSE);

    gold->quan = amount;
    add_to_minv(mtmp, gold);
}


static void
m_initinv(struct monst *mtmp)
{
    int cnt;
    struct obj *otmp;
    const struct permonst *ptr = mtmp->data;

    if (Is_rogue_level(&mtmp->dlevel->z))
        return;
/*
 * Soldiers get armour & rations - armour approximates their ac.
 * Nymphs may get mirror or potion of object detection.
 */
    switch (ptr->mlet) {

    case S_HUMAN:
        if (is_mercenary(ptr)) {
            int mac;

            switch (monsndx(ptr)) {
            case PM_GUARD:
                mac = -1;
                break;
            case PM_SOLDIER:
                mac = 3;
                break;
            case PM_SERGEANT:
                mac = 0;
                break;
            case PM_LIEUTENANT:
                mac = -2;
                break;
            case PM_CAPTAIN:
                mac = -3;
                break;
            case PM_WATCHMAN:
                mac = 3;
                break;
            case PM_WATCH_CAPTAIN:
                mac = -2;
                break;
            default:
                impossible("odd mercenary %d?", monsndx(ptr));
                mac = 0;
                break;
            }

            if (mac < -1 && rn2(5))
                mac +=
                    7 + mongets(mtmp,
                                (rn2(5)) ? PLATE_MAIL : CRYSTAL_PLATE_MAIL);
            else if (mac < 3 && rn2(5))
                mac += 6 + mongets(mtmp, (rn2(3)) ? SPLINT_MAIL : BANDED_MAIL);
            else if (rn2(5))
                mac +=
                    3 + mongets(mtmp,
                                (rn2(3)) ? RING_MAIL : STUDDED_LEATHER_ARMOR);
            else
                mac += 2 + mongets(mtmp, LEATHER_ARMOR);

            if (mac < 10 && rn2(3))
                mac += 1 + mongets(mtmp, HELMET);
            else if (mac < 10 && rn2(2))
                mac += 1 + mongets(mtmp, DENTED_POT);

            if (mac < 10 && rn2(3))
                mac += 1 + mongets(mtmp, SMALL_SHIELD);
            else if (mac < 10 && rn2(2))
                mac += 2 + mongets(mtmp, LARGE_SHIELD);

            if (mac < 10 && rn2(3))
                mac += 1 + mongets(mtmp, LOW_BOOTS);
            else if (mac < 10 && rn2(2))
                mac += 2 + mongets(mtmp, HIGH_BOOTS);

            if (mac < 10 && rn2(3))
                mongets(mtmp, LEATHER_GLOVES);
            else if (mac < 10 && rn2(2))
                mongets(mtmp, LEATHER_CLOAK);

            if (ptr != &mons[PM_GUARD] && ptr != &mons[PM_WATCHMAN] &&
                ptr != &mons[PM_WATCH_CAPTAIN]) {
                if (!rn2(3))
                    mongets(mtmp, K_RATION);
                if (!rn2(2))
                    mongets(mtmp, C_RATION);
                if (ptr != &mons[PM_SOLDIER] && !rn2(3))
                    mongets(mtmp, BUGLE);
            } else if (ptr == &mons[PM_WATCHMAN] && rn2(3))
                mongets(mtmp, TIN_WHISTLE);
        } else if (ptr == &mons[PM_SHOPKEEPER]) {
            mongets(mtmp, SKELETON_KEY);
            switch (rn2(4)) {
                /* MAJOR fall through ... */
            case 0:
                mongets(mtmp, WAN_MAGIC_MISSILE);
            case 1:
                mongets(mtmp, POT_EXTRA_HEALING);
            case 2:
                mongets(mtmp, POT_HEALING);
            case 3:
                mongets(mtmp, WAN_STRIKING);
            }
        } else if (ptr->msound == MS_PRIEST ||
                   quest_mon_represents_role(ptr, PM_PRIEST)) {
            mongets(mtmp,
                    rn2(7) ? ROBE : rn2(3) ? CLOAK_OF_PROTECTION :
                    CLOAK_OF_MAGIC_RESISTANCE);
            mongets(mtmp, SMALL_SHIELD);
            mkmonmoney(mtmp, (long)rn1(10, 20));
        } else if (quest_mon_represents_role(ptr, PM_MONK)) {
            mongets(mtmp, rn2(11) ? ROBE : CLOAK_OF_MAGIC_RESISTANCE);
        }
        break;
    case S_NYMPH:
        if (!rn2(2))
            mongets(mtmp, MIRROR);
        if (!rn2(2))
            mongets(mtmp, POT_OBJECT_DETECTION);
        break;
    case S_GIANT:
        if (ptr == &mons[PM_MINOTAUR]) {
            if (!rn2(3) || (in_mklev && Is_earthlevel(&mtmp->dlevel->z)))
                mongets(mtmp, WAN_DIGGING);
        } else if (is_giant(ptr)) {
            for (cnt = rn2((int)(mtmp->m_lev / 2)); cnt; cnt--) {
                otmp =
                    mksobj(level, rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1),
                           FALSE, FALSE);
                otmp->quan = (long)rn1(2, 3);
                otmp->owt = weight(otmp);
                mpickobj(mtmp, otmp);
            }
        }
        break;
    case S_WRAITH:
        if (ptr == &mons[PM_NAZGUL]) {
            otmp = mksobj(level, RIN_INVISIBILITY, FALSE, FALSE);
            curse(otmp);
            mpickobj(mtmp, otmp);
        }
        break;
    case S_LICH:
        if (ptr == &mons[PM_MASTER_LICH] && !rn2(13))
            mongets(mtmp, (rn2(7) ? ATHAME : WAN_NOTHING));
        else if (ptr == &mons[PM_ARCH_LICH] && !rn2(3)) {
            otmp =
                mksobj(level, rn2(3) ? ATHAME : QUARTERSTAFF, TRUE,
                       rn2(13) ? FALSE : TRUE);
            if (otmp->spe < 2)
                otmp->spe = rnd(3);
            if (!rn2(4))
                otmp->oerodeproof = 1;
            mpickobj(mtmp, otmp);
        }
        break;
    case S_MUMMY:
        if (rn2(7))
            mongets(mtmp, MUMMY_WRAPPING);
        break;
    case S_QUANTMECH:
        if (!rn2(20)) {
            otmp = mksobj(level, LARGE_BOX, FALSE, FALSE);
            otmp->spe = 1;      /* flag for special box */
            otmp->owt = weight(otmp);
            mpickobj(mtmp, otmp);
        }
        break;
    case S_LEPRECHAUN:
        mkmonmoney(mtmp, (long)dice(level_difficulty(&mtmp->dlevel->z), 30));
        break;
    case S_DEMON:
        /* moved here from m_initweap() because these don't have AT_WEAP so
           m_initweap() is not called for them */
        if (ptr == &mons[PM_ICE_DEVIL] && !rn2(4)) {
            mongets(mtmp, SPEAR);
        } else if (ptr == &mons[PM_ASMODEUS]) {
            mongets(mtmp, WAN_COLD);
            mongets(mtmp, WAN_FIRE);
        }
        break;
    case S_GNOME:
        /* In AceHack, these have a chance of generating with candles,
           especially on dark Mines levels. This tradition was stolen by
           UnNetHack, and continued onwards into NetHack 4. But in a much
           simpler way, to avoid littering inventory with junk candles of
           different lengths. */
        if (!rn2(4)) {
            mongets(mtmp, rn2(4) ? TALLOW_CANDLE : WAX_CANDLE);
        }

    default:
        break;
    }

    /* ordinary soldiers rarely have access to magic (or gold :-) */
    if (ptr == &mons[PM_SOLDIER] && rn2(13))
        return;

    if ((int)mtmp->m_lev > rn2(50))
        mongets(mtmp, rnd_defensive_item(mtmp));
    if ((int)mtmp->m_lev > rn2(100))
        mongets(mtmp, rnd_misc_item(mtmp));
    if (likes_gold(ptr) && !findgold(mtmp->minvent) && !rn2(5))
        mkmonmoney(mtmp,
                   (long)dice(level_difficulty(&mtmp->dlevel->z),
                              mtmp->minvent ? 5 : 10));
}

/* Note: for long worms, always call cutworm (cutworm calls clone_mon) */
struct monst *
clone_mon(struct monst *mon, xchar x, xchar y)
{       /* clone's preferred location or 0 (near mon) */
    coord mm;
    struct monst *m2;

    /* may be too weak or have been extinguished for population control */
    if (mon->mhp <= 1 || (mvitals[monsndx(mon->data)].mvflags & G_EXTINCT))
        return NULL;

    if (x == 0) {
        mm.x = mon->mx;
        mm.y = mon->my;
        if (!enexto(&mm, level, mm.x, mm.y, mon->data) ||
            MON_AT(level, mm.x, mm.y))
            return NULL;
    } else if (!isok(x, y)) {
        return NULL;    /* paranoia */
    } else {
        mm.x = x;
        mm.y = y;
        if (MON_AT(level, mm.x, mm.y)) {
            if (!enexto(&mm, level, mm.x, mm.y, mon->data) ||
                MON_AT(level, mm.x, mm.y))
                return NULL;
        }
    }
    m2 = newmonst(MX_NONE, 0);
    *m2 = *mon; /* copy condition of old monster */
    m2->nmon = level->monlist;
    level->monlist = m2;
    m2->m_id = flags.ident++;
    if (!m2->m_id)
        m2->m_id = flags.ident++;       /* ident overflowed */
    m2->mx = mm.x;
    m2->my = mm.y;

    m2->minvent = NULL; /* objects don't clone */
    m2->mleashed = FALSE;
    /* Max HP the same, but current HP halved for both.  The caller might want
       to override this by halving the max HP also. When current HP is odd, the 
       original keeps the extra point. */
    m2->mhpmax = mon->mhpmax;
    m2->mhp = mon->mhp / 2;
    mon->mhp -= m2->mhp;

    /* since shopkeepers and guards will only be cloned if they've been
       polymorphed away from their original forms, the clone doesn't have room
       for the extra information.  we also don't want two shopkeepers around
       for the same shop. */
    if (mon->isshk)
        m2->isshk = FALSE;
    if (mon->isgd)
        m2->isgd = FALSE;
    if (mon->ispriest)
        m2->ispriest = FALSE;
    m2->mxlth = 0;
    place_monster(m2, m2->mx, m2->my);
    if (emits_light(m2->data))
        new_light_source(m2->dlevel, m2->mx, m2->my, emits_light(m2->data),
                         LS_MONSTER, m2);
    if (m2->mnamelth) {
        m2->mnamelth = 0;       /* or it won't get allocated */
        m2 = christen_monst(m2, NAME(mon));
    } else if (mon->isshk) {
        m2 = christen_monst(m2, shkname(mon));
    }

    /* not all clones caused by player are tame or peaceful */
    if (!flags.mon_moving) {
        if (mon->mtame)
            m2->mtame = rn2(max(2 + u.uluck, 2)) ? mon->mtame : 0;
        else if (mon->mpeaceful)
            m2->mpeaceful = rn2(max(2 + u.uluck, 2)) ? 1 : 0;
    }

    newsym(m2->mx, m2->my);     /* display the new monster */
    if (m2->mtame) {
        struct monst *m3;

        if (mon->isminion) {
            m3 = newmonst(MX_EPRI, mon->mnamelth);
            *m3 = *m2;
            m3->mxtyp = MX_EPRI;
            m3->mxlth = sizeof (struct epri);
            if (m2->mnamelth)
                strcpy(NAME(m3), NAME(m2));
            *(EPRI(m3)) = *(EPRI(mon));
            replmon(m2, m3);
            m2 = m3;
        } else {
            /* because m2 is a copy of mon it is tame but not init'ed. however, 
               tamedog will not re-tame a tame dog, so m2 must be made non-tame 
               to get initialized properly. */
            m2->mtame = 0;
            if ((m3 = tamedog(m2, NULL)) != 0) {
                m2 = m3;
                *(EDOG(m2)) = *(EDOG(mon));
            }
        }
    }
    set_malign(m2);

    return m2;
}

/*
 * Propagate a species
 *
 * Once a certain number of monsters are created, don't create any more
 * at random (i.e. make them extinct).  The previous (3.2) behavior was
 * to do this when a certain number had _died_, which didn't make
 * much sense.
 *
 * Returns FALSE propagation unsuccessful
 *         TRUE  propagation successful
 */
boolean
propagate(int mndx, boolean tally, boolean ghostly)
{
    boolean result;
    uchar lim = mbirth_limit(mndx);
    boolean gone = (mvitals[mndx].mvflags & G_GONE); /* genocided or extinct */

    result = (((int)mvitals[mndx].born < lim) && !gone) ? TRUE : FALSE;

    /* if it's unique, don't ever make it again */
    if (mons[mndx].geno & G_UNIQ)
        mvitals[mndx].mvflags |= G_EXTINCT;

    if (mvitals[mndx].born < 255 && tally && (!ghostly || (ghostly && result)))
        mvitals[mndx].born++;
    if ((int)mvitals[mndx].born >= lim && !(mons[mndx].geno & G_NOGEN) &&
        !(mvitals[mndx].mvflags & G_EXTINCT)) {
        mvitals[mndx].mvflags |= G_EXTINCT;
        reset_rndmonst(mndx);
    }
    return result;
}

/*
 * called with [x,y] = coordinates;
 *      [0,0] means anyplace
 *      [u.ux,u.uy] means: near player (if !in_mklev)
 *
 *      In case we make a monster group, only return the one at [x,y].
 */
struct monst *
makemon(const struct permonst *ptr, struct level *lev, int x, int y,
        int mmflags)
{
    struct monst *mtmp;
    int mndx, mcham, ct, mitem, xtyp;
    boolean anymon = (!ptr);
    boolean byyou = (x == u.ux && y == u.uy);
    boolean allow_minvent = ((mmflags & NO_MINVENT) == 0);
    boolean countbirth = ((mmflags & MM_NOCOUNTBIRTH) == 0);
    unsigned gpflags = (mmflags & MM_IGNOREWATER) ? MM_IGNOREWATER : 0;

    /* if caller wants random location, do it here */
    if (x == 0 && y == 0) {
        int tryct = 0;  /* careful with bigrooms */
        struct monst fakemon;

        fakemon.data = ptr;     /* set up for goodpos */
        do {
            x = rn1(COLNO - 3, 2);
            y = rn2(ROWNO);
        } while (!goodpos(lev, x, y, ptr ? &fakemon : NULL, gpflags) ||
                 (!in_mklev && tryct++ < 50 && cansee(x, y)));
    } else if (byyou && !in_mklev) {
        coord bypos;

        if (enexto_core(&bypos, lev, u.ux, u.uy, ptr, gpflags)) {
            x = bypos.x;
            y = bypos.y;
        } else
            return NULL;
    }

    /* Does monster already exist at the position? */
    if (MON_AT(lev, x, y)) {
        if ((mmflags & MM_ADJACENTOK) != 0) {
            coord bypos;

            if (enexto_core(&bypos, lev, x, y, ptr, gpflags)) {
                x = bypos.x;
                y = bypos.y;
            } else
                return NULL;
        } else
            return NULL;
    }

    if (ptr) {
        mndx = monsndx(ptr);
        /* if you are to make a specific monster and it has already been
           genocided, return */
        if (mvitals[mndx].mvflags & G_GENOD)
            return NULL;
    } else {
        /* make a random (common) monster that can survive here. (the special
           levels ask for random monsters at specific positions, causing mass
           drowning on the medusa level, for instance.) */
        int tryct = 0;  /* maybe there are no good choices */
        struct monst fakemon;

        do {
            if (!(ptr = rndmonst(&lev->z))) {
                return NULL;    /* no more monsters! */
            }
            fakemon.data = ptr; /* set up for goodpos */
        } while (!goodpos(lev, x, y, &fakemon, gpflags) && tryct++ < 50);
        mndx = monsndx(ptr);
    }

    if (mndx == quest_info(MS_LEADER))
        ptr = &pm_leader;
    else if (mndx == quest_info(MS_GUARDIAN))
        ptr = &pm_guardian;
    else if (mndx == quest_info(MS_NEMESIS))
        ptr = &pm_nemesis;

    propagate(mndx, countbirth, FALSE);

    xtyp = ptr->pxtyp;
    if (mmflags & MM_EDOG)
        xtyp = MX_EDOG;
    else if (mmflags & MM_EMIN)
        xtyp = MX_EMIN;

    mtmp = newmonst(xtyp, 0);
    mtmp->nmon = lev->monlist;
    lev->monlist = mtmp;
    mtmp->m_id = flags.ident++;
    if (!mtmp->m_id)
        mtmp->m_id = flags.ident++;     /* ident overflowed */
    set_mon_data(mtmp, ptr, 0);

    if (mtmp->data->msound == MS_LEADER)
        quest_status.leader_m_id = mtmp->m_id;
    mtmp->mnum = mndx;

    mtmp->m_lev = adj_lev(&lev->z, ptr);
    if (is_golem(ptr)) {
        mtmp->mhpmax = mtmp->mhp = golemhp(mndx);
    } else if (is_rider(ptr)) {
        /* We want low HP, but a high mlevel so they can attack well */
        mtmp->mhpmax = mtmp->mhp = dice(10, 8);
    } else if (ptr->mlevel > 49) {
        /* "special" fixed hp monster the hit points are encoded in the mlevel
           in a somewhat strange way to fit in the 50..127 positive range of a
           signed character above the 1..49 that indicate "normal" monster
           levels */
        mtmp->mhpmax = mtmp->mhp = 2 * (ptr->mlevel - 6);
        mtmp->m_lev = mtmp->mhp / 4;    /* approximation */
    } else if (ptr->mlet == S_DRAGON && mndx >= PM_GRAY_DRAGON) {
        /* adult dragons */
        mtmp->mhpmax = mtmp->mhp = (int)(In_endgame(&lev->z) ? (8 * mtmp->m_lev)
                                         : (4 * mtmp->m_lev +
                                            dice((int)mtmp->m_lev, 4)));
    } else if (!mtmp->m_lev) {
        mtmp->mhpmax = mtmp->mhp = rnd(4);
    } else {
        mtmp->mhpmax = mtmp->mhp = dice((int)mtmp->m_lev, 8);
        if (is_home_elemental(&lev->z, ptr))
            mtmp->mhpmax = (mtmp->mhp *= 3);
    }

    if (is_female(ptr))
        mtmp->female = TRUE;
    else if (is_male(ptr))
        mtmp->female = FALSE;
    else
        mtmp->female = rn2(2);  /* ignored for neuters */

    if (In_sokoban(&lev->z) && !mindless(ptr))  /* know about traps here */
        mtmp->mtrapseen = (1L << (PIT - 1)) | (1L << (HOLE - 1));
    if (ptr->msound == MS_LEADER)       /* leader knows about portal */
        mtmp->mtrapseen |= (1L << (MAGIC_PORTAL - 1));

    mtmp->dlevel = lev;
    place_monster(mtmp, x, y);
    mtmp->mcansee = mtmp->mcanmove = TRUE;
    mtmp->mpeaceful = (mmflags & MM_ANGRY) ? FALSE : peace_minded(ptr);

    switch (ptr->mlet) {
    case S_MIMIC:
        set_mimic_sym(mtmp, lev);
        break;
    case S_SPIDER:
    case S_SNAKE:
        if (in_mklev)
            if (x && y)
                mkobj_at(0, lev, x, y, TRUE);
        if (hides_under(ptr) && OBJ_AT_LEV(lev, x, y))
            mtmp->mundetected = TRUE;
        break;
    case S_LIGHT:
    case S_ELEMENTAL:
        if (mndx == PM_STALKER || mndx == PM_BLACK_LIGHT) {
            mtmp->perminvis = TRUE;
            mtmp->minvis = TRUE;
        }
        break;
    case S_EEL:
        if (is_pool(lev, x, y))
            mtmp->mundetected = TRUE;
        break;
    case S_LEPRECHAUN:
        mtmp->msleeping = 1;
        break;
    case S_JABBERWOCK:
    case S_NYMPH:
        if (rn2(5) && !u.uhave.amulet)
            mtmp->msleeping = 1;
        break;
    case S_ORC:
        if (Race_if(PM_ELF))
            mtmp->mpeaceful = FALSE;
        break;
    case S_UNICORN:
        if (is_unicorn(ptr) && sgn(u.ualign.type) == sgn(ptr->maligntyp))
            mtmp->mpeaceful = TRUE;
        break;
    case S_BAT:
        if (In_hell(&lev->z) && is_bat(ptr))
            mon_adjust_speed(mtmp, 2, NULL);
        break;
    }
    if ((ct = emits_light(mtmp->data)) > 0)
        new_light_source(lev, mtmp->mx, mtmp->my, ct, LS_MONSTER, mtmp);
    mitem = 0;  /* extra inventory item for this monster */

    if ((mcham = pm_to_cham(mndx)) != CHAM_ORDINARY) {
        /* If you're protected with a ring, don't create any shape-changing
           chameleons -dgk */
        if (Protection_from_shape_changers)
            mtmp->cham = CHAM_ORDINARY;
        else {
            mtmp->cham = mcham;
            newcham(mtmp, rndmonst(&lev->z), FALSE, FALSE);
        }
    } else if (mndx == PM_WIZARD_OF_YENDOR) {
        mtmp->iswiz = TRUE;
        flags.no_of_wizards++;
        if (flags.no_of_wizards == 1 && Is_earthlevel(&lev->z))
            mitem = SPE_DIG;
    } else if (mndx == PM_DJINNI) {
        flags.djinni_count++;
    } else if (mndx == PM_GHOST) {
        flags.ghost_count++;
        if (!(mmflags & MM_NONAME))
            mtmp = christen_monst(mtmp, rndghostname());
    } else if (mndx == PM_VLAD_THE_IMPALER) {
        mitem = CANDELABRUM_OF_INVOCATION;
    } else if (mndx == PM_CROESUS) {
        mitem = TWO_HANDED_SWORD;
    } else if (ptr->msound == MS_NEMESIS) {
        mitem = BELL_OF_OPENING;
    } else if (mndx == PM_PESTILENCE) {
        mitem = POT_SICKNESS;
    }
    if (mitem && allow_minvent)
        mongets(mtmp, mitem);

    if (in_mklev) {
        if (((is_ndemon(ptr)) || (mndx == PM_WUMPUS) || (mndx == PM_LONG_WORM)
             || (mndx == PM_GIANT_EEL)) && !u.uhave.amulet && rn2(5))
            mtmp->msleeping = TRUE;
    } else {
        if (byyou) {
            newsym(mtmp->mx, mtmp->my);
            set_apparxy(mtmp);
        }
    }
    if (is_dprince(ptr) && ptr->msound == MS_BRIBE) {
        mtmp->mpeaceful = mtmp->minvis = mtmp->perminvis = 1;
        mtmp->mavenge = 0;
        if (uwep && uwep->oartifact == ART_EXCALIBUR)
            mtmp->mpeaceful = mtmp->mtame = FALSE;
    }
    if (mndx == PM_LONG_WORM && (mtmp->wormno = get_wormno(lev)) != 0) {
        /* we can now create worms with tails - 11/91 */
        initworm(mtmp, rn2(5));
        if (count_wsegs(mtmp))
            place_worm_tail_randomly(mtmp, x, y);
    }
    set_malign(mtmp);   /* having finished peaceful changes */
    if (anymon) {
        if ((ptr->geno & G_SGROUP) && rn2(2)) {
            m_initsgrp(mtmp, lev, mtmp->mx, mtmp->my);
        } else if (ptr->geno & G_LGROUP) {
            if (rn2(3))
                m_initlgrp(mtmp, lev, mtmp->mx, mtmp->my);
            else
                m_initsgrp(mtmp, lev, mtmp->mx, mtmp->my);
        }
    }

    if (allow_minvent) {
        if (is_armed(ptr))
            m_initweap(mtmp);   /* equip with weapons / armor */
        m_initinv(mtmp);        /* add on a few special items incl. more armor */
        m_dowear(mtmp, TRUE);
    } else {
        /* no initial inventory is allowed */
        if (mtmp->minvent)
            discard_minvent(mtmp);
        mtmp->minvent = NULL;   /* caller expects this */
    }
    if ((ptr->mflags3 & M3_WAITMASK) && !(mmflags & MM_NOWAIT)) {
        if (ptr->mflags3 & M3_WAITFORU)
            mtmp->mstrategy |= STRAT_WAITFORU;
        if (ptr->mflags3 & M3_CLOSE)
            mtmp->mstrategy |= STRAT_CLOSE;
    }

    if (!in_mklev)
        newsym(mtmp->mx, mtmp->my);     /* make sure the mon shows up */

    return mtmp;
}

int
mbirth_limit(int mndx)
{
    /* assert(MAXMONNO < 255); */
    return mndx == PM_NAZGUL ? 9 : mndx == PM_ERINYS ? 3 : MAXMONNO;
}

/* used for wand/scroll/spell of create monster */
/* returns TRUE iff you know monsters have been created */
boolean
create_critters(int cnt, const struct permonst * mptr)
{       /* usually null; used for confused reading */
    coord c;
    int x, y;
    struct monst *mon;
    boolean known = FALSE;
    boolean ask = wizard;

    while (cnt--) {
        if (ask) {
            if (create_particular()) {
                known = TRUE;
                continue;
            } else
                ask = FALSE;    /* ESC will shut off prompting */
        }
        x = u.ux, y = u.uy;
        /* if in water, try to encourage an aquatic monster by finding and then 
           specifying another wet location */
        if (!mptr && u.uinwater && enexto(&c, level, x, y, &mons[PM_GIANT_EEL]))
            x = c.x, y = c.y;

        mon = makemon(mptr, level, x, y, NO_MM_FLAGS);
        if (mon && canspotmon(mon))
            known = TRUE;
    }
    return known;
}


static boolean
uncommon(const d_level * dlev, int mndx)
{
    if (mons[mndx].geno & (G_NOGEN | G_UNIQ))
        return TRUE;
    if (mvitals[mndx].mvflags & G_GONE)
        return TRUE;
    if (In_hell(dlev))
        return mons[mndx].maligntyp > A_NEUTRAL;
    else
        return (mons[mndx].geno & G_HELL) != 0;
}

/*
 * shift the probability of a monster's generation by
 * comparing the dungeon alignment and monster alignment.
 * return an integer in the range of 0-5.
 */
static int
align_shift(const d_level * dlev, const struct permonst *ptr)
{
    s_level *lev = Is_special(dlev);
    int alshift;

    switch ((lev) ? lev->flags.align : dungeons[dlev->dnum].flags.align) {
    default:   /* just in case */
    case AM_NONE:
        alshift = 0;
        break;
    case AM_LAWFUL:
        alshift = (ptr->maligntyp + 20) / (2 * ALIGNWEIGHT);
        break;
    case AM_NEUTRAL:
        alshift = (20 - abs(ptr->maligntyp)) / ALIGNWEIGHT;
        break;
    case AM_CHAOTIC:
        alshift = (-(ptr->maligntyp - 20)) / (2 * ALIGNWEIGHT);
        break;
    }
    return alshift;
}

static struct rndmonst_state {
    int choice_count;
    char mchoices[SPECIAL_PM];  /* value range is 0..127 */
} rndmonst_state;

/* select a random monster type */
const struct permonst *
rndmonst(const d_level * dlev)
{
    const struct permonst *ptr;
    int mndx, ct;

    if (dlev->dnum == quest_dnum && rn2(7) && (ptr = qt_montype(dlev)) != 0)
        return ptr;

    if (rndmonst_state.choice_count < 0) {      /* need to recalculate */
        int zlevel, minmlev, maxmlev;
        boolean elemlevel;
        boolean upper;

        rndmonst_state.choice_count = 0;
        /* look for first common monster */
        for (mndx = LOW_PM; mndx < SPECIAL_PM; mndx++) {
            if (!uncommon(dlev, mndx))
                break;
            rndmonst_state.mchoices[mndx] = 0;
        }
        if (mndx == SPECIAL_PM) {
            /* evidently they've all been exterminated */
            return NULL;
        }       /* else `mndx' now ready for use below */
        zlevel = level_difficulty(dlev);
        /* determine the level of the weakest monster to make. */
        minmlev = zlevel / 6;
        /* determine the level of the strongest monster to make. The strength
           of the initial inhabitants of the level does not depend on the
           player level. */
        if (in_mklev)
            maxmlev = zlevel;
        else
            maxmlev = (zlevel + u.ulevel) / 2;
        upper = Is_rogue_level(dlev);
        elemlevel = In_endgame(dlev) && !Is_astralevel(dlev);

/*
 * Find out how many monsters exist in the range we have selected.
 */
        /* (`mndx' initialized above) */
        for (; mndx < SPECIAL_PM; mndx++) {
            ptr = &mons[mndx];
            rndmonst_state.mchoices[mndx] = 0;
            if (tooweak(mndx, minmlev) || toostrong(mndx, maxmlev))
                continue;
            if (upper && !isupper(def_monsyms[(int)(ptr->mlet)]))
                continue;
            if (elemlevel && wrong_elem_type(dlev, ptr))
                continue;
            if (uncommon(dlev, mndx))
                continue;
            if (In_hell(dlev) && (ptr->geno & G_NOHELL))
                continue;
            ct = (int)(ptr->geno & G_FREQ) + align_shift(dlev, ptr);
            if (ct < 0 || ct > 127)
                panic("rndmonst: bad count [#%d: %d]", mndx, ct);
            rndmonst_state.choice_count += ct;
            rndmonst_state.mchoices[mndx] = (char)ct;
        }
/*
 * Possible modification:  if choice_count is "too low",
 * expand minmlev..maxmlev range and try again.
 */
    }
    /* choice_count+mchoices[] recalc */
    if (rndmonst_state.choice_count <= 0) {
        /* maybe no common mons left, or all are too weak or too strong */
        return NULL;
    }

/*
 * Now, select a monster at random.
 */
    ct = rnd(rndmonst_state.choice_count);
    for (mndx = LOW_PM; mndx < SPECIAL_PM; mndx++)
        if ((ct -= (int)rndmonst_state.mchoices[mndx]) <= 0)
            break;

    if (mndx == SPECIAL_PM || uncommon(dlev, mndx)) {   /* shouldn't happen */
        impossible("rndmonst: bad `mndx' [#%d]", mndx);
        return NULL;
    }
    return &mons[mndx];
}

/* called when you change level (experience or dungeon depth) or when
   monster species can no longer be created (genocide or extinction) */
/* mndx: particular species that can no longer be created */
void
reset_rndmonst(int mndx)
{
    /* cached selection info is out of date */
    if (mndx == NON_PM) {
        rndmonst_state.choice_count = -1;       /* full recalc needed */
    } else if (mndx < SPECIAL_PM) {
        rndmonst_state.choice_count -= rndmonst_state.mchoices[mndx];
        rndmonst_state.mchoices[mndx] = 0;
    }   /* note: safe to ignore extinction of unique monsters */
}


void
save_rndmonst_state(struct memfile *mf)
{
    mtag(mf, 0, MTAG_RNDMONST);
    mwrite32(mf, rndmonst_state.choice_count);
    mwrite(mf, rndmonst_state.mchoices, sizeof (rndmonst_state.mchoices));
}


void
restore_rndmonst_state(struct memfile *mf)
{
    rndmonst_state.choice_count = mread32(mf);
    mread(mf, rndmonst_state.mchoices, sizeof (rndmonst_state.mchoices));
}


/* The routine below is used to make one of the multiple types
 * of a given monster class.  The second parameter specifies a
 * special casing bit mask to allow the normal genesis
 * masks to be deactivated.  Returns 0 if no monsters
 * in that class can be made.
 */

const struct permonst *
mkclass(const d_level * dlev, char class, int spc)
{
    int first, last, num = 0;
    int maxmlev, mask = (G_NOGEN | G_UNIQ) & ~spc;

    maxmlev = level_difficulty(dlev) >> 1;
    if (class < 1 || class >= MAXMCLASSES) {
        impossible("mkclass called with bad class!");
        return NULL;
    }
/* Assumption #1:  monsters of a given class are contiguous in the
 *                 mons[] array.
 */
    for (first = LOW_PM; first < SPECIAL_PM; first++)
        if (mons[first].mlet == class)
            break;
    if (first == SPECIAL_PM)
        return NULL;

    for (last = first; last < SPECIAL_PM && mons[last].mlet == class; last++)
        if (!(mvitals[last].mvflags & G_GONE) && !(mons[last].geno & mask)
            && !is_placeholder(&mons[last])) {
            /* consider it */
            if (num && toostrong(last, maxmlev) &&
                monstr[last] != monstr[last - 1] && rn2(2))
                break;
            num += mons[last].geno & G_FREQ;
        }

    if (!num)
        return NULL;

/* Assumption #2:  monsters of a given class are presented in ascending
 *                 order of strength.
 */
    for (num = rnd(num); num > 0; first++)
        if (!(mvitals[first].mvflags & G_GONE) && !(mons[first].geno & mask)
            && !is_placeholder(&mons[first])) {
            /* skew towards lower value monsters at lower exp. levels */
            num -= mons[first].geno & G_FREQ;
            if (num &&
                adj_lev(dlev, &mons[first]) > (level_difficulty(dlev) * 2)) {
                /* but not when multiple monsters are same level */
                if (mons[first].mlevel != mons[first + 1].mlevel)
                    num--;
            }
        }
    first--;    /* correct an off-by-one error */

    return &mons[first];
}

/* adjust strength of monsters based on depth */
int
adj_lev(const d_level * dlev, const struct permonst *ptr)
{
    int tmp, tmp2;

    if (ptr == &mons[PM_WIZARD_OF_YENDOR]) {
        /* does not depend on other strengths, but does get stronger every time 
           he is killed */
        tmp = ptr->mlevel + mvitals[PM_WIZARD_OF_YENDOR].died;
        if (tmp > 49)
            tmp = 49;
        return tmp;
    }

    tmp = ptr->mlevel;
    if (ptr->mlevel > 49)
        return 50;      /* "special" demons/devils */
    tmp2 = (level_difficulty(dlev) - ptr->mlevel);
    if (tmp2 < 0)
        tmp--;  /* if mlevel > u.uz decrement tmp */
    else
        tmp += (tmp2 / 5);      /* else increment 1 per five diff */

    tmp2 = (3 * ptr->mlevel) / 2;       /* crude upper limit */
    if (tmp2 > 49)
        tmp2 = 49;      /* hard upper limit */
    return (tmp > tmp2) ? tmp2 : (tmp > 0 ? tmp : 0);   /* 0 lower limit */
}


const struct permonst *
grow_up(struct monst *mtmp,     /* `mtmp' might "grow up" into a bigger version 
                                 */
        struct monst *victim)
{
    int oldtype, newtype, max_increase, cur_increase, lev_limit, hp_threshold;
    const struct permonst *ptr = mtmp->data;

    /* monster died after killing enemy but before calling this function */
    /* currently possible if killing a gas spore */
    if (mtmp->mhp <= 0)
        return NULL;

    /* note: none of the monsters with special hit point calculations have both 
       little and big forms */
    oldtype = monsndx(ptr);
    newtype = little_to_big(oldtype);
    if (newtype == PM_PRIEST && mtmp->female)
        newtype = PM_PRIESTESS;

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
                pline("As %s grows up into %s, %s %s!", mon_nam(mtmp),
                      an(ptr->mname), mhe(mtmp),
                      nonliving(ptr) ? "expires" : "dies");
            set_mon_data(mtmp, ptr, -1);        /* keep mvitals[] accurate */
            mondied(mtmp);
            return NULL;
        }
        set_mon_data(mtmp, ptr, 1);     /* preserve intrinsics */
        newsym(mtmp->mx, mtmp->my);     /* color may change */
        lev_limit = (int)mtmp->m_lev;   /* never undo increment */
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


int
mongets(struct monst *mtmp, int otyp)
{
    struct obj *otmp;
    int spe;

    if (!otyp)
        return 0;
    otmp = mksobj(level, otyp, TRUE, FALSE);
    if (otmp) {
        if (mtmp->data->mlet == S_DEMON) {
            /* demons never get blessed objects */
            if (otmp->blessed)
                curse(otmp);
        } else if (is_lminion(mtmp)) {
            /* lawful minions don't get cursed, bad, or rusting objects */
            otmp->cursed = FALSE;
            if (otmp->spe < 0)
                otmp->spe = 0;
            otmp->oerodeproof = TRUE;
        } else if (is_mplayer(mtmp->data) && is_sword(otmp)) {
            otmp->spe = (3 + rn2(4));
        }

        if (otmp->otyp == CANDELABRUM_OF_INVOCATION) {
            otmp->spe = 0;
            otmp->age = 0L;
            otmp->lamplit = FALSE;
            otmp->blessed = otmp->cursed = FALSE;
        } else if (otmp->otyp == BELL_OF_OPENING) {
            otmp->blessed = otmp->cursed = FALSE;
        } else if (otmp->otyp == SPE_BOOK_OF_THE_DEAD) {
            otmp->blessed = FALSE;
            otmp->cursed = TRUE;
        }

        /* leaders don't tolerate inferior quality battle gear */
        if (is_prince(mtmp->data)) {
            if (otmp->oclass == WEAPON_CLASS && otmp->spe < 1)
                otmp->spe = 1;
            else if (otmp->oclass == ARMOR_CLASS && otmp->spe < 0)
                otmp->spe = 0;
        }

        spe = otmp->spe;
        mpickobj(mtmp, otmp);   /* might free otmp */
        return spe;
    } else
        return 0;
}


int
golemhp(int type)
{
    switch (type) {
    case PM_STRAW_GOLEM:
        return 20;
    case PM_PAPER_GOLEM:
        return 20;
    case PM_ROPE_GOLEM:
        return 30;
    case PM_LEATHER_GOLEM:
        return 40;
    case PM_GOLD_GOLEM:
        return 40;
    case PM_WOOD_GOLEM:
        return 50;
    case PM_FLESH_GOLEM:
        return 40;
    case PM_CLAY_GOLEM:
        return 50;
    case PM_STONE_GOLEM:
        return 60;
    case PM_GLASS_GOLEM:
        return 60;
    case PM_IRON_GOLEM:
        return 80;
    default:
        return 0;
    }
}


/*
 * Alignment vs. yours determines monster's attitude to you.
 * ( some "animal" types are co-aligned, but also hungry )
 */
boolean
peace_minded(const struct permonst * ptr)
{
    aligntyp mal = ptr->maligntyp, ual = u.ualign.type;

    if (always_peaceful(ptr))
        return TRUE;
    if (always_hostile(ptr))
        return FALSE;
    if (ptr->msound == MS_LEADER || ptr->msound == MS_GUARDIAN)
        return TRUE;
    if (ptr->msound == MS_NEMESIS)
        return FALSE;

    if (race_peaceful(ptr))
        return TRUE;
    if (race_hostile(ptr))
        return FALSE;

    /* the monster is hostile if its alignment is different from the player's */
    if (sgn(mal) != sgn(ual))
        return FALSE;

    /* Negative monster hostile to player with Amulet. */
    if (mal < A_NEUTRAL && u.uhave.amulet)
        return FALSE;

    /* minions are hostile to players that have strayed at all */
    if (is_minion(ptr))
        return (boolean) (u.ualign.record >= 0);

    /* Last case: a chance of a co-aligned monster being hostile.  This chance
       is greater if the player has strayed (u.ualign.record negative) or the
       monster is not strongly aligned. */
    return ((boolean)
            (! !rn2(16 + (u.ualign.record < -15 ? -15 : u.ualign.record)) &&
             ! !rn2(2 + abs(mal))));
}

/* Set malign to have the proper effect on player alignment if monster is
 * killed.  Negative numbers mean it's bad to kill this monster; positive
 * numbers mean it's good.  Since there are more hostile monsters than
 * peaceful monsters, the penalty for killing a peaceful monster should be
 * greater than the bonus for killing a hostile monster to maintain balance.
 * Rules:
 *   it's bad to kill peaceful monsters, potentially worse to kill always-
 *      peaceful monsters
 *   it's never bad to kill a hostile monster, although it may not be good
 */
void
set_malign(struct monst *mtmp)
{
    schar mal = mtmp->data->maligntyp;
    boolean coaligned;

    if (mtmp->ispriest || mtmp->isminion) {
        /* some monsters have individual alignments; check them */
        if (mtmp->ispriest || (mtmp->isminion && roamer_type(mtmp->data)))
            mal = EPRI(mtmp)->shralign;
        else if (mtmp->isminion)
            mal = EMIN(mtmp)->min_align;
        /* unless alignment is none, set mal to -5,0,5 */
        /* (see align.h for valid aligntyp values) */
        if (mal != A_NONE)
            mal *= 5;
    }

    coaligned = (sgn(mal) == sgn(u.ualign.type));
    if (mtmp->data->msound == MS_LEADER) {
        mtmp->malign = -20;
    } else if (mal == A_NONE) {
        if (mtmp->mpeaceful)
            mtmp->malign = 0;
        else
            mtmp->malign = 20;  /* really hostile */
    } else if (always_peaceful(mtmp->data)) {
        int absmal = abs(mal);

        if (mtmp->mpeaceful)
            mtmp->malign = -3 * max(5, absmal);
        else
            mtmp->malign = 3 * max(5, absmal);  /* renegade */
    } else if (always_hostile(mtmp->data)) {
        int absmal = abs(mal);

        if (coaligned)
            mtmp->malign = 0;
        else
            mtmp->malign = max(5, absmal);
    } else if (coaligned) {
        int absmal = abs(mal);

        if (mtmp->mpeaceful)
            mtmp->malign = -3 * max(3, absmal);
        else    /* renegade */
            mtmp->malign = max(3, absmal);
    } else      /* not coaligned and therefore hostile */
        mtmp->malign = abs(mal);
}


static const char syms[] = {
    MAXOCLASSES, MAXOCLASSES + 1, RING_CLASS, WAND_CLASS, WEAPON_CLASS,
    FOOD_CLASS, COIN_CLASS, SCROLL_CLASS, POTION_CLASS, ARMOR_CLASS,
    AMULET_CLASS, TOOL_CLASS, ROCK_CLASS, GEM_CLASS, SPBOOK_CLASS,
    S_MIMIC_DEF, S_MIMIC_DEF, S_MIMIC_DEF,
};

void
set_mimic_sym(struct monst *mtmp, struct level *lev)
{
    int typ, roomno, rt;
    unsigned appear, ap_type;
    int s_sym;
    struct obj *otmp;
    struct trap *tt;
    int mx, my;
    boolean above_pit = FALSE;

    if (!mtmp)
        return;
    mx = mtmp->mx;
    my = mtmp->my;
    tt = t_at(lev, mx, my);
    above_pit = !(!tt ||
                  ((tt->ttyp == PIT) || (tt->ttyp == SPIKED_PIT) ||
                   (tt->ttyp == HOLE) || (tt->ttyp == TRAPDOOR)));
    typ = lev->locations[mx][my].typ;
    /* only valid for INSIDE of room */
    roomno = lev->locations[mx][my].roomno - ROOMOFFSET;
    if (roomno >= 0)
        rt = lev->rooms[roomno].rtype;
    else
        rt = 0; /* roomno < 0 case */

    if (OBJ_AT_LEV(lev, mx, my)) {
        ap_type = M_AP_OBJECT;
        appear = lev->objects[mx][my]->otyp;
    } else if (IS_DOOR(typ) || IS_WALL(typ) || typ == SDOOR || typ == SCORR) {
        ap_type = M_AP_FURNITURE;
        if (Is_rogue_level(&lev->z))
            appear = S_ndoor;
        /* 
         *  If there is a wall to the left that connects to this
         *  location, then the mimic mimics a horizontal closed door.
         *  This does not allow doors to be in corners of rooms.
         */
        else if (mx != 0 &&
                 (lev->locations[mx - 1][my].typ == HWALL ||
                  lev->locations[mx - 1][my].typ == TLCORNER ||
                  lev->locations[mx - 1][my].typ == TRWALL ||
                  lev->locations[mx - 1][my].typ == BLCORNER ||
                  lev->locations[mx - 1][my].typ == TDWALL ||
                  lev->locations[mx - 1][my].typ == CROSSWALL ||
                  lev->locations[mx - 1][my].typ == TUWALL))
            appear = S_hcdoor;
        else
            appear = S_vcdoor;

        if (!mtmp->minvis || See_invisible)
            block_point(mx, my);        /* vision */
    } else if (lev->flags.is_maze_lev && !above_pit && rn2(2)) {
        ap_type = M_AP_OBJECT;
        appear = STATUE;
    } else if ((roomno < 0) && !above_pit) {
        ap_type = M_AP_OBJECT;
        appear = BOULDER;
        if (!mtmp->minvis || See_invisible)
            block_point(mx, my);        /* vision */
    } else if (rt == ZOO || rt == VAULT) {
        ap_type = M_AP_OBJECT;
        appear = GOLD_PIECE;
    } else if ((rt == DELPHI) && !above_pit) {
        if (rn2(2)) {
            ap_type = M_AP_OBJECT;
            appear = STATUE;
        } else {
            ap_type = M_AP_FURNITURE;
            appear = S_fountain;
        }
    } else if (rt == TEMPLE) {
        ap_type = M_AP_FURNITURE;
        appear = S_altar;
        /* 
         * We won't bother with beehives, morgues, barracks, throne rooms
         * since they shouldn't contain too many mimics anyway...
         */
    } else if (rt >= SHOPBASE) {
        s_sym = get_shop_item(rt - SHOPBASE);
        if (s_sym < 0) {
            ap_type = M_AP_OBJECT;
            appear = -s_sym;
        } else {
            if (s_sym == RANDOM_CLASS)
                s_sym = syms[rn2((int)sizeof (syms) - 2) + 2];
            goto assign_sym;
        }
    } else
        while (1) {
            s_sym = syms[rn2((int)sizeof (syms))];
        assign_sym:
            if ((s_sym >= MAXOCLASSES) && !above_pit) {
                ap_type = M_AP_FURNITURE;
                appear = s_sym == MAXOCLASSES ? S_upstair : S_dnstair;
                break;
            } else if (s_sym == COIN_CLASS) {
                ap_type = M_AP_OBJECT;
                appear = GOLD_PIECE;
                break;
            } else if (s_sym < MAXOCLASSES) {
                ap_type = M_AP_OBJECT;
                if (s_sym == S_MIMIC_DEF) {
                    appear = STRANGE_OBJECT;
                } else {
                    otmp = mkobj(lev, (char)s_sym, FALSE);
                    appear = otmp->otyp;
                    /* make sure container contents are free'ed */
                    obfree(otmp, NULL);
                }
                break;
            }
        }
    mtmp->m_ap_type = ap_type;
    mtmp->mappearance = appear;
}

/* release a monster from a bag of tricks */
void
bagotricks(struct obj *bag)
{
    if (!bag || bag->otyp != BAG_OF_TRICKS) {
        impossible("bad bag o' tricks");
    } else if (bag->spe < 1) {
        pline("Nothing happens.");
    } else {
        boolean gotone = FALSE;
        int cnt = 1;

        consume_obj_charge(bag, TRUE);

        if (!rn2(23))
            cnt += rn1(7, 1);
        while (cnt-- > 0) {
            if (makemon(NULL, level, u.ux, u.uy, NO_MM_FLAGS))
                gotone = TRUE;
        }
        if (gotone)
            makeknown(BAG_OF_TRICKS);
    }
}


static void
restore_shkbill(struct memfile *mf, struct bill_x *b)
{
    b->bo_id = mread32(mf);
    b->price = mread32(mf);
    b->bquan = mread32(mf);
    b->useup = mread8(mf);
}


static void
restore_fcorr(struct memfile *mf, struct fakecorridor *f)
{
    f->fx = mread8(mf);
    f->fy = mread8(mf);
    f->ftyp = mread8(mf);
}


struct monst *
restore_mon(struct memfile *mf)
{
    struct monst *mon;
    short namelen, xtyp;
    int idx, i, billid;
    unsigned int mflags;
    struct eshk *shk;

    mfmagic_check(mf, MON_MAGIC);       /* 4 */

    namelen = mread16(mf);      /* 6 */
    xtyp = mread16(mf); /* 8 */
    mon = newmonst(xtyp, namelen);

    idx = mread32(mf);  /* 12 */
    switch (idx) {
    case -1000:
        mon->data = &pm_leader;
        break;
    case -2000:
        mon->data = &pm_guardian;
        break;
    case -3000:
        mon->data = &pm_nemesis;
        break;
    case -4000:
        mon->data = &pm_you_male;
        break;
    case -5000:
        mon->data = &pm_you_female;
        break;
    case -6000:
        mon->data = NULL;
        break;
    default:
        if (LOW_PM <= idx && idx < NUMMONS)
            mon->data = &mons[idx];
        else
            panic("Restoring bad monster data.");
        break;
    }

    mon->m_id = mread32(mf);    /* 16 */
    mon->mhp = mread32(mf);     /* 20 */
    mon->mhpmax = mread32(mf);  /* 24 */
    mon->mspec_used = mread32(mf);      /* 28 */
    mon->mtrapseen = mread32(mf);       /* 32 */
    mon->mlstmv = mread32(mf);  /* 36 */
    mon->mstrategy = mread32(mf);       /* 40 */
    mon->meating = mread32(mf); /* 44 */
    mread(mf, mon->mtrack, sizeof (mon->mtrack));       /* 52 */
    mon->mnum = mread16(mf);    /* 54 */
    mon->mx = mread8(mf);       /* 55 */
    mon->my = mread8(mf);       /* 56 */
    mon->mux = mread8(mf);      /* 57 */
    mon->muy = mread8(mf);      /* 58 */
    mon->m_lev = mread8(mf);    /* 59 */
    mon->malign = mread8(mf);   /* 60 */
    mon->movement = mread16(mf);        /* 62 */
    mon->mintrinsics = mread16(mf);     /* 64 */
    mon->mtame = mread8(mf);    /* 65 */
    mon->m_ap_type = mread8(mf);        /* 66 */
    mon->mfrozen = mread8(mf);  /* 67 */
    mon->mblinded = mread8(mf); /* 68 */
    mon->mappearance = mread32(mf);     /* 72 */
    mflags = mread32(mf);       /* 76 */

    mon->mfleetim = mread8(mf); /* 77 */
    mon->weapon_check = mread8(mf);     /* 78 */
    mon->misc_worn_check = mread32(mf); /* 82 */
    mon->wormno = mread8(mf);   /* 83 */

    /* just mark the pointers for later restoration */
    mon->minvent = mread8(mf) ? (void *)1 : NULL;       /* 84 */
    mon->mw = mread8(mf) ? (void *)1 : NULL;    /* 85 */

    if (mon->mnamelth)
        mread(mf, NAME(mon), mon->mnamelth);

    switch (mon->mxtyp) {
    case MX_EPRI:
        EPRI(mon)->shralign = mread8(mf);
        EPRI(mon)->shroom = mread8(mf);
        EPRI(mon)->shrpos.x = mread8(mf);
        EPRI(mon)->shrpos.y = mread8(mf);
        EPRI(mon)->shrlevel.dnum = mread8(mf);
        EPRI(mon)->shrlevel.dlevel = mread8(mf);
        break;

    case MX_EMIN:
        EMIN(mon)->min_align = mread8(mf);
        break;

    case MX_EDOG:
        EDOG(mon)->droptime = mread32(mf);
        EDOG(mon)->dropdist = mread32(mf);
        EDOG(mon)->apport = mread32(mf);
        EDOG(mon)->whistletime = mread32(mf);
        EDOG(mon)->hungrytime = mread32(mf);
        EDOG(mon)->abuse = mread32(mf);
        EDOG(mon)->revivals = mread32(mf);
        EDOG(mon)->mhpmax_penalty = mread32(mf);
        EDOG(mon)->ogoal.x = mread8(mf);
        EDOG(mon)->ogoal.y = mread8(mf);
        EDOG(mon)->killed_by_u = mread8(mf);
        break;

    case MX_ESHK:
        shk = ESHK(mon);
        billid = mread32(mf);
        shk->bill_p =
            (billid == -1000) ? (struct bill_x *)-1000 :
            (billid == -2000) ? NULL : &shk-> bill[billid];
        shk->shk.x = mread8(mf);
        shk->shk.y = mread8(mf);
        shk->shd.x = mread8(mf);
        shk->shd.y = mread8(mf);
        shk->robbed = mread32(mf);
        shk->credit = mread32(mf);
        shk->debit = mread32(mf);
        shk->loan = mread32(mf);
        shk->shoptype = mread16(mf);
        shk->billct = mread16(mf);
        shk->visitct = mread16(mf);
        shk->shoplevel.dnum = mread8(mf);
        shk->shoplevel.dlevel = mread8(mf);
        shk->shoproom = mread8(mf);
        shk->following = mread8(mf);
        shk->surcharge = mread8(mf);
        mread(mf, shk->customer, sizeof (shk->customer));
        mread(mf, shk->shknam, sizeof (shk->shknam));
        for (i = 0; i < BILLSZ; i++)
            restore_shkbill(mf, &shk->bill[i]);
        break;

    case MX_EGD:
        EGD(mon)->fcbeg = mread32(mf);
        EGD(mon)->fcend = mread32(mf);
        EGD(mon)->vroom = mread32(mf);
        EGD(mon)->gdx = mread8(mf);
        EGD(mon)->gdy = mread8(mf);
        EGD(mon)->ogx = mread8(mf);
        EGD(mon)->ogy = mread8(mf);
        EGD(mon)->gdlevel.dnum = mread8(mf);
        EGD(mon)->gdlevel.dlevel = mread8(mf);
        EGD(mon)->warncnt = mread8(mf);
        EGD(mon)->gddone = mread8(mf);
        for (i = 0; i < FCSIZ; i++)
            restore_fcorr(mf, &EGD(mon)->fakecorr[i]);
        break;
    }

    mon->female = (mflags >> 31) & 1;
    mon->minvis = (mflags >> 30) & 1;
    mon->invis_blkd = (mflags >> 29) & 1;
    mon->perminvis = (mflags >> 28) & 1;
    mon->cham = (mflags >> 25) & 7;
    mon->mundetected = (mflags >> 24) & 1;
    mon->mcan = (mflags >> 23) & 1;
    mon->mburied = (mflags >> 22) & 1;
    mon->mspeed = (mflags >> 20) & 3;
    mon->permspeed = (mflags >> 18) & 3;
    mon->mrevived = (mflags >> 17) & 1;
    mon->mavenge = (mflags >> 16) & 1;
    mon->mflee = (mflags >> 15) & 1;
    mon->mcansee = (mflags >> 14) & 1;
    mon->mcanmove = (mflags >> 13) & 1;
    mon->msleeping = (mflags >> 12) & 1;
    mon->mstun = (mflags >> 11) & 1;
    mon->mconf = (mflags >> 10) & 1;
    mon->mpeaceful = (mflags >> 9) & 1;
    mon->mtrapped = (mflags >> 8) & 1;
    mon->mleashed = (mflags >> 7) & 1;
    mon->isshk = (mflags >> 6) & 1;
    mon->isminion = (mflags >> 5) & 1;
    mon->isgd = (mflags >> 4) & 1;
    mon->ispriest = (mflags >> 3) & 1;
    mon->iswiz = (mflags >> 2) & 1;

    return mon;
}


static void
save_shkbill(struct memfile *mf, const struct bill_x *b)
{
    /* no mtag needed; saved as part of a particular shk's data */
    mwrite32(mf, b->bo_id);
    mwrite32(mf, b->price);
    mwrite32(mf, b->bquan);
    mwrite8(mf, b->useup);
}


static void
save_fcorr(struct memfile *mf, const struct fakecorridor *f)
{
    /* no mtag needed; saved as part of a particular guard's data */
    mwrite8(mf, f->fx);
    mwrite8(mf, f->fy);
    mwrite8(mf, f->ftyp);
}


void
save_mon(struct memfile *mf, const struct monst *mon)
{
    int idx, i;
    unsigned int mflags;
    struct eshk *shk;

    mtag(mf, mon->m_id, MTAG_MON);
    mfmagic_set(mf, MON_MAGIC);

    mwrite16(mf, mon->mnamelth);
    mwrite16(mf, mon->mxtyp);

    /* mon->data is null for monst structs that are attached to objects.
       mon->data is non-null but not in the mons array for customized monsters
       monsndx() does not handle these cases. */
    idx = mon->data - mons;
    idx = (LOW_PM <= idx && idx < NUMMONS) ? idx : 0;
    if (&mons[idx] != mon->data) {
        if (mon->data == &pm_leader)
            idx = -1000;
        else if (mon->data == &pm_guardian)
            idx = -2000;
        else if (mon->data == &pm_nemesis)
            idx = -3000;
        else if (mon->data == &pm_you_male)
            idx = -4000;
        else if (mon->data == &pm_you_female)
            idx = -5000;
        else if (mon->data == NULL)
            idx = -6000;
        else
            impossible("Bad monster type detected.");
    }
    mwrite32(mf, idx);
    mwrite32(mf, mon->m_id);
    mwrite32(mf, mon->mhp);
    mwrite32(mf, mon->mhpmax);
    mwrite32(mf, mon->mspec_used);
    mwrite32(mf, mon->mtrapseen);
    mwrite32(mf, mon->mlstmv);
    mwrite32(mf, mon->mstrategy);
    mwrite32(mf, mon->meating);
    mwrite(mf, mon->mtrack, sizeof (mon->mtrack));
    mwrite16(mf, mon->mnum);
    mwrite8(mf, mon->mx);
    mwrite8(mf, mon->my);
    mwrite8(mf, mon->mux);
    mwrite8(mf, mon->muy);
    mwrite8(mf, mon->m_lev);
    mwrite8(mf, mon->malign);
    mwrite16(mf, mon->movement);
    mwrite16(mf, mon->mintrinsics);
    mwrite8(mf, mon->mtame);
    mwrite8(mf, mon->m_ap_type);
    mwrite8(mf, mon->mfrozen);
    mwrite8(mf, mon->mblinded);
    mwrite32(mf, mon->mappearance);

    mflags =
        (mon->female << 31) | (mon->minvis << 30) |
        (mon->invis_blkd << 29) | (mon->perminvis << 28) |
        (mon->cham << 25) | (mon->mundetected << 24) |
        (mon->mcan << 23) | (mon->mburied << 22) |
        (mon->mspeed << 20) | (mon->permspeed << 18) |
        (mon->mrevived << 17) | (mon->mavenge << 16) |
        (mon->mflee << 15) | (mon->mcansee << 14) |
        (mon->mcanmove << 13) | (mon->msleeping << 12) |
        (mon->mstun << 11) | (mon->mconf << 10) | (mon->mpeaceful << 9) |
        (mon->mtrapped << 8) | (mon->mleashed << 7) | (mon->isshk << 6) |
        (mon->isminion << 5) | (mon->isgd << 4) |
        (mon->ispriest << 3) | (mon->iswiz << 2);
    mwrite32(mf, mflags);

    mwrite8(mf, mon->mfleetim);
    mwrite8(mf, mon->weapon_check);
    mwrite32(mf, mon->misc_worn_check);
    mwrite8(mf, mon->wormno);

    /* just mark that the pointers had values */
    mwrite8(mf, mon->minvent ? 1 : 0);
    mwrite8(mf, mon->mw ? 1 : 0);

    if (mon->mnamelth)
        mwrite(mf, NAME(mon), mon->mnamelth);

    switch (mon->mxtyp) {
    case MX_EPRI:
        mwrite8(mf, EPRI(mon)->shralign);
        mwrite8(mf, EPRI(mon)->shroom);
        mwrite8(mf, EPRI(mon)->shrpos.x);
        mwrite8(mf, EPRI(mon)->shrpos.y);
        mwrite8(mf, EPRI(mon)->shrlevel.dnum);
        mwrite8(mf, EPRI(mon)->shrlevel.dlevel);
        break;

    case MX_EMIN:
        mwrite8(mf, EMIN(mon)->min_align);
        break;

    case MX_EDOG:
        mwrite32(mf, EDOG(mon)->droptime);
        mwrite32(mf, EDOG(mon)->dropdist);
        mwrite32(mf, EDOG(mon)->apport);
        mwrite32(mf, EDOG(mon)->whistletime);
        mwrite32(mf, EDOG(mon)->hungrytime);
        mwrite32(mf, EDOG(mon)->abuse);
        mwrite32(mf, EDOG(mon)->revivals);
        mwrite32(mf, EDOG(mon)->mhpmax_penalty);
        mwrite8(mf, EDOG(mon)->ogoal.x);
        mwrite8(mf, EDOG(mon)->ogoal.y);
        mwrite8(mf, EDOG(mon)->killed_by_u);
        break;

    case MX_ESHK:
        shk = ESHK(mon);
        mwrite32(mf,
                 (shk->bill_p ==
                  (struct bill_x *)-1000) ? -1000 : !shk->
                 bill_p ? -2000 : (shk->bill_p - shk->bill));
        mwrite8(mf, shk->shk.x);
        mwrite8(mf, shk->shk.y);
        mwrite8(mf, shk->shd.x);
        mwrite8(mf, shk->shd.y);
        mwrite32(mf, shk->robbed);
        mwrite32(mf, shk->credit);
        mwrite32(mf, shk->debit);
        mwrite32(mf, shk->loan);
        mwrite16(mf, shk->shoptype);
        mwrite16(mf, shk->billct);
        mwrite16(mf, shk->visitct);
        mwrite8(mf, shk->shoplevel.dnum);
        mwrite8(mf, shk->shoplevel.dlevel);
        mwrite8(mf, shk->shoproom);
        mwrite8(mf, shk->following);
        mwrite8(mf, shk->surcharge);
        mwrite(mf, shk->customer, sizeof (shk->customer));
        mwrite(mf, shk->shknam, sizeof (shk->shknam));
        for (i = 0; i < BILLSZ; i++)
            save_shkbill(mf, &shk->bill[i]);
        break;

    case MX_EGD:
        mwrite32(mf, EGD(mon)->fcbeg);
        mwrite32(mf, EGD(mon)->fcend);
        mwrite32(mf, EGD(mon)->vroom);
        mwrite8(mf, EGD(mon)->gdx);
        mwrite8(mf, EGD(mon)->gdy);
        mwrite8(mf, EGD(mon)->ogx);
        mwrite8(mf, EGD(mon)->ogy);
        mwrite8(mf, EGD(mon)->gdlevel.dnum);
        mwrite8(mf, EGD(mon)->gdlevel.dlevel);
        mwrite8(mf, EGD(mon)->warncnt);
        mwrite8(mf, EGD(mon)->gddone);
        for (i = 0; i < FCSIZ; i++)
            save_fcorr(mf, &EGD(mon)->fakecorr[i]);
        break;
    }
}

/*makemon.c*/
