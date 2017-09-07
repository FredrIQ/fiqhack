/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Slot name for messages like "You are already wearing a shirt". */
static const char * const c_slotnames[] = {
    [os_arm]     = "suit",
    [os_armc]    = "cloak",
    [os_armh]    = "helmet",
    [os_arms]    = "shield",
    [os_armg]    = "gloves",
    [os_armf]    = "boots",
    [os_armu]    = "shirt",
    [os_amul]    = "amulet",
    [os_ringl]   = "ring",
    [os_ringr]   = "ring",
    [os_tool]    = "eyewear",
    [os_wep]     = "weapon",
    [os_swapwep] = "readied weapon",
    [os_quiver]  = "quivered ammunition",
};

/* Slot name as it appears in the doequip() menu */
static const char * const c_slotnames_menu[] = {
    [os_arm]     = "Armor",
    [os_armc]    = "Cloak",
    [os_armh]    = "Helmet",
    [os_arms]    = "Shield",
    [os_armg]    = "Gloves",
    [os_armf]    = "Boots",
    [os_armu]    = "Shirt",
    [os_amul]    = "Amulet",
    [os_ringl]   = "L. Ring",
    [os_ringr]   = "R. Ring",
    [os_tool]    = "Eyewear",
    [os_wep]     = "Weapon",
    [os_swapwep] = "Readied", /* special case: "Offhand" if twoweaponing */
    [os_quiver]  = "Quiver",
};
#define LONGEST_SLOTNAME 7 /* longest length of a c_slotnames_menu entry */

static const char c_that_[] = "that";

/* The order in which items should be equipped or unequipped, when the user
   requests multiple simultaneous equips.

   The basic rules here are that all slots should be unequipped before they are
   equipped, and that a slot can only be equipped or unequipped before every
   slot covering it is equipped or unequipped. We also group together actions
   that take less time when combined (e.g. unequipping then equipping a weapon
   takes 1 action when given as one command, 2 actions when given as two
   commands).

   For wield/ready/quiver slots, we don't have separate "equip" and "unequip"
   actions; we just change the item in-place. This can unequip it from slots
   it's already in in the process. */
enum equip_direction { ed_equip, ed_unequip, ed_swap };
static const struct equip_order {
    enum objslot slot;
    enum equip_direction direction;
} equip_order [] = {
    /* Tool comes first and last, because you'd typically want to be able to
       see what you're doing when switching out armor. */
    {os_tool,    ed_unequip},

    /* Hand slots. The weapon doesn't cover the glove slot (i.e. wielding a
       weapon doesn't make gloves any slower to equip), but it does block the
       glove and ring slot if cursed. Thus, we want to do this before swapping
       out the weapon, so that we do the right thing if the player specifies a
       ring and a cursed weapon as the items they want to equip. We also must
       unequip both rings before equipping either, in case the player wants to
       swap rings.

       TODO: We should really come to a decision on whether rings are worn above
       or below gloves. The code's currently inconsistent. Note that if rings
       can be covered by gloves, the logic in dounequip() will need changing. */
    {os_armg,    ed_unequip},
    {os_ringl,   ed_unequip},
    {os_ringr,   ed_unequip},
    {os_ringr,   ed_equip  },
    {os_ringl,   ed_equip  },
    {os_armg,    ed_equip  },

    /* Shields have to be unequipped before weapon changing, and equipped after,
       because a two-handed weapon conflicts with a shield. */
    {os_arms,    ed_unequip},

    /* Swap the weapon slots around next. swapwep/wep can be swapped in 0
       actions; swapwep and wep can each be changed in 1, without necessarily
       unequipping in between. The quiver takes 0 no matter what it's combined
       with. */
    {os_swapwep, ed_swap   }, /* must come immediately before uwep */
    {os_wep,     ed_swap   },
    {os_quiver,  ed_swap   },

    {os_arms,    ed_equip  },

    /* Body slots. */
    {os_armc,    ed_unequip},
    {os_arm,     ed_unequip},
    {os_armu,    ed_unequip},
    {os_armu,    ed_equip  },
    {os_arm,     ed_equip  },
    {os_armc,    ed_equip  },

    /* Slots that don't block anything. */
    {os_amul,    ed_unequip},
    {os_amul,    ed_equip},
    {os_armh,    ed_unequip},
    {os_armh,    ed_equip},
    {os_armf,    ed_unequip},
    {os_armf,    ed_equip},

    /* And finally, put the blindfold back on. */
    {os_tool,    ed_equip  },
};

static boolean canwearobjon(struct obj *, enum objslot,
                            boolean, boolean, boolean);
static void on_msg(struct obj *);
static void already_wearing(const char *);

static boolean
slot_covers(enum objslot above, enum objslot beneath)
{
    /* Weapons do not currently cover rings or gloves. A cursed weapon prevents
       a ring or glove being removed; but a noncursed weapon does not slow the
       removal of a ring or glove. */
    /* TODO: Currently, neither ring nor glove covers the other. We should
       change that. */
    if (above == os_armc)
        return beneath == os_arm || beneath == os_armu;
    if (above == os_arm)
        return beneath == os_armu;
    return FALSE;
}

static void
on_msg(struct obj *otmp)
{
    if (flags.verbose) {
        const char *how = "";

        if (otmp->otyp == TOWEL)
            how = msgprintf(" around your %s", body_part(HEAD));

        pline(msgc_actionboring, "You are now %s %s%s.%s",
              otmp->owornmask & W_MASK(os_arms) ? "holding" : "wearing",
              obj_is_pname(otmp) ? the(xname(otmp)) : an(xname(otmp)), how,
              Hallucination && otmp->otyp == BLACK_DRAGON_SCALE_MAIL
                  ? " Kinky." : "");
    }
}

/* Changes which item is placed in a slot. This is a reasonably low-level
   function; it doesn't check whether the change is possible, it just does it.
   Likewise, it takes zero time. (The assumption is that the caller has either
   checked things like time and possibility, or else the item's being unequipped
   magically, being destroyed, or something similar.) The main reason for the
   existence of this function is to print messages (if msgtype != em_silent),
   and to trigger side effects of equipping (such as identifying the enchantment
   of equipped armor, or changing the gender of a player who equips an amulet
   of change).

   This function currently only works for A_WORN slots (not weapon, secondary
   weapon, or quiver). TODO: Merge this with the wield code so that it works
   with A_EQUIP slots in general.

   Returns TRUE if attempting to equip the item destroyed it (e.g. amulet of
   change). */
