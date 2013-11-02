/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-02 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static const char see_yourself[] = "see yourself";
static const char unknown_type[] = "Unknown type of %s (%d)";

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
    [os_arm]     = "Armour",
    [os_armc]    = "Cloak",
    [os_armh]    = "Helmet",
    [os_arms]    = "Shield",
    [os_armg]    = "Gloves",
    [os_armf]    = "Boots",
    [os_armu]    = "Shirt",
    [os_amul]    = "Amulet",
    [os_ringl]   = "Left ring",
    [os_ringr]   = "Right ring",
    [os_tool]    = "Eyewear",
    [os_wep]     = "Weapon",
    [os_swapwep] = "Secondary weapon",
    [os_quiver]  = "Quiver",
};

static const char c_that_[] = "that";

/* The order in which items should be equipped or unequipped, when the user
   requests multiple simultaneous equips.

   The basic rules here are that all slots should be unequipped before they are
   equipped, and that a slot can only be equipped or unequipped before every
   slot covering it is equipped or unequipped. We also group together actions
   that take less time when combined (e.g. unequipping then equipping a weapon
   takes 1 action when given as one command, 2 actions when given as two
   commands). */
enum equip_direction { ed_equip, ed_unequip };
const struct equip_order {
    enum objslot slot;
    enum equip_direction direction;
} equip_order [] = {
    /* Tool comes first and last, because you'd typically want to be able to
       see what you're doing when switching out armour. */
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

    /* Swap the weapon slots around next. swapwep/wep can be swapped in 0
       actions; swapwep and wep can each be changed in 1, without necessarily
       unequipping in between. The quiver takes 0 no matter what it's combined
       with. */
    {os_swapwep, ed_unequip}, /* must come immediately before uwep */
    {os_swapwep, ed_equip  },
    {os_wep,     ed_unequip},
    {os_wep,     ed_equip  },
    {os_quiver,  ed_unequip},
    {os_quiver,  ed_equip  },

    /* The shield follows the same reasoning as the weapon. */
    {os_arms,    ed_unequip},
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
static void Ring_off_or_gone(struct obj *, boolean);
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

void
off_msg(struct obj *otmp)
{
    if (flags.verbose)
        pline("You take off %s.", yname(otmp));
}

static void
on_msg(struct obj *otmp)
{
    if (flags.verbose) {
        char how[BUFSZ];

        how[0] = '\0';
        if (otmp->otyp == TOWEL)
            sprintf(how, " around your %s", body_part(HEAD));
        pline("You are now %s %s%s.",
              otmp->owornmask & W_MASK(os_arms) ? "holding" : "wearing",
              obj_is_pname(otmp) ? the(xname(otmp)) : an(xname(otmp)), how);
    }
}

/*
 * The Type_on() functions should be called *after* setworn().
 * The Type_off() functions call setworn() themselves.
 */

int
Boots_on(void)
{
    if (!uarmf) return 0;

    long oldprop =
        worn_extrinsic(objects[uarmf->otyp].oc_oprop) & ~W_MASK(os_armf);

    uarmf->known = TRUE;
    switch (uarmf->otyp) {
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case KICKING_BOOTS:
        break;
    case JUMPING_BOOTS:
        /* jumping is obvious no matter what the situation */
        makeknown(uarmf->otyp);
        pline("Your %s feel longer.", makeplural(body_part(LEG)));
        break;
    case WATER_WALKING_BOOTS:
        if (u.uinwater)
            spoteffects(TRUE);
        break;
    case SPEED_BOOTS:
        /* Speed boots are still better than intrinsic speed, */
        /* though not better than potion speed */
        if (!oldprop && !(HFast & TIMEOUT)) {
            makeknown(uarmf->otyp);
            pline("You feel yourself speed up%s.",
                  (oldprop || HFast) ? " a bit more" : "");
        }
        break;
    case ELVEN_BOOTS:
        if (!oldprop && !HStealth && !BStealth) {
            makeknown(uarmf->otyp);
            pline("You walk very quietly.");
        }
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, rnd(20));
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation) {
            makeknown(uarmf->otyp);
            float_up();
            spoteffects(FALSE); /* can deallocate uarmf */
            if (!uarmf) return 0;
        }
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armf], uarmf->otyp);
    }

    on_msg(uarmf);
    return 0;
}

int
Boots_off(void)
{
    if (!uarmf) return 0;
    off_msg(uarmf);

    int otyp = uarmf->otyp;
    long oldprop = worn_extrinsic(objects[otyp].oc_oprop) & ~W_MASK(os_armf);

    /* For levitation, float_down() returns if Levitation, so we must do a
       setworn() _before_ the levitation case. */
    setworn(NULL, W_MASK(os_armf));
    switch (otyp) {
    case SPEED_BOOTS:
        if (!Very_fast) {
            makeknown(otyp);
            pline("You feel yourself slow down%s.", Fast ? " a bit" : "");
        }
        break;
    case WATER_WALKING_BOOTS:
        if (is_pool(level, u.ux, u.uy) && !Levitation && !Flying &&
            !is_clinger(youmonst.data)) {
            makeknown(otyp);
            /* make boots known in case you survive the drowning */
            spoteffects(TRUE);
        }
        break;
    case ELVEN_BOOTS:
        if (!oldprop && !HStealth && !BStealth) {
            makeknown(otyp);
            pline("You sure are noisy.");
        }
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = 0;
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation) {
            float_down(0L);
            makeknown(otyp);
        }
        break;
    case JUMPING_BOOTS:
        /* jumping is obvious no matter what the situation */
        makeknown(otyp);
        pline("Your %s feel shorter.", makeplural(body_part(LEG)));
        break;
    case LOW_BOOTS:
    case IRON_SHOES:
    case HIGH_BOOTS:
    case KICKING_BOOTS:
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armf], otyp);
    }
    
    return 0;
}

int
Cloak_on(void)
{
    if (!uarmc) return 0;

    long oldprop =
        worn_extrinsic(objects[uarmc->otyp].oc_oprop) & ~W_MASK(os_armc);

    uarmc->known = TRUE;
    switch (uarmc->otyp) {
    case ELVEN_CLOAK:
    case CLOAK_OF_PROTECTION:
    case CLOAK_OF_DISPLACEMENT:
        makeknown(uarmc->otyp);
        break;
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case ROBE:
    case LEATHER_CLOAK:
    case ALCHEMY_SMOCK:
        break;
    case MUMMY_WRAPPING:
        /* Note: it's already being worn, so we have to cheat here. */
        if ((HInvis || EInvis || pm_invisible(youmonst.data)) && !Blind) {
            newsym(u.ux, u.uy);
            pline("You can %s!",
                  See_invisible ? "no longer see through yourself" :
                  see_yourself);
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        /* since cloak of invisibility was worn, we know mummy wrapping wasn't, 
           so no need to check `oldprop' against blocked */
        if (!oldprop && !HInvis && !Blind) {
            makeknown(uarmc->otyp);
            newsym(u.ux, u.uy);
            pline("Suddenly you can%s yourself.",
                  See_invisible ? " see through" : "not see");
        }
        break;
    case OILSKIN_CLOAK:
        pline("%s very tightly.", Tobjnam(uarmc, "fit"));
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armc], uarmc->otyp);
    }
    on_msg(uarmc);
    return 0;
}

