/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static long find_extrinsic(struct obj *, int, int *, boolean *);
static void m_lose_armor(struct monst *, struct obj *);
static void m_dowear_type(struct monst *, enum objslot, boolean, boolean);

/* This only allows for one blocked property per item */
#define w_blocks(o,m) \
    ((o->otyp == MUMMY_WRAPPING && ((m) & W_MASK(os_armc))) ? INVIS :   \
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
        if (extrinsic == w_blocks(chain, chain->owornmask) ||
            (extrinsic == HALLUC && Halluc_resistance))
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
    return find_extrinsic(m_minvent(mon), extrinsic, &warntype, &blocked);
}

boolean
mworn_blocked(const struct monst *mon, int extrinsic)
{
    int warntype;
    boolean blocked;
    find_extrinsic(m_minvent(mon), extrinsic, &warntype, &blocked);
    return blocked;
}

int
mworn_warntype(const struct monst *mon)
{
    int warntype;
    boolean blocked;
    return find_extrinsic(m_minvent(mon), WARN_OF_MON, &warntype, &blocked)
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

void
mon_set_minvis(struct monst *mon)
{
    mon->perminvis = 1;
    if (!mon->invis_blkd) {
        mon->minvis = 1;
        if (mon->dlevel == level)
            newsym(mon->mx, mon->my);       /* make it disappear */
        if (mon->wormno)
            see_wsegs(mon);     /* and any tail too */
    }
}

void
mon_adjust_speed(struct monst *mon, int adjust, /* positive => increase speed,
                                                   negative => decrease */
                 struct obj *obj)
{       /* item to make known if effect can be seen */
    struct obj *otmp;
    boolean give_msg = !in_mklev, petrify = FALSE;
    unsigned int oldspeed = mon->mspeed;
    int oldmoverate = mcalcmove(mon);

    switch (adjust) {
    case 2:
        mon->permspeed = MFAST;
        give_msg = FALSE;       /* special case monster creation */
        break;
    case 1:
        if (mon->permspeed == MSLOW)
            mon->permspeed = 0;
        else
            mon->permspeed = MFAST;
        break;
    case 0:    /* just check for worn speed boots */
        break;
    case -1:
        if (mon->permspeed == MFAST)
            mon->permspeed = 0;
        else
            mon->permspeed = MSLOW;
        break;
    case -2:
        mon->permspeed = MSLOW;
        give_msg = FALSE;       /* (not currently used) */
        break;
    case -3:   /* petrification */
        /* take away intrinsic speed but don't reduce normal speed */
        if (mon->permspeed == MFAST)
            mon->permspeed = 0;
        petrify = TRUE;
        break;
    }

    for (otmp = mon->minvent; otmp; otmp = otmp->nobj)
        if (otmp->owornmask && objects[otmp->otyp].oc_oprop == FAST)
            break;
    if (otmp)   /* speed boots */
        mon->mspeed = MFAST;
    else
        mon->mspeed = mon->permspeed;

    if (give_msg && (mon->mspeed != oldspeed || petrify) && canseemon(mon)) {
        /* fast to slow (skipping intermediate state) or vice versa */
        const char *howmuch =
            (mon->mspeed + oldspeed == MFAST + MSLOW) ? "much " : "";

        if (petrify) {
            /* mimic the player's petrification countdown; "slowing down" even
               if fast movement rate retained via worn speed boots */
            if (flags.verbose)
                pline("%s is slowing down.", Monnam(mon));
        } else if (adjust > 0 || mon->mspeed == MFAST)
            pline("%s is suddenly moving %sfaster.", Monnam(mon), howmuch);
        else
            pline("%s seems to be moving %sslower.", Monnam(mon), howmuch);

        /* might discover an object if we see the speed change happen, but
           avoid making possibly forgotten book known when casting its spell */
        if (obj != 0 && obj->dknown &&
            objects[obj->otyp].oc_class != SPBOOK_CLASS)
            makeknown(obj->otyp);
    }

    adjust_move_offset(mon, oldmoverate, mcalcmove(mon));
}

/* armor put on or taken off; might be magical variety */
void
update_mon_intrinsics(struct monst *mon, struct obj *obj, boolean on,
                      boolean silently)
{
    int unseen;
    uchar mask;
    struct obj *otmp;
    int which = (int)objects[obj->otyp].oc_oprop;

    unseen = silently || !canseemon(mon);
    if (!which)
        goto maybe_blocks;

    if (on) {
        switch (which) {
        case INVIS:
            mon->minvis = !mon->invis_blkd;
            break;
        case FAST:
            {
                boolean save_in_mklev = in_mklev;

                if (silently)
                    in_mklev = TRUE;
                mon_adjust_speed(mon, 0, obj);
                in_mklev = save_in_mklev;
                break;
            }
            /* properties handled elsewhere */
        case ANTIMAGIC:
        case REFLECTING:
            break;
            /* properties which have no effect for monsters */
        case CLAIRVOYANT:
        case STEALTH:
        case TELEPAT:
            break;
            /* properties which should have an effect but aren't implemented */
        case LEVITATION:
        case WWALKING:
            break;
            /* properties which maybe should have an effect but don't */
        case DISPLACED:
        case FUMBLING:
        case JUMPING:
        case PROTECTION:
            break;
        default:
            if (which <= 8) {   /* 1 thru 8 correspond to MR_xxx mask values */
                /* FIRE,COLD,SLEEP,DISINT,SHOCK,POISON,ACID,STONE */
                mask = (uchar) (1 << (which - 1));
                mon->mintrinsics |= (unsigned short)mask;
            }
            break;
        }
    } else {    /* off */
        switch (which) {
        case INVIS:
            mon->minvis = mon->perminvis;
            break;
        case FAST:
            {
                boolean save_in_mklev = in_mklev;

                if (silently)
                    in_mklev = TRUE;
                mon_adjust_speed(mon, 0, obj);
                in_mklev = save_in_mklev;
                break;
            }
        case FIRE_RES:
        case COLD_RES:
        case SLEEP_RES:
        case DISINT_RES:
        case SHOCK_RES:
        case POISON_RES:
        case ACID_RES:
        case STONE_RES:
            mask = (uchar) (1 << (which - 1));
            /* If the monster doesn't have this resistance intrinsically, check 
               whether any other worn item confers it.  Note that we don't
               currently check for anything conferred via simply carrying an
               object. */
            if (!(mon->data->mresists & mask)) {
                for (otmp = mon->minvent; otmp; otmp = otmp->nobj)
                    if (otmp->owornmask &&
                        (int)objects[otmp->otyp].oc_oprop == which)
                        break;
                if (!otmp)
                    mon->mintrinsics &= ~((unsigned short)mask);
            }
            break;
        default:
            break;
        }
    }

maybe_blocks:
    /* obj->owornmask has been cleared by this point, so we can't use it.
       However, since monsters don't wield armor, we don't have to guard
       against that and can get away with a blanket worn-mask value. */
    switch (w_blocks(obj, ~0L)) {
    case INVIS:
        mon->invis_blkd = on ? 1 : 0;
        mon->minvis = on ? 0 : mon->perminvis;
        break;
    default:
        break;
    }

    if (!on && mon == u.usteed && obj->otyp == SADDLE)
        dismount_steed(DISMOUNT_FELL);

    /* if couldn't see it but now can, or vice versa, update display */
    if (!silently && (unseen ^ !canseemon(mon)) && mon->dlevel == level)
        newsym(mon->mx, mon->my);
}

int
find_mac(struct monst *mon)
{
    struct obj *obj;
    int base = mon->data->ac;
    long mwflags = mon->misc_worn_check;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        if (obj->owornmask & mwflags)
            base -= ARM_BONUS(obj);
        /* since ARM_BONUS is positive, subtracting it increases AC */
    }

    if (mon->mtame) {
        /* Hopefully, this codepath never runs; badly injured tame monsters
           aren't meant to be attacked. This is a fallback to keep the pets
           alive in the case that some hostile monster isn't calling
           mm_aggression correctly. */
        if (mon->mhp * 3 <= mon->mhpmax)
            base -= 20;
    }

    return base;
}

