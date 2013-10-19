/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-10-16 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static long takeoff_mask = 0L;
static enum objslot taking_off = os_invalid;

static int todelay;
static boolean cancelled_don = FALSE;

static const char see_yourself[] = "see yourself";
static const char unknown_type[] = "Unknown type of %s (%d)";

static const char * const c_slotnames[] = {
    [os_arm]   = "suit",
    [os_armc]  = "cloak",
    [os_armh]  = "helmet",
    [os_arms]  = "shield",
    [os_armg]  = "gloves",
    [os_armf]  = "boots",
    [os_armu]  = "shirt",
    [os_amul]  = "amulet",
    [os_ringl] = "ring",
    [os_ringr] = "ring",
    [os_tool]  = "eyewear",
    [os_wep]   = "weapon",
};

static const char c_that_[] = "that";

static enum objslot takeoff_order[] = {
    os_tool, os_wep, os_arms, os_armg, os_ringl, os_ringr, os_armc,
    os_armh, os_amul, os_arm, os_armu, os_armf, os_swapwep, os_quiver,
    os_invalid
};

static void on_msg(struct obj *);
static void Ring_off_or_gone(struct obj *, boolean);
static int select_off(struct obj *);
static struct obj *do_takeoff(void);
static int take_off(void);
static int menu_remarm(int);
static void already_wearing(const char *);
static int armoroff(struct obj *);

void
off_msg(struct obj *otmp)
{
    if (flags.verbose)
        pline("You were wearing %s.", doname(otmp));
}

/* for items that involve no delay */
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
    long oldprop =
        u.uprops[objects[uarmf->otyp].oc_oprop].extrinsic & ~W_MASK(os_armf);

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
            spoteffects(FALSE);
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
    int otyp = uarmf->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic &
        ~W_MASK(os_armf);

    takeoff_mask &= ~W_MASK(os_armf);
    /* For levitation, float_down() returns if Levitation, so we must do a
       setworn() _before_ the levitation case. */
    setworn(NULL, W_MASK(os_armf));
    switch (otyp) {
    case SPEED_BOOTS:
        if (!Very_fast && !cancelled_don) {
            makeknown(otyp);
            pline("You feel yourself slow down%s.", Fast ? " a bit" : "");
        }
        break;
    case WATER_WALKING_BOOTS:
        if (is_pool(level, u.ux, u.uy) && !Levitation && !Flying &&
            !is_clinger(youmonst.data) && !cancelled_don) {
            makeknown(otyp);
            /* make boots known in case you survive the drowning */
            spoteffects(TRUE);
        }
        break;
    case ELVEN_BOOTS:
        if (!oldprop && !HStealth && !BStealth && !cancelled_don) {
            makeknown(otyp);
            pline("You sure are noisy.");
        }
        break;
    case FUMBLE_BOOTS:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case LEVITATION_BOOTS:
        if (!oldprop && !HLevitation && !cancelled_don) {
            float_down(0L, 0L);
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
    cancelled_don = FALSE;
    return 0;
}

int
Cloak_on(void)
{
    long oldprop =
        u.uprops[objects[uarmc->otyp].oc_oprop].extrinsic & ~W_MASK(os_armc);

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
        /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance |= W_MASK(os_armc);
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
    int otyp = uarmc->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].extrinsic &
        ~W_MASK(os_armc);

    takeoff_mask &= ~W_MASK(os_armc);
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
        /* Alchemy smock gives poison _and_ acid resistance */
    case ALCHEMY_SMOCK:
        EAcid_resistance &= ~W_MASK(os_armc);
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armc], otyp);
    }
    return 0;
}

int
Helmet_on(void)
{
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
    takeoff_mask &= ~W_MASK(os_armh);

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
        if (!cancelled_don) {
            ABON(A_CHA) += (Role_if(PM_WIZARD) ? -1 : 1);
            iflags.botl = 1;
        }
        break;
    case HELM_OF_TELEPATHY:
        /* need to update ability before calling see_monsters() */
        setworn(NULL, W_MASK(os_armh));
        see_monsters();
        return 0;
    case HELM_OF_BRILLIANCE:
        if (!cancelled_don)
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
    cancelled_don = FALSE;
    return 0;
}