boolean
setequip(enum objslot slot, struct obj *otmp, enum equipmsg msgtype)
{
    int oldcap = near_capacity();
    /* o holds the item that is being equipped or removed. */
    struct obj *o = otmp;
    /* Work out whether we're equipping, unequipping, both, or neither. */
    if (o == &zeroobj)
        o = NULL;
    if (!o)
        o = EQUIP(slot); /* we're unequipping */
    else if (EQUIP(slot))
        setequip(slot, NULL, msgtype); /* empty the slot to equip into it */
    if (!o)
        return FALSE; /* nothing to do */
    boolean equipping = otmp && otmp != &zeroobj;
    int otyp = o->otyp;
    uint64_t props = obj_properties(o);
    int prop = objects[otyp].oc_oprop;
    /* TODO: Effects that are redundant to racial properties. I'm not sure if
       this can actually come up, but we should handle it anyway. */
    boolean redundant = FALSE;
    if (prop) {
        boolean redundant = !!(has_property(&youmonst, prop) & ~W_MASK(slot));
        redundant = redundant && !worn_blocked(prop);
    }
    boolean destroyed = 0;

    /* Change the item in the slot. */
    if (equipping) {
        setworn(o, W_MASK(slot));
        o->owt = weight(o);
        /* There is a redundant update_property when taken off.
           The reason this is performed at this point is to give
           the identified description if update_property identifies
           the item */
        if (update_property(&youmonst, prop, slot))
            makeknown(o->otyp);
        update_property_for_oprops(&youmonst, o, slot);
        if (o->spe)
            learn_oprop(o, (opm_dexterity | opm_brilliance));
        learn_oprop(o, opm_power | opm_oilskin | opm_carrying);
        if (props & opm_power)
            oldcap = encumber_msg(oldcap);
        update_inventory();
        if (msgtype != em_silent)
            on_msg(o);
        if (o->cursed && !o->bknown) {
            o->bknown = TRUE;
            if (msgtype != em_silent)
                pline(msgc_substitute, "Oops!  %s deathly cold.",
                      is_plural(o) ? "They feel" : "That feels");
        }
    } else {
        setworn(NULL, W_MASK(slot));
        o->owt = weight(o);
        /* It's not obvious whether we should uninvoke or not here. We need to
           uninvoke if the item is being destroyed or dropped, but not
           otherwise. However, of the items with togglable invokes, the Sceptre
           of Might is a weapon (thus doesn't go into armor slots); the Heart
           of Ahriman is a luckstone and thus can't be worn; and the Orb of
           Detection is a crystal ball and thus also can't be worn.

           Thus, uninvoking unconditionally works just fine; there are no
           items that trigger the case where it wouldn't work. */
        if (o->oartifact)
            uninvoke_artifact(o);
        if (msgtype == em_voluntary)
            pline(msgc_actionboring, "You take off %s.", yname(o));
        else if (msgtype == em_magicheal)
            pline(msgc_statusheal, "%s falls off your %s!", Yname2(o),
                  (slot == os_ringl || slot == os_ringr) ?
                  body_part(FINGER) :
                  slot == os_amul ? body_part(NECK) :
                  slot == os_tool ? body_part(FACE) : "body");
        if (update_property(&youmonst, prop, slot))
            makeknown(o->otyp);
        update_property_for_oprops(&youmonst, o, slot);
        if (o->spe)
            learn_oprop(o, (opm_dexterity | opm_brilliance));
        learn_oprop(o, opm_power);
        if (props & opm_power)
            oldcap = encumber_msg(oldcap);
        update_inventory();
    }

    /* Equipping armor makes its enchantment obvious. */
    if (slot <= os_last_armor)
        o->known = TRUE;

    /* Side effects for all armor in the game.

       WARNING: Some of these can destroy objects. That includes the object
       we're trying to equip! Thus, o and otmp should not be used after this
       switch returns. (Checking EQUIP(slot) is fine, though.) */
    enum msg_channel good_msgc = redundant ? msgc_statusextend :
        equipping ? msgc_statusgood : msgc_statusend;
    enum msg_channel bad_msgc = equipping ? msgc_statusbad : msgc_statusheal;
    switch (otyp) {
        /* Boots */
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case KICKING_BOOTS:
    case JUMPING_BOOTS:
    case WATER_WALKING_BOOTS:
    case SPEED_BOOTS:
    case ELVEN_BOOTS:
    case FUMBLE_BOOTS:
    case GAUNTLETS_OF_FUMBLING:
        /* Cloaks */
    case ELVEN_CLOAK:
    case CLOAK_OF_PROTECTION:
    case CLOAK_OF_DISPLACEMENT:
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case ROBE:
    case LEATHER_CLOAK:
    case ALCHEMY_SMOCK:
    case MUMMY_WRAPPING:
    case CLOAK_OF_INVISIBILITY:
        break;
    case OILSKIN_CLOAK:
        if (equipping) {
            pline(good_msgc, "%s very tightly.", Tobjnam(o, "fit"));
            makeknown(otyp);
        }
        break;

        /* Helmets */
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
    case HELM_OF_TELEPATHY:
        break;
    case CORNUTHAUM:
        /* people think marked wizards know what they're talking about, but it
           takes trained arrogance to pull it off, and the actual enchantment
           of the hat is irrelevant.
           This increases (Wizards) or decrease charisma, so auto-ID it */
        makeknown(otyp);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        if (!equipping)
            u.ualign.type = u.ualignbase[A_CURRENT];
        else if (u.ualign.type == A_NEUTRAL)
            u.ualign.type = rn2_on_rng(2, rng_helm_alignment) ?
                A_CHAOTIC : A_LAWFUL;
        else
            u.ualign.type = -(u.ualign.type);
        u.ualign.record = -1;   /* consistent with altar conversion */
        u.ublessed = 0; /* lose the appropriate god's protection */
        /* Run makeknown() only after printing messages */
        /*FALLTHRU*/
    case DUNCE_CAP:
        if (equipping && !o->cursed) {
            if (Blind)
                pline(bad_msgc, "%s for a moment.", Tobjnam(o, "vibrate"));
            else
                pline(bad_msgc, "%s %s for a moment.", Tobjnam(o, "glow"),
                      hcolor("black"));
            curse(o);
            o->bknown = TRUE;
        }
        if (Hallucination) {
            /* from Monty Python's Flying Circus */
            pline(bad_msgc, "My brain hurts!");
        } else if (equipping && otyp == DUNCE_CAP) {
            pline(bad_msgc, "You feel %s.",   /* track INT change; ignore WIS */
                  ACURR(A_INT) <=
                  (ABASE(A_INT) + ATEMP(A_INT)) ?
                  "like sitting in a corner" : "giddy");
        } else if (otyp == HELM_OF_OPPOSITE_ALIGNMENT) {
            pline(bad_msgc, "Your mind oscillates briefly.");
        }
        makeknown(otyp);
        break;

        /* Gloves */
    case LEATHER_GLOVES:
        break;
        /* gauntlets of fumbling handled by the boots codepath */
    case GAUNTLETS_OF_POWER:
        makeknown(otyp);
        break;

        /* Amulets */
    case AMULET_OF_ESP:
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_MAGICAL_BREATHING:
    case FAKE_AMULET_OF_YENDOR:
    case AMULET_OF_UNCHANGING:
        break;
    case AMULET_OF_CHANGE:
        if (equipping) {
            int orig_sex = poly_gender();

            if (Unchanging)
                break;
            change_sex();
            /* Don't use same message as polymorph */
            if (orig_sex != poly_gender())
                pline(msgc_intrloss, "You are suddenly very %s!",
                      u.ufemale ? "feminine" : "masculine");
            else
                /* already polymorphed into single-gender monster; only changed
                   the character's base sex */
                pline(msgc_intrloss, "You don't feel like yourself.");
            pline(msgc_consequence, "The amulet disintegrates!");
            destroyed = 1;
            makeknown(otyp);
            useup(o);
        }
        break;
    case AMULET_OF_STRANGULATION:
    case AMULET_OF_RESTFUL_SLEEP:
    case AMULET_OF_YENDOR:
        break;

        /* Rings */
    case RIN_TELEPORTATION:
    case RIN_REGENERATION:
    case RIN_SEARCHING:
    case RIN_STEALTH:
    case RIN_HUNGER:
    case RIN_AGGRAVATE_MONSTER:
    case RIN_POISON_RESISTANCE:
    case RIN_FIRE_RESISTANCE:
    case RIN_COLD_RESISTANCE:
    case RIN_SHOCK_RESISTANCE:
    case RIN_CONFLICT:
    case RIN_TELEPORT_CONTROL:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case RIN_FREE_ACTION:
    case RIN_SLOW_DIGESTION:
    case RIN_SUSTAIN_ABILITY:
    case MEAT_RING:
    case RIN_WARNING:
    case RIN_SEE_INVISIBLE:
    case RIN_INVISIBILITY:
    case LEVITATION_BOOTS: /* moved from the boots section */
    case RIN_LEVITATION:
    case RIN_INCREASE_ACCURACY:
    case RIN_INCREASE_DAMAGE:
        break;
    case HELM_OF_BRILLIANCE:
    case GAUNTLETS_OF_DEXTERITY:
    case RIN_GAIN_STRENGTH:
    case RIN_GAIN_CONSTITUTION:
    case RIN_ADORNMENT:
    case RIN_PROTECTION:
        if (o->spe || objects[otyp].oc_name_known) {
            makeknown(o->otyp);
            o->known = 1;
            update_inventory();
        }
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
        break;

        /* tools */
    case BLINDFOLD:
    case TOWEL:
    case LENSES:
        break;

        /* Shields, shirts, body armor: no special cases! */
    default:
        if (slot != os_arms && slot != os_armu && slot != os_arm) {
            impossible("otyp %d is missing equip code in setequip()",
                       otyp);
        }
        break;
    }

    /* For gauntlets of power, etc */
    encumber_msg(oldcap);

    /* at this point o, otmp are invalid */

    /* Prevent wielding cockatrice when not wearing gloves */
    if (uwep && !uarmg && uwep->otyp == CORPSE &&
        touch_petrifies(&mons[uwep->corpsenm])) {
        pline(msgc_badidea, "You wield the %s in your bare %s.",
              corpse_xname(uwep, TRUE), makeplural(body_part(HAND)));
        instapetrify(killer_msg(STONING,
            msgprintf("removing %s gloves while wielding %s", uhis(),
                      an(corpse_xname(uwep, TRUE)))));
        uwepgone();     /* life-saved still doesn't allow touching cockatrice */
    }

    /* KMH -- ...or your secondary weapon when you're wielding it */
    if (u.twoweap && uswapwep && uswapwep->otyp == CORPSE &&
        touch_petrifies(&mons[uswapwep->corpsenm])) {

        pline(msgc_badidea, "You wield the %s in your bare %s.",
              corpse_xname(uswapwep, TRUE), body_part(HAND));
        instapetrify(killer_msg(STONING,
            msgprintf("removing %s gloves while wielding %s", uhis(),
                      an(corpse_xname(uwep, TRUE)))));
        uswapwepgone(); /* lifesaved still doesn't allow touching cockatrice */
    }

    return destroyed;
}

/* Convenience wrapper around setequip. */
void
setunequip(struct obj *otmp)
{
    if (otmp->owornmask & W_WORN)
        setequip(objslot_from_mask(otmp->owornmask & W_WORN),
                 NULL, em_silent);
}

enum objslot
objslot_from_mask(int wearmask)
{
    enum objslot i;
    for (i = 0; i <= os_last_maskable; i++)
        if ((wearmask & W_MASKABLE) == W_MASK(i))
            return i;
    panic("Could not find equipment slot for wear mask %d", wearmask);
}

/* Called in main to set extrinsics of worn start-up items. TODO: The call
   pattern of this function looks /very/ suspect. I suspect that it's always a
   no-op in practice. If it isn't, the way it's being called is wrong. */
void
set_wear(void)
{
    int i;
    /* We wear the items in the reverse of their takeoff order. */
    for (i = (sizeof equip_order) / (sizeof *equip_order) - 1; i; i--)
        if (equip_order[i].slot <= os_last_armor &&
            equip_order[i].direction == ed_equip &&
            EQUIP(equip_order[i].slot)) {
            setequip(equip_order[i].slot, EQUIP(equip_order[i].slot),
                     em_silent);
        }
}

