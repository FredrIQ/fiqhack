/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static long find_extrinsic(struct obj *, int, int *, boolean *);
static void m_lose_armor(struct monst *, struct obj *);
static void m_dowear_type(struct monst *, enum objslot, boolean, boolean);
static unsigned do_equip(struct monst *, struct obj *, boolean, boolean);

/* This only allows for one blocked property per item */
/* TODO: maybe make this into a real function */
#define w_blocks(o,m) \
    ((o->otyp == MUMMY_WRAPPING && ((m) & W_MASK(os_armc))) ? INVIS :   \
     ((o->oartifact == ART_EYES_OF_THE_OVERWORLD ||                     \
       o->oartifact == ART_SUNSWORD) &&                                 \
      ((m) & W_MASK(os_tool))) ? BLINDED :                              \
     (o->otyp == CORNUTHAUM && ((m) & W_MASK(os_armh)) &&               \
      !Role_if (PM_WIZARD)) ? CLAIRVOYANT : 0)
                /* note: monsters don't have clairvoyance, so your role has no
                   significant effect on their use of w_blocks() */

/* Finds all items on the given chain with the given extrinsic. */
static long
find_extrinsic(struct obj *chain, int extrinsic, int *warntype,
               boolean *blocked)
{
    if (program_state.restoring_binary_save)
        return 0L; /* inventory chains may not be linked yet */

    long mask = 0L;
    *blocked = FALSE;
    while (chain) {
        mask |= item_provides_extrinsic(chain, extrinsic, warntype);
        if (extrinsic == w_blocks(chain, chain->owornmask))
             *blocked = TRUE;
        chain = chain->nobj;
    }
    if (*blocked) return 0L;
    return mask;
}

long
mworn_extrinsic(const struct monst *mon, int extrinsic)
{
    int warntype;
    boolean blocked;
    return find_extrinsic(mon->minvent, extrinsic, &warntype, &blocked);
}

boolean
mworn_blocked(const struct monst *mon, int extrinsic)
{
    int warntype;
    boolean blocked;
    find_extrinsic(mon->minvent, extrinsic, &warntype, &blocked);
    return blocked;
}

int
mworn_warntype(const struct monst *mon)
{
    int warntype;
    boolean blocked;
    return find_extrinsic(mon->minvent, WARN_OF_MON, &warntype, &blocked)
        ? warntype : 0;
}

void
setworn(struct obj *obj, long mask)
{
    enum objslot i;
    struct obj *oobj;

    for (i = 0; i <= os_last_maskable; i++)
        if (W_MASK(i) & mask) {
            oobj = EQUIP(i);
            if (oobj) {
                if (oobj->owornmask & (W_MASK(os_wep) | W_MASK(os_swapwep)))
                    u.twoweap = 0;

                /* Equipment slots are now based entirely on owornmask.
                   So this clears the equipment slot. */
                oobj->owornmask &= ~W_MASK(i);
            }

            if (obj) {
                /* Put the new object in the slot. */
                obj->owornmask |= W_MASK(i);
            }
        }

    if (!program_state.restoring_binary_save) {
        /* this might have changed the XRAY property */
        turnstate.vision_full_recalc = TRUE;
        /* it might have changed Teleportation or Jumping */
        update_supernatural_abilities();
        see_monsters(FALSE);    /* for the WARN_OF_MON property */
        update_inventory();     /* and it definitely changed equip slots */
    }
}

/* called e.g. when obj is destroyed */
void
setnotworn(struct obj *obj)
{
    if (!obj)
        return;
    if (obj == uwep || obj == uswapwep)
        u.twoweap = 0;
    if (obj->oartifact)
        uninvoke_artifact(obj);
    obj->owornmask = 0L;

    obj->owt = weight(obj);
    turnstate.vision_full_recalc = TRUE;
    see_monsters(FALSE);
    update_inventory();
}

boolean
obj_worn_on(struct obj *obj, enum objslot slot)
{
    /* Two checks: we check that something is wearing the object in the given
       slot, and that the player's appropriate slot holds the given item.

       We also return false for a NULL obj, to help save on special cases in
       the callers. */
    return obj && obj->owornmask & W_MASK(slot) && obj == EQUIP(slot);
}