int
Cloak_off(void)
{
    if (!uarmc) return 0;
    off_msg(uarmc);

    int otyp = uarmc->otyp;
    long oldprop = worn_extrinsic(objects[otyp].oc_oprop) & ~W_MASK(os_armc);

    /* For mummy wrapping, taking it off first resets `Invisible'. */
    setworn(NULL, W_MASK(os_armc));
    switch (otyp) {
    case ELVEN_CLOAK:
    case ORCISH_CLOAK:
    case DWARVISH_CLOAK:
    case CLOAK_OF_PROTECTION:
    case CLOAK_OF_MAGIC_RESISTANCE:
    case CLOAK_OF_DISPLACEMENT:
    case OILSKIN_CLOAK:
    case ROBE:
    case LEATHER_CLOAK:
    case ALCHEMY_SMOCK:
        break;
    case MUMMY_WRAPPING:
        if (Invis && !Blind) {
            newsym(u.ux, u.uy);
            pline("You can %s.",
                  See_invisible ? "see through yourself" :
                  "no longer see yourself");
        }
        break;
    case CLOAK_OF_INVISIBILITY:
        if (!oldprop && !HInvis && !Blind) {
            makeknown(CLOAK_OF_INVISIBILITY);
            newsym(u.ux, u.uy);
            pline("Suddenly you can %s.",
                  See_invisible ? "no longer see through yourself" :
                  see_yourself);
        }
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armc], otyp);
    }
    return 0;
}

int
Helmet_on(void)
{
    if (!uarmh) return 0;

    uarmh->known = TRUE;
    switch (uarmh->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
    case HELM_OF_TELEPATHY:
        break;
    case HELM_OF_BRILLIANCE:
        adj_abon(uarmh, uarmh->spe);
        break;
    case CORNUTHAUM:
        /* people think marked wizards know what they're talking about, but it
           takes trained arrogance to pull it off, and the actual enchantment
           of the hat is irrelevant. */
        ABON(A_CHA) += (Role_if(PM_WIZARD) ? 1 : -1);
        iflags.botl = 1;
        makeknown(uarmh->otyp);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        if (u.ualign.type == A_NEUTRAL)
            u.ualign.type = rn2(2) ? A_CHAOTIC : A_LAWFUL;
        else
            u.ualign.type = -(u.ualign.type);
        u.ualign.record = -1;   /* consistent with altar conversion */
        u.ublessed = 0; /* lose your god's protection */
        /* makeknown(uarmh->otyp); -- moved below, after xname() */
     /*FALLTHRU*/ case DUNCE_CAP:
        if (!uarmh->cursed) {
            if (Blind)
                pline("%s for a moment.", Tobjnam(uarmh, "vibrate"));
            else
                pline("%s %s for a moment.", Tobjnam(uarmh, "glow"),
                      hcolor("black"));
            curse(uarmh);
            uarmh->bknown = TRUE;
        }
        iflags.botl = 1;        /* reveal new alignment or INT & WIS */
        if (Hallucination) {
            pline("My brain hurts!");   /* Monty Python's Flying Circus */
        } else if (uarmh->otyp == DUNCE_CAP) {
            pline("You feel %s.",       /* track INT change; ignore WIS */
                  ACURR(A_INT) <=
                  (ABASE(A_INT) + ABON(A_INT) +
                   ATEMP(A_INT)) ? "like sitting in a corner" : "giddy");
        } else {
            pline("Your mind oscillates briefly.");
            makeknown(HELM_OF_OPPOSITE_ALIGNMENT);
        }
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armh], uarmh->otyp);
    }
    on_msg(uarmh);
    return 0;
}

int
Helmet_off(void)
{
    if (!uarmh) return 0;
    off_msg(uarmh);

    switch (uarmh->otyp) {
    case FEDORA:
    case HELMET:
    case DENTED_POT:
    case ELVEN_LEATHER_HELM:
    case DWARVISH_IRON_HELM:
    case ORCISH_HELM:
        break;
    case DUNCE_CAP:
        iflags.botl = 1;
        break;
    case CORNUTHAUM:
        ABON(A_CHA) += (Role_if(PM_WIZARD) ? -1 : 1);
        iflags.botl = 1;
        break;
    case HELM_OF_TELEPATHY:
        /* need to update ability before calling see_monsters() */
        setworn(NULL, W_MASK(os_armh));
        see_monsters();
        return 0;
    case HELM_OF_BRILLIANCE:
        adj_abon(uarmh, -uarmh->spe);
        break;
    case HELM_OF_OPPOSITE_ALIGNMENT:
        u.ualign.type = u.ualignbase[A_CURRENT];
        u.ualign.record = -1;   /* consistent with altar conversion */
        u.ublessed = 0; /* lose the other god's protection */
        iflags.botl = 1;
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armh], uarmh->otyp);
    }
    setworn(NULL, W_MASK(os_armh));
    return 0;
}

int
Gloves_on(void)
{
    if (!uarmg) return 0;

    long oldprop =
        worn_extrinsic(objects[uarmg->otyp].oc_oprop) & ~W_MASK(os_armg);

    uarmg->known = TRUE;
    switch (uarmg->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            incr_itimeout(&HFumbling, rnd(20));
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(uarmg->otyp);
        iflags.botl = 1;        /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        adj_abon(uarmg, uarmg->spe);
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armg], uarmg->otyp);
    }
    on_msg(uarmg);
    return 0;
}

int
Gloves_off(void)
{
    if (!uarmg) return 0;
    off_msg(uarmg);

    long oldprop =
        worn_extrinsic(objects[uarmg->otyp].oc_oprop) & ~W_MASK(os_armg);

    switch (uarmg->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = 0;
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(uarmg->otyp);
        iflags.botl = 1;        /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        adj_abon(uarmg, -uarmg->spe);
    default:
        impossible(unknown_type, c_slotnames[os_armg], uarmg->otyp);
    }
    setworn(NULL, W_MASK(os_armg));
    encumber_msg();     /* immediate feedback for GoP */

    /* Prevent wielding cockatrice when not wearing gloves */
    if (uwep && uwep->otyp == CORPSE && touch_petrifies(&mons[uwep->corpsenm])) {
        char kbuf[BUFSZ];

        pline("You wield the %s in your bare %s.", corpse_xname(uwep, TRUE),
              makeplural(body_part(HAND)));
        sprintf(kbuf, "removing %s gloves while wielding %s", uhis(),
                an(corpse_xname(uwep, TRUE)));
        instapetrify(kbuf);
        uwepgone();     /* life-saved still doesn't allow touching cockatrice */
    }

    /* KMH -- ...or your secondary weapon when you're wielding it */
    if (u.twoweap && uswapwep && uswapwep->otyp == CORPSE &&
        touch_petrifies(&mons[uswapwep->corpsenm])) {
        char kbuf[BUFSZ];

        pline("You wield the %s in your bare %s.", corpse_xname(uswapwep, TRUE),
              body_part(HAND));
        sprintf(kbuf, "removing %s gloves while wielding %s", uhis(),
                an(corpse_xname(uwep, TRUE)));
        instapetrify(kbuf);
        uswapwepgone(); /* lifesaved still doesn't allow touching cockatrice */
    }

    return 0;
}