/* The commands wxQWTRPA (and the use of a to equip eye-slot items) are all now
   merged into one standard API for equipping and unequipping. (There are still
   some codepaths for equipping that don't use this API; for instance, the use
   of V to wield a lamp, or a to wield a pick-axe. That happens when a command
   needs to equip an item and do something else in the same action; in the case
   of the lamp, it would be rubbing it.)

   u.utracked[tos_first_equip ... tos_last_equip] hold the item that the player
   wants to place in each swap; this is a persistent value, saved in the save
   file. They can be legally set to any valid object pointer, regardless of
   whether it's appropriate for the slot, or even whether it's in the player's
   inventory. Use &zeroobj to request that nothing is stored in the slot; NULL
   represents that no change is desired. We use this convention for two reasons:
   a) because if an object is destroyed while in utracked, the corresponding
   slot of utracked becomes NULL; b) because that way, if an item is stolen
   independently of the equip code, the player won't automatically re-equip it
   if they subsequently pick it up.

   This function handles the actual equipping. The rules are as follows:

   - If an action can't even be attempted because the item isn't in inventory,
     it's skipped entirely. This means that the player can start to wear an
     item, be interrupted, and choose to continue wearing once they reclaim it,
     with the benefits of their accumulated progress. (TODO: This makes no
     sense at all.)

   - If the action can be attempted but doesn't work (e.g. is blocked by a
     cursed item), we print out a message and NULL the slot. This takes 1
     action, or 0 if the action could normally be done for free, rather than the
     normal time it takes to equip the item.

   - If all the actions that the player wants to perform are free (such as
     quivering, or swapping the main and offhand), then this function performs
     all of them (printing appropriate messages), and returns 0.

   - Otherwise, it increases the action reservoir (uoccupation_progress for the
     appropriate slot) by 1, and compares it to the length of time taken by the
     next action to perform:

       - If the action takes more time than is in the reservoir, the function
         returns 2 with no further action.

       - If the action takes no more time than is in the reservoir, then the
         time is taken from the reservoir and the action is performed. (The slot
         is NULLed out in response, except when unequipping an item temporarily
         to put on another, when it's set to the old item.) Then any number of
         zero-time actions that might be scheduled to happen after that occur,
         and then the function returns 2 if there are actions remaining, 1
         otherwise.

   The correct use for this function in a command, then, is to set utracked,
   reset the reservoir if any objects were removed from utracked, then call
   equip_heartbeat. If it returns 0 or 1, return the same value. If it returns
   2, call action_incomplete(), then return 1. */