int
find_mac(struct monst *mon)
{
    struct obj *obj;
    int base = mon->data->ac;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        /* Armor transformed into dragon skin gives no AC bonus */
        if (mon == &youmonst && obj == uskin())
            continue;
        if (obj->owornmask & W_ARMOR)
            base -= ARM_BONUS(obj);
        /* since ARM_BONUS is positive, subtracting it increases AC */
    }

    if (mon != &youmonst && mon->mtame) {
        /* Hopefully, this codepath never runs; badly injured tame monsters
           aren't meant to be attacked. This is a fallback to keep the pets
           alive in the case that some hostile monster isn't calling
           mm_aggression correctly. */
        if (mon->mhp * 3 <= mon->mhpmax)
            base -= 20;
    }

    base -= m_mspellprot(mon);
    base -= protbon(mon);

    /* Add divine protection */
    if (mon == &youmonst)
        base -= u.ublessed;

    /* Trim to valid schar range. */
    if (base < -128)
        base = -128;
    if (base > 127)
        base = 127;

    return base;
}

/* weapons are handled separately; eyewear aren't used by monsters */

/* Wear the best object of each type that the monster has.  During creation,
 * the monster can put everything on at once; otherwise, wearing takes time.
 * This doesn't affect monster searching for objects--a monster may very well
 * search for objects it would not want to wear, because we don't want to
 * check which_armor() each round.
 *
 * We'll let monsters put on shirts and/or suits under worn cloaks, but
 * not shirts under worn suits.  This is somewhat arbitrary, but it's
 * too tedious to have them remove and later replace outer garments,
 * and preventing suits under cloaks makes it a little bit too easy for
 * players to influence what gets worn.  Putting on a shirt underneath
 * already worn body armor is too obviously buggy...
 */
void
m_dowear(struct monst *mon, boolean creation)
{
#define RACE_EXCEPTION TRUE

    /* Workaround for the fact that newcham() has lost track of creation state.
       TODO: Find a better implementation of this. */
    if (in_mklev)
        creation = TRUE;

    /* Note the restrictions here are the same as in dowear in do_wear.c except
       for the additional restriction on intelligence.  (Players are always
       intelligent, even if polymorphed). */
    if (verysmall(mon->data) || nohands(mon->data) || is_animal(mon->data))
        return;
    /* give mummies a chance to wear their wrappings and let skeletons wear
       their initial armor */
    if (mindless(mon->data) &&
        (!creation ||
         (mon->data->mlet != S_MUMMY && mon->data != &mons[PM_SKELETON])))
        return;

    m_dowear_type(mon, os_amul, creation, FALSE);

    if (mon->data == &mons[PM_KI_RIN] || mon->data == &mons[PM_COUATL])
        return;

    /* TODO: handle cursed equipment better (can't put on suit if cursed cloak, etc),
       it is currently not handled at all */
    /* can't put on shirt if already wearing suit */
    if (!cantweararm(mon->data) || (mon->misc_worn_check & W_MASK(os_arm)))
        m_dowear_type(mon, os_armu, creation, FALSE);
    /* treating small as a special case allows hobbits, gnomes, and kobolds to
       wear cloaks */
    if (!cantweararm(mon->data) || mon->data->msize == MZ_SMALL)
        m_dowear_type(mon, os_armc, creation, FALSE);
    m_dowear_type(mon, os_armh, creation, FALSE);
    if (!MON_WEP(mon) || !bimanual(MON_WEP(mon)))
        m_dowear_type(mon, os_arms, creation, FALSE);
    m_dowear_type(mon, os_ringl, creation, FALSE);
    m_dowear_type(mon, os_ringr, creation, FALSE);
    m_dowear_type(mon, os_armg, creation, FALSE);
    if (!slithy(mon->data) && mon->data->mlet != S_CENTAUR)
        m_dowear_type(mon, os_armf, creation, FALSE);
    if (!cantweararm(mon->data))
        m_dowear_type(mon, os_arm, creation, FALSE);
    else
        m_dowear_type(mon, os_arm, creation, RACE_EXCEPTION);
}