int
Gloves_on(void)
{
    long oldprop =
        u.uprops[objects[uarmg->otyp].oc_oprop].extrinsic & ~W_MASK(os_armg);

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
    long oldprop =
        u.uprops[objects[uarmg->otyp].oc_oprop].extrinsic & ~W_MASK(os_armg);

    takeoff_mask &= ~W_MASK(os_armg);

    switch (uarmg->otyp) {
    case LEATHER_GLOVES:
        break;
    case GAUNTLETS_OF_FUMBLING:
        if (!oldprop && !(HFumbling & ~TIMEOUT))
            HFumbling = EFumbling = 0;
        break;
    case GAUNTLETS_OF_POWER:
        makeknown(uarmg->otyp);
        iflags.botl = 1;        /* taken care of in attrib.c */
        break;
    case GAUNTLETS_OF_DEXTERITY:
        if (!cancelled_don)
            adj_abon(uarmg, -uarmg->spe);
        break;
    default:
        impossible(unknown_type, c_slotnames[os_armg], uarmg->otyp);
    }
    setworn(NULL, W_MASK(os_armg));
    cancelled_don = FALSE;
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
    uarms->known = TRUE;
    on_msg(uarms);
    return 0;
}

int
Shield_off(void)
{
    takeoff_mask &= ~W_MASK(os_arms);

    setworn(NULL, W_MASK(os_arms));
    return 0;
}

int
Shirt_on(void)
{
    uarmu->known = TRUE;
    on_msg(uarmu);
    return 0;
}

int
Shirt_off(void)
{
    takeoff_mask &= ~W_MASK(os_armu);

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
    uarm->known = TRUE;
    on_msg(uarm);
    return 0;
}

int
Armor_off(void)
{
    takeoff_mask &= ~W_MASK(os_arm);
    setworn(NULL, W_MASK(os_arm));
    cancelled_don = FALSE;
    return 0;
}

/* The gone functions differ from the off functions in that if you die from
 * taking it off and have life saving, you still die.
 */
int
Armor_gone(void)
{
    takeoff_mask &= ~W_MASK(os_arm);
    setnotworn(uarm);
    cancelled_don = FALSE;
    return 0;
}

/* Return TRUE if the amulet is consumed in the process */
boolean
Amulet_on(void)
{
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
    return FALSE;
}

void
Amulet_off(void)
{
    takeoff_mask &= ~W_MASK(os_amul);

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
    long oldprop = u.uprops[objects[obj->otyp].oc_oprop].extrinsic;
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
}