static inline boolean
want_to_change_slot(enum objslot os)
{
    struct obj *current = EQUIP(os);
    struct obj *desired = u.utracked[tos_first_equip + os];
    if (desired == NULL) return FALSE;
    if (desired == &zeroobj && current == NULL) return FALSE;
    if (desired == current) return FALSE;
    if (desired != &zeroobj && desired->where != OBJ_INVENT)
        return FALSE; /* no change yet */
    return TRUE;
}
static inline boolean
free_to_change_slot(struct obj *obj, boolean noisy,
                    boolean spoil, boolean cblock)
{
    if (obj == &zeroobj || !obj)
        return TRUE;
    if (!(obj->owornmask & W_EQUIP))
        return TRUE;
    return canunwearobj(obj, noisy, spoil, cblock);
}
int
equip_heartbeat(void)
{
    int i;
    enum objslot j;
    boolean reservoir_increased = FALSE;

    /* Loop over the equip order, performing equip actions in equip order. */
    for (i = 0; i < (sizeof equip_order) / (sizeof *equip_order); i++) {
        enum objslot islot = equip_order[i].slot;
        struct obj *desired = u.utracked[tos_first_equip + islot];
        struct obj *current = EQUIP(islot);
        boolean temp_unequip = FALSE;
        boolean fast_weapon_swap = FALSE;
        boolean cant_equip = FALSE;
        int action_cost = 0;

        /* Do we want to temporarily unequip an item to equip it later? */
        if (equip_order[i].direction == ed_unequip)
            for (j = 0; j <= os_last_equip; j++) {
                if (islot == j) continue;
                if (slot_covers(islot, j) && want_to_change_slot(j))
                    temp_unequip = TRUE;
            }

        /* In the special case of a player trying to equip an object over
           itself, we want to print a message and then remove the desire to
           change the slot. (We might nonetheless remove the object, if it's
           blocking an equip that the player wants to do.) */
        if (current && desired == current) {
            pline(msgc_mispaste, "You are already %s %s.",
                  islot == os_wep ? "wielding" :
                  islot == os_swapwep ? "readying" :
                  islot == os_quiver ? "quivering" : "wearing", yname(current));
            u.utracked[tos_first_equip + islot] = desired = NULL;
        }

        /* Likewise, we have a special case for trying to unequip an object
           over an empty slot. */
        if (!current && desired == &zeroobj) {
            pline(msgc_mispaste, "You have no %s equipped.",
                  c_slotnames[islot]);
            u.utracked[tos_first_equip + islot] = desired = NULL;
        }

        /*
         * Swapping weapons is implemented as a special case of the first action
         * that could represent it (either "unequip swapwep", or "equip swapwep"
         * if we're swapping a main weapon into an empty offhand slot); that's
         * the point at which we must decide whether it's a swap or a standard
         * wield/unwield. A partial swap (main weapon into readied slot, empty
         * main weapon slot) is also zero actions if the main weapon slot is
         * filled, so as to prevent the pushweapon option allowing you to do
         * things faster than you otherwise would be able to. This further
         * implies that it's possible to empty the weapon or offhand slot in
         * zero time via repeated swapping, so we add that as a separate case
         * (that isn't a fast weapon swap internally, though; just an unequip
         * that happens to take zero time).
         *
         * All the swap-like cases which take zero time have uwep == desired,
         * and it turns out that there are no other restrictions. If this
         * condition is met, we set fast_weapon_swap, which overrides the time
         * taken to 0, and informs the later code that the main weapon will need
         * to be swapped into the offhand slot (and possibly vice versa). This
         * also has the side-effect of cancelling the unequip in the offhand
         * slot if the mainhand weapon turns out to be cursed; this is
         * reasonable behaviour, though, so it's just allowed to happen. (The
         * opposite does not happen; there's no reason to not go through with
         * the swap, and get welded hands, if it's the offhand weapon that's
         * cursed.)
         *
         * If the player desires to move the offhand slot to the mainhand slot,
         * but not vice versa, there's no way to do that in zero time, unless
         * the offhand slot is to end up empty. It can, however, be done in one
         * action (fast swap, followed by re-equipping the offhand slot), and
         * this is perfectly safe (moving a cursed item to offhand is not a
         * problem, moving a cursed item to mainhand is what the user requested
         * so it's OK to do even if the item is cursed). Therefore, we also
         * enable the fast swap in this case. We don't use it if the offhand is
         * empty and the mainhand is desired empty, though; just unequipping the
         * mainhand directly works just as fast in that case, and is clearer.
         */
        if (islot == os_swapwep &&
            (desired == uwep || (desired == &zeroobj && uwep == NULL) ||
             (current && current == u.utracked[tos_first_equip + os_wep])))
            fast_weapon_swap = TRUE;

        if (!want_to_change_slot(islot) && !temp_unequip) continue;

        /* If the slot is empty and this is an unequip entry, do nothing. */
        if (equip_order[i].direction == ed_unequip && !current)
            continue;
        /* If we want to empty the slot and this is an equip entry, do
           nothing. This case is probably unreachable, because the unequip
           should have been checked first. The !desired check is definitely
           unreachable - it's checked in want_to_change_slot - but that means
           that the compiler will just optimize it out, and it's there to
           avoid null-pointer-related crashes if the code ever changes. */
        if (equip_order[i].direction == ed_equip && !fast_weapon_swap &&
            (!desired || desired == &zeroobj))
            continue;

        /* Before spending any time, check to see if the character knows
           the action to be impossible. */
        if (fast_weapon_swap) {
            /* Note: we don't check to see if we can ready the currently wielded
               weapon; we can't, because it's currently wielded. If the unwield
               works, the ready will also work, though. We need three checks;
               two to see if the swap is possible, one that allows for a
               potential slow equip afterwards. */
            cant_equip =
                (uwep && !canunwearobj(uwep, TRUE, FALSE, FALSE)) ||
                !canwieldobj(uswapwep, TRUE, FALSE, FALSE) ||
                (desired != uwep && !canreadyobj(desired, TRUE, FALSE, FALSE));
        } else if (equip_order[i].direction == ed_unequip) {
            cant_equip = !canunwearobj(current, TRUE, FALSE, FALSE);
        } else if (equip_order[i].direction == ed_swap) {
            cant_equip = !free_to_change_slot(current, TRUE, FALSE, FALSE) ||
                (islot == os_wep ? !canwieldobj(desired, TRUE, FALSE, FALSE) :
                 !canreadyobj(desired, TRUE, FALSE, FALSE));
        } else {
            cant_equip = !canwearobjon(desired, islot, TRUE, FALSE, FALSE);
        }

        if (cant_equip) {
            /* Abort the attempt to equip. */
            u.utracked[tos_first_equip + islot] = NULL;
            /* On an impossible swap, also cancel the wield, to avoid duplicate
               messages. */
            if (fast_weapon_swap)
                u.utracked[tos_first_equip + os_wep] = NULL;
            /* Move on without costing time. */
            continue;
        }

        /*
         * At this point, the character is going to attempt to equip/unequip the
         * item. Does he/she/it have time to do so?
         *
         * The timings used for equipping in NetHack up to 4.2 depended on the
         * command used, and were inconsistent in some cases:
         *
         * Q   0 actions
         * x   0 actions (in AceHack, at least; 1 in 3.4.3 and earlier)
         * w   1 action
         * a   1 action
         * P   1 action
         * R   1 action
         * W   1 action  + oc_delay turns
         * T   1 action  + oc_delay turns
         * A   0 actions + max(oc_delay, 1) actions; except 2 for os_tool
         *
         * (This has been verified experimentally as well as via reading the
         * code.) This means that A is normally faster (except for removing
         * blindfolds and lenses), but T is faster for armor with a long delay,
         * if you're polymorphed into something slower than normal speed. (This
         * might come up if, say, you're polymorphed into a dwarf lord.)
         *
         * The timings in NetHack 4.3 depend on the slot, not the command, and
         * are always measured entirely in actions (technically, because that's
         * how occupation callbacks work; but it also makes sense in that speed
         * should allow you to put on armor faster):
         *
         * os_quiver               0 for equip, unequip, change
         * os_wep/swapwep          0 if swapping os_wep and os_swapwep
         *                         0 to move os_wep to os_swapwep, emptying it
         *                         1 for equip, unequip, change
         * os_ring[lr]/amul/blindf 1 for equip, unequip
         *                         2 to change
         * os_arm*                 1 + oc_delay for equip, unequip
         *                         change takes as much time as equip + unequip
         *
         * The time spent in 4.3 is thus usually a little slower than A, and a
         * little faster than W/T. (It's equal in the case of the non-armor
         * slots, which had consistent timing anyway.)
         */
        switch (islot) {
        case os_quiver:
            action_cost = 0;
            break;
        case os_wep:
        case os_swapwep:
            action_cost = fast_weapon_swap ? 0 : 1;
            break;
        case os_ringl:
        case os_ringr:
        case os_amul:
        case os_tool:
            action_cost = 1;
            break;
        default:
            /* This is the code to change if it's ever desired to make the
               delays for equipping and unequipping asymmetrical. */
            if (equip_order[i].direction == ed_unequip)
                action_cost = 1 + objects[current->otyp].oc_delay;
            else
                action_cost = 1 + objects[desired->otyp].oc_delay;
            break;
        }

        /* If the action is free, or if we have at least 1 action in the
           reservoir, or if we have leeway to increase the reservoir, check to
           see if it's possible, and stop trying to perform it if it isn't. */
        if (action_cost == 0 || !reservoir_increased ||
            u.uoccupation_progress[tos_first_equip + islot]) {

            /* Objects can always be wielded/readied/quivered unless they're
               already worn in some other slot. However, there may be a
               replacement item. This code calls the appropriate check functions
               to ensure that the equip is possible.

               There's no need for twoweapon checks, because we can fix such
               problems simply by ending twoweapon.

               These are similar to the earlier checks, except now we have spoil
               == TRUE, because the character's actually spending time
               attempting to equip the item. */
            if (fast_weapon_swap) {
                cant_equip =
                    (uwep && !canunwearobj(uwep, TRUE, TRUE, FALSE)) ||
                    !canwieldobj(uswapwep, TRUE, TRUE, FALSE) ||
                    (desired != uwep &&
                     !canreadyobj(desired, TRUE, TRUE, FALSE));
            } else if (equip_order[i].direction == ed_unequip) {
                cant_equip = !canunwearobj(current, TRUE, TRUE, FALSE);
            } else if (equip_order[i].direction == ed_swap) {
                cant_equip =
                    !free_to_change_slot(current, TRUE, FALSE, FALSE) ||
                    (islot == os_wep ?
                     !canwieldobj(desired, TRUE, TRUE, FALSE) :
                     !canreadyobj(desired, TRUE, TRUE, FALSE));
            } else {
                cant_equip = !canwearobjon(desired, islot, TRUE, TRUE, FALSE);
            }

            if (cant_equip && action_cost)
                action_cost = 1;
        }

        /* Compare the cost to the time we've already spent trying to equip the
           item. If it's too high, spend an action (unless it was spent earlier
           in this function; we can only spend one per call because we need to
           give the monsters a chance to move), then check again. */
        if (action_cost > u.uoccupation_progress[tos_first_equip + islot] &&
            !reservoir_increased) {
            u.uoccupation_progress[tos_first_equip + islot]++;
            reservoir_increased = TRUE;
        }
        /* If we still don't have enough time, let the caller know, and
           continue with the equip later. */
        if (action_cost > u.uoccupation_progress[tos_first_equip + islot])
            return 2;

        u.uoccupation_progress[tos_first_equip + islot] -= action_cost;

        /* If the equip action is impossible, then now we've paid the 1 action
           cost for discovering that, cancel the action and move on. */
        if (cant_equip) {
            u.utracked[tos_first_equip + islot] = NULL;
            /* Cancel the wield, too, on an impossible swap. The result could
               otherwise include duplicate messages. */
            if (fast_weapon_swap)
                u.utracked[tos_first_equip + os_wep] = NULL;
            continue;
        }

        /* At this point, the equip/unequip will happen. */
        if (fast_weapon_swap) {
            /* The fast weapon swap is the only thing we do this time round the
               loop. If there's a slow offhand equip after that, we do it on the
               next iteration. */
            if (u.utracked[tos_first_equip + os_wep] == current ||
                (!current &&
                 u.utracked[tos_first_equip + os_wep] == &zeroobj)) {
                /* code above enforces: desired is uwep, current is uswapwep */
                setuswapwep(NULL);
                ready_weapon(current);
                if (uwep == current) { /* it worked */
                    setuswapwep(desired == &zeroobj ? NULL : desired);
                    if (desired == &zeroobj)
                        pline(msgc_actionboring,
                              "You have no secondary weapon readied.");
                    else
                        prinv(NULL, desired, 0L);
                } else { /* something went wrong equipping */
                    setuswapwep(current);
                }
            } else { /* half-swap: uwep is readied, uswapwep is unequipped */
                ready_weapon(NULL);
                if (desired != &zeroobj && !(desired->owornmask & W_EQUIP))
                    setuswapwep(desired == &zeroobj ? NULL : desired);
                    if (desired == &zeroobj)
                        pline(msgc_actionboring,
                              "You have no secondary weapon readied.");
                    else
                        prinv(NULL, desired, 0L);
            }
            /* If we swapped the object we wanted into a slot, remember that
               the slot no longer has a desire to change. */
            if (!want_to_change_slot(os_swapwep))
                u.utracked[tos_first_equip + os_swapwep] = NULL;
            if (!want_to_change_slot(os_wep))
                u.utracked[tos_first_equip + os_wep] = NULL;
            if (u.utracked[tos_first_equip + os_swapwep] &&
                u.utracked[tos_first_equip + os_wep])
                panic("Swapping items did not help with desire");
        } else if (islot == os_wep) {
            ready_weapon(desired == &zeroobj ? NULL : desired);
        } else if (islot == os_swapwep) {
            if (desired != &zeroobj) unwield_silently(desired);
            setuswapwep(desired == &zeroobj ? NULL : desired);
            /* The can_twoweapon check is done early, so that we don't get
               "wielded in other hand" if it actually isn't. */
            if (u.twoweap && !can_twoweapon())
                untwoweapon();
            if (desired == &zeroobj)
                pline(msgc_actionboring,
                      "You have no secondary weapon readied.");
            else
                prinv(NULL, desired, 0L);
        } else if (islot == os_quiver) {
            if (desired != &zeroobj) unwield_silently(desired);
            setuqwep(desired == &zeroobj ? NULL : desired);
            if (desired == &zeroobj)
                pline(msgc_actionboring, "You now have no ammunition readied.");
            else
                prinv(NULL, desired, 0L);
        } else if (equip_order[i].direction == ed_unequip) {
            /* Unequip the item. (This might be only temporary to change a slot
               underneath, but in that case, we don't have to worry about an
               immediate re-equip because the equip and unequip have separate
               lines on the table.) */
            setequip(islot, NULL, em_voluntary);
            /* Unequipping artifact armor will succeed despite a blast, but
               you still get blasted. */
            if (current->oartifact)
                touch_artifact(current, &youmonst);
            /* If it's temporary, set a desire to re-equip. */
            if (u.utracked[tos_first_equip + islot] == NULL && temp_unequip)
                u.utracked[tos_first_equip + islot] = current;
        } else {
            /* Equip the item. (We know desired != &zeroobj if we get here.)
               setworn() doesn't have an artifact touch check, so we do that
               here. canwearobj() doesn't have a check for already wielded,
               so we unwield here too. The two checks are in the same order
               as in 3.4.3: if you try to wear a wielded artifact, and it
               blasts you, the attempt to wear fails but you can still hang
               on to it. */
            if (!desired->oartifact || touch_artifact(desired, &youmonst)) {
                unwield_silently(desired);
                setequip(islot, desired, em_voluntary);
            }
        }
        /* Remove the desire to change a slot that has had the correct item
           placed in it. (Temporary unequips mean that the wrong item might
           have been placed in it even if there was a change.) */
        if (!want_to_change_slot(islot))
            u.utracked[tos_first_equip + islot] = NULL;
        /* Remove the desire to change a slot where we tried and failed to equip
           something in it, to avoid an infinite loop. (This case can come up if
           for example the player lifesaves from wielding a c corpse.) */
        if (current == EQUIP(islot))
            u.utracked[tos_first_equip + islot] = NULL;

        /* Cancel two-weaponing if it's no longer possible. */
        if (u.twoweap && !can_twoweapon())
            untwoweapon();

        update_inventory();
    }
    return reservoir_increased ? 1 : 0;
}

/* Called when the player requests to equip otmp in slot. (You can empty the
   slot using either NULL or &zeroobj.) This function may be called with any
   object and any slot; it will do nothing (except maybe reset the equip
   occupation state) and return 0 if the equip is impossible and the character
   knows it's impossible, do nothing and return 1 if it's impossible and the
   character has to experiment to find that out, and otherwise will perform the
   equip, taking 0, 1, or more actions, and return whether or not it took
   time. (In the case where it takes more than 1 action, it will mark the action
   as incomplete.) */
