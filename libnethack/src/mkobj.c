/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "prop.h"

#include <limits.h>

static void mkbox_cnts(struct obj *, enum rng rng);
static struct obj *mksobj_basic(struct level *lev, int otyp);
static void obj_timer_checks(struct obj *, xchar, xchar, int);
static void container_weight(struct obj *);
static struct obj *save_mtraits(struct obj *, struct monst *);
static void extract_nexthere(struct obj *, struct obj **);

/* #define DEBUG_EFFECTS *//* show some messages for debugging */

struct icp {
    int iprob;  /* probability of an item type */
    char iclass;        /* item class */
};


static const struct icp mkobjprobs[] = {
    {10, WEAPON_CLASS},
    {10, ARMOR_CLASS},
    {20, FOOD_CLASS},
    {8, TOOL_CLASS},
    {8, GEM_CLASS},
    {16, POTION_CLASS},
    {16, SCROLL_CLASS},
    {4, SPBOOK_CLASS},
    {4, WAND_CLASS},
    {3, RING_CLASS},
    {1, AMULET_CLASS}
};

static const struct icp boxiprobs[] = {
    {18, GEM_CLASS},
    {15, FOOD_CLASS},
    {18, POTION_CLASS},
    {18, SCROLL_CLASS},
    {12, SPBOOK_CLASS},
    {7, COIN_CLASS},
    {6, WAND_CLASS},
    {5, RING_CLASS},
    {1, AMULET_CLASS}
};

static const struct icp rogueprobs[] = {
    {12, WEAPON_CLASS},
    {12, ARMOR_CLASS},
    {22, FOOD_CLASS},
    {22, POTION_CLASS},
    {22, SCROLL_CLASS},
    {5, WAND_CLASS},
    {5, RING_CLASS}
};

static const struct icp hellprobs[] = {
    {20, WEAPON_CLASS},
    {20, ARMOR_CLASS},
    {16, FOOD_CLASS},
    {12, TOOL_CLASS},
    {10, GEM_CLASS},
    {1, POTION_CLASS},
    {1, SCROLL_CLASS},
    {8, WAND_CLASS},
    {8, RING_CLASS},
    {4, AMULET_CLASS}
};

unsigned
next_ident(void)
{
    if (flags.ident == UINT_MAX)
        flags.ident = FIRST_PERMANENT_IDENT;
    return flags.ident++;
}

struct obj *
mkobj_at(char let, struct level *lev, int x, int y, boolean artif, enum rng rng)
{
    struct obj *otmp;

    otmp = mkobj(lev, let, artif, rng);
    place_object(otmp, lev, x, y);
    return otmp;
}

struct obj *
mksobj_at(int otyp, struct level *lev, int x, int y, boolean init,
          boolean artif, enum rng rng)
{
    struct obj *otmp;

    otmp = mksobj(lev, otyp, init, artif, rng);
    place_object(otmp, lev, x, y);
    return otmp;
}

struct obj *
mkobj(struct level *lev, char oclass, boolean artif, enum rng rng)
{
    int tprob;

    if (oclass == RANDOM_CLASS) {
        const struct icp *iprobs =
            (Is_rogue_level(&lev->z)) ? (const struct icp *)rogueprobs :
            In_hell(&lev->z) ? (const struct icp *)hellprobs :
            (const struct icp *) mkobjprobs;

        for (tprob = rn2_on_rng(100, rng) + 1;
             (tprob -= iprobs->iprob) > 0; iprobs++)
            ;
        oclass = iprobs->iclass;
    }

    return mkobj_of_class(lev, oclass, artif, rng);
}

struct obj *
mkobj_of_class(struct level *lev, char oclass, boolean artif, enum rng rng)
{
    int i, prob = 1 + rn2_on_rng(1000, rng);

    if (oclass == RANDOM_CLASS) {
        impossible("mkobj_of_class called with RANDOM_CLASS");
    }

    if (oclass == GEM_CLASS) {
        int gems = LAST_GEM - bases[GEM_CLASS] + 1, num = gems, total;
        int z = ledger_no(&lev->z), removed = 0;
        int j;

        for (j = 0; objects[j + bases[GEM_CLASS]].oc_class == GEM_CLASS; ++j)
            ;

        total = j;

        int probs[total];

        for (j = 0; j < 9 - z / 3; ++j) {
            if (j >= gems)
                panic("Not enough gems: on dlevel %d needed %d gems.", z, j);

            removed += objects[bases[GEM_CLASS] + j].oc_prob;
            probs[j] = 0;
            --num;
        }

        for (; j < gems; ++j)
            /* Here we redistribute the probability removed. The +1 at the end
             * is to ensure that we don't round to a sum less than 1000: going
             * over is ok, but going under might cause a panic. */
            probs[j] = ((objects[j + bases[GEM_CLASS]].oc_prob * num + removed)
                         / num) + 1;

        for (; j < total; ++j)
            probs[j] = objects[j + bases[GEM_CLASS]].oc_prob;

        j = 0;
        while ((prob -= probs[j]) > 0)
            j++;

        i = j + bases[GEM_CLASS];
    } else {
        i = bases[(int)oclass];
        while ((prob -= objects[i].oc_prob) > 0)
            i++;
    }

    if (objects[i].oc_class != oclass || !OBJ_NAME(objects[i]))
        panic("probtype error, oclass=%d i=%d", (int)oclass, i);

    return mksobj(lev, i, TRUE, artif, rng);
}

static void
mkbox_cnts(struct obj *box, enum rng rng)
{
    int n;
    struct obj *otmp;

    box->cobj = NULL;

    switch (box->otyp) {
    case ICE_BOX:
        n = 20;
        break;
    case CHEST:
        n = 5;
        break;
    case LARGE_BOX:
        n = 3;
        break;
    case SACK:
    case OILSKIN_SACK:
        /* initial inventory: sack starts out empty */
        if (moves <= 1 && !in_mklev) {
            n = 0;
            break;
        }
        /* else FALLTHRU */
    case BAG_OF_HOLDING:
        n = 1;
        break;
    default:
        n = 0;
        break;
    }

    for (n = rn2_on_rng(n + 1, rng); n > 0; n--) {
        if (box->otyp == ICE_BOX) {
            if (!(otmp = mksobj(box->olev, CORPSE, TRUE, TRUE, rng)))
                continue;
            /* Note: setting age to 0 is correct.  Age has a different from
               usual meaning for objects stored in ice boxes. -KAA */
            otmp->age = 0L;
            if (otmp->timed) {
                stop_timer(otmp->olev, ROT_CORPSE, otmp);
                stop_timer(otmp->olev, REVIVE_MON, otmp);
            }
        } else {
            int tprob;
            const struct icp *iprobs = boxiprobs;

            for (tprob = 1 + rn2_on_rng(100, rng);
                 (tprob -= iprobs->iprob) > 0; iprobs++)
                ;
            if (!(otmp = mkobj(box->olev, iprobs->iclass, TRUE, rng)))
                continue;

            /* handle a couple of special cases */
            if (otmp->oclass == COIN_CLASS) {
                /* 2.5 x level's usual amount; weight adjusted below */
                otmp->quan =
                    (1 + rn2_on_rng(level_difficulty(&box->olev->z) + 2, rng)) *
                    (1 + rn2_on_rng(75, rng));
                otmp->owt = weight(otmp);
            } else
                while (otmp->otyp == ROCK) {
                    otmp->otyp = rnd_class(DILITHIUM_CRYSTAL, LOADSTONE, rng);
                    if (otmp->quan > 2L)
                        otmp->quan = 1L;
                    otmp->owt = weight(otmp);
                }
            if (box->otyp == BAG_OF_HOLDING) {
                if (Is_mbag(otmp)) {
                    otmp->otyp = SACK;
                    otmp->spe = 0;
                    otmp->owt = weight(otmp);
                } else
                    while (otmp->otyp == WAN_CANCELLATION)
                        otmp->otyp = rnd_class(WAN_LIGHT, WAN_LIGHTNING, rng);
            }
        }
        add_to_container(box, otmp);
    }
}