static void
m_dowear_type(struct monst *mon, enum objslot slot, boolean creation,
              boolean racialexception)
{
    struct obj *old, *best, *obj;
    int m_delay = 0;

    if (mon->mfrozen)
        return; /* probably putting previous item on */

    old = which_armor(mon, slot);
    if (old && old->cursed)
        return;
    if (old && slot == os_amul)
        return; /* no such thing as better amulets */
    best = old;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        switch (slot) {
        case os_amul:
            if (obj->oclass != AMULET_CLASS ||
                (obj->otyp != AMULET_OF_ESP &&
                 obj->otyp != AMULET_OF_LIFE_SAVING &&
                 obj->otyp != AMULET_OF_MAGICAL_BREATHING &&
                 obj->otyp != AMULET_OF_REFLECTION &&
                 obj->otyp != AMULET_OF_RESTFUL_SLEEP &&
                 obj->otyp != AMULET_OF_UNCHANGING &&
                 obj->otyp != AMULET_VERSUS_POISON))
                continue;
            break;
        case os_ringl:
        case os_ringr:
            /* Monsters can put on only the following rings. */
            if (obj->oclass != RING_CLASS ||
                (obj->otyp != RIN_GAIN_CONSTITUTION &&
                 obj->otyp != RIN_INCREASE_ACCURACY &&
                 obj->otyp != RIN_INCREASE_DAMAGE &&
                 obj->otyp != RIN_PROTECTION &&
                 obj->otyp != RIN_REGENERATION &&
              /* obj->otyp != RIN_LEVITATION && TODO */
                 obj->otyp != RIN_HUNGER &&
                 obj->otyp != RIN_AGGRAVATE_MONSTER &&
                 obj->otyp != RIN_WARNING &&
                 obj->otyp != RIN_POISON_IMMUNITY &&
                 obj->otyp != RIN_FIRE_IMMUNITY &&
                 obj->otyp != RIN_COLD_IMMUNITY &&
                 obj->otyp != RIN_SHOCK_IMMUNITY &&
                 obj->otyp != RIN_FREE_ACTION &&
                 obj->otyp != RIN_SLOW_DIGESTION &&
                 obj->otyp != RIN_TELEPORTATION &&
                 obj->otyp != RIN_TELEPORT_CONTROL &&
                 obj->otyp != RIN_POLYMORPH &&
                 obj->otyp != RIN_POLYMORPH_CONTROL &&
                 obj->otyp != RIN_INVISIBILITY &&
                 obj->otyp != RIN_SEE_INVISIBLE))
                continue;
            break;
        case os_armu:
            if (!is_shirt(obj))
                continue;
            break;
        case os_armc:
            if (!is_cloak(obj))
                continue;
            break;
        case os_armh:
            if (!is_helmet(obj))
                continue;
            /* (flimsy exception matches polyself handling) */
            if (has_horns(mon->data) && !is_flimsy(obj))
                continue;
            break;
        case os_arms:
            if (!is_shield(obj))
                continue;
            break;
        case os_armg:
            if (!is_gloves(obj))
                continue;
            break;
        case os_armf:
            if (!is_boots(obj) || obj->otyp == LEVITATION_BOOTS) /* levi is TODO */
                continue;
            break;
        case os_arm:
            if (!is_suit(obj))
                continue;
            if (racialexception && (racial_exception(mon, obj) < 1))
                continue;
            break;
        default:
            break;
        }
        if (obj->owornmask)
            continue;
        /* I'd like to define a VISIBLE_ARM_BONUS which doesn't assume the
           monster knows obj->spe, but if I did that, a monster would keep
           switching forever between two -2 caps since when it took off one it
           would forget spe and once again think the object is better than what
           it already has. */
        if (best &&
            (ARM_BONUS(best) + extra_pref(mon, best) >=
             ARM_BONUS(obj) + extra_pref(mon, obj)))
            continue;
        best = obj;
    }
    if (!best || best == old)
        return;

    /* If wearing body armor, account for time spent removing and wearing it
       when putting on a shirt. */
    if ((slot == os_armu) && (mon->misc_worn_check & W_MASK(os_arm)))
        m_delay += 2 * objects[which_armor(mon, os_arm)->otyp].oc_delay;
    /* if wearing a cloak, account for the time spent removing and re-wearing
       it when putting on a suit or shirt */
    if ((slot == os_arm || slot == os_armu) &&
        (mon->misc_worn_check & W_MASK(os_armc)))
        m_delay += 2;
    /* when upgrading a piece of armor, account for time spent taking off
       current one */
    if (old)
        m_delay += objects[old->otyp].oc_delay;

    if (old)    /* do this first to avoid "(being worn)" */
        old->owornmask = 0L;
    if (!creation) {
        if (canseemon(mon)) {
            const char *buf;

            if (old)
                buf = msgprintf(" removes %s and", distant_name(old, doname));
            else
                buf = "";
            pline(mon->mtame ? msgc_petneutral : msgc_monneutral,
                  "%s%s puts on %s.", Monnam(mon), buf,
                  distant_name(best, doname));
            /* we know enchantment now */
            best->mknown = 1;
        }       /* can see it */
        m_delay += objects[best->otyp].oc_delay;
        mon->mfrozen = m_delay;
        if (mon->mfrozen)
            mon->mcanmove = 0;
    }
    /* TODO: more general check for autocursing? */
    if ((best->otyp == DUNCE_CAP || best->otyp == HELM_OF_OPPOSITE_ALIGNMENT) &&
        !best->cursed) {
        if (!creation && canseemon(mon)) {
            pline(msgc_consequence, "%s %s for a moment.",
                  Tobjnam(best, "glow"),
                  hcolor("black"));
            best->bknown = 1; /* player learned that the object is cursed */
            best->mbknown = 1; /* monster did too */
        }
        curse(best);
    }

    /* if the hero puts on a cursed object, he gets "Oops, that felt deathly cold."
       and IDs the cursed state. Let monsters do so as well */
    if (best->cursed)
        best->mbknown = 1;

    if (old) {
        old->owt = weight(old);
        if (update_property(mon, objects[old->otyp].oc_oprop, slot))
            makeknown(old->otyp);
        update_property_for_oprops(mon, old, slot);
    }
    mon->misc_worn_check |= W_MASK(slot);
    best->owornmask |= W_MASK(slot);
    best->owt = weight(best);
    if (update_property(mon, objects[best->otyp].oc_oprop, slot))
        makeknown(best->otyp);
    update_property_for_oprops(mon, best, slot);
}