int
equip_in_slot(struct obj *otmp, enum objslot slot, boolean keep_swapwep)
{
    int t;
    enum objslot j;
    struct obj *weapon = NULL;
    if (!otmp)
        otmp = &zeroobj;

    /* Equipping a two-handed weapon while holding a shield and cblock is
       actually implemented by setting the equipment state up as though we
       were intending to unequip the shield, but temporarily unequipped the
       weapon in order to do so. This means that the code will unequip the
       shield, and then equip the weapon afterwards, without needing extra
       codepaths and thus minimizing on special cases. */
    if (flags.cblock && slot == os_wep && otmp != &zeroobj &&
        bimanual(otmp) && uarms) {
        weapon = otmp;
        otmp = &zeroobj;
        slot = os_arms;
    }

    /* Meanwhile, for equipping a shield while holding a two-handed weapon
       and cblock, we keep the shield as the focus, but nonetheless add a
       desire to unequip the weapon. */
    if (flags.cblock && slot == os_arms && otmp != &zeroobj &&
        uwep && bimanual(uwep))
        weapon = &zeroobj;

    /* We're resuming if we request an equip that's already the case (the
       "putting items back on" stage, or an equip that's already desired (the
       "taking items off to free the slot" stage, or the "equipping the item
       itself" stage). We aren't resuming if there are no desires marked
       anywhere, though. */
    boolean resuming = FALSE;
    for (j = 0; j <= os_last_equip; j++)
        if (u.utracked[tos_first_equip + j] &&
            (!keep_swapwep || j != os_swapwep))
            resuming = TRUE;
    resuming = resuming &&
        (u.utracked[tos_first_equip + slot] == otmp ||
         EQUIP(slot) == otmp || (!EQUIP(slot) && otmp == &zeroobj));

    /* Make sure that each of our slots has the correct desired item and the
       correct occupation progress. */
    for (j = 0; j <= os_last_equip; j++) {
        struct obj **desired = u.utracked + tos_first_equip + j;
        struct obj *target = j == slot ? otmp : NULL;

        if (keep_swapwep && j == os_swapwep)
            continue;

        /* If we're continuing a compound equip, don't cancel desires for slots
           that cover the slot we want to change. (They may be temporarily
           unequipped items.) Also, don't cancel a desire for a weapon when
           unequipping a shield (this special case is needed because shields and
           two-handed weapons conflict, but neither covers the other). We do
           cancel desires for other slots, though, so that it's possible to
           abort a compound equip via equipping a higher slot. */
        if (resuming && (slot_covers(j, slot) || j == slot ||
                         (j == os_wep && slot == os_arms && otmp == &zeroobj)))
            continue;
        /* If we're placing a different object in a slot from what was there
           before, cancel our progress in equipping that slot. */
        if (*desired != target)
            u.uoccupation_progress[tos_first_equip + j] = 0;
        *desired = target;
    }

    if (weapon)
        u.utracked[tos_first_equip + os_wep] = weapon;

    /* Equips in time-consuming slots should print messages to let the player
       know what's happening. Potentially time-consuming slots are any armor
       slot, and any slot that covers another slot (but all covering slots
       are armor slots at the moment). */
    if (flags.verbose && slot <= os_last_armor) {
        if (otmp != &zeroobj && otmp != EQUIP(slot)) {
            if (!resuming || turnstate.continue_message)
                pline(msgc_occstart, "You %s equipping %s.",
                      resuming ? "continue" : "start", yname(otmp));
        } else if (otmp == &zeroobj && EQUIP(slot)) {
            if (!resuming || turnstate.continue_message)
                pline(msgc_occstart, "You %s removing %s.",
                      resuming ? "continue" : "start", yname(EQUIP(slot)));
        } else {
            /* Either we're putting items back on, or else we're not making any
               changes at all and this is very trivial. */
            if (turnstate.continue_message)
                for (j = 0; j <= os_last_equip; j++) {
                    if (u.utracked[tos_first_equip + j] != NULL && j != slot) {
                        pline(msgc_occstart,
                              "You start putting your other items back on.");
                        break;
                    }
                }
        }
    }

    /* Do the equip. */
    t = equip_heartbeat();
    if (t == 2)
        action_incomplete("changing your equipment", occ_equip);

    return t > 0;
}

/* The logic for this is now moved to object_selection_checks, meaning that
   this can be as simple as possible. */
static const char clothes_and_accessories[] = { ALL_CLASSES, 0 };
static const char equippables_and_nothing[] = { ALL_CLASSES, ALLOW_NONE, 0 };

/* the 'T'/'R' command */
int
dounequip(const struct nh_cmd_arg *arg)
{
    enum objslot j;
    long mask;
    struct obj *otmp;

    otmp = getargobj(arg, clothes_and_accessories, "take off");
    if (!otmp)
        return 0;

    /* We could make 'T'/'R' capable of unwielding the worn weapon, quiver, etc.
       simply by changing os_last_worn to os_last_equip. Should we? */
    for (j = 0; j <= os_last_worn; j++)
        if (otmp->owornmask & W_MASK(j)) {
            if (!canunwearobj(otmp, TRUE, FALSE, flags.cblock))
                return 0; /* user knows it's impossible */
            return equip_in_slot(NULL, j, FALSE);
        }

    /* We get here if an equip was interrupted while we were putting items back
       on; we also get here if someone tries to unequip an item that isn't
       equipped. To distinguish between the cases, we rely on the fact that
       rings can't be covered, and so we can look at which slots the object can
       naturally be equipped in to determine which slot to use. */
    if (!canwearobj(otmp, &mask, FALSE, FALSE, TRUE) || mask & W_RING) {
        /* Either it isn't equipment at all, or else the slot's covered by a
           cursed item, or else it's a ring. In all these cases, we can't
           sensibly continue, especially not without manual confirmation. */
        pline(msgc_mispaste, "You are not wearing that.");
        return 0;
    }

    /* Now de-equip the slot the (unequipped) item should be in. This will
       continue with a compound unequip if necessary, or else print a message
       explaining that the action doesn't make sense. Exception: don't do this
       if the slot is already filled by some other item, because then we'd
       unequip that item, which is not what the user wants. */
    for (j = 0; j <= os_last_worn; j++)
        if (mask & W_MASK(j) && !EQUIP(j))
            return equip_in_slot(NULL, j, FALSE);

    pline(msgc_mispaste, "You are not wearing that.");
    return 0;
}

/* the 'W'/'P' command */
int
dowear(const struct nh_cmd_arg *arg)
{
    long mask;
    enum objslot j;
    struct obj *otmp;

    otmp = getargobj(arg, clothes_and_accessories, "wear");
    if (!otmp)
        return 0;

    /* Work out which slot it should go in. If it doesn't go into any slot, then
       we can't reasonably use equip_in_slot, so we allow canwearobj to print
       the message, then return 0. */
    if (!canwearobj(otmp, &mask, TRUE, FALSE, flags.cblock))
        return 0;

    /* If we can place a ring on either hand, try without cblock in order to see
       if one ring hand is faster to equip (because there's already a ring on
       the other hand). */
    if (mask == W_RING && !canwearobj(otmp, &mask, FALSE, FALSE, FALSE)) {
        /* We could reach this point if there's a ring on both hands, or if the
           selected ring is already worn. We don't want to auto-de-equip an
           arbitrary ring. */
        if (otmp->owornmask & W_RING) {
            /* There is actually a sensible response to this situation: we could
               move the ring to the other hand. For the time being, though, we
               don't do that unless explicitly requested with A, to avoid
               ridiculous accidents. */
            pline(msgc_mispaste, "That ring is already on your finger!");
            return 0;
        } else {
            pline(msgc_cancelled, "Both your ring fingers are already full.");
            return 0;
        }
    }

    /* The way this (mask & W_MASK(j)) check is written, we pick an arbitrary
       ring slot for rings that could still reasonably go on either hand, rather
       than prompting.  The player can place a ring on a specific hand using the
       'A' command.

       This also continues a compound equip, if necessary; we don't have an
       already-worn check, because equip_in_slot does that for us (via
       equip_heartbeat). */
    for (j = 0; j <= os_last_worn; j++)
        if (mask & W_MASK(j))
            return equip_in_slot(otmp, j, FALSE);

    impossible("Bad mask from canwearobj?");
    return 0;
}

static void
already_wearing(const char *cc)
{
    pline(cc == c_that_ ? msgc_mispaste : msgc_cancelled,
          "You are already wearing %s%c", cc,
          (cc == c_that_) ? '!' : '.');
}

/* welded() sets bknown; so only call it if the wielded weapon is bknown or
   we're actually experimenting to see if we can unwield. Otherwise, act
   like the weapon is uncursed. (It's very hard (impossible?) to have the
   weapon cursed without knowing it, so this is mostly (entirely?)
   paranoia, but best to make sure we don't cause side effects in zero
   time.) */
static inline boolean
known_welded(boolean spoil)
{
    return (spoil || (uwep && uwep->bknown)) && welded(uwep);
}