/* weapons are handled separately; rings and eyewear aren't used by monsters */

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
    boolean unseen = creation ? 1 : !canseemon(mon);
    const char *nambuf;

    if (mon->mfrozen)
        return; /* probably putting previous item on */

    /* Get a copy of monster's name before altering its visibility */
    nambuf = creation ? NULL : See_invisible ? Monnam(mon) : mon_nam(mon);

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
                (obj->otyp != AMULET_OF_LIFE_SAVING &&
                 obj->otyp != AMULET_OF_REFLECTION))
                continue;
            best = obj;
            goto outer_break;   /* no such thing as better amulets */
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
            if (!is_boots(obj))
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
outer_break:
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
            pline("%s%s puts on %s.", Monnam(mon), buf,
                  distant_name(best, doname));
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
            pline("%s %s for a moment.", Tobjnam(best, "glow"),
                  hcolor("black"));
        }
        curse(best);
    }
    if (old)
        update_mon_intrinsics(mon, old, FALSE, creation);
    mon->misc_worn_check |= W_MASK(slot);
    best->owornmask |= W_MASK(slot);
    update_mon_intrinsics(mon, best, TRUE, creation);
    /* if couldn't see it but now can, or vice versa, */
    if (!creation && (unseen ^ !canseemon(mon))) {
        if (mon->minvis && !See_invisible) {
            pline("Suddenly you cannot see %s.", nambuf);
            makeknown(best->otyp);
        }       /* else if (!mon->minvis) pline("%s suddenly appears!",
                   Amonnam(mon)); */
    }
}