#undef RACE_EXCEPTION

/* Performs equipment prechecks. If stuff needs to be taken off first,
   it will end up removing said equipment instead of the given object. */
unsigned
equip(struct monst *mon, struct obj *obj,
               boolean on, boolean verbose)
{
    boolean tell = (verbose && canseemon(mon));
    int slot = which_slot(obj);

    if (!!obj->owornmask == on) /* object already on/off */
        return 0;

    /* check for stuff in the way */
    if (is_shirt(obj) || is_suit(obj)) {
        if (mon->misc_worn_check & W_MASK(os_armc))
            return do_equip(mon, which_armor(mon, os_armc), FALSE, verbose);
        else if (is_shirt(obj) && (mon->misc_worn_check & W_MASK(os_arm)))
            return do_equip(mon, which_armor(mon, os_arm), FALSE, verbose);
    }

    if (obj->oclass == RING_CLASS) {
        /* For rings, normally gloves are bypassed. However, rings can't be
           put on/off if the gloves are cursed, so we need to check for this
           seperately */
        if (mon->misc_worn_check & W_MASK(os_armg)) {
            struct obj *gloves = which_armor(mon, os_armg);
            if (gloves->cursed) {
                if (tell)
                    pline(msgc_info,
                          "%s tries to take off %s, but it is cursed.",
                          Monnam(mon), an(xname(gloves)));
                return 0;
            }
        }
    }

    /* slot taken by something else */
    if (mon->misc_worn_check & W_MASK(slot)) {
        if (obj->oclass == RING_CLASS) {
            if ((mon->misc_worn_check & W_MASK(os_ringl)) &&
                (mon->misc_worn_check & W_MASK(os_ringr)) &&
                on) {
                /* Since we can't determine what ring the monster wants to
                    unequip, code has to do that before calling equip() */
                impossible("monster is equipping a ring, but is out of slots");
                return 0;
            } else
                return do_equip(mon, obj, on, verbose);
        } else
            return do_equip(mon, which_armor(mon, W_MASK(slot)),
                            FALSE, verbose);
    }

    return do_equip(mon, obj, on, verbose);
}