/* Checks how many slots of a given type a given monster has (works correctly
   with "youmonst"). At present, this should only return 0 or 1 because not all
   the code works with more. Some day, 2 and higher will be valid return values,
   and we can have ettins with multiple helmets, etc..

   If noisy is set, the function will print out a complaint upon returning 0, as
   if the item were being equipped by the player (but will still be silent if
   returning a positive number).

   Note that this checks whether the slot exist at all; if slot_count is 0 for a
   slot, items always fall off during polymorph, but depending on the situation
   they might fall off even without this. There is no guarantee that items can
   be equipped to the slot, or unequipped from it. Additionally, it's possible
   that only specific items will fit into the slot, in some cases.
*/
static int
slot_count(struct monst *mon, enum objslot slot, boolean noisy)
{
    if ((slot == os_arm || slot == os_armc || slot == os_armu) &&
        (breakarm(mon->data) || sliparm(mon->data)) &&
        /* TODO: get m_dowear() to look at this function rather than
           repeating the check */
        (slot != os_armc || mon->data->msize != MZ_SMALL) &&
        /* Hobbits have an os_arm slot that can be used for elven armor only */
        (raceptr(mon) != &mons[PM_HOBBIT] || slot != os_arm)) {
        if (noisy)
            pline(msgc_cancelled, "The %s will not fit on your body.",
                  c_slotnames[slot]);
        return 0;
    }
    /* Horned monsters /do/ have a head slot; they can wear elven leather helms
       and similar. So we don't check for os_armh against horns.

       TODO: 3.4.3 behaviour is that having no hands also means you can't wear a
       helmet or boots. This has been preserved, but I'm not sure it's what we
       want; at least, the function is misleadingly named in that case. */
    if ((slot == os_armg || slot == os_arms || slot == os_armh) &&
        (nohands(mon->data) || verysmall(mon->data))) {
        if (noisy)
            pline(msgc_cancelled, "You can't balance the %s on your body.",
                  c_slotnames[slot]);
        return 0;
    }
    if (slot == os_armf &&
        ((nohands(mon->data) || verysmall(mon->data) ||
          slithy(mon->data) || mon->data->mlet == S_CENTAUR))) {
        if (noisy)
            pline(msgc_cancelled, "You can't fit boots on your %s.",
                  makeplural(mbodypart(mon, FOOT)));
        return 0;
    }

    /* for doequip() */
    if (slot == os_wep && cantwield(mon->data)) {
        if (noisy)
            pline(msgc_cancelled,
                  "You are physically incapable of holding items.");
        return 0;
    }

    /* Everything has ring, amulet, quiver, and secondary weapon slots. */
    return 1;
}


/*
 * canwearobj checks to see whether the player can wear a piece of armor or
 * jewellery (i.e. anything that can meaningfully have bits in W_WORN set). It
 * does not return useful information for the wield slot, because that would
 * cause a TRUE return in almost any situation (basically anything is
 * wieldable), and thus rather complicate the code for W, which doesn't wield.
 * (You can determine if an item is potentially wieldable via seeing whether
 * it's currently worn; all items that are neither worn (including as a saddle)
 * nor welded are wieldable, although c corpses may well cause instant death
 * upon trying, and they aren't the only try-then-fail case; ready_weapon is the
 * appropriate function to use to attempt the actual wielding.)
 *
 * This is used in three basic contexts:
 *
 * a) As a check in response to the user giving the command to wear a piece of
 *    armor (noisy != FALSE, spoil == FALSE). In this case, the code will
 *    pline() out explanations about why the armor can't be worn, if it can't be
 *    worn. This is based on character knowledge; if it returns FALSE, the
 *    character will know it won't work and won't waste a turn trying.
 *
 * b) To check to see if and where an item can be equipped, in preparation for
 *    presenting a menu of sensible options to the user, or to determine
 *    whether the item is still worn after polymorph. In this case, the code
 *    will stay quiet about mistakes and just return FALSE. This is based only
 *    on character knowledge, so may return TRUE in situations where the item
 *    cannot be equipped (if the character does not know why it cannot be
 *    equipped).
 *
 * c) While the character is trying to equip the item (neither noisy nor spoil
 *    is false). This will use actual knowledge about BCUs, and set bknown
 *    accordingly.
 *
 * This function now also checks for the amulet, ring, and blindfold slots. In
 * the case of rings, *mask will be W_RING if both ring slots are free,
 * W_MASK(os_ringl) or W_MASK(os_ringr) if only one ring slot is free.
 *
 * You can get different outcomes from this function using the cblock value.
 * With cblock == TRUE, the return values are based on what could be
 * accomplished via removing items in other slots. With cblock == FALSE, the
 * return values are based on what could be accomplished right now (e.g. armor
 * will be blocked by a perfectly removable suit of armor or a cloak). cblock
 * is mutually exclusive with spoil; if you're trying to move the item right
 * now, it's important that the slot is unblocked even if you theoretically
 * could unblock it.
 *
 * inputs: otmp (the piece of armor)
 *         noisy (TRUE to print messages; FALSE to stay silent)
 *         spoil (TRUE to use game knowledge; FALSE to use character knowledge)
 *         cblock (when TRUE, consider items in blocking slots to block the
 *                 equip only if known to be cursed)
 * output: mask (otmp's armor type)
 */
#define CBLOCK(x) ((x) && (!cblock || ((x)->bknown && (x)->cursed)))
boolean
canwearobj(struct obj *otmp, long *mask,
           boolean noisy, boolean spoil, boolean cblock)
{
    enum objslot slot = which_slot(otmp);
    /* ensure that we are dealing with equippables */
    if (slot > os_last_equip)
        slot = os_invalid;
    int temp_mask;

    enum msg_channel msgc =
        !noisy ? msgc_mute : spoil ? msgc_failcurse : msgc_cancelled;
    
    /* Generic checks: wearing armor is possible; the armor is not already
       equipped (if !cblock); the item is equippable; the equip is not blocked
       by a cursed two-handed weapon */
    if (slot_count(&youmonst, slot, noisy) == 0) {
        return FALSE;
    } if ((verysmall(youmonst.data) || nohands(youmonst.data)) &&
          slot != os_amul && slot != os_ringl && slot != os_ringr &&
          slot != os_tool) {
        pline(msgc, "You are in no state to equip armor!");
        return FALSE;
    } else if (otmp->owornmask & W_WORN && !cblock) {
        if (noisy)
            already_wearing(c_that_);
        return FALSE;
    } else if (slot == os_invalid) {
        if (noisy)
            silly_thing("equip", otmp);
        return FALSE;
    } else if (slot == os_arm || slot == os_armu || slot == os_ringl) {
        if (uwep && bimanual(uwep) && known_welded(spoil)) {
            pline(msgc, "You cannot do that while your hands "
                  "are welded to your %s.",
                  is_sword(uwep) ? "sword" : c_slotnames[os_wep]);
            return FALSE;
        }
    }

    /* Find which slots are free to equip this item, and complain if there
       aren't any. */
    temp_mask = W_MASK(slot);
    if (slot == os_ringl || slot == os_ringr) {
        /* Rings are a special case because we have two slots to equip them
           into, and thus we complain only if both are blocked. */
        temp_mask = W_RING;
        if (CBLOCK(EQUIP(os_ringl))) temp_mask &= ~W_MASK(os_ringl);
        if (CBLOCK(EQUIP(os_ringr))) temp_mask &= ~W_MASK(os_ringr);
        if (!temp_mask) {
            if (noisy)
                already_wearing("two rings");
            return FALSE;
        }
    } else {
        if (CBLOCK(EQUIP(slot))) {
            if (noisy)
                already_wearing(an(c_slotnames[slot]));
            return FALSE;
        }
    }

    /* Checks for specific slots */
    switch (slot) {
    case os_armh:
        if (Upolyd && has_horns(youmonst.data) && !is_flimsy(otmp)) {
            /* (flimsy exception matches polyself handling) */
            pline(msgc, "The %s won't fit over your horn%s.", helmet_name(otmp),
                  plur(num_horns(youmonst.data)));
            return FALSE;
        }
        if (otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT &&
            qstart_level.dnum == u.uz.dnum && spoil) {   /* in quest */
            if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
                pline(msgc,
                      "You narrowly avoid losing all chance at your goal.");
            else    /* converted */
                pline(msgc, "You are suddenly overcome with shame "
                      "and change your mind.");
            u.ublessed = 0; /* lose your god's protection */
            makeknown(otmp->otyp);
            return FALSE;
        }
        break;

    case os_arms:
        if (CBLOCK(uwep) && uwep && bimanual(uwep)) {
            pline(msgc,
                  "You cannot wear a shield while wielding a two-handed %s.",
                  is_sword(uwep) ? "sword" :
                  (uwep->otyp == BATTLE_AXE) ? "axe" : "weapon");
            return FALSE;
        }
        break;

    case os_armf:
        if (u.utrap &&
                   (u.utraptype == TT_BEARTRAP || u.utraptype == TT_INFLOOR)) {
            if (u.utraptype == TT_BEARTRAP) {
                pline(msgc, "Your %s is trapped!", body_part(FOOT));
            } else {
                pline(msgc, "Your %s are stuck in the %s!",
                      makeplural(body_part(FOOT)), surface(u.ux, u.uy));
            }
            return FALSE;
        }
        break;

        /* We don't use slot_covers for these because we want custom messages */

    case os_armg:
        if (known_welded(spoil)) {
            pline(msgc, "You cannot wear gloves over your %s.",
                  (uwep && is_sword(uwep)) ? "sword" : "weapon");
            return FALSE;
        }
        break;

    case os_armu:
        if (CBLOCK(uarm) || CBLOCK(uarmc)) {
            pline(msgc, "You can't wear that over your %s.",
                  (uarm && !uarmc) ? "armor" : cloak_simple_name(uarmc));
            return FALSE;
        }
        break;

    case os_arm:
        if (CBLOCK(uarmc)) {
            pline(msgc, "You cannot wear armor over a %s.",
                  cloak_simple_name(uarmc));
            return FALSE;
        }
        if (youmonst.data->msize == MZ_SMALL &&
            racial_exception(&youmonst, otmp) < 1) {
            /* The different message is to imply that some small monsters may
               be able to wear some sorts of armor. */
            pline(msgc, "The armor almost fits, but is slightly too large.");
            return FALSE;
        }
        break;

    case os_ringl:
    case os_ringr:
        if (nolimbs(youmonst.data)) {
            pline(msgc, "You cannot make the ring stick to your body.");
            return FALSE;
        }
        /* It's possible to have cursed gloves and not know it. In such a case,
           we shouldn't spoil this fact until the user actually tries to equip
           a ring. */
        if (uarmg && uarmg->cursed && spoil)
            uarmg->bknown = TRUE;
        if (uarmg && uarmg->cursed && uarmg->bknown) {
            pline(msgc, "You cannot remove your gloves to put on the ring.");
            return FALSE;
        }
        if (known_welded(spoil)) {
            /* We've already checked for a weld to both hands, but we also need
               to check for a weld to just the right hand; this forces the ring
               onto the left hand, or prevents wearing it if the left hand is
               full.

               TODO: Currently the left hand isn't blocked by a welded shield
               (neither for glove nor ring).  Should it be? */
            temp_mask &= ~W_MASK(os_ringr);
            if (!temp_mask) {
                pline(msgc,
                      "You cannot free your weapon hand to wear the ring.");
                return FALSE;
            }
        }
        break;

    /* Other cases are uninteresting */
    default:
        break;
    }

    *mask = temp_mask;
    return TRUE;
}