static void
Ring_off_or_gone(struct obj *obj, boolean gone)
{
    long mask = (obj->owornmask & W_RING);
    int old_attrib, which;

    takeoff_mask &= ~mask;
    if (!(u.uprops[objects[obj->otyp].oc_oprop].extrinsic & mask))
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
        float_down(0L, 0L);
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

    takeoff_mask &= ~W_MASK(os_tool);
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

/* Calls appropriate *_on / *_off functions for a given slot, respectively
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

enum objslot
objslot_from_mask(int wearmask)
{
    enum objslot i;
    for (i = 0; i <= os_last_maskable; i++)
        if ((wearmask & W_MASKABLE) == W_MASK(i))
            return i;
    panic("Could not find equipment slot for wear mask %d", wearmask);
}

/* called in main to set intrinsics of worn start-up items */
void
set_wear(void)
{
    int i;
    /* We wear the items in the reverse of their takeoff order. */
    for (i = (sizeof takeoff_order) / (sizeof *takeoff_order) - 1; i; i--)
        if (takeoff_order[i] <= os_last_armor &&
            takeoff_order[i] != os_invalid && EQUIP(takeoff_order[i]))
            Slot_on(takeoff_order[i]);
}

/* check whether the target object is currently being put on (or taken off) */
boolean
donning(struct obj *otmp)
{       /* also checks for doffing */
    enum objslot what = taking_off;     /* if valid, occupation is implied */
    boolean result = FALSE;

    /* Sadly, this doesn't simplify, beause we have to compare all the
       function pointers. */
    if (otmp == uarm)
        result = (afternmv == Armor_on || afternmv == Armor_off ||
                  what == os_arm);
    else if (otmp == uarmu)
        result = (afternmv == Shirt_on || afternmv == Shirt_off ||
                  what == os_armu);
    else if (otmp == uarmc)
        result = (afternmv == Cloak_on || afternmv == Cloak_off ||
                  what == os_armc);
    else if (otmp == uarmf)
        result = (afternmv == Boots_on || afternmv == Boots_off ||
                  what == os_armf);
    else if (otmp == uarmh)
        result = (afternmv == Helmet_on || afternmv == Helmet_off ||
                  what == os_armh);
    else if (otmp == uarmg)
        result = (afternmv == Gloves_on || afternmv == Gloves_off ||
                  what == os_armg);
    else if (otmp == uarms)
        result = (afternmv == Shield_on || afternmv == Shield_off ||
                  what == os_arms);

    return result;
}

void
cancel_don(void)
{
    /* the piece of armor we were donning/doffing has vanished, so stop wasting 
       time on it (and don't dereference it when donning would otherwise
       finish) */
    cancelled_don = (afternmv == Boots_on || afternmv == Helmet_on ||
                     afternmv == Gloves_on || afternmv == Armor_on);
    afternmv = 0;
    nomovemsg = NULL;
    multi = 0;
    todelay = 0;
    taking_off = os_invalid;
}

static const char clothes_and_accessories[] =
    { ARMOR_CLASS, RING_CLASS, AMULET_CLASS, TOOL_CLASS, FOOD_CLASS, 0 };

/* the 'T' command */
int
dotakeoff(struct obj *otmp)
{
    if (otmp && !validate_object(otmp, clothes_and_accessories, "take off"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "take off");
    if (!otmp)
        return 0;
    if (otmp->oclass != ARMOR_CLASS)
        return doremring(otmp);
    if (!(otmp->owornmask & W_ARMOR)) {
        pline("You are not wearing that.");
        return 0;
    }
    if (otmp == uskin()) {
        pline("The %s merged with your skin!",
              uskin()->otyp >=
              GRAY_DRAGON_SCALES ? "dragon scales are" :
              "dragon scale mail is");
        return 0;
    }
    if (((otmp == uarm) && uarmc) || ((otmp == uarmu) && (uarmc || uarm))) {
        /* TODO: replace this with a multistep remove */
        pline("The rest of your armor is in the way.");
        return 0;
    }

    reset_remarm();     /* clear takeoff_mask and taking_off */
    select_off(otmp);
    if (!takeoff_mask)
        return 0;
    reset_remarm();     /* armoroff() doesn't use takeoff_mask */

    armoroff(otmp);
    return 1;
}

/* the 'R' command */
int
doremring(struct obj *otmp)
{
    if (otmp && !validate_object(otmp, clothes_and_accessories, "remove"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "remove");
    if (!otmp)
        return 0;
    /* TODO: Merge this with the 'T' command. */
    if (otmp->oclass == ARMOR_CLASS)
        return dotakeoff(otmp);
    if (!(otmp->owornmask & W_WORN & ~W_ARMOR)) {
        pline("You are not wearing that.");
        return 0;
    }

    reset_remarm();     /* clear takeoff_mask and taking_off */
    select_off(otmp);
    if (!takeoff_mask)
        return 0;
    reset_remarm();     /* not used by Ring_/Amulet_/Blindf_off() */

    if (otmp == uright || otmp == uleft) {
        /* Sometimes we want to give the off_msg before removing and sometimes
           after; for instance, "you were wearing a moonstone ring (on right
           hand)" is desired but "you were wearing a square amulet (being
           worn)" is not because of the redundant "being worn". */
        off_msg(otmp);
        Ring_off(otmp);
    } else if (otmp == uamul) {
        Amulet_off();
        off_msg(otmp);
    } else if (otmp == ublindf) {
        Blindf_off(otmp);       /* does its own off_msg */
    } else {
        impossible("removing strange accessory?");
    }
    return 1;
}

/* Check if something worn is cursed _and_ unremovable. */
int
cursed(struct obj *otmp)
{
    /* Curses, like chickens, come home to roost. */
    if ((otmp == uwep) ? welded(otmp) : (int)otmp->cursed) {
        pline("You can't.  %s cursed.",
              (is_boots(otmp) || is_gloves(otmp) || otmp->otyp == LENSES ||
               otmp->quan > 1L)
              ? "They are" : "It is");
        otmp->bknown = TRUE;
        return 1;
    }
    return 0;
}

int
armoroff(struct obj *otmp)
{
    int delay = -objects[otmp->otyp].oc_delay;

    if (cursed(otmp))
        return 0;
    if (delay) {
        nomul(delay, "disrobing");
        if (is_helmet(otmp)) {
            nomovemsg = "You finish taking off your helmet.";
            afternmv = Helmet_off;
        } else if (is_gloves(otmp)) {
            nomovemsg = "You finish taking off your gloves.";
            afternmv = Gloves_off;
        } else if (is_boots(otmp)) {
            nomovemsg = "You finish taking off your boots.";
            afternmv = Boots_off;
        } else {
            nomovemsg = "You finish taking off your suit.";
            afternmv = Armor_off;
        }
    } else {
        /* Be warned! We want off_msg after removing the item to avoid "You
           were wearing ____ (being worn)." However, an item which grants fire
           resistance might cause some trouble if removed in Hell and
           lifesaving puts it back on; in this case the message will be printed 
           at the wrong time (after the messages saying you died and were
           lifesaved).  Luckily, no cloak, shield, or fast-removable armor
           grants fire resistance, so we can safely do the off_msg afterwards.
           Rings do grant fire resistance, but for rings we want the off_msg
           before removal anyway so there's no problem.  Take care in adding
           armors granting fire resistance; this code might need modification.
           3.2 (actually 3.1 even): this comment is obsolete since fire
           resistance is not needed for Gehennom. */
        if (is_cloak(otmp))
            Cloak_off();
        else if (is_shield(otmp))
            Shield_off();
        else
            setworn(NULL, otmp->owornmask & W_ARMOR);
        off_msg(otmp);
    }
    takeoff_mask = 0L;
    taking_off = os_invalid;
    return 1;
}

static void
already_wearing(const char *cc)
{
    pline("You are already wearing %s%c", cc, (cc == c_that_) ? '!' : '.');
}

/*
 * canwearobj checks to see whether the player can wear a piece of armor or
 * jewellery (i.e. anything that can meaningfully have bits in W_WORN set).
 *
 * This is used in two basic contexts:
 *
 * a) As a check in response to the user attempting to wear a piece of armor.
 *    In this case, the code will pline() out explanations about why the
 *    armor can't be worn, if it can't be worn. With this case (noisy != FALSE),
 *    it will use actual knowledge about BCUs, and set bknown accordingly.
 *
 * b) To check to see if and where an item can be equipped, in preparation for
 *    presenting a menu of sensible options to the user, or to determine
 *    whether the item is still worn after polymorph. In this case, the code
 *    will stay quiet about mistakes and just return FALSE. This is based only
 *    on character knowledge, so may return TRUE in situations where the item
 *    cannot be equipped (if the character does not know why it cannot be
 *    equipped).
 *
 * This function now also checks for the amulet, ring, and blindfold slots.
 * In the case of rings, *mask will be W_RING if both ring slots are free,
 * W_MASK(os_ringl) or W_MASK(os_ringr) if only one ring slot is free.
 *
 * inputs: otmp (the piece of armor)
 *         noisy (TRUE to complain if wearing is not possible)
 * output: mask (otmp's armor type)
 */
boolean
canwearobj(struct obj *otmp, long *mask, boolean noisy)
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
       equipped; the item is equippable; the equip is not blocked by a cursed
       two-handed weapon */
    if ((slot == os_arm || slot == os_armc || slot == os_armu) &&
        cantweararm(youmonst.data) &&
        /* same exception for cloaks as used in m_dowear() */
        (slot != os_armc || youmonst.data->msize != MZ_SMALL) &&
        (racial_exception(&youmonst, otmp) < 1)) {
        if (noisy)
            pline("The %s will not fit on your body.", c_slotnames[slot]);
        return FALSE;
    } else if (otmp->owornmask & W_EQUIP) {
        if (noisy)
            already_wearing(c_that_);
        return FALSE;
    } else if (slot == os_invalid) {
        if (noisy)
            silly_thing("equip", otmp);
        return FALSE;
    } else if (slot == os_arm || slot == os_armu || slot == os_ringl) {
        if (noisy && uwep)
            uwep->bknown = TRUE;
        if (uwep && welded(uwep) && bimanual(uwep)) {
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
        if (EQUIP(os_ringl)) temp_mask &= ~W_MASK(os_ringl);
        if (EQUIP(os_ringr)) temp_mask &= ~W_MASK(os_ringr);
        if (!temp_mask) {
            if (noisy)
                already_wearing("two rings");
            return FALSE;
        }
    } else {
        if (EQUIP(slot)) {
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
        break;

    case os_arms:
        if (uwep && bimanual(uwep)) {
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

    case os_armg:
        if (welded(uwep)) {
            if (noisy)
                pline("You cannot wear gloves over your %s.",
                      is_sword(uwep) ? "sword" : "weapon");
            return FALSE;
        }
        break;

    case os_armu:
        if (uarm || uarmc) {
            if (noisy)
                pline("You can't wear that over your %s.",
                      (uarm && !uarmc) ? "armor" : cloak_simple_name(uarmc));
            return FALSE;
        }
        break;

    case os_arm:
        if (uarmc) {
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
        if (uarmg && noisy)
            uarmg->bknown = TRUE;
        if (uarmg && uarmg->cursed && uarmg->bknown) {
            if (noisy)
                pline("You cannot remove your gloves to put on the ring.");
            return FALSE;
        }
        if (uwep && uwep->bknown && welded(uwep)) {
            /* We've already checked for a weld to both hands, but we also need
               to check for a weld to just the right hand; this forces the ring
               onto the left hand, or prevents wearing it if the left hand is
               full. */
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

/* the 'W' command */
int
dowear(struct obj *otmp)
{
    int delay;
    long mask = 0L;

    if (otmp && !validate_object(otmp, clothes_and_accessories, "wear"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "wear");
    if (!otmp)
        return 0;

    /* TODO: merge doputon into dowear */
    if (otmp->oclass != ARMOR_CLASS)
        return doputon(otmp);

    /* cantweararm checks for suits of armor */
    /* verysmall or nohands checks for shields, gloves, etc... */
    if ((verysmall(youmonst.data) || nohands(youmonst.data))) {
        pline("Don't even bother.");
        return 0;
    }

    if (!canwearobj(otmp, &mask, TRUE) || !(mask & W_ARMOR))
        return 0;

    if (otmp->oartifact && !touch_artifact(otmp, &youmonst))
        return 1;       /* costs a turn even though it didn't get worn */

    if (otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT &&
        qstart_level.dnum == u.uz.dnum) {   /* in quest */
        if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
            pline("You narrowly avoid losing all chance at your goal.");
        else    /* converted */
            pline("You are suddenly overcome with shame and change your mind.");
        u.ublessed = 0; /* lose your god's protection */
        makeknown(otmp->otyp);
        iflags.botl = 1;
        return 1;
    }

    unwield_silently(otmp);
    setworn(otmp, mask);
    delay = -objects[otmp->otyp].oc_delay;

    int (*after) (void) = NULL;

    if (is_boots(otmp))
        after = Boots_on;
    if (is_helmet(otmp))
        after = Helmet_on;
    if (is_gloves(otmp))
        after = Gloves_on;
    if (otmp == uarm)
        after = Armor_on;
    if (is_cloak(otmp))
        after = Cloak_on;
    if (is_shirt(otmp))
        after = Shirt_on;
    if (is_shield(otmp))
        after = Shield_on;

    if (!after) {
        impossible("No after function for equipping?");
    } else if (delay) {
        nomul(delay, "dressing up");
        afternmv = after;
        nomovemsg = "You finish your dressing maneuver.";
    } else {
        after();
    }
    takeoff_mask = 0L;
    taking_off = os_invalid;
    return 1;
}

int
doputon(struct obj *otmp)
{
    long mask = 0L;

    if (otmp && !validate_object(otmp, clothes_and_accessories, "put on"))
        return 0;
    else if (!otmp)
        otmp = getobj(clothes_and_accessories, "put on");
    if (!otmp)
        return 0;
    if (otmp->oclass == ARMOR_CLASS)
        return dowear(otmp);

    if (welded(otmp)) {
        weldmsg(otmp);
        return 0;
    }

    if (!canwearobj(otmp, &mask, TRUE))
        return 0;

    unwield_silently(otmp);

    /* If a ring could go on either finger, inquire as to which to use. */
    while (mask == W_RING) {
        char qbuf[QBUFSZ];
        char answer;
        
        sprintf(qbuf, "Which %s%s, Right or Left?",
                humanoid(youmonst.data) ? "ring-" : "",
                body_part(FINGER));
        if (!(answer = yn_function(qbuf, "rl", '\0')))
            return 0;
        switch (answer) {
        case 'l':
        case 'L':
            mask = W_MASK(os_ringl);
            break;
        case 'r':
        case 'R':
            mask = W_MASK(os_ringr);
            break;
        }
    };

    if (otmp->oartifact && !touch_artifact(otmp, &youmonst))
        return 1;   /* costs a turn even though it didn't get worn */

    setworn(otmp, mask);
    /* Don't do a prinv if an amulet is consumed. */
    if (Slot_on(objslot_from_mask(mask)))
        return 1;

    if (is_worn(otmp))
        prinv(NULL, otmp, 0L);

    return 1;
}


void
find_ac(void)
{
    int uac = mons[u.umonnum].ac;

    /* Armor transformed into dragon skin gives no AC bonus. TODO: Should it at
       least give a bonus/penaltyt from its enchantment? */
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
            Ring_off(uleft);
            dropx(otmp);
        }
        if (rightfall) {
            otmp = uright;
            Ring_off(uright);
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

    otmph = (victim == &youmonst) ? uarmc : which_armor(victim, os_armc);
    if (!otmph && !uskin())
        otmph = (victim == &youmonst) ? uarm : which_armor(victim, os_arm);
    if (!otmph)
        otmph = (victim == &youmonst) ? uarmu : which_armor(victim, os_armu);

    otmp = (victim == &youmonst) ? uarmh : which_armor(victim, os_armh);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarmg : which_armor(victim, os_armg);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarmf : which_armor(victim, os_armf);
    if (otmp && (!otmph || !rn2(4)))
        otmph = otmp;
    otmp = (victim == &youmonst) ? uarms : which_armor(victim, os_arms);
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
    if (ring != uleft && ring != uright) {
        impossible("stuck_ring: neither left nor right?");
        return NULL;
    }

    if (ring && ring->otyp == otyp) {
        /* reasons ring can't be removed match those checked by select_off();
           limbless case has extra checks because ordinarily it's temporary */
        if (nolimbs(youmonst.data) && uamul &&
            uamul->otyp == AMULET_OF_UNCHANGING && uamul->cursed)
            return uamul;
        if (welded(uwep) && (ring == uright || bimanual(uwep)))
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

/* occupation callback for 'A' */
static int
select_off(struct obj *otmp)
{
    struct obj *why;
    char buf[BUFSZ];
    enum objslot i;

    if (!otmp)
        return 0;
    *buf = '\0';        /* lint suppresion */

    /* special ring checks */
    if (otmp == uright || otmp == uleft) {
        if (nolimbs(youmonst.data)) {
            pline("The ring is stuck.");
            return 0;
        }
        why = 0;        /* the item which prevents ring removal */
        if (welded(uwep) && (otmp == uright || bimanual(uwep))) {
            sprintf(buf, "free a weapon %s", body_part(HAND));
            why = uwep;
        } else if (uarmg && uarmg->cursed) {
            sprintf(buf, "take off your gloves");
            why = uarmg;
        }
        if (why) {
            pline("You cannot %s to remove the ring.", buf);
            why->bknown = TRUE;
            return 0;
        }
    }
    /* special glove checks */
    if (otmp == uarmg) {
        if (welded(uwep)) {
            pline(
                "You are unable to take off your gloves while wielding that %s.",
                "gloves", is_sword(uwep) ? "sword" : "weapon");
            uwep->bknown = TRUE;
            return 0;
        } else if (Glib) {
            pline(
                "You can't take off the slippery gloves with your slippery %s.",
                makeplural(body_part(FINGER)));
            return 0;
        }
    }
    /* special boot checks */
    if (otmp == uarmf) {
        if (u.utrap && u.utraptype == TT_BEARTRAP) {
            pline("The bear trap prevents you from pulling your %s out.",
                  body_part(FOOT));
            return 0;
        } else if (u.utrap && u.utraptype == TT_INFLOOR) {
            pline("You are stuck in the %s, and cannot pull your %s out.",
                  surface(u.ux, u.uy), makeplural(body_part(FOOT)));
            return 0;
        }
    }
    /* special suit and shirt checks */
    if (otmp == uarm || otmp == uarmu) {
        why = 0;        /* the item which prevents disrobing */
        if (uarmc && uarmc->cursed) {
            sprintf(buf, "remove your %s", cloak_simple_name(uarmc));
            why = uarmc;
        } else if (otmp == uarmu && uarm && uarm->cursed) {
            /* We could add a check for removing a shirt underneath skin, but
               that scenario is kind-of absurd and currently can't happen. */
            sprintf(buf, "remove your armor");
            why = uarm;
        } else if (welded(uwep) && bimanual(uwep)) {
            sprintf(buf, "release your %s",
                    is_sword(uwep) ? "sword" :
                    (uwep->otyp == BATTLE_AXE) ? "axe" : "weapon");
            why = uwep;
        }
        if (why) {
            pline("You cannot %s to take off %s.", buf, the(xname(otmp)));
            why->bknown = TRUE;
            return 0;
        }
    }
    /* basic curse check */
    if (otmp == uquiver || (otmp == uswapwep && !u.twoweap)) {
        ;       /* some items can be removed even when cursed */
    } else {
        /* otherwise, this is fundamental */
        if (cursed(otmp))
            return 0;
    }

    for (i = 0; i <= os_last_equip; i++) {
        if (otmp == EQUIP(i)) {
            takeoff_mask |= W_MASK(i);
            break;
        }
        if (i == os_last_equip)
            impossible("select_off: %s???", doname(otmp));
    }

    return 0;
}

static struct obj *
do_takeoff(void)
{
    struct obj *otmp = NULL;

    if (taking_off == os_wep) {
        if (!cursed(uwep)) {
            setuwep(NULL);
            pline("You are empty %s.", body_part(HANDED));
            u.twoweap = FALSE;
        }
    } else if (taking_off == os_swapwep) {
        setuswapwep(NULL);
        pline("You no longer have a second weapon readied.");
        u.twoweap = FALSE;
    } else if (taking_off == os_quiver) {
        setuqwep(NULL);
        pline("You no longer have ammunition readied.");
    } else if (taking_off <= os_last_worn) {
        otmp = EQUIP(taking_off);
        if (!cursed(otmp))
            Slot_off(taking_off);
    } else
        impossible("do_takeoff: taking off %d", (int)taking_off);

    return otmp;
}

static const char *disrobing;

static int
take_off(void)
{
    int i;
    struct obj *otmp;

    if (taking_off != os_invalid) {
        if (todelay > 0) {
            todelay--;
            return 1;   /* still busy */
        } else {
            if ((otmp = do_takeoff()))
                off_msg(otmp);
        }
        takeoff_mask &= ~W_MASK(taking_off);
        taking_off = os_invalid;
    }

    for (i = 0; takeoff_order[i] != os_invalid; i++)
        if (takeoff_mask & W_MASK(takeoff_order[i])) {
            taking_off = takeoff_order[i];
            break;
        }

    otmp = NULL;
    todelay = 0;

    if (taking_off == os_invalid) {
        pline("You finish %s.", disrobing);
        return 0;
    } else if (taking_off <= os_last_armor) {
        otmp = EQUIP(taking_off);
        /* If a cloak is being worn, add the time to take it off and put it
           back on again.  Kludge alert! since that time is 0 for all known
           cloaks, add 1 so that it actually matters... */
        if (taking_off == os_arm && uarmc)
            todelay += 2 * objects[uarmc->otyp].oc_delay + 1;
        if (taking_off == os_armu) {
            if (uarm)
                todelay += 2 * objects[uarm->otyp].oc_delay;
            if (uarmc)
                todelay += 2 * objects[uarmc->otyp].oc_delay + 1;
        }
    } else if (taking_off == os_tool) {
        todelay = 2;
    } else if (taking_off <= os_last_equip) {
        /* TODO: 3.4.3 has a delay of 1 for secondary weapon and quiver.
           This seems wrong, especially for the quiver, but is currently
           preserved; this whole code needs a rewrite anyway. */
        todelay = 1;
    } else {
        impossible("take_off: taking off %d", (int)taking_off);
        return 0;       /* force done */
    }

    if (otmp)
        todelay += objects[otmp->otyp].oc_delay;

    /* Since setting the occupation now starts the counter next move, that
       would always produce a delay 1 too big per item unless we subtract 1
       here to account for it. */
    if (todelay > 0)
        todelay--;

    set_occupation(take_off, disrobing, 0);
    return 1;   /* get busy */
}

/* clear saved context to avoid inappropriate resumption of interrupted 'A' */
void
reset_remarm(void)
{
    takeoff_mask = 0L;
    taking_off = os_invalid;
    disrobing = NULL;
}

/* the 'A' command -- remove multiple worn items */
int
doddoremarm(void)
{
    if (taking_off != os_invalid || takeoff_mask) {
        pline("You continue %s.", disrobing);
        set_occupation(take_off, disrobing, 0);
        return 0;
    } else if (!uwep && !uswapwep && !uquiver && !uamul && !ublindf && !uleft &&
               !uright && !wearing_armor()) {
        pline("You are not wearing anything.");
        return 0;
    }

    add_valid_menu_class(0);    /* reset */
    menu_remarm(0);

    if (takeoff_mask) {
        /* default activity for armor and/or accessories, possibly combined
           with weapons */
        disrobing = "disrobing";
        /* if removing only weapons (i.e. unworn equipment), use a different
           verb */
        if (!(takeoff_mask & ~(W_EQUIP & ~W_WORN)))
            disrobing = "disarming";
        take_off();
    }
    /* The time to perform the command is already completely accounted for in
       take_off(); if we return 1, that would add an extra turn to each
       disrobe. */
    return 0;
}

static int
menu_remarm(int retry)
{
    int n, i = 0;
    int pick_list[30];
    struct object_pick *obj_pick_list;
    boolean all_worn_categories = TRUE;

    if (retry) {
        all_worn_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_worn_categories = FALSE;
        n = query_category("What type of things do you want to take off?",
                           invent, WORN_TYPES | ALL_TYPES, pick_list, PICK_ANY);
        if (!n)
            return 0;
        for (i = 0; i < n; i++) {
            if (pick_list[i] == ALL_TYPES_SELECTED)
                all_worn_categories = TRUE;
            else
                add_valid_menu_class(pick_list[i]);
        }
    }

    n = query_objlist("What do you want to take off?", invent,
                      SIGNAL_NOMENU | USE_INVLET | INVORDER_SORT,
                      &obj_pick_list, PICK_ANY,
                      all_worn_categories ? is_worn : is_worn_by_type);
    if (n > 0) {
        for (i = 0; i < n; i++)
            select_off(obj_pick_list[i].obj);
        free(obj_pick_list);
    } else if (n < 0) {
        pline("There is nothing else you can remove or unwield.");
    }
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

    if (DESTROY_ARM(uarmc)) {
        if (donning(otmp))
            cancel_don();
        pline("Your %s crumbles and turns to dust!", cloak_simple_name(uarmc));
        Cloak_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarm)) {
        if (donning(otmp))
            cancel_don();
        pline("Your armor turns to dust and falls to the %s!",
              surface(u.ux, u.uy));
        Armor_gone();
        useup(otmp);
    } else if (DESTROY_ARM(uarmu)) {
        if (donning(otmp))
            cancel_don();
        pline("Your shirt crumbles into tiny threads and falls apart!");
        Shirt_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarmh)) {
        if (donning(otmp))
            cancel_don();
        pline("Your helmet turns to dust and is blown away!");
        Helmet_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarmg)) {
        char kbuf[BUFSZ];

        sprintf(kbuf, "losing %s gloves while wielding", uhis());
        if (donning(otmp))
            cancel_don();
        pline("Your gloves vanish!");
        Gloves_off();
        useup(otmp);
        selftouch("You", kbuf);
    } else if (DESTROY_ARM(uarmf)) {
        if (donning(otmp))
            cancel_don();
        pline("Your boots disintegrate!");
        Boots_off();
        useup(otmp);
    } else if (DESTROY_ARM(uarms)) {
        if (donning(otmp))
            cancel_don();
        pline("Your shield crumbles away!");
        Shield_off();
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