unsigned
do_equip(struct monst *mon, struct obj *obj,
               boolean on, boolean verbose)
{
    boolean tell = (verbose && canseemon(mon));
    int prop = objects[obj->otyp].oc_oprop;
    int slot = which_slot(obj);
    boolean redundant = !!(has_property(mon, prop) & ~W_MASK(slot));
    redundant = redundant && !mworn_blocked(mon, prop);
    if (!on && obj->cursed) {
        if (tell)
            pline(msgc_info,
                  "%s tries to take off %s, but it is cursed.",
                  Monnam(mon), an(xname(obj)));
        return 0;
    }
    if (!on) {
        if (tell)
            pline(msgc_monneutral, "%s takes off %s.",
                  Monnam(mon), an(xname(obj)));
        mon->misc_worn_check &= ~obj->owornmask;
        obj->owornmask = 0L;
    } else {
        if (obj->oclass == RING_CLASS) {
            if (mon->misc_worn_check & W_MASK(os_ringl))
                obj->owornmask |= W_MASK(os_ringr);
            else
                obj->owornmask |= W_MASK(os_ringl);
        } else
            obj->owornmask |= W_MASK(slot);
        if (tell)
            pline(msgc_monneutral, "%s puts on %s.",
                  Monnam(mon), an(xname(obj)));
        mon->misc_worn_check |= obj->owornmask;
    }

    obj->owt = weight(obj);
    if (!redundant) {
        if (update_property(mon, prop, slot))
            makeknown(obj->otyp);
        update_property_for_oprops(mon, obj, slot);
        if (!mon->mhp) /* died (lost a critical property) */
            return 2;
    }

    mon->mfrozen += objects[obj->otyp].oc_delay;
    if (mon->mfrozen)
        mon->mcanmove = 0;

    return 1;
}

/* What kind of slot this item goes into */
unsigned
which_slot(const struct obj *otmp)
{
    enum objslot slot = os_invalid;
    if (is_suit(otmp))
        slot = os_arm;
    else if (is_cloak(otmp))
        slot = os_armc;
    else if (is_helmet(otmp))
        slot = os_armh;
    else if (is_shield(otmp))
        slot = os_arms;
    else if (is_gloves(otmp))
        slot = os_armg;
    else if (is_boots(otmp))
        slot = os_armf;
    else if (is_shirt(otmp))
        slot = os_armu;
    else if (otmp->oclass == AMULET_CLASS)
        slot = os_amul;
    else if (otmp->oclass == RING_CLASS || otmp->otyp == MEAT_RING)
        slot = os_ringl;
    else if (otmp->otyp == BLINDFOLD || otmp->otyp == LENSES ||
             otmp->otyp == TOWEL)
        slot = os_tool;
    else if (otmp->otyp == SADDLE)
        slot = os_saddle;
    return slot;
}
/* The centralised function for finding equipment; given an equipment slot,
   finds worn armor in that slot for the player (mon == &youmonst) or a
   monster. */
struct obj *
which_armor(const struct monst *mon, enum objslot slot)
{
    struct obj *obj;

    for (obj = mon->minvent; obj; obj = obj->nobj)
        if (obj->owornmask & W_MASK(slot))
            return obj;
    return NULL;
}