/* Like the above, but for a specific slot, and it works even for weaponn
   slots. */
static boolean
canwearobjon(struct obj *otmp, enum objslot slot,
             boolean noisy, boolean spoil, boolean cblock)
{
    long mask;

    if (slot == os_wep)
        return !!canwieldobj(otmp, noisy, spoil, cblock);
    else if (slot == os_swapwep || slot == os_quiver)
        return !!canreadyobj(otmp, noisy, spoil, cblock);

    if (!canwearobj(otmp, &mask, noisy, spoil, cblock))
        return FALSE;
    if (mask & W_MASK(slot))
        return TRUE;

    /* We're trying to wear in the wrong slot. */
    if (!noisy)
        return FALSE;

    /* Is this an attempt to wear a ring on a blocked hand, when the other
       hand is available? */
    if (mask & W_RING && (slot == os_ringl || slot == os_ringr))
        pline(spoil ? msgc_failcurse : msgc_cancelled,
              "%s will only fit on your other hand.", Yname2(otmp));
    else
        pline(spoil ? msgc_failcurse : msgc_cancelled,
              "%s won't fit on that part of your body.", Yname2(otmp));

    return FALSE;
}

/* Like the above, but for unwearing. We don't return a mask this time, because
   it makes no sense to request which slots the item won't be worn in. Although
   it probably doesn't matter, this should complain even about uncursed items
   that are in the way; that way, the player isn't forced into a potentially
   slow temporary unequip cycle unless they override the list of letters in a
   prompt, and this function is the only check necessary to ensure that a
   particular unequip command is legal.

   Unlike canwearobj, this works even for the weapon slots. (There is a check in
   the code for R/T to ensure that it does not suggest the wielded weapon as
   something that can be unequipped.) The asymmetry is because it's possible to
   determine whether it's the weapon slots or worn slots that we need to talk
   about via seeing which the item is actually in. */
boolean
canunwearobj(struct obj *otmp, boolean noisy, boolean spoil, boolean cblock)
{
    struct obj *why;

    enum msg_channel msgc =
        !noisy ? msgc_mute : spoil ? msgc_failcurse : msgc_cancelled;
    
    if (!otmp || otmp == &zeroobj)
        return FALSE;
    if (!(otmp->owornmask & W_EQUIP)) {   /* to unequip, it must be equipped */
        if (noisy)
            pline(msgc_mispaste, "You are not wearing %s.", yname(otmp));
        return FALSE;
    }

    /* special ring checks */
    if (otmp->owornmask & W_RING) {
        const char *buf = NULL;

        if (nolimbs(youmonst.data)) {
            pline(msgc, "The ring is stuck.");
            return 0;
        }

        why = 0;        /* the item which prevents ring removal */
        if ((otmp == uright || (uwep && bimanual(uwep))) &&
            known_welded(spoil)) {
            buf = msgprintf("free a weapon %s", body_part(HAND));
            why = uwep;
        } else if (uarmg && uarmg->cursed && (spoil || uarmg->bknown)) {
            buf = msgprintf("take off your gloves");
            why = uarmg;
        }

        if (why) {
            pline(msgc, "You cannot %s to remove the ring.", buf);
            if (spoil)
                why->bknown = TRUE;
            return FALSE;
        }
    }
    /* special glove checks */
    if (otmp->owornmask & W_MASK(os_armg)) {
        if (uwep && known_welded(spoil)) {
            pline(msgc, "You are unable to take off your gloves "
                  "while wielding that %s.",
                  is_sword(uwep) ? "sword" : "weapon");
            if (spoil)
                uwep->bknown = TRUE;
            return FALSE;
        } else if (slippery_fingers(&youmonst)) {
            if (noisy)
                pline(msgc, "You can't take off the slippery gloves "
                      "with your slippery %s.", makeplural(body_part(FINGER)));
            return FALSE;
        }
    }
    /* special boot checks */
    if (otmp->owornmask & W_MASK(os_armf)) {
        if (u.utrap && u.utraptype == TT_BEARTRAP) {
            pline(msgc, "The bear trap prevents you from pulling your %s out.",
                  body_part(FOOT));
            return FALSE;
        } else if (u.utrap && u.utraptype == TT_INFLOOR) {
            pline(msgc, "You are stuck in the %s, and cannot pull your %s out.",
                  surface(u.ux, u.uy), makeplural(body_part(FOOT)));
            return FALSE;
        }
    }
    /* special suit and shirt checks */
    if (otmp->owornmask & (W_MASK(os_arm) | W_MASK(os_armu))) {
        why = 0;        /* the item which prevents disrobing */
        const char *buf = NULL;
        if (CBLOCK(uarmc)) {
            buf = msgcat("remove your ", cloak_simple_name(uarmc));
            why = uarmc;
        } else if ((otmp->owornmask & W_MASK(os_armu)) && CBLOCK(uarm)) {
            /* We could add a different message for removing a shirt underneath
               skin, but that scenario is kind-of absurd and currently can't
               happen, because draconic forms break shirts. */
            buf = "remove your armor";
            why = uarm;
        } else if (uwep && bimanual(uwep) && known_welded(spoil)) {
            buf = msgcat("release your ",
                         is_sword(uwep) ? "sword" :
                         (uwep->otyp == BATTLE_AXE) ? "axe" : "weapon");
            why = uwep;
            cblock = FALSE; /* always give the "cannot release" version of the
                               message */
        }
        if (why) {
            if (!cblock)
                pline(msgc, "You cannot take off %s with %s in the way.",
                      the(xname(otmp)), an(xname(why)));
            else
                pline(msgc, "You cannot %s to take off %s.", buf,
                      the(xname(otmp)));
            return FALSE;
        }
        if (otmp == uskin()) {
            pline(msgc, "The %s is merged with your skin!",
                  otmp->otyp >= GRAY_DRAGON_SCALES ?
                  "dragon scales are" : "dragon scale mail is");
        }
    }
    /* basic curse checks: anything welds in equip slots, only specific items
       weld in the hands, nothing welds in weapon or quiver */
    if (otmp->owornmask & W_WORN) {
        if (spoil && otmp->cursed)
            otmp->bknown = TRUE;
        if (otmp->cursed && otmp->bknown) {
            pline(msgc, "You can't remove %s.  It is cursed.",
                  the(xname(otmp)));
            return FALSE;
        }
    }
    if (otmp->owornmask & W_MASK(os_wep) && known_welded(spoil)) {
        if (noisy)
            weldmsg(msgc, otmp);
        return FALSE;
    }

    return TRUE;
}
#undef CBLOCK

void
glibr(void)
{
    struct obj *otmp;
    int xfl = 0;
    boolean leftfall, rightfall;
    const char *otherwep = 0;

    leftfall = (uleft && !uleft->cursed &&
                (!uwep || !welded(uwep) || !bimanual(uwep)));
    rightfall = (uright && !uright->cursed && (!welded(uwep)));
    if (!uarmg && (leftfall || rightfall) && !nolimbs(youmonst.data)) {
        /* changed so cursed rings don't fall off, GAN 10/30/86 */
        pline(msgc_statusbad, "Your %s off your %s.",
              (leftfall && rightfall) ? "rings slip" : "ring slips",
              (leftfall && rightfall) ?
              makeplural(body_part(FINGER)) : body_part(FINGER));
        xfl++;
        if (leftfall) {
            otmp = uleft;
            setunequip(otmp);
            dropx(otmp);
        }
        if (rightfall) {
            otmp = uright;
            setunequip(otmp);
            dropx(otmp);
        }
    }

    otmp = uswapwep;
    if (u.twoweap && otmp) {
        otherwep = is_sword(otmp) ? "sword" :
            makesingular(oclass_names[(int)otmp->oclass]);
        pline(msgc_statusbad, "Your %s %sslips from your %s.", otherwep,
              xfl ? "also " : "", makeplural(body_part(HAND)));
        setuswapwep(NULL);
        xfl++;
        if (otmp->otyp != LOADSTONE || !otmp->cursed)
            dropx(otmp);
    }
    otmp = uwep;
    if (otmp && !welded(otmp)) {
        const char *thiswep;

        /* nice wording if both weapons are the same type */
        thiswep = is_sword(otmp) ? "sword" :
            makesingular(oclass_names[(int)otmp->oclass]);
        if (otherwep && strcmp(thiswep, otherwep))
            otherwep = 0;

        /* changed so cursed weapons don't fall, GAN 10/30/86 */
        pline(msgc_statusbad, "Your %s%s %sslips from your %s.",
              otherwep ? "other " : "",
              thiswep, xfl ? "also " : "", makeplural(body_part(HAND)));
        setuwep(NULL);
        if (otmp->otyp != LOADSTONE || !otmp->cursed)
            dropx(otmp);
    }
}