/* select a random, common monster type */
int
rndmonnum(const d_level *dlev, enum rng rng)
{
    const struct permonst *ptr;
    int i;

    /* Plan A: get a level-appropriate common monster */
    ptr = rndmonst(dlev, rng);
    if (ptr)
        return monsndx(ptr);

    /* Plan B: get any common monster */
    do {
        i = LOW_PM + rn2_on_rng(SPECIAL_PM - LOW_PM, rng);
        ptr = &mons[i];
    } while ((ptr->geno & G_NOGEN) ||
             (!In_hell(dlev) && (ptr->geno & G_HELL)));

    return i;
}

/*
 * Split obj so that it gets size gets reduced by num. The quantity num is
 * put in the object structure delivered by this call.  The returned object
 * has its wornmask cleared and is positioned just following the original
 * in the nobj chain (and nexthere chain when on the floor).
 */
struct obj *
splitobj(struct obj *obj, long num)
{
    struct obj *otmp;

    if (obj->cobj || num <= 0L || obj->quan <= num)
        panic("splitobj");      /* can't split containers */
    otmp = newobj(obj->oxlth + obj->onamelth, obj);
    extract_nobj(otmp, &turnstate.floating_objects, NULL, 0);
    otmp->nobj = obj->nobj;
    obj->nobj = otmp;
    otmp->where = obj->where;
    otmp->o_id = next_ident();
    otmp->timed = 0;    /* not timed, yet */
    otmp->lamplit = 0;  /* ditto */
    otmp->owornmask = 0L;       /* new object isn't worn */
    obj->quan -= num;
    obj->owt = weight(obj);
    otmp->quan = num;
    otmp->owt = weight(otmp);   /* -= obj->owt ? */

    /* Only set nexthere when on the floor, nexthere is also used */
    /* as a back pointer to the container object when contained. */
    if (obj->where == OBJ_FLOOR)
        obj->nexthere = otmp;
    if (obj->oxlth)
        memcpy(otmp->oextra, (void *)obj->oextra, obj->oxlth);
    if (obj->onamelth)
        strncpy(ONAME_MUTABLE(otmp), ONAME(obj), (int)obj->onamelth);
    if (obj->unpaid)
        splitbill(obj, otmp);
    if (obj->timed)
        obj_split_timers(obj, otmp);
    if (obj_sheds_light(obj))
        obj_split_light_source(obj, otmp);
    return otmp;
}

/*
 * Insert otmp right after obj in whatever chain(s) it is on.  Then extract
 * obj from the chain(s).  This function does a literal swap.  It is up to
 * the caller to provide a valid context for the swap.  When done, obj will
 * still exist, but as a floating item. Before calling, otmp should be a
 * floating item.
 *
 * Note:  Don't use use obj_extract_self() -- we are doing an in-place swap,
 * not actually moving something.
 */