/* remove an item of armor and then drop it */
static void
m_lose_armor(struct monst *mon, struct obj *obj)
{
    struct level *lev = mon->dlevel;

    mon->misc_worn_check &= ~obj->owornmask;
    if (obj->owornmask) {
        update_property(mon, objects[obj->otyp].oc_oprop, which_slot(obj));
        update_property_for_oprops(mon, obj, which_slot(obj));
    }
    obj->owornmask = 0L;
    obj->owt = weight(obj);

    obj_extract_self(obj);
    place_object(obj, lev, mon->mx, mon->my);
    /* call stackobj() if we ever drop anything that can merge */
    if (lev == level)
        newsym(mon->mx, mon->my);
}

void
mon_break_armor(struct monst *mon, boolean polyspot)
{
    struct level *lev = mon->dlevel;
    struct obj *otmp;
    const struct permonst *mdat = mon->data;
    boolean show_msg = (lev == level);
    boolean vis = (lev == level && canseemon(mon));
    boolean handless_or_tiny = (nohands(mdat) || verysmall(mdat));
    const char *pronoun = vis ? mhim(mon) : NULL,
        *ppronoun = vis ? mhis(mon) : NULL;

    if (breakarm(mdat)) {
        if ((otmp = which_armor(mon, os_arm)) != 0) {
            if ((Is_dragon_scales(otmp) && mdat == Dragon_scales_to_pm(otmp)) ||
                (Is_dragon_mail(otmp) && mdat == Dragon_mail_to_pm(otmp)))
                ;
            /* no message here; "the dragon merges with his scaly armor" is odd
               and the monster's previous form is already gone */
            else if (show_msg) {
                if (vis)
                    pline(msgc_monneutral, "%s breaks out of %s armor!",
                          Monnam(mon), ppronoun);
                else
                    You_hear(msgc_levelsound, "a cracking sound.");
            }
            m_useup(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armc)) != 0) {
            if (otmp->oartifact) {
                if (vis)
                    pline(msgc_monneutral, "%s %s falls off!",
                          s_suffix(Monnam(mon)), cloak_simple_name(otmp));
                m_lose_armor(mon, otmp);
            } else {
                if (show_msg) {
                    if (vis)
                        pline(msgc_monneutral, "%s %s tears apart!",
                              s_suffix(Monnam(mon)), cloak_simple_name(otmp));
                    else
                        You_hear(msgc_levelsound, "a ripping sound.");
                }
                m_useup(mon, otmp);
            }
        }
        if ((otmp = which_armor(mon, os_armu)) != 0) {
            if (show_msg) {
                if (vis)
                    pline(msgc_monneutral, "%s shirt rips to shreds!",
                          s_suffix(Monnam(mon)));
                else
                    You_hear(msgc_levelsound, "a ripping sound.");
            }
            m_useup(mon, otmp);
        }
    } else if (sliparm(mdat)) {
        if ((otmp = which_armor(mon, os_arm)) != 0) {
            if (show_msg) {
                if (vis)
                    pline(msgc_monneutral, "%s armor falls around %s!",
                          s_suffix(Monnam(mon)), pronoun);
                else
                    You_hear(msgc_levelsound, "a thud.");
            }
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armc)) != 0) {
            if (vis) {
                if (is_whirly(mon->data))
                    pline(msgc_monneutral, "%s %s falls, unsupported!",
                          s_suffix(Monnam(mon)), cloak_simple_name(otmp));
                else
                    pline(msgc_levelsound, "%s shrinks out of %s %s!",
                          Monnam(mon), ppronoun, cloak_simple_name(otmp));
            }
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armu)) != 0) {
            if (vis) {
                if (sliparm(mon->data))
                    pline(msgc_monneutral, "%s seeps right through %s shirt!",
                          Monnam(mon), ppronoun);
                else
                    pline(msgc_levelsound,
                          "%s becomes much too small for %s shirt!",
                          Monnam(mon), ppronoun);
            }
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny) {
        /* [caller needs to handle weapon checks] */
        if ((otmp = which_armor(mon, os_armg)) != 0) {
            if (vis)
                pline(msgc_monneutral, "%s drops %s gloves%s!", Monnam(mon),
                      ppronoun, MON_WEP(mon) ? " and weapon" : "");
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_arms)) != 0) {
            if (show_msg) {
                if (vis)
                    pline(msgc_monneutral, "%s can no longer hold %s shield!",
                          Monnam(mon), ppronoun);
                else
                    You_hear(msgc_levelsound, "a clank.");
            }
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny || has_horns(mdat)) {
        if ((otmp = which_armor(mon, os_armh)) != 0 &&
            /* flimsy test for horns matches polyself handling */
            (handless_or_tiny || !is_flimsy(otmp))) {
            if (show_msg) {
                if (vis)
                    pline(msgc_monneutral, "%s %s falls to the %s!",
                          s_suffix(Monnam(mon)), helmet_name(otmp),
                          surface(mon->mx, mon->my));
                else if (is_metallic(otmp))  /* soft hats don't make a sound */
                    You_hear(msgc_levelsound, "a clank.");
            }
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny || slithy(mdat) || mdat->mlet == S_CENTAUR) {
        if ((otmp = which_armor(mon, os_armf)) != 0) {
            if (vis) {
                if (is_whirly(mon->data))
                    pline(msgc_monneutral, "%s boots fall away!",
                          s_suffix(Monnam(mon)));
                else
                    pline(msgc_monneutral, "%s boots %s off %s feet!",
                          s_suffix(Monnam(mon)),
                          verysmall(mdat) ? "slide" : "are pushed", ppronoun);
            }
            m_lose_armor(mon, otmp);
        }
    }
    if (!can_saddle(mon)) {
        if ((otmp = which_armor(mon, os_saddle)) != 0) {
            m_lose_armor(mon, otmp);
            if (vis)
                pline(mon->mtame ? msgc_petneutral : msgc_monneutral,
                      "%s saddle falls off.", s_suffix(Monnam(mon)));
        }
        if (mon == u.usteed)
            goto noride;
    } else if (mon == u.usteed && !can_ride(mon)) {
    noride:
        pline(msgc_statusend, "You can no longer ride %s.", mon_nam(mon));
        if (touch_petrifies(u.usteed->data) && !Stone_resistance && rnl(3)) {
            pline(msgc_fatal_predone, "You touch %s.", mon_nam(u.usteed));
            instapetrify(killer_msg(STONING,
                                    msgcat("falling of ", an(pm_name(u.usteed)))));
        }
        dismount_steed(DISMOUNT_FELL);
    }
    return;
}