int
Shield_on(void)
{
    if (!uarms) return 0;

    uarms->known = TRUE;
    on_msg(uarms);
    return 0;
}

int
Shield_off(void)
{
    if (!uarms) return 0;
    off_msg(uarms);

    setworn(NULL, W_MASK(os_arms));
    return 0;
}

int
Shirt_on(void)
{
    if (!uarmu) return 0;

    uarmu->known = TRUE;
    on_msg(uarmu);
    return 0;
}

int
Shirt_off(void)
{
    if (!uarmu) return 0;
    off_msg(uarmu);

    setworn(NULL, W_MASK(os_armu));
    return 0;
}

/* This must be done in worn.c, because one of the possible intrinsics conferred
 * is fire resistance, and we have to immediately set HFire_resistance in worn.c
 * since worn.c will check it before returning.
 */
int
Armor_on(void)
{
    if (!uarm) return 0;

    uarm->known = TRUE;
    on_msg(uarm);
    return 0;
}

int
Armor_off(void)
{
    if (!uarm) return 0;
    off_msg(uarm);

    setworn(NULL, W_MASK(os_arm));

    return 0;
}

/* The gone functions differ from the off functions in that they don't print
   messages. Eventually, they'll be removed. */
int
Armor_gone(void)
{
    if (!uarm) return 0;

    setnotworn(uarm);

    return 0;
}

/* Return TRUE if the amulet is consumed in the process */
boolean
Amulet_on(void)
{
    if (!uamul) return TRUE;

    switch (uamul->otyp) {
    case AMULET_OF_ESP:
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_MAGICAL_BREATHING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_UNCHANGING:
        if (Slimed) {
            Slimed = 0;
            iflags.botl = 1;
        }
        break;
    case AMULET_OF_CHANGE:
        {
            int orig_sex = poly_gender();

            if (Unchanging)
                break;
            change_sex();
            /* Don't use same message as polymorph */
            if (orig_sex != poly_gender()) {
                makeknown(AMULET_OF_CHANGE);
                pline("You are suddenly very %s!",
                      flags.female ? "feminine" : "masculine");
                iflags.botl = 1;
            } else
                /* already polymorphed into single-gender monster; only changed 
                   the character's base sex */
                pline("You don't feel like yourself.");
            pline("The amulet disintegrates!");
            if (orig_sex == poly_gender() && uamul->dknown &&
                !objects[AMULET_OF_CHANGE].oc_name_known &&
                !objects[AMULET_OF_CHANGE].oc_uname)
                docall(uamul);
            useup(uamul);
            return TRUE;
        }
    case AMULET_OF_STRANGULATION:
        makeknown(AMULET_OF_STRANGULATION);
        pline("It constricts your throat!");
        Strangled = 6;
        break;
    case AMULET_OF_RESTFUL_SLEEP:
        HSleeping = rnd(100);
        break;
    case AMULET_OF_YENDOR:
        break;
    }
    on_msg(uamul);
    return FALSE;
}

void
Amulet_off(void)
{
    if (!uamul) return;
    off_msg(uamul);

    switch (uamul->otyp) {
    case AMULET_OF_ESP:
        /* need to update ability before calling see_monsters() */
        setworn(NULL, W_MASK(os_amul));
        see_monsters();
        return;
    case AMULET_OF_LIFE_SAVING:
    case AMULET_VERSUS_POISON:
    case AMULET_OF_REFLECTION:
    case AMULET_OF_CHANGE:
    case AMULET_OF_UNCHANGING:
    case FAKE_AMULET_OF_YENDOR:
        break;
    case AMULET_OF_MAGICAL_BREATHING:
        if (Underwater) {
            /* HMagical_breathing must be set off before calling drown() */
            setworn(NULL, W_MASK(os_amul));
            if (!breathless(youmonst.data) && !amphibious(youmonst.data)
                && !Swimming) {
                pline("You suddenly inhale an unhealthy amount of water!");
                drown();
            }
            return;
        }
        break;
    case AMULET_OF_STRANGULATION:
        if (Strangled) {
            pline("You can breathe more easily!");
            Strangled = 0;
        }
        break;
    case AMULET_OF_RESTFUL_SLEEP:
        setworn(NULL, W_MASK(os_amul));
        if (!ESleeping && !(HSleeping & INTRINSIC))
            HSleeping = 0;
        return;
    case AMULET_OF_YENDOR:
        break;
    }
    setworn(NULL, W_MASK(os_amul));
    return;
}