void
replace_object(struct obj *obj, struct obj *otmp)
{
    if (obj->where == OBJ_FREE)
        return; /* nothing to do */

    extract_nobj(otmp, &turnstate.floating_objects, NULL, 0);

    otmp->where = obj->where;
    switch (obj->where) {
    case OBJ_FREE:
        panic("extracting otmp from floating_objects freed obj");
        break;
    case OBJ_INVENT:
        otmp->nobj = obj->nobj;
        obj->nobj = otmp;
        extract_nobj(obj, &invent, &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_CONTAINED:
        otmp->nobj = obj->nobj;
        otmp->ocontainer = obj->ocontainer;
        obj->nobj = otmp;
        extract_nobj(obj, &obj->ocontainer->cobj,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_MINVENT:
        otmp->nobj = obj->nobj;
        otmp->ocarry = obj->ocarry;
        obj->nobj = otmp;
        extract_nobj(obj, &obj->ocarry->minvent,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_FLOOR:
        otmp->nobj = obj->nobj;
        otmp->nexthere = obj->nexthere;
        otmp->ox = obj->ox;
        otmp->oy = obj->oy;
        obj->nobj = otmp;
        obj->nexthere = otmp;
        extract_nobj(obj, &obj->olev->objlist,
                     &turnstate.floating_objects, OBJ_FREE);
        extract_nexthere(obj, &obj->olev->objects[obj->ox][obj->oy]);
        break;
    case OBJ_MIGRATING:
        otmp->nobj = obj->nobj;
        obj->nobj = otmp;
        extract_nobj(obj, &turnstate.migrating_objs,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    default:
        panic("replace_object: obj position");
        break;
    }
}

/*
 * Create a dummy duplicate to put on shop bill.  The duplicate exists
 * only in the billobjs chain.  This function is used when a shop object
 * is being altered, and a copy of the original is needed for billing
 * purposes.  For example, when eating, where an interruption will yield
 * an object which is different from what it started out as; the "I x"
 * command needs to display the original object.
 *
 * The caller is responsible for checking otmp->unpaid and
 * costly_spot(u.ux, u.uy).  This function will make otmp no charge.
 *
 * Note that check_unpaid_usage() should be used instead for partial
 * usage of an object.
 */
void
bill_dummy_object(struct obj *otmp)
{
    struct obj *dummy;

    if (otmp->unpaid)
        subfrombill(otmp, shop_keeper(level, *u.ushops));
    dummy = newobj(otmp->oxlth + otmp->onamelth, otmp);
    dummy->o_id = next_ident();
    dummy->timed = 0;
    if (otmp->oxlth)
        memcpy(dummy->oextra, otmp->oextra, otmp->oxlth);
    if (otmp->onamelth)
        strncpy(ONAME_MUTABLE(dummy), ONAME(otmp), (int)otmp->onamelth);
    if (Is_candle(dummy))
        dummy->lamplit = 0;
    addtobill(dummy, FALSE, TRUE, TRUE);
    otmp->no_charge = 1;
    otmp->unpaid = 0;
    return;
}


static const char dknowns[] = {
    WAND_CLASS, RING_CLASS, POTION_CLASS, SCROLL_CLASS,
    GEM_CLASS, SPBOOK_CLASS, WEAPON_CLASS, TOOL_CLASS, 0
};

static struct obj *
mksobj_basic(struct level *lev, int otyp)
{
    /* Just create the basic object, do not do anything that might be random,
       like beatitude, enchantment, etc. or disrupt game state. */
    struct obj *otmp;
    const char let = objects[otyp].oc_class;

    otmp = newobj(0, &zeroobj);
    otmp->age = moves;
    otmp->o_id = TEMPORARY_IDENT; /* temporary ID, no good for saving */
    otmp->quan = 1L;
    otmp->oclass = let;
    otmp->otyp = otyp;
    otmp->olev = lev;
    /* In most situations the following defaults (and in some cases a couple of
       the above ones also) will get overridden by mksobj, but there are a
       couple of cases where that won't happen, such as when we're creating a
       temporary object for display purposes.  So we go ahead here and
       initialize things to basic safe values...  */
    switch (otmp->otyp) {
    case CORPSE:
        otmp->corpsenm = PM_HUMAN;
        break;
    case EGG:
    case TIN:
    case FIGURINE:
    case STATUE:
        otmp->corpsenm = NON_PM;
        break;
    case SLIME_MOLD:
        otmp->spe = gamestate.fruits.current;
        break;
    }

    otmp->owt = weight(otmp);

    if ((otmp->otyp >= ELVEN_SHIELD && otmp->otyp <= ORCISH_SHIELD) ||
        otmp->otyp == SHIELD_OF_REFLECTION)
        otmp->dknown = 0;
    else
        otmp->dknown = strchr(dknowns, let) ? 0 : 1;

    if (!objects[otmp->otyp].oc_uses_known)
        otmp->known = 1;

    return otmp;
}

struct obj *
mksobj(struct level *lev, int otyp, boolean init, boolean artif, enum rng rng)
{
    int mndx, tryct;
    struct obj *otmp;
    const char let = objects[otyp].oc_class;

    otmp = mksobj_basic(lev, otyp);
    otmp->o_id = next_ident();

    if (init) {
#ifdef INVISIBLE_OBJECTS
        otmp->oinvis = !rn2_on_rng(1250, rng);
#endif
        switch (let) {
        case WEAPON_CLASS:
            otmp->quan = is_multigen(otmp) ? 6 + rn2_on_rng(6, rng) : 1;
            if (!rn2_on_rng(11, rng)) {
                otmp->spe = rne_on_rng(3, rng);
                otmp->blessed = rn2_on_rng(2, rng);
            } else if (!rn2_on_rng(10, rng)) {
                curse(otmp);
                otmp->spe = -rne_on_rng(3, rng);
            } else
                blessorcurse(otmp, 10, rng);
            if (is_poisonable(otmp) && !rn2_on_rng(100, rng))
                otmp->opoisoned = 1;

            if (artif && !rn2_on_rng(20, rng))
                otmp = mk_artifact(lev, otmp, (aligntyp) A_NONE, rng);
            break;
        case FOOD_CLASS:
            otmp->oeaten = 0;
            switch (otmp->otyp) {
            case CORPSE:
                /* possibly overridden by mkcorpstat() */
                tryct = 50;
                do
                    otmp->corpsenm = undead_to_corpse(rndmonnum(&lev->z, rng));
                while ((mvitals[otmp->corpsenm].mvflags & G_NOCORPSE) &&
                       (--tryct > 0));
                if (tryct == 0) {
                    /* perhaps rndmonnum() only wants to make G_NOCORPSE
                       monsters on this level; let's create an adventurer's
                       corpse instead, then */
                    otmp->corpsenm = PM_HUMAN;
                }
                /* timer set below */
                break;
            case EGG:
                otmp->corpsenm = NON_PM;        /* generic egg */
                if (!rn2_on_rng(3, rng))
                    for (tryct = 200; tryct > 0; --tryct) {
                        /* can't use the level RNG for this due to genocide */
                        mndx = can_be_hatched(
                            rndmonnum(&lev->z, rng_main));
                        if (mndx != NON_PM && !dead_species(mndx, TRUE)) {
                            otmp->corpsenm = mndx;      /* typed egg */
                            attach_egg_hatch_timeout(otmp);
                            break;
                        }
                    }
                break;
            case TIN:
                otmp->corpsenm = NON_PM;        /* empty (so far) */
                if (!rn2_on_rng(6, rng))
                    otmp->spe = 1;      /* spinach */
                else
                    for (tryct = 200; tryct > 0; --tryct) {
                        mndx = undead_to_corpse(rndmonnum(&lev->z, rng));
                        if (mons[mndx].cnutrit &&
                            !(mvitals[mndx].mvflags & G_NOCORPSE)) {
                            otmp->corpsenm = mndx;
                            break;
                        }
                    }
                blessorcurse(otmp, 10, rng);
                break;
            case SLIME_MOLD:
                otmp->spe = gamestate.fruits.current;
                break;
            case KELP_FROND:
                otmp->quan = 1 + rn2_on_rng(2, rng);
                break;
            }
            if (otmp->otyp == CORPSE || otmp->otyp == MEAT_RING ||
                otmp->otyp == KELP_FROND)
                break;
            /* fall into next case */

        case GEM_CLASS:
            if (otmp->otyp == LOADSTONE)
                curse(otmp);
            else if (otmp->otyp == ROCK)
                otmp->quan = 6 + rn2_on_rng(6, rng);
            else if (otmp->otyp != LUCKSTONE && !rn2_on_rng(6, rng))
                otmp->quan = 2L;
            else
                otmp->quan = 1L;
            break;
        case TOOL_CLASS:
            switch (otmp->otyp) {
            case TALLOW_CANDLE:
            case WAX_CANDLE:
                otmp->spe = 1;
                otmp->age = 20L *       /* 400 or 200 */
                    (long)objects[otmp->otyp].oc_cost;
                otmp->lamplit = 0;
                otmp->quan = 1 + (rn2_on_rng(2, rng) ? rn2_on_rng(7, rng) : 0);
                blessorcurse(otmp, 5, rng);
                break;
            case BRASS_LANTERN:
            case OIL_LAMP:
                otmp->spe = 1;
                otmp->age = 1000 + rn2_on_rng(500, rng);
                otmp->lamplit = 0;
                blessorcurse(otmp, 5, rng);
                break;
            case MAGIC_LAMP:
                otmp->spe = 1;
                otmp->lamplit = 0;
                blessorcurse(otmp, 2, rng);
                break;
            case CHEST:
            case LARGE_BOX:
                otmp->olocked = !!rn2_on_rng(5, rng);
                otmp->otrapped = !rn2_on_rng(10, rng);
            case ICE_BOX:
            case SACK:
            case OILSKIN_SACK:
            case BAG_OF_HOLDING:
                mkbox_cnts(otmp, rng);
                break;
            case EXPENSIVE_CAMERA:
            case TINNING_KIT:
            case MAGIC_MARKER:
                otmp->spe = 30 + rn2_on_rng(70, rng);
                break;
            case CAN_OF_GREASE:
                otmp->spe = 1 + rn2_on_rng(25, rng);
                blessorcurse(otmp, 10, rng);
                break;
            case CRYSTAL_BALL:
                otmp->spe = 1 + rn2_on_rng(5, rng);
                blessorcurse(otmp, 2, rng);
                break;
            case HORN_OF_PLENTY:
            case BAG_OF_TRICKS:
                otmp->spe = 1 + rn2_on_rng(20, rng);
                break;
            case FIGURINE:{
                    int tryct2 = 0;

                    do
                        otmp->corpsenm = rndmonnum(&lev->z, rng);
                    while (is_human(&mons[otmp->corpsenm])
                           && tryct2++ < 30);
                    blessorcurse(otmp, 4, rng);
                    break;
                }
            case BELL_OF_OPENING:
                otmp->spe = 3;
                break;
            case MAGIC_FLUTE:
            case MAGIC_HARP:
            case FROST_HORN:
            case FIRE_HORN:
            case DRUM_OF_EARTHQUAKE:
                otmp->spe = 4 + rn2_on_rng(5, rng);
                break;
            }
            break;
        case AMULET_CLASS:
            if (otmp->otyp == AMULET_OF_YENDOR)
                flags.made_amulet = TRUE;
            if (rn2_on_rng(10, rng) &&
                (otmp->otyp == AMULET_OF_STRANGULATION ||
                 otmp->otyp == AMULET_OF_CHANGE ||
                 otmp->otyp == AMULET_OF_RESTFUL_SLEEP)) {
                curse(otmp);
            } else
                blessorcurse(otmp, 10, rng);
            break;
        case VENOM_CLASS:
            impossible("Making permanent venom object!");
            break;
        case CHAIN_CLASS:
        case BALL_CLASS:
            break;
        case POTION_CLASS:
            if (otmp->otyp == POT_OIL)
                otmp->age = MAX_OIL_IN_FLASK;   /* amount of oil */
            /* fall through */
        case SCROLL_CLASS:
            blessorcurse(otmp, 4, rng);
            break;
        case SPBOOK_CLASS:
            blessorcurse(otmp, 17, rng);
            break;
        case ARMOR_CLASS:
            if (rn2_on_rng(10, rng) &&
                (otmp->otyp == FUMBLE_BOOTS || otmp->otyp == LEVITATION_BOOTS ||
                 otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT ||
                 otmp->otyp == GAUNTLETS_OF_FUMBLING ||
                 !rn2_on_rng(11, rng))) {
                curse(otmp);
                otmp->spe = -rne_on_rng(3, rng);
            } else if (!rn2_on_rng(10, rng)) {
                otmp->blessed = rn2_on_rng(2, rng);
                otmp->spe = rne_on_rng(3, rng);
            } else
                blessorcurse(otmp, 10, rng);
            if (artif && !rn2_on_rng(40, rng))
                otmp = mk_artifact(lev, otmp, (aligntyp) A_NONE, rng);
            /* simulate lacquered armor for samurai */
            if (Role_if(PM_SAMURAI) && otmp->otyp == SPLINT_MAIL &&
                In_quest(&u.uz)) {
                otmp->oerodeproof = otmp->rknown = 1;
            }
            break;
        case WAND_CLASS:
            /* Note: the RNG in question is probably either the death drop RNG,
               or the level creation RNG; in both cases we want the charges on
               the wand of wishing to be determined on the same RNG, rather than
               using a separate RNG for wand of wishing charges */
            if (otmp->otyp == WAN_WISHING)
                otmp->spe = 1 + rn2_on_rng(3, rng);
            else
                otmp->spe = rn2_on_rng(5, rng) +
                    ((objects[otmp->otyp].oc_dir == NODIR) ? 11 : 4);
            blessorcurse(otmp, 17, rng);
            otmp->recharged = 0;        /* used to control recharging */
            break;
        case RING_CLASS:
            if (objects[otmp->otyp].oc_charged) {
                blessorcurse(otmp, 3, rng);
                if (rn2_on_rng(10, rng)) {
                    if (rn2_on_rng(10, rng) && bcsign(otmp))
                        otmp->spe = bcsign(otmp) * rne_on_rng(3, rng);
                    else
                        otmp->spe = rn2_on_rng(2, rng) ?
                            rne_on_rng(3, rng) : -rne_on_rng(3, rng);
                }
                /* make useless +0 rings much less common */
                if (otmp->spe == 0)
                    otmp->spe = rn2_on_rng(4, rng) - rn2_on_rng(3, rng);
                /* negative rings are usually cursed */
                if (otmp->spe < 0 && rn2_on_rng(5, rng))
                    curse(otmp);
            } else if (rn2_on_rng(10, rng) &&
                       (otmp->otyp == RIN_TELEPORTATION ||
                        otmp->otyp == RIN_POLYMORPH ||
                        otmp->otyp == RIN_AGGRAVATE_MONSTER ||
                        otmp->otyp == RIN_HUNGER || !rn2_on_rng(9, rng))) {
                curse(otmp);
            }
            break;
        case ROCK_CLASS:
            switch (otmp->otyp) {
            case STATUE:
                /* possibly overridden by mkcorpstat() */
                otmp->corpsenm = rndmonnum(&lev->z, rng);
                if (!verysmall(&mons[otmp->corpsenm]) &&
                    rn2_on_rng(level_difficulty(&lev->z) / 2 + 10, rng) > 10)
                    add_to_container(otmp,
                                     mkobj(lev, SPBOOK_CLASS, FALSE, rng));
            }
            break;
        case COIN_CLASS:
            break;      /* do nothing */
        default:
            impossible("impossible mkobj %d, sym '%c'.", otmp->otyp,
                       objects[otmp->otyp].oc_class);
            return NULL;
        }
    }

    /* Some things must get done (timers) even if init = 0 */
    switch (otmp->otyp) {
    case CORPSE:
        start_corpse_timeout(otmp);
        break;
    }

    /* unique objects may have an associated artifact entry */
    if (objects[otyp].oc_unique && !otmp->oartifact)
        otmp = mk_artifact(lev, otmp, (aligntyp) A_NONE, rng);

    otmp->owt = weight(otmp); /* update, in case we changed it */
    return otmp;
}

struct obj *
mktemp_sobj(struct level *lev, int otyp) {
    return mksobj_basic(lev, otyp);
}

/*
 * Start a corpse decay or revive timer.
 * This takes the age of the corpse into consideration as of 3.4.0.
 */
void
start_corpse_timeout(struct obj *body)
{
    long when;  /* rot away when this old */
    long corpse_age;    /* age of corpse */
    int rot_adjust;
    short action;

#define TAINT_AGE (50L) /* age when corpses go bad */
#define TROLL_REVIVE_CHANCE 37  /* 1/37 chance for 50 turns ~ 75% chance */
#define ROT_AGE (250L)  /* age when corpses rot away */

    /* lizards and lichen don't rot or revive */
    if (body->corpsenm == PM_LIZARD || body->corpsenm == PM_LICHEN)
        return;

    action = ROT_CORPSE;        /* default action: rot away */
    rot_adjust = in_mklev ? 25 : 10;    /* give some variation */
    corpse_age = moves - body->age;
    if (corpse_age > ROT_AGE)
        when = rot_adjust;
    else
        when = ROT_AGE - corpse_age;
    when += (long)(rnz(rot_adjust) - rot_adjust);

    if (is_rider(&mons[body->corpsenm])) {
        /*
         * Riders always revive.  They have a 1/3 chance per turn
         * of reviving after 12 turns.  Always revive by 500.
         */
        action = REVIVE_MON;
        for (when = 12L; when < 500L; when++)
            if (!rn2(3))
                break;

    } else if (mons[body->corpsenm].mlet == S_TROLL && !body->norevive) {
        long age;

        for (age = 2; age <= TAINT_AGE; age++)
            if (!rn2(TROLL_REVIVE_CHANCE)) {    /* troll revives */
                action = REVIVE_MON;
                when = age;
                break;
            }
    }

    if (body->norevive)
        body->norevive = 0;
    start_timer(body->olev, when, TIMER_OBJECT, action, body);
}

void
bless(struct obj *otmp)
{
    if (otmp->oclass == COIN_CLASS)
        return;
    otmp->cursed = 0;
    otmp->blessed = 1;
    if (carried(otmp) && confers_luck(otmp))
        set_moreluck();
    else if (otmp->otyp == BAG_OF_HOLDING)
        otmp->owt = weight(otmp);
    else if (otmp->otyp == FIGURINE && otmp->timed)
        stop_timer(otmp->olev, FIG_TRANSFORM, otmp);
    return;
}

void
unbless(struct obj *otmp)
{
    otmp->blessed = 0;
    if (carried(otmp) && confers_luck(otmp))
        set_moreluck();
    else if (otmp->otyp == BAG_OF_HOLDING)
        otmp->owt = weight(otmp);
}

void
curse(struct obj *otmp)
{
    if (otmp->oclass == COIN_CLASS)
        return;
    otmp->blessed = 0;
    otmp->cursed = 1;
    /* rules at top of wield.c state that twoweapon cannot be done with cursed
       alternate weapon */
    if (otmp == uswapwep && u.twoweap)
        drop_uswapwep();
    /* some cursed items need immediate updating */
    if (carried(otmp) && confers_luck(otmp))
        set_moreluck();
    else if (otmp->otyp == BAG_OF_HOLDING)
        otmp->owt = weight(otmp);
    else if (otmp->otyp == FIGURINE) {
        if (otmp->corpsenm != NON_PM && !dead_species(otmp->corpsenm, TRUE)
            && (carried(otmp) || mcarried(otmp)))
            attach_fig_transform_timeout(otmp);
    }
    return;
}

void
uncurse(struct obj *otmp)
{
    otmp->cursed = 0;
    if (carried(otmp) && confers_luck(otmp))
        set_moreluck();
    else if (otmp->otyp == BAG_OF_HOLDING)
        otmp->owt = weight(otmp);
    else if (otmp->otyp == FIGURINE && otmp->timed)
        stop_timer(otmp->olev, FIG_TRANSFORM, otmp);
    return;
}


void
blessorcurse(struct obj *otmp, int chance, enum rng rng)
{
    if (otmp->blessed || otmp->cursed)
        return;

    if (!rn2_on_rng(chance, rng)) {
        if (!rn2_on_rng(2, rng)) {
            curse(otmp);
        } else {
            bless(otmp);
        }
    }
    return;
}


int
bcsign(struct obj *otmp)
{
    return !!otmp->blessed - !!otmp->cursed;
}


/*
 *  Calculate the weight of the given object.  This will recursively follow
 *  and calculate the weight of any containers.
 *
 *  Note:  It is possible to end up with an incorrect weight if some part
 *         of the code messes with a contained object and doesn't update the
 *         container's weight.
 */
int
weight(struct obj *obj)
{
    int wt = objects[obj->otyp].oc_weight;

    if (obj->otyp == LARGE_BOX && obj->spe == 1)        /* Schroedinger's Cat */
        wt += mons[PM_HOUSECAT].cwt;
    if (Is_container(obj) || obj->otyp == STATUE) {
        struct obj *contents;
        int cwt = 0;

        if (obj->otyp == STATUE && obj->corpsenm >= LOW_PM)
            wt = (int)obj->quan * ((int)mons[obj->corpsenm].cwt * 3 / 2);

        for (contents = obj->cobj; contents; contents = contents->nobj)
            cwt += weight(contents);
        /*
         *  The weight of bags of holding is calculated as the weight
         *  of the bag plus the weight of the bag's contents modified
         *  as follows:
         *
         *      Bag status      Weight of contents
         *      ----------      ------------------
         *      cursed                  2x
         *      blessed                 x/4 (rounded up)
         *      otherwise               x/2 (rounded up)
         *
         *  The macro DELTA_CWT in pickup.c also implements these
         *  weight equations.
         *
         *  Note:  The above checks are performed in the given order.
         *         this means that if an object is both blessed and
         *         cursed (not supposed to happen), it will be treated
         *         as cursed.
         */
        if (obj->otyp == BAG_OF_HOLDING)
            cwt =
                obj->cursed ? (cwt * 2) : (obj->blessed ? (cwt + 3) /
                                           4 : (cwt + 1) / 2);
        return wt + cwt;
    }
    if (obj->otyp == CORPSE && obj->corpsenm >= LOW_PM) {
        long long_wt = obj->quan * (long)mons[obj->corpsenm].cwt;

        wt = (long_wt > LARGEST_INT) ? LARGEST_INT : (int)long_wt;
        if (obj->oeaten)
            wt = eaten_stat(wt, obj);
        return wt;
    } else if (obj->oclass == FOOD_CLASS && obj->oeaten) {
        return eaten_stat((int)obj->quan * wt, obj);
    } else if (obj->oclass == COIN_CLASS)
        return (int)((obj->quan + 50L) / 100L);
    else if (obj->otyp == HEAVY_IRON_BALL && obj->owt != 0)
        return (int)(obj->owt); /* kludge for "very" heavy iron ball */
    else if (obj->otyp == CANDELABRUM_OF_INVOCATION)
        return (wt + obj->spe * objects[TALLOW_CANDLE].oc_weight);
    return wt ? wt * (int)obj->quan : ((int)obj->quan + 1) >> 1;
}

static const int treefruits[] =
    { APPLE, ORANGE, PEAR, BANANA, EUCALYPTUS_LEAF };

struct obj *
rnd_treefruit_at(int x, int y)
{
    return mksobj_at(treefruits[rn2(SIZE(treefruits))], level, x, y, TRUE,
                     FALSE, rng_main);
}

struct obj *
mkgold(long amount, struct level *lev, int x, int y, enum rng rng)
{
    struct obj *gold = gold_at(lev, x, y);

    if (amount <= 0L) {
        amount = 1 + rn2_on_rng(level_difficulty(&lev->z) + 2, rng);
        amount *= 1 + rn2_on_rng(30, rng);
        amount++;
    }
    if (gold) {
        gold->quan += amount;
    } else {
        gold = mksobj_at(GOLD_PIECE, lev, x, y, TRUE, FALSE, rng);
        gold->quan = amount;
    }
    gold->owt = weight(gold);
    return gold;
}


/* return TRUE if the corpse has special timing */
#define special_corpse(num)  (((num) == PM_LIZARD)              \
                                || ((num) == PM_LICHEN)         \
                                || (is_rider(&mons[num]))       \
                                || (mons[num].mlet == S_TROLL))

/*
 * OEXTRA note: Passing mtmp causes mtraits to be saved
 * even if ptr passed as well, but ptr is always used for
 * the corpse type (corpsenm). That allows the corpse type
 * to be different from the original monster,
 *      i.e.  vampire -> human corpse
 * yet still allow restoration of the original monster upon
 * resurrection.
 */
struct obj *
mkcorpstat(int objtype, /* CORPSE or STATUE */
           struct monst *mtmp, const struct permonst *ptr, struct level *lev,
           int x, int y, boolean init, enum rng rng)
{
    struct obj *otmp;

    if (objtype != CORPSE && objtype != STATUE)
        impossible("making corpstat type %d", objtype);
    if (x == COLNO && y == ROWNO) {     /* special case - random placement */
        otmp = mksobj(lev, objtype, init, FALSE, rng);
        if (otmp)
            rloco(otmp);
    } else
        otmp = mksobj_at(objtype, lev, x, y, init, FALSE, rng);
    if (otmp) {
        if (mtmp) {
            struct obj *otmp2;

            if (!ptr)
                ptr = mtmp->data;
            /* save_mtraits frees original data pointed to by otmp */
            otmp2 = save_mtraits(otmp, mtmp);
            if (otmp2)
                otmp = otmp2;
        }

        /* use the corpse or statue produced by mksobj() as-is unless `ptr' is
           non-null */
        if (ptr) {
            int old_corpsenm = otmp->corpsenm;

            otmp->corpsenm = monsndx(ptr);
            otmp->owt = weight(otmp);
            if (otmp->otyp == CORPSE &&
                (special_corpse(old_corpsenm) ||
                 special_corpse(otmp->corpsenm))) {
                obj_stop_timers(otmp);
                start_corpse_timeout(otmp);
            }
        }
    }
    return otmp;
}

/*
 * Attach a monster id to an object, to provide
 * a lasting association between the two.
 */
struct obj *
obj_attach_mid(struct obj *obj, unsigned mid)
{
    struct obj *otmp;
    int namelth;

    if (!mid || !obj)
        return NULL;

    if (obj->oxlth) {
        impossible("xlth set when attaching mid");
        return NULL;
    }

    namelth = obj->onamelth ? strlen(ONAME(obj)) + 1 : 0;
    otmp = realloc_obj(obj, sizeof mid, &mid, namelth, ONAME(obj));
    if (otmp && otmp->oxlth)
        otmp->oattached = OATTACHED_M_ID;       /* mark it */
    return otmp;
}

static struct obj *
save_mtraits(struct obj *obj, struct monst *mtmp)
{
    struct obj *otmp;
    int lth, namelth;

    lth = sizeof (struct monst) + mtmp->mxlth + mtmp->mnamelth;
    namelth = obj->onamelth ? strlen(ONAME(obj)) + 1 : 0;
    otmp = realloc_obj(obj, lth, mtmp, namelth, ONAME(obj));
    if (otmp && otmp->oxlth) {
        struct monst *mtmp2 = (struct monst *)otmp->oextra;

        if (mtmp->data)
            mtmp2->orig_mnum = monsndx(mtmp->data);
        /* invalidate pointers */
        /* m_id is needed to know if this is a revived quest leader */
        /* but m_id must be cleared when loading bones */
        mtmp2->nmon = NULL;
        mtmp2->data = NULL;
        mtmp2->minvent = NULL;

        /* clear monster turnstate */
        mtmp2->deadmonster = 0;

        otmp->oattached = OATTACHED_MONST;      /* mark it */
    }
    return otmp;
}

/* returns a pointer to a new monst structure based on
 * the one contained within the obj.
 */
struct monst *
get_mtraits(struct obj *obj, boolean copyof)
{
    struct monst *mtmp = NULL;
    struct monst *mnew = NULL;

    if (obj->oxlth && obj->oattached == OATTACHED_MONST)
        mtmp = (struct monst *)obj->oextra;
    if (mtmp) {
        if (copyof) {
            int lth = mtmp->mxlth + mtmp->mnamelth + sizeof (struct monst);

            mnew = newmonst(mtmp->mxtyp, mtmp->mnamelth);
            memcpy(mnew, mtmp, lth);
        } else {
            /* Never insert this returned pointer into mon chains! */
            mnew = mtmp;
        }
    }
    return mnew;
}


/* make an object named after someone listed in the scoreboard file */
struct obj *
mk_tt_object(struct level *lev, int objtype,    /* CORPSE or STATUE */
             int x, int y)
{
    struct obj *otmp, *otmp2;
    boolean initialize_it;

    /* player statues never contain books */
    initialize_it = (objtype != STATUE);
    if ((otmp = mksobj_at(objtype, lev, x, y, initialize_it, FALSE,
                          rng_main)) != 0) {
        /* tt_oname will return null if the scoreboard is empty */
        if ((otmp2 = tt_oname(otmp)) != 0)
            otmp = otmp2;
    }
    return otmp;
}

/* make a new corpse or statue, uninitialized if a statue (i.e. no books);
   uses the main RNG (because that's what all callers want) */
struct obj *
mk_named_object(int objtype,    /* CORPSE or STATUE */
                const struct permonst *ptr, int x, int y, const char *nm)
{
    struct obj *otmp;

    otmp = mkcorpstat(objtype, NULL, ptr, level, x, y,
                      (boolean) (objtype != STATUE), rng_main);
    if (nm)
        otmp = oname(otmp, nm);
    return otmp;
}

boolean
is_flammable(const struct obj * otmp)
{
    int otyp = otmp->otyp;
    int omat = objects[otyp].oc_material;

    if (objects[otyp].oc_oprop == FIRE_RES || otyp == WAN_FIRE)
        return FALSE;

    return (boolean) ((omat <= WOOD && omat != LIQUID) || omat == PLASTIC);
}

boolean
is_rottable(const struct obj * otmp)
{
    int otyp = otmp->otyp;

    return ((boolean)
            (objects[otyp].oc_material <= WOOD &&
             objects[otyp].oc_material != LIQUID));
}


/*
 * These routines maintain the single-linked lists headed in level->objects[][]
 * and threaded through the nexthere fields in the object-instance structure.
 */

/* put the object at the given location */
void
place_object(struct obj *otmp, struct level *lev, int x, int y)
{
    struct obj *otmp2 = lev->objects[x][y];

    if (otmp->where != OBJ_FREE)
        panic("place_object: obj not free");
    if (!isok(x, y))
        panic("placing object at bad position");

    obj_no_longer_held(otmp);
    if (otmp->otyp == BOULDER && lev == level)
        block_point(x, y);      /* vision */

    /* obj goes under boulders */
    if (otmp2 && (otmp2->otyp == BOULDER)) {
        otmp->nexthere = otmp2->nexthere;
        otmp2->nexthere = otmp;
    } else {
        otmp->nexthere = otmp2;
        lev->objects[x][y] = otmp;
    }

    /* set the new object's location */
    otmp->ox = x;
    otmp->oy = y;

    set_obj_level(lev, otmp);   /* set the level recursively for containers */
    extract_nobj(otmp, &turnstate.floating_objects, &lev->objlist, OBJ_FLOOR);

    if (otmp->timed)
        obj_timer_checks(otmp, x, y, 0);
}

#define ON_ICE(a) ((a)->recharged)
#define ROT_ICE_ADJUSTMENT 2    /* rotting on ice takes 2 times as long */

/* If ice was affecting any objects correct that now
 * Also used for starting ice effects too. [zap.c]
 */
void
obj_ice_effects(struct level *lev, int x, int y, boolean do_buried)
{
    struct obj *otmp;

    for (otmp = lev->objects[x][y]; otmp; otmp = otmp->nexthere) {
        if (otmp->timed)
            obj_timer_checks(otmp, x, y, 0);
    }
    if (do_buried) {
        for (otmp = lev->buriedobjlist; otmp; otmp = otmp->nobj) {
            if (otmp->ox == x && otmp->oy == y) {
                if (otmp->timed)
                    obj_timer_checks(otmp, x, y, 0);
            }
        }
    }
}

/*
 * Returns an obj->age for a corpse object on ice, that would be the
 * actual obj->age if the corpse had just been lifted from the ice.
 * This is useful when just using obj->age in a check or calculation because
 * rot timers pertaining to the object don't have to be stopped and
 * restarted etc.
 */
long
peek_at_iced_corpse_age(struct obj *otmp)
{
    long age, retval = otmp->age;

    if (otmp->otyp == CORPSE && ON_ICE(otmp)) {
        /* Adjust the age; must be same as obj_timer_checks() for off ice */
        age = moves - otmp->age;
        retval = otmp->age + (age / ROT_ICE_ADJUSTMENT);
    }
    return retval;
}

static void
obj_timer_checks(struct obj *otmp, xchar x, xchar y, int force)
{       /* 0 = no force so do checks, <0 = force off, >0 force on */
    long tleft = 0L;
    short action = ROT_CORPSE;
    boolean restart_timer = FALSE;
    boolean on_floor = (otmp->where == OBJ_FLOOR);
    boolean buried = (otmp->where == OBJ_BURIED);

    /* Check for corpses just placed on or in ice */
    if (otmp->otyp == CORPSE && (on_floor || buried) &&
        is_ice(otmp->olev, x, y)) {
        tleft = stop_timer(otmp->olev, action, otmp);
        if (tleft == 0L) {
            action = REVIVE_MON;
            tleft = stop_timer(otmp->olev, action, otmp);
        }
        if (tleft != 0L) {
            long age;

            tleft = tleft - moves;
            /* mark the corpse as being on ice */
            ON_ICE(otmp) = 1;

            /* Adjust the time remaining */
            tleft *= ROT_ICE_ADJUSTMENT;
            restart_timer = TRUE;
            /* Adjust the age; must be same as in obj_ice_age() */
            age = moves - otmp->age;
            otmp->age = moves - (age * ROT_ICE_ADJUSTMENT);
        }
    }
    /* Check for corpses coming off ice */
    else if ((force < 0) ||
             (otmp->otyp == CORPSE && ON_ICE(otmp) &&
              ((on_floor && !is_ice(otmp->olev, x, y)) || !on_floor))) {
        tleft = stop_timer(otmp->olev, action, otmp);
        if (tleft == 0L) {
            action = REVIVE_MON;
            tleft = stop_timer(otmp->olev, action, otmp);
        }
        if (tleft != 0L) {
            long age;

            tleft = tleft - moves;
            ON_ICE(otmp) = 0;

            /* Adjust the remaining time */
            tleft /= ROT_ICE_ADJUSTMENT;
            restart_timer = TRUE;
            /* Adjust the age */
            age = moves - otmp->age;
            otmp->age = otmp->age + (age / ROT_ICE_ADJUSTMENT);
        }
    }
    /* now re-start the timer with the appropriate modifications */
    if (restart_timer)
        start_timer(otmp->olev, tleft, TIMER_OBJECT, action, otmp);
}

#undef ON_ICE
#undef ROT_ICE_ADJUSTMENT

void
remove_object(struct obj *otmp)
{
    xchar x = otmp->ox;
    xchar y = otmp->oy;

    if (otmp->where != OBJ_FLOOR)
        panic("remove_object: obj not on floor");
    extract_nexthere(otmp, &otmp->olev->objects[x][y]);
    extract_nobj(otmp, &otmp->olev->objlist,
                 &turnstate.floating_objects, OBJ_FREE);
    if (otmp->otyp == BOULDER && otmp->olev == level &&
        !sobj_at(BOULDER, level, x, y)) /* vision */
        unblock_point(x, y);
    if (otmp->timed)
        obj_timer_checks(otmp, x, y, 0);
}

/* throw away all of a monster's inventory */
void
discard_minvent(struct monst *mtmp)
{
    struct obj *otmp;

    while ((otmp = mtmp->minvent) != 0) {
        obj_extract_self(otmp);
        obfree(otmp, NULL);     /* dealloc_obj() isn't sufficient */
    }
}

/*
 * Free obj from whatever list it is on in preperation of deleting it or
 * moving it elsewhere.
 *
 * [The documentation here previously said "this will perform all high-level
 * consequences involved with removing the item.  E.g. if the object is in the
 * hero's inventory and confers heat resistance, the hero will lose it." This
 * needs to be treated with caution, e.g. freeinv() doesn't have a call to
 * setnotworn().]
 *
 * Object positions:
 *      OBJ_FREE        turnstate->floating_objects
 *      OBJ_FLOOR       level->objlist, level->locations[][] chains (use
 *                      remove_object)
 *      OBJ_CONTAINED   cobj chain of container object
 *      OBJ_INVENT      hero's invent chain (use freeinv)
 *      OBJ_MINVENT     monster's invent chain
 *      OBJ_BURIED      level->buriedobjs chain
 *      OBJ_ONBILL      on billobjs chain
 *      OBJ_MIGRATING   on migrating_objects chain
 */
void
obj_extract_self(struct obj *obj)
{
    switch (obj->where) {
    case OBJ_FREE:
        break;
    case OBJ_FLOOR:
        remove_object(obj);
        break;
    case OBJ_CONTAINED:
        extract_nobj(obj, &obj->ocontainer->cobj,
                     &turnstate.floating_objects, OBJ_FREE);
        container_weight(obj->ocontainer);
        break;
    case OBJ_INVENT:
        freeinv(obj);
        break;
    case OBJ_MINVENT:
        extract_nobj(obj, &obj->ocarry->minvent,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_BURIED:
        extract_nobj(obj, &obj->olev->buriedobjlist,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_ONBILL:
        extract_nobj(obj, &obj->olev->billobjs,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_MIGRATING:
        extract_nobj(obj, &turnstate.migrating_objs,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    default:
        panic("obj_extract_self");
        break;
    }
}


/* Extract the given object from the chain, following nobj chain. This puts the
   object onto the given alternative chain instead (which can be NULL). */
void
extract_nobj(struct obj *obj, struct obj **head_ptr,
             struct obj **new_chain, enum obj_where where)
{
    struct obj *curr, *prev;

    curr = *head_ptr;
    for (prev = NULL; curr; prev = curr, curr = curr->nobj) {
        if (curr == obj) {
            if (prev)
                prev->nobj = curr->nobj;
            else
                *head_ptr = curr->nobj;
            break;
        }
    }
    if (!curr)
        panic("extract_nobj: object lost");

    if (new_chain) {
        obj->nobj = *new_chain;
        *new_chain = obj;
        obj->where = where;
    }
}


/*
 * Extract the given object from the chain, following nexthere chain.
 *
 * This does not set obj->where, this function is expected to be called
 * in tandem with extract_nobj, which does set it.
 */
void
extract_nexthere(struct obj *obj, struct obj **head_ptr)
{
    struct obj *curr, *prev;

    curr = *head_ptr;
    for (prev = NULL; curr; prev = curr, curr = curr->nexthere) {
        if (curr == obj) {
            if (prev)
                prev->nexthere = curr->nexthere;
            else
                *head_ptr = curr->nexthere;
            break;
        }
    }
    if (!curr)
        panic("extract_nexthere: object lost");
}


/*
 * Add obj to mon's inventory.  If obj is able to merge with something already
 * in the inventory, then the passed obj is deleted and 1 is returned.
 * Otherwise 0 is returned.
 */
int
add_to_minv(struct monst *mon, struct obj *obj)
{
    struct obj *otmp;

    if (obj->where != OBJ_FREE)
        panic("add_to_minv: obj not free");

    /* merge if possible */
    for (otmp = mon->minvent; otmp; otmp = otmp->nobj)
        if (merged(&otmp, &obj))
            return 1;   /* obj merged and then free'd */
    /* else insert; don't bother forcing it to end of chain */
    extract_nobj(obj, &turnstate.floating_objects, &mon->minvent, OBJ_MINVENT);
    obj->ocarry = mon;

    return 0;   /* obj on mon's inventory chain */
}


/*
 * Add obj to container, make sure obj is "free".  Returns (merged) obj.
 * The input obj may be deleted in the process.
 */
struct obj *
add_to_container(struct obj *container, struct obj *obj)
{
    struct obj *otmp;

    if (obj->where != OBJ_FREE)
        panic("add_to_container: obj not free");
    if (container->where != OBJ_INVENT && container->where != OBJ_MINVENT)
        obj_no_longer_held(obj);

    /* merge if possible */
    for (otmp = container->cobj; otmp; otmp = otmp->nobj)
        if (merged(&otmp, &obj))
            return otmp;

    extract_nobj(obj, &turnstate.floating_objects,
                 &container->cobj, OBJ_CONTAINED);
    obj->ocontainer = container;
    return obj;
}


void
add_to_buried(struct obj *obj)
{
    if (obj->where != OBJ_FREE)
        panic("add_to_buried: obj not free");

    extract_nobj(obj, &turnstate.floating_objects,
                 &obj->olev->buriedobjlist, OBJ_BURIED);
}


/* Recalculate the weight of this container and all of _its_ containers. */
static void
container_weight(struct obj *container)
{
    container->owt = weight(container);
    if (container->where == OBJ_CONTAINED)
        container_weight(container->ocontainer);
/*
    else if (container->where == OBJ_INVENT)
        recalculate load delay here ???
*/
}

/* Allocates an object. */
struct obj *
newobj(int extra_bytes, struct obj *initfrom)
{
    struct obj *otmp = malloc((unsigned)extra_bytes + sizeof(struct obj));
    *otmp = *initfrom;
    /* note: extra data not copied by newobj */
    otmp->where = OBJ_FREE;
    otmp->nobj = turnstate.floating_objects;
    turnstate.floating_objects = otmp;
    return otmp;
}

/* Deallocates an object. _All_ objects should be run through here for
   them to be deallocated. */
void
dealloc_obj(struct obj *obj)
{
    if (obj->where != OBJ_FREE)
        panic("dealloc_obj: obj not free");

    /* free up any timers attached to the object */
    if (obj->timed)
        obj_stop_timers(obj);

    /*
     * Free up any light sources attached to the object.
     *
     * We may want to just call del_light_source() without any
     * checks (requires a code change there).  Otherwise this
     * list must track all objects that can have a light source
     * attached to it (and also requires lamplit to be set).
     */
    if (obj_sheds_light(obj))
        del_light_source(obj->olev, LS_OBJECT, obj);

    if (obj == thrownobj)
        thrownobj = NULL;

    extract_nobj(obj, &turnstate.floating_objects, NULL, 0);

    free(obj);
}


/* update which level the object points back to when it, its container or the
 * monster holding it migrate.
 */
void
set_obj_level(struct level *lev, struct obj *obj)
{
    struct obj *cobj;

    if (obj->timed)
	transfer_timers(obj->olev, lev, obj->o_id);
    if (obj->lamplit)
	transfer_lights(obj->olev, lev, obj->o_id);

    obj->olev = lev;

    for (cobj = obj->cobj; cobj; cobj = cobj->nobj)
        set_obj_level(lev, cobj);
}


/* Loads an object onto the floating objects chain, but with the OBJ_WHERE from
   the save file. */
struct obj *
restore_obj(struct memfile *mf)
{
    unsigned int oflags;
    int namelen;
    struct obj *otmp;

    mfmagic_check(mf, OBJ_MAGIC);

    namelen = mread32(mf);
    otmp = newobj(namelen, &zeroobj);
    memset(otmp, 0, namelen + sizeof (struct obj));

    otmp->o_id = mread32(mf);
    otmp->owt = mread32(mf);
    otmp->quan = mread32(mf);
    otmp->corpsenm = mread32(mf);
    otmp->oeaten = mread32(mf);
    otmp->age = mread32(mf);
    otmp->owornmask = mread32(mf);
    oflags = mread32(mf);

    otmp->otyp = mread16(mf);

    otmp->ox = mread8(mf);
    otmp->oy = mread8(mf);
    otmp->spe = mread8(mf);
    otmp->oclass = mread8(mf);
    otmp->invlet = mread8(mf);
    otmp->oartifact = mread8(mf);
    otmp->where = mread8(mf);
    otmp->timed = mread8(mf);
    otmp->cobj = mread8(mf) ? (void *)1 : NULL; /* set the pointer to 1 if
                                                   there will be contents */
    otmp->onamelth = namelen;

    otmp->cursed = (oflags >> 31) & 1;
    otmp->blessed = (oflags >> 30) & 1;
    otmp->unpaid = (oflags >> 29) & 1;
    otmp->no_charge = (oflags >> 28) & 1;
    otmp->known = (oflags >> 27) & 1;
    otmp->dknown = (oflags >> 26) & 1;
    otmp->bknown = (oflags >> 25) & 1;
    otmp->rknown = (oflags >> 24) & 1;
    otmp->oeroded = (oflags >> 22) & 3;
    otmp->oeroded2 = (oflags >> 20) & 3;
    otmp->oerodeproof = (oflags >> 19) & 1;
    otmp->olocked = (oflags >> 18) & 1;
    otmp->obroken = (oflags >> 17) & 1;
    otmp->otrapped = (oflags >> 16) & 1;
    otmp->recharged = (oflags >> 13) & 7;
    otmp->lamplit = (oflags >> 12) & 1;
    otmp->greased = (oflags >> 11) & 1;
    otmp->oattached = (oflags >> 9) & 3;
    otmp->in_use = (oflags >> 8) & 1;
    otmp->was_thrown = (oflags >> 7) & 1;
    otmp->bypass = (oflags >> 6) & 1;
    otmp->was_dropped = (oflags >> 5) & 1;

    if (otmp->onamelth)
        mread(mf, ONAME_MUTABLE(otmp), otmp->onamelth);

    if (otmp->oattached == OATTACHED_MONST) {
        struct monst *mtmp = restore_mon(mf, NULL);
        int monlen = sizeof (struct monst) + mtmp->mnamelth + mtmp->mxlth;

        otmp = realloc_obj(otmp, monlen, mtmp, otmp->onamelth, ONAME(otmp));
        dealloc_monst(mtmp);
    } else if (otmp->oattached == OATTACHED_M_ID) {
        unsigned int mid = mread32(mf);

        otmp = obj_attach_mid(otmp, mid);
    }

    return otmp;
}


void
save_obj(struct memfile *mf, struct obj *obj)
{
    unsigned int oflags;

    if (obj->o_id == TEMPORARY_IDENT) {
        impossible("Temporary object encountered in save code!");
        return;
    }

    oflags =
        (obj->cursed << 31) | (obj->blessed << 30) |
        (obj->unpaid << 29) | (obj->no_charge << 28) |
        (obj->known << 27) | (obj->dknown << 26) |
        (obj->bknown << 25) | (obj->rknown << 24) |
        (obj->oeroded << 22) | (obj->oeroded2 << 20) |
        (obj->oerodeproof << 19) | (obj->olocked << 18) |
        (obj->obroken << 17) | (obj->otrapped << 16) |
        (obj->recharged << 13) | (obj->lamplit << 12) |
        (obj->greased << 11) | (obj->oattached << 9) |
        (obj->in_use << 8) | (obj->was_thrown << 7) | (obj->bypass << 6) |
        (obj->was_dropped << 5);

    mfmagic_set(mf, OBJ_MAGIC);
    mtag(mf, obj->o_id, MTAG_OBJ);

    mwrite32(mf, obj->onamelth);
    mwrite32(mf, obj->o_id);
    mwrite32(mf, obj->owt);
    mwrite32(mf, obj->quan);
    mwrite32(mf, obj->corpsenm);
    mwrite32(mf, obj->oeaten);
    mwrite32(mf, obj->age);
    mwrite32(mf, obj->owornmask);
    mwrite32(mf, oflags);

    mwrite16(mf, obj->otyp);

    mwrite8(mf, obj->ox);
    mwrite8(mf, obj->oy);
    mwrite8(mf, obj->spe);
    mwrite8(mf, obj->oclass);
    mwrite8(mf, obj->invlet);
    mwrite8(mf, obj->oartifact);
    mwrite8(mf, obj->where);
    mwrite8(mf, obj->timed);
    /* no need to save the value of the cobj pointer, but we will need to know
       if there is something in here that needs to be restored */
    mwrite8(mf, obj->cobj ? 1 : 0);

    if (obj->onamelth)
        mwrite(mf, ONAME(obj), obj->onamelth);

    if (obj->oattached == OATTACHED_MONST)
        save_mon(mf, (struct monst *)obj->oextra, NULL);
    else if (obj->oattached == OATTACHED_M_ID) {
        unsigned m_id;

        memcpy(&m_id, obj->oextra, sizeof m_id);

        mwrite32(mf, m_id);
    }
}

/*mkobj.c*/