#undef RACE_EXCEPTION

/* The centralised function for finding equipment; given an equipment slot,
   finds worn armor in that slot for the player (mon == &youmonst) or a
   monster. */
struct obj *
which_armor(const struct monst *mon, enum objslot slot)
{
    struct obj *obj;

    for (obj = m_minvent(mon); obj; obj = obj->nobj)
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
    if (obj->owornmask)
        update_mon_intrinsics(mon, obj, FALSE, FALSE);
    obj->owornmask = 0L;

    obj_extract_self(obj);
    place_object(obj, lev, mon->mx, mon->my);
    /* call stackobj() if we ever drop anything that can merge */
    if (lev == level)
        newsym(mon->mx, mon->my);
}

/* all objects with their bypass bit set should now be reset to normal */
void
clear_bypasses(void)
{
    struct obj *otmp, *nobj;
    struct monst *mtmp;

    for (otmp = level->objlist; otmp; otmp = nobj) {
        nobj = otmp->nobj;
        if (otmp->bypass) {
            otmp->bypass = 0;
            /* bypass will have inhibited any stacking, but since it's used for 
               polymorph handling, the objects here probably have been
               transformed and won't be stacked in the usual manner afterwards; 
               so don't bother with this */
        }
    }
    /* invent and migrating_pets chains shouldn't matter here */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
            otmp->bypass = 0;
    }
    for (mtmp = migrating_mons; mtmp; mtmp = mtmp->nmon) {
        for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
            otmp->bypass = 0;
    }
    flags.bypasses = FALSE;
}

void
bypass_obj(struct obj *obj)
{
    obj->bypass = 1;
    flags.bypasses = TRUE;
}