void
Ring_on(struct obj *obj)
{
    if (!obj) return;

    long oldprop = worn_extrinsic(objects[obj->otyp].oc_oprop);
    int old_attrib, which;

    /* only mask out W_RING when we don't have both left and right rings of the 
       same type */
    if ((oldprop & W_RING) != W_RING)
        oldprop &= ~W_RING;

    switch (obj->otyp) {
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
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* can now see invisible monsters */
        set_mimic_blocking();   /* do special mimic handling */
        see_monsters();
#ifdef INVISIBLE_OBJECTS
        see_objects();
#endif

        if (Invis && !oldprop && !HSee_invisible && !perceives(youmonst.data) &&
            !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you are transparent, but there!");
            makeknown(RIN_SEE_INVISIBLE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!oldprop && !HInvis && !BInvis && !Blind) {
            makeknown(RIN_INVISIBILITY);
            newsym(u.ux, u.uy);
            self_invis_message();
        }
        break;
    case RIN_LEVITATION:
        if (!oldprop && !HLevitation) {
            float_up();
            makeknown(RIN_LEVITATION);
            spoteffects(FALSE); /* for sinks */
        }
        break;
    case RIN_GAIN_STRENGTH:
        which = A_STR;
        goto adjust_attrib;
    case RIN_GAIN_CONSTITUTION:
        which = A_CON;
        goto adjust_attrib;
    case RIN_ADORNMENT:
        which = A_CHA;
    adjust_attrib:
        old_attrib = ACURR(which);
        ABON(which) += obj->spe;
        if (ACURR(which) != old_attrib ||
            (objects[obj->otyp].oc_name_known && old_attrib != 25 &&
             old_attrib != 3)) {
            iflags.botl = 1;
            makeknown(obj->otyp);
            obj->known = 1;
            update_inventory();
        }
        break;
    case RIN_INCREASE_ACCURACY:        /* KMH */
        u.uhitinc += obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.udaminc += obj->spe;
        break;
    case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
        resistcham();
        break;
    case RIN_PROTECTION:
        if (obj->spe || objects[RIN_PROTECTION].oc_name_known) {
            iflags.botl = 1;
            makeknown(RIN_PROTECTION);
            obj->known = 1;
            update_inventory();
        }
        break;
    }
    on_msg(obj);
}

static void
Ring_off_or_gone(struct obj *obj, boolean gone)
{
    if (!obj) return;
    if (!gone) off_msg(obj);

    long mask = (obj->owornmask & W_RING);
    int old_attrib, which;

    if (objects[obj->otyp].oc_oprop &&
        !(worn_extrinsic(objects[obj->otyp].oc_oprop) & mask))
        impossible("Strange... I didn't know you had that ring.");
    if (gone)
        setnotworn(obj);
    else
        setworn(NULL, obj->owornmask);

    switch (obj->otyp) {
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
        break;
    case RIN_WARNING:
        see_monsters();
        break;
    case RIN_SEE_INVISIBLE:
        /* Make invisible monsters go away */
        if (!See_invisible) {
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters();
#ifdef INVISIBLE_OBJECTS
            see_objects();
#endif
        }

        if (Invisible && !Blind) {
            newsym(u.ux, u.uy);
            pline("Suddenly you cannot see yourself.");
            makeknown(RIN_SEE_INVISIBLE);
        }
        break;
    case RIN_INVISIBILITY:
        if (!Invis && !BInvis && !Blind) {
            newsym(u.ux, u.uy);
            pline("Your body seems to unfade%s.",
                  See_invisible ? " completely" : "..");
            makeknown(RIN_INVISIBILITY);
        }
        break;
    case RIN_LEVITATION:
        float_down(0L);
        if (!Levitation)
            makeknown(RIN_LEVITATION);
        break;
    case RIN_GAIN_STRENGTH:
        which = A_STR;
        goto adjust_attrib;
    case RIN_GAIN_CONSTITUTION:
        which = A_CON;
        goto adjust_attrib;
    case RIN_ADORNMENT:
        which = A_CHA;
    adjust_attrib:
        old_attrib = ACURR(which);
        ABON(which) -= obj->spe;
        if (ACURR(which) != old_attrib) {
            iflags.botl = 1;
            makeknown(obj->otyp);
            obj->known = 1;
            update_inventory();
        }
        break;
    case RIN_INCREASE_ACCURACY:        /* KMH */
        u.uhitinc -= obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        u.udaminc -= obj->spe;
        break;
    case RIN_PROTECTION:
        /* might have forgotten it due to amnesia */
        if (obj->spe) {
            iflags.botl = 1;
            makeknown(RIN_PROTECTION);
            obj->known = 1;
            update_inventory();
        }
    case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
        /* If you're no longer protected, let the chameleons change shape again 
           -dgk */
        restartcham();
        break;
    }
}

void
Ring_off(struct obj *obj)
{
    Ring_off_or_gone(obj, FALSE);
}

void
Ring_gone(struct obj *obj)
{
    Ring_off_or_gone(obj, TRUE);
}

void
Blindf_on(struct obj *otmp)
{
    boolean already_blind = Blind, changed = FALSE;

    if (Blind && !already_blind) {
        changed = TRUE;
        if (flags.verbose)
            pline("You can't see any more.");
        /* set ball&chain variables before the hero goes blind */
        if (Punished)
            set_bc(0);
    } else if (already_blind && !Blind) {
        changed = TRUE;
        /* "You are now wearing the Eyes of the Overworld." */
        pline("You can see!");
    }
    if (changed) {
        /* blindness has just been toggled */
        if (Blind_telepat || Infravision)
            see_monsters();
        vision_full_recalc = 1; /* recalc vision limits */
        iflags.botl = 1;
    }
    on_msg(otmp);
}

void
Blindf_off(struct obj *otmp)
{
    boolean was_blind = Blind, changed = FALSE;

    setworn(NULL, otmp->owornmask);
    off_msg(otmp);

    if (Blind) {
        if (was_blind) {
            /* "still cannot see" makes no sense when removing lenses since
               they can't have been the cause of your blindness */
            if (otmp->otyp != LENSES)
                pline("You still cannot see.");
        } else {
            changed = TRUE;     /* !was_blind */
            /* "You were wearing the Eyes of the Overworld." */
            pline("You can't see anything now!");
            /* set ball&chain variables before the hero goes blind */
            if (Punished)
                set_bc(0);
        }
    } else if (was_blind) {
        changed = TRUE; /* !Blind */
        pline("You can see again.");
    }
    if (changed) {
        /* blindness has just been toggled */
        if (Blind_telepat || Infravision)
            see_monsters();
        vision_full_recalc = 1; /* recalc vision limits */
        iflags.botl = 1;
    }
}

/* Calls appropriate *_on / *_off functions for a given slot, respectively.
   Wearing an item can destroy it; this function returns true if the item is
   destroyed. */
boolean
Slot_on(enum objslot slot)
{
    switch(slot) {
    case os_arm: Armor_on(); break;
    case os_armh: Helmet_on(); break;
    case os_armg: Gloves_on(); break;
    case os_armf: Boots_on(); break;
    case os_armc: Cloak_on(); break;
    case os_arms: Shield_on(); break;
    case os_armu: Shirt_on(); break;
    case os_amul: return Amulet_on(); /* equipping an amulet can destroy it */
    case os_ringl:
    case os_ringr:
        Ring_on(EQUIP(slot)); break;
    case os_tool:
        Blindf_on(EQUIP(slot)); break;
    default:
        impossible("Equipping strange item slot %d");
    };
    return FALSE;
}
void
Slot_off(enum objslot slot)
{
    switch(slot) {
    case os_arm: Armor_off(); break;
    case os_armh: Helmet_off(); break;
    case os_armg: Gloves_off(); break;
    case os_armf: Boots_off(); break;
    case os_armc: Cloak_off(); break;
    case os_arms: Shield_off(); break;
    case os_armu: Shirt_off(); break;
    case os_amul: Amulet_off(); break;
    case os_ringl:
    case os_ringr:
        Ring_off(EQUIP(slot)); break;
    case os_tool:
        Blindf_off(EQUIP(slot)); break;
    default:
        impossible("Unequipping strange item slot %d");
    };
}
void
Slot_gone(enum objslot slot)
{
    /* TODO: Merge with Slot_off. This currently doesn't always print the right
       messages because many options don't have a _gone. */
    switch(slot) {
    case os_arm: Armor_gone(); break;
    case os_armh: Helmet_off(); break;
    case os_armg: Gloves_off(); break;
    case os_armf: Boots_off(); break;
    case os_armc: Cloak_off(); break;
    case os_arms: Shield_off(); break;
    case os_armu: Shirt_off(); break;
    case os_amul: Amulet_off(); break;
    case os_ringl:
    case os_ringr:
        Ring_gone(EQUIP(slot)); break;
    case os_tool:
        Blindf_off(EQUIP(slot)); break;
    default:
        impossible("Unequipping strange item slot %d");
    };
}

/* Changes which item is placed in a slot. This is a reasonably low-level
   function; it doesn't check whether the change is possible, it just does it.
   Likewise, it takes zero time. (The assumption is that the caller has either
   checked things like time and possibility, or else the item's being unequipped
   magically, being destroyed, or something similar.)

   Returns TRUE if attempting to equip the item destroyed it (e.g. amulet of
   change). */
boolean
setequip(enum objslot slot, struct obj *otmp, enum equipmsg msgtype)
{
    /* TODO: This is just a wrapper for now. It should eventually do the
       work itself. */
    if (!otmp || otmp == &zeroobj) {
        (msgtype == em_silent ? Slot_gone : Slot_off)(slot);
    } else {
        setworn(otmp, W_MASK(slot));
        return Slot_on(slot);
    }
    return FALSE;
}

/* Convenience wrapper around setequip. */
void
setunequip(struct obj *otmp)
{
    if (otmp->owornmask & W_EQUIP)
        setequip(objslot_from_mask(otmp->owornmask & W_EQUIP),
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
     with the benefits of their accumulated progress.

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
   equip_heartbeat yourself once. If it returns 0 or 1, return the same
   value. If it returns 2, set equip_occupation_callback as an occupation, then
   return 1. */
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
            pline("You are already %s %s.",
                  islot == os_wep ? "wielding" :
                  islot == os_swapwep ? "readying" :
                  islot == os_quiver ? "quivering" : "wearing", yname(current));
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
             current == u.utracked[tos_first_equip + os_wep]))
            fast_weapon_swap = TRUE;

        /* Likewise, we have a special case for trying to unequip an object
           over an empty slot. */
        if (!current && desired == &zeroobj) {
            pline("You have no %s equipped.", c_slotnames[islot]);
            u.utracked[tos_first_equip + islot] = desired = NULL;
        }

        if (!want_to_change_slot(islot) && !temp_unequip) continue;

        /* If the slot is empty and this is an unequip entry, do nothing,
           unless this is also a fast weapon swap.*/
        if (equip_order[i].direction == ed_unequip && !current &&
            !fast_weapon_swap)
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
        if (fast_weapon_swap)
            /* Note: we don't check to see if we can ready the currently wielded
               weapon; we can't, because it's currently wielded. If the unwield
               works, the ready will also work, though. We need three checks;
               two to see if the swap is possible, one that allows for a
               potential slow equip afterwards. */
            cant_equip = 
                (uwep && !canunwearobj(uwep, TRUE, FALSE, FALSE)) ||
                !canwieldobj(uswapwep, TRUE, FALSE, FALSE) ||
                (desired != uwep &&
                 !canreadyobj(desired, TRUE, FALSE, FALSE));
        else if (islot == os_wep)
            cant_equip = !canwieldobj(desired, TRUE, FALSE, FALSE);
        else if (islot == os_swapwep || islot == os_quiver)
            cant_equip = !canreadyobj(desired, TRUE, FALSE, FALSE);
        else if (equip_order[i].direction == ed_unequip)
            cant_equip = !canunwearobj(current, TRUE, FALSE, FALSE);
        else
            cant_equip = !canwearobjon(desired, islot, TRUE, FALSE, FALSE);
            
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
         * A   0 actions + min(oc_delay, 1) actions; except 2 for os_tool
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
         * should allow you to put on armour faster):
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
            action_cost = fast_weapon_swap ||
                equip_order[i].direction == ed_unequip ? 0 : 1;
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
            if (fast_weapon_swap)
                cant_equip =
                    (uwep && !canunwearobj(uwep, TRUE, TRUE, FALSE)) ||
                    !canwieldobj(uswapwep, TRUE, TRUE, FALSE) ||
                    (desired != uwep &&
                     !canreadyobj(desired, TRUE, TRUE, FALSE));
            else if (islot == os_wep)
                cant_equip = !canwieldobj(desired, TRUE, TRUE, FALSE);
            else if (islot == os_swapwep || islot == os_quiver)
                cant_equip = !canreadyobj(desired, TRUE, TRUE, FALSE);
            else if (equip_order[i].direction == ed_unequip)
                cant_equip = !canunwearobj(current, TRUE, TRUE, FALSE);
            else
                cant_equip = !canwearobjon(desired, islot, TRUE, TRUE, FALSE);

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
                        pline("You have no secondary weapon readied.");
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
                        pline("You have no secondary weapon readied.");
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
                pline("You have no secondary weapon readied.");
            else
                prinv(NULL, desired, 0L);
        } else if (islot == os_quiver) {
            if (desired != &zeroobj) unwield_silently(desired);
            setuqwep(desired == &zeroobj ? NULL : desired);
            if (desired == &zeroobj)
                pline("You now have no ammunition readied.");
            else
                prinv(NULL, desired, 0L);
        } else if (equip_order[i].direction == ed_unequip) {
            /* Unequip the item. (This might be only temporary to change a slot
               underneath, but in that case, we don't have to worry about an
               immediate re-equip because the equip and unequip have separate
               lines on the table.) */
            setequip(islot, NULL, em_voluntary);
            /* Unequipping artifact armour will succeed despite a blast, but
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
static int
equip_occupation_callback(void)
{
    return equip_heartbeat() == 2 ? 1 : 0;
}

/* Called when the player requests to equip otmp in slot. (You can empty the
   slot using either NULL or &zeroobj.) This function may be called with any
   object and any slot; it will do nothing (except maybe reset the equip
   occupation state) and return 0 if the equip is impossible and the character
   knows it's impossible, do nothing and return 1 if it's impossible and the
   character has to experiment to find that out, and otherwise will perform the
   equip, taking 0, 1, or more actions, and return whether or not it took
   time. (In the case where it takes more than 1 action, it will set an
   occupation callback that will do the actual equipping at the appropriate time
   in the future; this can be interrupted, and will be resumed by equip_in_slot
   if it's called with the same arguments with no equipping done in between, and
   in a few other cases, such as wearing an item that was temporarily taken
   off.) */
int
equip_in_slot(struct obj *otmp, enum objslot slot)
{
    int t;
    enum objslot j;
    if (!otmp)
        otmp = &zeroobj;

    /* We're resuming if we request an equip that's already the case (the
       "putting items back on" stage, or an equip that's already desired (the
       "taking items off to free the slot" stage, or the "equipping the item
       itself" stage). We aren't resuming if there are no desires marked
       anywhere, though. */
    boolean resuming = FALSE;
    for (j = 0; j <= os_last_equip; j++)
        if (u.utracked[tos_first_equip + j])
            resuming = TRUE;
    resuming = resuming &&
        (u.utracked[tos_first_equip + slot] == otmp ||
         EQUIP(slot) == otmp || (!EQUIP(slot) && otmp == &zeroobj));

    /* Make sure that each of our slots has the correct desired item and the
       correct occupation progress. */
    for (j = 0; j <= os_last_equip; j++) {
        struct obj **desired = u.utracked + tos_first_equip + j;
        struct obj *target = j == slot ? otmp : NULL;
        /* If we're continuing a compound equip, don't cancel desires for slots
           that cover the slot we want to change. (They may be temporarily
           unequipped items.) We do cancel desires for other slots, though, so
           that it's possible to abort a compound equip via equipping a higher
           slot. */
        if (resuming && (slot_covers(j, slot) || j == slot))
            continue;
        /* If we're placing a different object in a slot from what was there
           before, cancel our progress in equipping that slot. */
        if (*desired != target)
            u.uoccupation_progress[tos_first_equip + j] = 0;
        *desired = target;
    }

    /* Equips in time-consuming slots should print messages to let the player
       know what's happening. Potentially time-consuming slots are any armor
       slot, and any slot that covers another slot (but all covering slots
       are armor slots at the moment. */
    if (flags.verbose && slot <= os_last_armor) {
        if (otmp != &zeroobj && otmp != EQUIP(slot))
            pline("You %s equipping %s.", 
                  resuming ? "continue" : "start", yname(otmp));
        else if (otmp == &zeroobj && EQUIP(slot))
            pline("You %s removing %s.", 
                  resuming ? "continue" : "start", yname(EQUIP(slot)));
        else {
            /* Either we're putting items back on, or else we're not making any
               changes at all and this is very trivial. */
            for (j = 0; j <= os_last_equip; j++) {
                if (u.utracked[tos_first_equip + j] != NULL && j != slot) {
                    pline("You start putting your other items back on.");
                    break;
                }
            }
        }
    }

    /* Do the equip. */
    t = equip_heartbeat();
    if (t == 2) {
        set_occupation(equip_occupation_callback, "changing your equipment");
    }
    return t > 0;
}

/* The logic for this is now moved to object_selection_checks, meaning that
   this can be as simple as possible. */
static const char clothes_and_accessories[] = { ALL_CLASSES, 0 };

/* the 'T'/'R' command */
int
dounequip(struct obj *otmp)
{
    enum objslot j;
    long mask;

    if (otmp && !validate_object(otmp, clothes_and_accessories, "take off"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "take off");
    if (!otmp)
        return 0;

    /* We could make 'T'/'R' capable of unwielding the worn weapon, quiver, etc.
       simply by changing os_last_worn to os_last_equip. Should we? */
    for (j = 0; j <= os_last_worn; j++)
        if (otmp->owornmask & W_MASK(j))
            return equip_in_slot(NULL, j);

    /* We get here if an equip was interrupted while we were putting items back
       on; we also get here if someone tries to unequip an item that isn't
       equipped. To distinguish between the cases, we rely on the fact that
       rings can't be covered, and so we can look at which slots the object can
       naturally be equipped in to determine which slot to use. */
    if (!canwearobj(otmp, &mask, FALSE, FALSE, TRUE) || mask & W_RING) {
        /* Either it isn't equipment at all, or else the slot's covered by a
           cursed item, or else it's a ring. In all these cases, we can't
           sensibly continue, especially not without manual confirmation. */
        pline("You are not wearing that.");
        return 0;
    }

    /* Now de-equip the slot the (unequipped) item should be in. This will
       continue with a compound unequip if necessary, or else print a message
       explaining that the action doesn't make sense. Exception: don't do this
       if the slot is already filled by some other item, because then we'd
       unequip that item, which is not what the user wants. */
    for (j = 0; j <= os_last_worn; j++)
        if (mask & W_MASK(j) && !EQUIP(j))
            return equip_in_slot(NULL, j);

    pline("You are not wearing that.");
    return 0;
}

/* the 'W'/'P' command */
int
dowear(struct obj *otmp)
{
    long mask;
    enum objslot j;

    if (otmp && !validate_object(otmp, clothes_and_accessories, "wear"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "wear");
    if (!otmp)
        return 0;

    /* Work out which slot it should go in. If it doesn't go into any slot, then
       we can't reasonably use equip_in_slot, so we allow canwearobj to print
       the message, then return 0. */
    if (!canwearobj(otmp, &mask, TRUE, FALSE, TRUE))
        return 0;

    /* If we can place a ring on either hand, try without cblock in order to see
       if one ring hand is faster to equip (because there's already a ring on the
       other hand). */
    if (mask == W_RING && !canwearobj(otmp, &mask, FALSE, FALSE, FALSE)) {
        /* We could reach this point if there's a ring on both hands, or if the
           selected ring is already worn. We don't want to auto-de-equip an
           arbitrary ring. */
        if (otmp->owornmask & W_RING) {
            /* There is actually a sensible response to this situation: we could
               move the ring to the other hand. For the time being, though, we
               don't do that unless explicitly requested with A, to avoid
               ridiculous accidents. */
            pline("That ring is already on your finger!");
            return 0;
        } else {
            pline("Both your ring fingers are already full.");
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
            return equip_in_slot(otmp, j);

    impossible("Bad mask from canwearobj?");
    return 0;
}

static void
already_wearing(const char *cc)
{
    pline("You are already wearing %s%c", cc, (cc == c_that_) ? '!' : '.');
}

/* welded() sets bknown; so only call it if the wielded weapon is bknown or
   we're actually experimenting to see if we can unwield. Otherwise, act
   like the weapon is uncursed. (It's very hard (impossible?) to have the
   weapon cursed without knowing it, so this is mostly (entirely?)
   paranoia, but best to make sure we don't cause side effects in zero
   time.) */
static inline boolean
known_welded(boolean noisy)
{
    return (noisy || (uwep && uwep->bknown)) && welded(uwep);
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
 * return values are based on what could be accomplished right now (e.g. armour
 * will be blocked by a perfectly removable suit of armour or a cloak). cblock
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
    enum objslot slot = os_invalid;
    int temp_mask;

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
        slot = os_ringl; /* also os_ringr */
    else if (otmp->otyp == BLINDFOLD || otmp->otyp == LENSES ||
             otmp->otyp == TOWEL)
        slot = os_tool;

    /* Generic checks: wearing armour is possible; the armour is not already
       equipped (if !cblock); the item is equippable; the equip is not blocked
       by a cursed two-handed weapon */
    if (verysmall(youmonst.data) || nohands(youmonst.data)) {
        if (noisy)
            pline("You are in no state to equip items!");
        return FALSE;
    } else if ((slot == os_arm || slot == os_armc || slot == os_armu) &&
        cantweararm(youmonst.data) &&
        /* same exception for cloaks as used in m_dowear() */
        (slot != os_armc || youmonst.data->msize != MZ_SMALL) &&
        (racial_exception(&youmonst, otmp) < 1)) {
        if (noisy)
            pline("The %s will not fit on your body.", c_slotnames[slot]);
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
            if (noisy)
                pline("You cannot do that while your hands "
                      "are welded to your %s.",
                      is_sword(uwep) ? "sword" : c_slotnames[os_wep]);
            return FALSE;
        }
    }

    /* Find which slots are free to equip this item, and complain if there
       aren't any. */
    temp_mask = W_MASK(slot);
    if (slot == os_ringl) {
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
            if (noisy)
                pline("The %s won't fit over your horn%s.", helmet_name(otmp),
                      plur(num_horns(youmonst.data)));
            return FALSE;
        }
        if (otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT &&
            qstart_level.dnum == u.uz.dnum && spoil) {   /* in quest */
            if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
                pline("You narrowly avoid losing all chance at your goal.");
            else    /* converted */
                pline("You are suddenly overcome with shame "
                      "and change your mind.");
            u.ublessed = 0; /* lose your god's protection */
            makeknown(otmp->otyp);
            iflags.botl = 1;
            return FALSE;
        }        
        break;

    case os_arms:
        if (CBLOCK(uwep) && uwep && bimanual(uwep)) {
            if (noisy)
                pline
                    ("You cannot wear a shield while wielding a two-handed %s.",
                     is_sword(uwep) ? "sword" :
                     (uwep->otyp == BATTLE_AXE) ? "axe" : "weapon");
            return FALSE;
        }
        break;

    case os_armf:
        if (Upolyd && slithy(youmonst.data)) {
            if (noisy)
                pline("You have no feet...");   /* not body_part(FOOT) */
            return FALSE;
        } else if (Upolyd && youmonst.data->mlet == S_CENTAUR) {
            /* break_armor() pushes boots off for centaurs, so don't let
               dowear() put them back on... */
            if (noisy)
                pline("You have too many hooves to wear boots.");
            /* makeplural(body_part(FOOT)) yields "rear hooves" which sounds
               odd */
            return FALSE;
        } else if (u.utrap &&
                   (u.utraptype == TT_BEARTRAP || u.utraptype == TT_INFLOOR)) {
            if (u.utraptype == TT_BEARTRAP) {
                if (noisy)
                    pline("Your %s is trapped!", body_part(FOOT));
            } else {
                if (noisy)
                    pline("Your %s are stuck in the %s!",
                          makeplural(body_part(FOOT)), surface(u.ux, u.uy));
            }
            return FALSE;
        }
        break;

        /* We don't use slot_covers for these because we want custom messages */

    case os_armg:
        if (known_welded(spoil)) {
            if (noisy)
                pline("You cannot wear gloves over your %s.",
                      (uwep && is_sword(uwep)) ? "sword" : "weapon");
            return FALSE;
        }
        break;

    case os_armu:
        if (CBLOCK(uarm) || CBLOCK(uarmc)) {
            if (noisy)
                pline("You can't wear that over your %s.",
                      (uarm && !uarmc) ? "armor" : cloak_simple_name(uarmc));
            return FALSE;
        }
        break;

    case os_arm:
        if (CBLOCK(uarmc)) {
            if (noisy)
                pline("You cannot wear armor over a %s.",
                      cloak_simple_name(uarmc));
            return FALSE;
        }
        break;

    case os_ringl:
    case os_ringr:
        if (nolimbs(youmonst.data)) {
            if (noisy)
                pline("You cannot make the ring stick to your body.");
            return FALSE;
        }
        /* It's possible to have cursed gloves and not know it. In such a case,
           we shouldn't spoil this fact until the user actually tries to equip
           a ring. */
        if (uarmg && uarmg->cursed && spoil)
            uarmg->bknown = TRUE;
        if (uarmg && uarmg->cursed && uarmg->bknown) {
            if (noisy)
                pline("You cannot remove your gloves to put on the ring.");
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
                if (noisy)
                    pline("You cannot free your weapon hand to wear the ring.");
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

/* Like the above, but for a specific slot. */
static boolean
canwearobjon(struct obj *otmp, enum objslot slot,
             boolean noisy, boolean spoil, boolean cblock)
{
    long mask;
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
        pline("%s will only fit on your other hand.", Yname2(otmp));
    else
        pline("%s won't fit on that part of your body.", Yname2(otmp));

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
    char buf[BUFSZ];

    if (!otmp || otmp == &zeroobj)
        return FALSE;
    if (!(otmp->owornmask & W_EQUIP)) {   /* to unequip, it must be equipped */
        if (noisy)
            pline("You are not wearing %s", yname(otmp));
        return FALSE;
    }

    *buf = '\0';        /* lint suppresion */

    /* special ring checks */
    if (otmp->owornmask & W_RING) {
        if (nolimbs(youmonst.data)) {
            if (noisy)
                pline("The ring is stuck.");
            return 0;
        }
        why = 0;        /* the item which prevents ring removal */
        if ((otmp == uright || (uwep && bimanual(uwep))) &&
            known_welded(spoil)) {
            sprintf(buf, "free a weapon %s", body_part(HAND));
            why = uwep;
        } else if (uarmg && uarmg->cursed && (spoil || uarmg->bknown)) {
            sprintf(buf, "take off your gloves");
            why = uarmg;
        }
        if (why) {
            if (noisy)
                pline("You cannot %s to remove the ring.", buf);
            if (spoil)
                why->bknown = TRUE;
            return FALSE;
        }
    }
    /* special glove checks */
    if (otmp->owornmask & W_MASK(os_armg)) {
        if (uwep && known_welded(spoil)) {
            if (noisy) {
                pline("You are unable to take off your gloves "
                      "while wielding that %s.",
                      "gloves", is_sword(uwep) ? "sword" : "weapon");
                uwep->bknown = TRUE;
            }
            return FALSE;
        } else if (Glib) {
            if (noisy)
                pline("You can't take off the slippery gloves ",
                      "with your slippery %s.", makeplural(body_part(FINGER)));
            return FALSE;
        }
    }
    /* special boot checks */
    if (otmp->owornmask & W_MASK(os_armf)) {
        if (u.utrap && u.utraptype == TT_BEARTRAP) {
            if (noisy)
                pline("The bear trap prevents you from pulling your %s out.",
                      body_part(FOOT));
            return FALSE;
        } else if (u.utrap && u.utraptype == TT_INFLOOR) {
            if (noisy)
                pline("You are stuck in the %s, and cannot pull your %s out.",
                      surface(u.ux, u.uy), makeplural(body_part(FOOT)));
            return FALSE;
        }
    }
    /* special suit and shirt checks */
    if (otmp->owornmask & (W_MASK(os_arm) | W_MASK(os_armu))) {
        why = 0;        /* the item which prevents disrobing */
        if (CBLOCK(uarmc)) {
            sprintf(buf, "remove your %s", cloak_simple_name(uarmc));
            why = uarmc;
        } else if ((otmp->owornmask & W_MASK(os_armu)) && CBLOCK(uarm)) {
            /* We could add a different message for removing a shirt underneath
               skin, but that scenario is kind-of absurd and currently can't
               happen, because draconic forms break shirts. */
            sprintf(buf, "remove your armor");
            why = uarm;
        } else if (uwep && bimanual(uwep) && known_welded(spoil)) {
            sprintf(buf, "release your %s",
                    is_sword(uwep) ? "sword" :
                    (uwep->otyp == BATTLE_AXE) ? "axe" : "weapon");
            why = uwep;
        }
        if (why) {
            if (noisy)
                pline("You cannot %s to take off %s.", buf, the(xname(otmp)));
            return FALSE;
        }
        if (otmp == uskin()) {
            if (noisy)
                pline("The %s is merged with your skin!",
                      otmp->otyp >= GRAY_DRAGON_SCALES ?
                      "dragon scales are" : "dragon scale mail is");
        }
    }
    /* basic curse checks: anything welds in equip slots, only specific items
       weld in the hands, nothing welds in weapon or quiver */
    if (otmp->owornmask & W_EQUIP) {
        if (spoil && otmp->cursed)
            otmp->bknown = TRUE;
        if (otmp->cursed && otmp->bknown) {
            if (noisy)
                pline("You can't remove %s.  It is cursed.", the(xname(otmp)));
            return FALSE;
        }
    }
    if (otmp->owornmask & W_MASK(os_wep) && known_welded(spoil)) {
        if (noisy)
            weldmsg(otmp);
        return FALSE;
    }

    return TRUE;
}
#undef CBLOCK

void
find_ac(void)
{
    int uac = mons[u.umonnum].ac;

    /* Armor transformed into dragon skin gives no AC bonus. TODO: Should it at
       least give a bonus/penalty from its enchantment? */
    if (uarm && !uskin())
        uac -= ARM_BONUS(uarm);
    if (uarmc)
        uac -= ARM_BONUS(uarmc);
    if (uarmh)
        uac -= ARM_BONUS(uarmh);
    if (uarmf)
        uac -= ARM_BONUS(uarmf);
    if (uarms)
        uac -= ARM_BONUS(uarms);
    if (uarmg)
        uac -= ARM_BONUS(uarmg);
    if (uarmu)
        uac -= ARM_BONUS(uarmu);
    if (uleft && uleft->otyp == RIN_PROTECTION)
        uac -= uleft->spe;
    if (uright && uright->otyp == RIN_PROTECTION)
        uac -= uright->spe;
    if (HProtection & INTRINSIC)
        uac -= u.ublessed;
    uac -= u.uspellprot;
    if (uac < -128)
        uac = -128;     /* u.uac is an schar */
    if (uac != u.uac) {
        u.uac = uac;
        iflags.botl = 1;
    }
}


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
        pline("Your %s off your %s.",
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
        pline("Your %s %sslips from your %s.", otherwep, xfl ? "also " : "",
              makeplural(body_part(HAND)));
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
        pline("Your %s%s %sslips from your %s.", otherwep ? "other " : "",
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

/* erode some arbitrary armor worn by the victim */
void
erode_armor(struct monst *victim, boolean acid_dmg)
{
    struct obj *otmph = some_armor(victim);

    if (otmph && (otmph != uarmf)) {
        erode_obj(otmph, acid_dmg, FALSE);
        if (carried(otmph))
            update_inventory();
    }
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
   more sensible :( */
int
doequip(void)
{
    pline("This function is currently unimplemented.");
    return 0;
}

/* hit by destroy armor scroll/black dragon breath/monster spell */
int
destroy_arm(struct obj *atmp)
{
    struct obj *otmp;

#define DESTROY_ARM(o) ((otmp = (o)) != 0 && otmp != uskin() &&    \
                        (!atmp || atmp == otmp) &&                 \
                        (!obj_resists(otmp, 0, 90)))

    /* For anyone confused by the following code: DESTROY_ARM sets otmp as a
       side effect. Yes, it's confusing. */
    if (DESTROY_ARM(uarmc)) {
        pline("Your %s crumbles and turns to dust!", cloak_simple_name(uarmc));
        setunequip(otmp);
        useup(otmp);
    } else if (DESTROY_ARM(uarm)) {
        pline("Your armor turns to dust and falls to the %s!",
              surface(u.ux, u.uy));
        setunequip(otmp);
        useup(otmp);
    } else if (DESTROY_ARM(uarmu)) {
        pline("Your shirt crumbles into tiny threads and falls apart!");
        setunequip(otmp);
        useup(otmp);
    } else if (DESTROY_ARM(uarmh)) {
        pline("Your helmet turns to dust and is blown away!");
        setunequip(otmp);
        useup(otmp);
    } else if (DESTROY_ARM(uarmg)) {
        char kbuf[BUFSZ];

        sprintf(kbuf, "losing %s gloves while wielding", uhis());
        pline("Your gloves vanish!");
        setunequip(otmp);
        useup(otmp);
        selftouch("You", kbuf);
    } else if (DESTROY_ARM(uarmf)) {
        pline("Your boots disintegrate!");
        setunequip(otmp);
        useup(otmp);
    } else if (DESTROY_ARM(uarms)) {
        pline("Your shield crumbles away!");
        setunequip(otmp);
        useup(otmp);
    } else {
        return 0;       /* could not destroy anything */
    }

#undef DESTROY_ARM
    stop_occupation();
    return 1;
}

void
adj_abon(struct obj *otmp, schar delta)
{
    if (uarmg && uarmg == otmp && otmp->otyp == GAUNTLETS_OF_DEXTERITY) {
        if (delta) {
            makeknown(uarmg->otyp);
            ABON(A_DEX) += (delta);
        }
        iflags.botl = 1;
    }
    if (uarmh && uarmh == otmp && otmp->otyp == HELM_OF_BRILLIANCE) {
        if (delta) {
            makeknown(uarmh->otyp);
            ABON(A_INT) += (delta);
            ABON(A_WIS) += (delta);
        }
        iflags.botl = 1;
    }
}

/*do_wear.c*/