struct obj *
some_armor(struct monst *victim)
{
    struct obj *otmph, *otmp;

    otmph = which_armor(victim, os_armc);
    if (!otmph && !uskin())
        otmph = which_armor(victim, os_arm);
    if (!otmph)
        otmph = which_armor(victim, os_armu);

    otmp = which_armor(victim, os_armh);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = which_armor(victim, os_armg);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = which_armor(victim, os_armf);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = which_armor(victim, os_arms);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    return otmph;
}


/* used for praying to check and fix levitation trouble */
struct obj *
stuck_ring(struct obj *ring, int otyp)
{
    if (ring && !(ring->owornmask & W_RING)) {
        impossible("stuck_ring: not worn as a ring?");
        return NULL;
    }

    if (ring && ring->otyp == otyp) {
        /* reasons ring can't be removed match those checked by select_off();
           limbless case has extra checks because ordinarily it's temporary */
        if (nolimbs(youmonst.data) && uamul &&
            uamul->otyp == AMULET_OF_UNCHANGING && uamul->cursed)
            return uamul;
        if ((ring == uright || (uwep && bimanual(uwep))) && welded(uwep))
            return uwep;
        if (uarmg && uarmg->cursed)
            return uarmg;
        if (ring->cursed)
            return ring;
    }
    /* either no ring or not right type or nothing prevents its removal */
    return NULL;
}

/* also for praying; find worn item that confers "Unchanging" attribute */
struct obj *
unchanger(void)
{
    if (uamul && uamul->otyp == AMULET_OF_UNCHANGING)
        return uamul;
    return 0;
}

/* The 'A' command: make arbitrary equipment changes. This used to have the
   awesome name "doddoremarm", but I felt like I had to change it to something
   more sensible :-(

   This function allows equipping any item in any slot, or continuing an aborted
   equip action (even if there were other intervening actions). */
int
doequip(const struct nh_cmd_arg *arg)
{
    enum objslot j;
    int n;
    const int *selected;
    static const int default_selected[1] = {os_last_slot+1 + 1};
    struct nh_menulist menu;
    int changes, time_consuming_changes = 0;
    boolean resuming = TRUE;

    do {
        changes = time_consuming_changes = 0;
        resuming = FALSE;

        if (turnstate.continue_message)

            init_menulist(&menu);

        for (j = 0; j <= os_last_equip; j++) {
            const char *buf;
            struct obj *otmp = EQUIP(j);
            const int linelen = 80; /* don't produce lines longer than this */

            buf = msgprintf("%*s: %.*s", LONGEST_SLOTNAME,
                            ((j == os_swapwep && u.twoweap) ? "Offhand" :
                             c_slotnames_menu[j]),
                            linelen - LONGEST_SLOTNAME - 4,
                            otmp ? doname(otmp) : "(empty)");

            if (turnstate.continue_message)
                add_menuitem(&menu, j+1, buf, 0, FALSE);

            if (u.utracked[tos_first_equip + j]) {
                boolean progress = !!u.uoccupation_progress[
                    tos_first_equip + j];
                buf = msgprintf("%*s %.*s%s", LONGEST_SLOTNAME + 5,
                                "->", linelen - LONGEST_SLOTNAME - 7 -
                                (int)strlen(" (in progress)"),
                                u.utracked[tos_first_equip + j] != &zeroobj ?
                                doname(u.utracked[tos_first_equip + j]) :
                                "(empty)",
                        progress ? " (in progress)" : "" );
                if (turnstate.continue_message)
                    add_menuitem(&menu, 0, buf, 0, FALSE);
                changes++;
                if (j <= os_last_armor)
                    time_consuming_changes += 2;
                else if (j != os_quiver)
                    time_consuming_changes++;
                if (progress)
                    resuming = TRUE;
            }
        }

        if (turnstate.continue_message) {
            add_menutext(&menu, "");
            if (changes) {
                add_menuitem(
                    &menu, os_last_slot+1 + 1, resuming ?
                    "Continue changing your equipment as shown above" :
                    "Change your equipment as shown above", 'C', FALSE);
                add_menuitem(
                    &menu, os_last_slot+1 + 2,
                    "Cancel changing your equipment", 'X', FALSE);
            }
            add_menuitem(
                &menu, os_last_slot+1 + 3,
                "Remove all equipment", 'T', FALSE);
            
            n = display_menu(&menu, "Your Equipment",
                             PICK_ONE, PLHINT_INVENTORY, &selected);
            if (n <= 0) /* no selection made */
                return 0;
        } else
            selected = default_selected;

        if (*selected == os_last_slot+1 + 2) {
            for (j = 0; j <= os_last_equip; j++) {
                u.utracked[tos_first_equip + j] = NULL;
                u.uoccupation_progress[tos_first_equip + j] = 0;
            }
        } else if (*selected == os_last_slot+1 + 3) {
            for (j = 0; j <= os_last_equip; j++) {
                if (u.utracked[tos_first_equip + j] != &zeroobj)
                    u.uoccupation_progress[tos_first_equip + j] = 0;
                u.utracked[tos_first_equip + j] = EQUIP(j) ? &zeroobj : 0;
            }
        } else if (*selected <= os_last_equip + 1) {
            struct obj *otmp;
            j = *selected - 1;

            /* check that unequipping is not known to be impossible */
            otmp = EQUIP(j);
            if (otmp && !canunwearobj(otmp, TRUE, FALSE, flags.cblock))
                continue;

            /* prompt the user */
            otmp = getobj(equippables_and_nothing,
                          j == os_wep ? "wield" :
                          j == os_swapwep ? "ready" :
                          j == os_quiver ? "quiver" : "wear", FALSE);

            /* check that equipping is not known to be impossible */
            if (otmp && (otmp == &zeroobj ||
                         canwearobjon(otmp, j, TRUE, FALSE, flags.cblock))) {
                /* schedule the equip */
                u.utracked[tos_first_equip + j] = otmp;
                if (otmp == &zeroobj)
                    otmp = NULL;
                if (otmp == EQUIP(j))
                    u.utracked[tos_first_equip + j] = NULL;
                u.uoccupation_progress[tos_first_equip + j] = 0;
            }
        }

    } while (*selected != os_last_slot+1 + 1);

    /* we can get here upon control-A after equipping is finished */
    if (!changes) {
        pline(msgc_mispaste,
              "You have already finished changing your equipment.");
        return 0;
    }

    /* If changing any slow item or two or more fast items, print a message,
       because this will probably take multiple turns. */
    if (time_consuming_changes >= 2 && flags.verbose &&
        (!resuming || turnstate.continue_message))
        pline(msgc_occstart, "You %s equipping yourself.",
              resuming ? "continue" : "start");

    /* Do the equip. */
    n = equip_heartbeat();
    if (n == 2)
        action_incomplete("changing your equipment", occ_equip);
    return n > 0;
}

/* hit by destroy armor scroll/black dragon breath */
int
destroy_arm(struct monst *mon, struct obj *obj)
{
    /* Sanity checks: atmp exist and is worn */
    if (!obj || !obj->owornmask || (mon == &youmonst && obj == uskin()) ||
        obj_resists(obj, 0, 90))
        return 0;

    boolean you = mon == &youmonst;
    boolean vis = canseemon(mon);
    enum msg_channel msgc = you ? msgc_itemloss : msgc_monneutral;
    enum objslot slot = which_slot(obj);
    const char *your = you ? "Your" : s_suffix(Monnam(mon));

    if (you || vis) {
        switch (slot) {
        case os_armc:
            pline(msgc, "%s %s crumbles and turns to dust!", your,
                  cloak_simple_name(obj));
            break;
        case os_arm:
            pline(msgc, "%s armor turns to dust and falls to the %s!", your,
                  surface(m_mx(mon), m_my(mon)));
            break;
        case os_armu:
            pline(msgc, "%s shirt crumbles into tiny threads and falls apart!",
                  your);
            break;
        case os_armh:
            pline(msgc, "%s helmet turns to dust and is blown away!", your);
            break;
        case os_armg:
            pline(msgc, "%s gloves vanish!", your);
            break;
        case os_armf:
            pline(msgc, "%s boots disintegrate!", your);
            break;
        case os_arms:
            pline(msgc, "%s shield crumbles away!", your);
            break;
        default:
            return 0;
        }
    }
    if (you) {
        const char *kbuf;

        kbuf = msgprintf("losing %s gloves while wielding", uhis());
        setunequip(obj);
        useup(obj);
        if (slot == os_armg)
            selftouch("You", kbuf);
        action_interrupted();
    } else {
        mon->misc_worn_check &= ~obj->owornmask;
        obj->owornmask = 0;
        update_property(mon, objects[obj->otyp].oc_oprop, which_slot(obj));
        update_property_for_oprops(mon, obj, slot);
        m_useup(mon, obj);
        struct obj *weapon;
        weapon = m_mwep(mon);
        if (weapon && weapon->otyp == CORPSE &&
            touch_petrifies(&mons[weapon->corpsenm]))
            /* TODO: add culprit to destroy_arm */
            mselftouch(mon, "Losing gloves, ",
                       !flags.mon_moving ? &youmonst : NULL);
    }
    return 1;
}

/*do_wear.c*/