void
mon_break_armor(struct monst *mon, boolean polyspot)
{
    struct level *lev = mon->dlevel;
    struct obj *otmp;
    const struct permonst *mdat = mon->data;
    boolean show_msg = (lev == level);
    boolean vis = (lev == level && cansee(mon->mx, mon->my));
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
                    pline("%s breaks out of %s armor!", Monnam(mon), ppronoun);
                else
                    You_hear("a cracking sound.");
            }
            m_useup(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armc)) != 0) {
            if (otmp->oartifact) {
                if (vis)
                    pline("%s %s falls off!", s_suffix(Monnam(mon)),
                          cloak_simple_name(otmp));
                if (polyspot)
                    bypass_obj(otmp);
                m_lose_armor(mon, otmp);
            } else {
                if (show_msg) {
                    if (vis)
                        pline("%s %s tears apart!", s_suffix(Monnam(mon)),
                              cloak_simple_name(otmp));
                    else
                        You_hear("a ripping sound.");
                }
                m_useup(mon, otmp);
            }
        }
        if ((otmp = which_armor(mon, os_armu)) != 0) {
            if (show_msg) {
                if (vis)
                    pline("%s shirt rips to shreds!", s_suffix(Monnam(mon)));
                else
                    You_hear("a ripping sound.");
            }
            m_useup(mon, otmp);
        }
    } else if (sliparm(mdat)) {
        if ((otmp = which_armor(mon, os_arm)) != 0) {
            if (show_msg) {
                if (vis)
                    pline("%s armor falls around %s!", s_suffix(Monnam(mon)),
                          pronoun);
                else
                    You_hear("a thud.");
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armc)) != 0) {
            if (vis) {
                if (is_whirly(mon->data))
                    pline("%s %s falls, unsupported!", s_suffix(Monnam(mon)),
                          cloak_simple_name(otmp));
                else
                    pline("%s shrinks out of %s %s!", Monnam(mon), ppronoun,
                          cloak_simple_name(otmp));
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_armu)) != 0) {
            if (vis) {
                if (sliparm(mon->data))
                    pline("%s seeps right through %s shirt!", Monnam(mon),
                          ppronoun);
                else
                    pline("%s becomes much too small for %s shirt!",
                          Monnam(mon), ppronoun);
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny) {
        /* [caller needs to handle weapon checks] */
        if ((otmp = which_armor(mon, os_armg)) != 0) {
            if (vis)
                pline("%s drops %s gloves%s!", Monnam(mon), ppronoun,
                      MON_WEP(mon) ? " and weapon" : "");
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
        if ((otmp = which_armor(mon, os_arms)) != 0) {
            if (show_msg) {
                if (vis)
                    pline("%s can no longer hold %s shield!", Monnam(mon),
                          ppronoun);
                else
                    You_hear("a clank.");
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny || has_horns(mdat)) {
        if ((otmp = which_armor(mon, os_armh)) != 0 &&
            /* flimsy test for horns matches polyself handling */
            (handless_or_tiny || !is_flimsy(otmp))) {
            if (show_msg) {
                if (vis)
                    pline("%s %s falls to the %s!", s_suffix(Monnam(mon)),
                          helmet_name(otmp), surface(mon->mx, mon->my));
                else if (is_metallic(otmp))     /* soft hats don't make a sound 
                                                 */
                    You_hear("a clank.");
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
    }
    if (handless_or_tiny || slithy(mdat) || mdat->mlet == S_CENTAUR) {
        if ((otmp = which_armor(mon, os_armf)) != 0) {
            if (vis) {
                if (is_whirly(mon->data))
                    pline("%s boots fall away!", s_suffix(Monnam(mon)));
                else
                    pline("%s boots %s off %s feet!", s_suffix(Monnam(mon)),
                          verysmall(mdat) ? "slide" : "are pushed", ppronoun);
            }
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
        }
    }
    if (!can_saddle(mon)) {
        if ((otmp = which_armor(mon, os_saddle)) != 0) {
            if (polyspot)
                bypass_obj(otmp);
            m_lose_armor(mon, otmp);
            if (vis)
                pline("%s saddle falls off.", s_suffix(Monnam(mon)));
        }
        if (mon == u.usteed)
            goto noride;
    } else if (mon == u.usteed && !can_ride(mon)) {
    noride:
        pline("You can no longer ride %s.", mon_nam(mon));
        if (touch_petrifies(u.usteed->data) && !Stone_resistance && rnl(3)) {
            pline("You touch %s.", mon_nam(u.usteed));
            instapetrify(killer_msg(STONING,
                msgcat("falling of ", an(u.usteed->data->mname))));
        }
        dismount_steed(DISMOUNT_FELL);
    }
    return;
}

/* bias a monster's preferences towards armor that has special benefits. */
/* currently only does speed boots, but might be expanded if monsters get to
   use more armor abilities */
int
extra_pref(const struct monst *mon, struct obj *obj)
{
    if (obj) {
        if (obj->otyp == SPEED_BOOTS && mon->permspeed != MFAST)
            return 20;
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