/* Bias a monster's preferences towards armor that has special benefits.
   Currently does speed boots, various rings and for tame monsters, items
   thrown at it. */
int
extra_pref(const struct monst *mon, struct obj *obj)
{
    if (!obj)
        return 0;

    /* Check for throwntime */
    if (obj->thrown_time) {
        /* Figure out which item was thrown last for the appropriate slot. */
        struct obj *oobj;
        for (oobj = mon->minvent; oobj; oobj = oobj->nobj)
            if (oobj != obj && which_slot(oobj) == which_slot(obj) &&
                oobj->thrown_time > obj->thrown_time)
                break;

        if (!oobj)
            return 100;
    }

    /* Check for speed boots */
    if (obj->otyp == SPEED_BOOTS && !very_fast(mon))
        return 20;

    /* Don't wear boots of levitation for now */
    if (obj->otyp == LEVITATION_BOOTS)
        return -80;

    /* Check for rings or amulets below this */
    if (obj->oclass != RING_CLASS &&
        obj->oclass != AMULET_CLASS)
        return 0;

    /* Charge-based rings (increase X/protection) is seperate.
       Monsters currently don't use adornment or gain strength. */
    if (obj->otyp == RIN_ADORNMENT ||
        obj->otyp == RIN_GAIN_STRENGTH ||
        obj->otyp == RIN_GAIN_CONSTITUTION ||
        obj->otyp == RIN_INCREASE_ACCURACY ||
        obj->otyp == RIN_INCREASE_DAMAGE ||
        obj->otyp == RIN_PROTECTION) {
        int desire = 10;
        /* attribute rings are lower priority */
        if (obj->otyp == RIN_ADORNMENT ||
            obj->otyp == RIN_GAIN_STRENGTH ||
            obj->otyp == RIN_GAIN_CONSTITUTION)
            desire -= 5;
        /* nymphs or foocubi like adornment */
        if (obj->otyp == RIN_ADORNMENT &&
            (mon->data->mlet == S_NYMPH ||
             monsndx(mon->data) == PM_INCUBUS))
            desire += 20;
        else if (obj->spe <= 0 && obj->mknown) /* even if they're badly enchanted */
            return 0;
        if (!obj->mknown && !obj->mbknown)
            return desire + 3;
        if (!obj->mknown && obj->mbknown && obj->cursed)
            return -1; /* we don't know +N, but we know it's cursed, so avoid it */
        return (desire + 3*(obj->spe));
    }

    /* If something gives a redundant property, abort. */
    if ((obj->oclass == RING_CLASS &&
         m_has_property(mon, objects[obj->otyp].oc_oprop, ~W_RING, TRUE)) ||
        (obj->oclass != RING_CLASS &&
         m_has_property(mon, objects[obj->otyp].oc_oprop,
                        ~W_MASK(which_slot(obj)), TRUE)))
        return 0;

    /* If we already have an equipped ring of the same type on the left slot (not chargeable
       rings), ignore it. This introduces oddities when reorganizing rings, but is better
       than wearing duplicates (that would happen otherwise) */
    if (which_armor(mon, os_ringl) && obj->otyp == (which_armor(mon, os_ringl))->otyp)
        return 0;
    switch (obj->otyp) {
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION: /* only way for monsters to avoid digestion instadeath */
        return 50;
    case RIN_POISON_IMMUNITY:
    case RIN_REGENERATION:
    case AMULET_OF_REFLECTION:
        return 40;
    case RIN_SEE_INVISIBLE:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_ESP:
        return 30;
    case RIN_FIRE_IMMUNITY:
    case RIN_COLD_IMMUNITY:
    case RIN_SHOCK_IMMUNITY:
    case AMULET_OF_MAGICAL_BREATHING:
    case AMULET_OF_UNCHANGING:
        return 20;
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case AMULET_OF_RESTFUL_SLEEP:
        return 0; /* not desirable -- but the side effect is that monsters will still wear them if they lack better */
    case RIN_WARNING:
        /* Ring of warning is an OK replacement for see invis */
        return (see_invisible(mon) ? 5 : 25);
    case RIN_INVISIBILITY:
        if (!see_invisible(&youmonst)) {
            if (mon->mtame || mon->mpeaceful)
                return 0;
        }
        return 30;
    case RIN_TELEPORT_CONTROL:
        return (teleportitis(mon) ? 20 : 5);
    case RIN_POLYMORPH_CONTROL:
        return (polymorphitis(mon) ? 40 : 5);
    case RIN_TELEPORTATION:
        return (teleport_control(mon) ? 30 : 15);
    case RIN_POLYMORPH:
        return (polymorph_control(mon) ? 30 : 0);
    case AMULET_OF_LIFE_SAVING:
        return (nonliving(mon->data) ? 0 : 50);
    }
    return 0;
}

/*
 * Exceptions to things based on race. Correctly checks polymorphed player race.
 * Returns:
 *     0  No exception, normal rules apply.
 *     1  If the race/object combination is acceptable.
 *     -1 If the race/object combination is unacceptable.
 */
int
racial_exception(struct monst *mon, struct obj *obj)
{
    const struct permonst *ptr = raceptr(mon);

    /* Acceptable Exceptions: */
    /* Allow hobbits to wear elven armor - LoTR */
    if (ptr == &mons[PM_HOBBIT] && is_elven_armor(obj))
        return 1;
    /* Unacceptable Exceptions: */
    /* Checks for object that certain races should never use go here */
    /* return -1; */

    return 0;
}

/*worn.c*/

