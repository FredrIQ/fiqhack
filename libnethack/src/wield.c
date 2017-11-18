/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* KMH -- Differences between the three weapon slots.
 *
 * The main weapon (uwep):
 * 1.  Is filled by the (w)ield command.
 * 2.  Can be filled with any type of item.
 * 3.  May be carried in one or both hands.
 * 4.  Is used as the melee weapon and as the launcher for ammunition.
 * 5.  Only conveys extrinsics when it is a weapon, weapon-tool, or artifact.
 * 6.  Certain cursed items will weld to the hand and cannot be unwielded or
 *     dropped.  See sensible_wep() and will_weld() below for the list of which
 *     items apply.
 *
 * The secondary weapon (uswapwep):
 * 1.  Is filled by the e(x)change command, which swaps this slot with the main
 *     weapon.  If the "pushweapon" option is set, the (w)ield command will also
 *     store the old weapon in the secondary slot.
 * 2.  Can be filled with anything that will fit in the main weapon slot; that
 *     is, any type of item.
 * 3.  Is usually NOT considered to be carried in the hands.  That would force
 *     too many checks among the main weapon, second weapon, shield, gloves, and
 *     rings; and it would further be complicated by bimanual weapons.  A
 *     special exception is made for two-weapon combat.
 * 4.  Is used as the second weapon for two-weapon combat, and as a convenience
 *     to swap with the main weapon.
 * 5.  Never conveys extrinsics.
 * 6.  Cursed items never weld (see #3 for reasons), but they also prevent
 *     two-weapon combat.
 *
 * The quiver (uquiver):
 * 1.  Is filled by the (Q)uiver command.
 * 2.  Can be filled with any type of item.
 * 3.  Is considered to be carried in a special part of the pack.
 * 4.  Is used as the item to throw with the (f)ire command.  This is a
 *     convenience over the normal (t)hrow command.
 * 5.  Never conveys extrinsics.
 * 6.  Cursed items never weld; their effect is handled by the normal
 *     throwing code.
 *
 * No item may be in more than one of these slots.
 */

/* An item that it makes sense for the player to use as a main or readied
   weapon: weapons (including launchers but not including ammo), weapon-tools,
   and iron balls. */
#define sensible_wep(optr) (((optr)->oclass == WEAPON_CLASS && !is_ammo(optr)) \
                            || is_weptool(optr)                         \
                            || (optr)->otyp == HEAVY_IRON_BALL)

/* An item that will weld to the hands if wielded. */
#define will_weld(optr)    ((optr)->cursed  &&                          \
                            (sensible_wep(optr) || (optr)->otyp == TIN_OPENER))


/*** Functions that place a given item in a slot ***/
/* Proper usage includes:
 * 1.  Initializing the slot during character generation or a
 *     restore.
 * 2.  Setting the slot due to a player's actions.
 * 3.  If one of the objects in the slot are split off, these
 *     functions can be used to put the remainder back in the slot.
 * 4.  Putting an item that was thrown and returned back into the slot.
 * 5.  Emptying the slot, by passing a null object.  NEVER pass
 *     zeroobj!
 *
 * If the item is being moved from another slot, it is the caller's
 * responsibility to handle that.  It's also the caller's responsibility
 * to print the appropriate messages.
 */
void
setuwep(struct obj *obj)
{
    struct obj *olduwep = uwep;

    if (obj == uwep)
        return; /* necessary to not set bashmsg */
    /* This message isn't printed in the caller because it happens *whenever*
       Sunsword is unwielded, from whatever cause. */
    setworn(obj, W_MASK(os_wep));
    if (uwep == obj && artifact_light(olduwep) && olduwep->lamplit) {
        end_burn(olduwep, FALSE);
        if (!Blind)
            pline(msgc_consequence, "%s glowing.", Tobjnam(olduwep, "stop"));
    }
    /* Note: Explicitly wielding a pick-axe will not give a "bashing" message.
       Wielding one via 'a'pplying it will. 3.2.2: Wielding arbitrary objects
       will give bashing message too. */
    if (obj) {
        u.bashmsg = !is_wep(obj);
    } else
        u.bashmsg = TRUE;        /* for "bare hands" message */
    update_inventory();
}

/*
 * Returns a value depending on whether the player can attempt to wield the
 * given object. (Giving NULL or &zeroobj checks to see if unwielding the
 * current weapon is possible, via simply calling canunwearobj(uwep, ...).)
 *
 * The return values are:
 *
 * 0    Wielding the given object is currently impossible.
 * 1    Wielding the given object is possible, but not sensible; that is, it
 *      will not show if you give ? at the wield prompt (but will succeed if you
 *      wield "manually" or via using *).
 * 2    Wielding the given object is both possible and sensible.
 *
 * As always for the canwearobj family of functions, this is based on character
 * knowledge if spoil is FALSE, and on full knowledge of the gamestate if spoil
 * is TRUE.
 *
 * Note that this function returns nonzero in cases where the character may try
 * and fail to wield an item (e.g. barehanded c corpse, or blasting artifact).
 *
 * As with canwearobj, if this function returns zero even with spoil == FALSE,
 * then the wield attempt should not cost a turn. If it does not return zero
 * until spoil == TRUE, then the wield attempt should cost a turn.
 *
 * Also as with canwearobj, the cblock argument should not be set to TRUE at the
 * same time as spoil; cblock == TRUE means that existing equipment should not
 * be considered to block the wielding of a weapon unless it's known to be
 * cursed, whereas cblock == FALSE means that existing equipment slots should be
 * considered to prevent wielding. (For instance, cblock == FALSE will cause the
 * function to return 0 if the item in question is worn, or if it's bimanual and
 * a shield is worn; cblock == TRUE will return nonzero in these cases, unless
 * the offending object is known cursed.)
 *
 * Note that a weapon being in the offhand or quiver slot does not prevent it
 * being wielded. (It'll just be evicted from its previous slot.)
 */
int
canwieldobj(struct obj *wep, boolean noisy, boolean spoil, boolean cblock)
{
    if (uwep && wep != uwep && !canunwearobj(uwep, noisy, spoil, cblock))
        return 0;
    if (!wep || wep == &zeroobj)
        return 2; /* - is a reasonable wield choice */
    if (cantwield(youmonst.data)) {
        if (noisy)
            pline(msgc_cancelled, "You are physically incapable of holding %s.",
                  yname(wep));
        return 0;
    }
    if (uarms && (!cblock || (uarms->cursed && (uarms->bknown || spoil))) &&
        bimanual(wep)) {
        if (noisy) {
            if (!uarms->bknown && cblock) {
                if (!spoil)
                    impossible("Character magically knows their "
                               "shield is cursed?");
                uarms->bknown = 1;
                pline(msgc_failcurse,
                      "You can't remove your shield to wield the weapon.");
            } else {
                pline(msgc_cancelled,
                      "You cannot wield a two-handed %s while wearing "
                      "a shield.", is_sword(wep) ? "sword" : wep->otyp ==
                      BATTLE_AXE ? "axe" : "weapon");
            }
        }
        return 0;
    }
    /* If we can't ready something, we can't wield it either, unless it's our
       currently wielded weapon. */
    if (wep != uwep && !canreadyobj(wep, noisy, spoil, cblock))
        return 0;
    if (sensible_wep(wep))
        return 2;

    return 1;
}

/*
 * As above, but for the swapwep or quiver slots.
 *
 * Return values:
 * 0    Readying and quivering the given object are currently impossible.
 * 1    Readying and quivering the given object are possible but not sensible.
 * 3    Readying the object is possible; quivering is possible and sensible.
 * 5    Quivering the object is possible; readying is possible and sensible.
 * 7    Readying and quivering the given object are both possible and sensible.
 */
int
canreadyobj(struct obj *wep, boolean noisy, boolean spoil, boolean cblock)
{
    if (!wep || wep == &zeroobj)
        return 7;

    if (wep->owornmask & (W_WORN | W_MASK(os_saddle) | W_MASK(os_wep))
        && (!cblock || (wep->cursed && wep->bknown))) {
        if (noisy)
            pline(msgc_cancelled, "You are currently %s %s!",
                  wep == uwep ? "wielding" : "wearing", yname(wep));
        return 0;
    }

    return 1 | (sensible_wep(wep) ? 4 : 0) |
        (is_ammo(wep) || throwing_weapon(wep) ? 2 : 0);
}

/* Places wep in the wielded weapon slot, if possible. wep can be NULL in order
   to unwield the wielded weapon. Returns 0 if the character knew that the wield
   was impossible before starting, and thus didn't even try; 1 if the character
   spent time, regardless of whether the wield succeeded or not. (To determine
   whether the wield suceeded, check uwep, or else wep->owornmask.) */
int
ready_weapon(struct obj *wep)
{
    /* Does the character know it's impossible? */
    if (uwep && !canunwearobj(uwep, TRUE, FALSE, FALSE))
        return 0;
    if (wep && !canwieldobj(wep, TRUE, FALSE, FALSE))
        return 0;
    if (!wep && !uwep) {
        pline(msgc_cancelled, "You are already empty %s.", body_part(HANDED));
        return 0;
    }

    /* At this point, the character attempts to do the wield/unwield. */
    unwield_silently(wep); /* remove from quiver/swapwep */
    if (uwep && !canunwearobj(uwep, TRUE, TRUE, FALSE))
        return 1;
    if (wep && !canwieldobj(wep, TRUE, TRUE, FALSE))
        return 1;

    if (!wep) {
        pline(msgc_actionok, "You are empty %s.", body_part(HANDED));
        setuwep(NULL);
    } else if (!uarmg && wep->otyp == CORPSE &&
               touched_monster(wep->corpsenm)) {
        /* Prevent wielding cockatrice when not wearing gloves --KAA */
        pline(msgc_fatal_predone, "You wield the %s corpse in your bare %s.",
              mons[wep->corpsenm].mname, makeplural(body_part(HAND)));
        instapetrify(killer_msg(STONING,
            msgprintf("wielding %s corpse without gloves",
                      an(mons[wep->corpsenm].mname))));
        /* if the player lifesaves from that, don't wield */
    } else if (wep->oartifact && !touch_artifact(wep, &youmonst)) {
        ; /* you got blasted, don't attempt to wield */
    } else {
        /* Weapon WILL be wielded after this point */
        if (will_weld(wep)) {
            const char *tmp = xname(wep), *thestr = "The ";

            if (strncmp(tmp, thestr, 4) && !strncmp(The(tmp), thestr, 4))
                tmp = thestr;
            else
                tmp = "";
            pline(msgc_substitute, "%s%s %s to your %s!", tmp,
                  aobjnam(wep, "weld"),
                  (wep->quan == 1L) ? "itself" : "themselves", /* a3 */
                  bimanual(wep) ? (const char *)makeplural(body_part(HAND))
                  : body_part(HAND));
            wep->bknown = TRUE;
        } else {
            pline(msgc_actionok, "You are now wielding %s.", yname(wep));
        }
        setuwep(wep);

        /* KMH -- Talking artifacts are finally implemented */
        arti_speak(wep);

        if (artifact_light(wep) && !wep->lamplit) {
            begin_burn(wep, FALSE);
            if (!Blind)
                pline(msgc_consequence, "%s to glow brilliantly!",
                      Tobjnam(wep, "begin"));
        }

        if (wep->unpaid) {
            struct monst *this_shkp;

            if ((this_shkp =
                 shop_keeper(level, inside_shop(level, u.ux, u.uy))) != NULL) {
                pline(msgc_unpaid, "%s says \"You be careful with my %s!\"",
                      shkname(this_shkp), xname(wep));
            }
        }
    }
    return 1;
    }

void
setuqwep(struct obj *obj)
{
    setworn(obj, W_MASK(os_quiver));
    update_inventory();
}

void
setuswapwep(struct obj *obj)
{
    setworn(obj, W_MASK(os_swapwep));
    update_inventory();
}


/*** Commands to change particular slot(s) ***/

static const char wield_objs[] = { ALL_CLASSES, ALLOW_NONE, 0 }; 

int
dowield(const struct nh_cmd_arg *arg)
{
    struct obj *wep = getargobj(arg, wield_objs, "wield");
    if (!wep)
        return 0;

    if (flags.pushweapon) {
        u.utracked[tos_first_equip + os_swapwep] = uwep;
        u.uoccupation_progress[tos_first_equip + os_swapwep] = 0;
    }

    return equip_in_slot(wep, os_wep, TRUE);
}

int
doswapweapon(const struct nh_cmd_arg *arg)
{
    (void) arg;

    enum objslot j;
    for (j = 0; j <= os_last_equip; j++) {
        if (j == os_wep)
            u.utracked[tos_first_equip + j] = uswapwep ? uswapwep : &zeroobj;
        else if (j == os_swapwep)
            u.utracked[tos_first_equip + j] = uwep ? uwep : &zeroobj;
        else
            u.utracked[tos_first_equip + j] = NULL;
        /* We can just set uoccupation_progress to 0 unconditionally because
           wielding does not take multiple turns. */
        u.uoccupation_progress[tos_first_equip + j] = 0;
    }

    if (equip_heartbeat() != 0)
        impossible("Swapping weapons took time?");
    return 0;
}

int
dowieldquiver(const struct nh_cmd_arg *arg)
{
    struct obj *newquiver = getargobj(arg, wield_objs, "quiver");
    if (!newquiver)
        return 0;

    return equip_in_slot(newquiver, os_quiver, FALSE);
}

/* Used to wield an item and then do something else; this is used for tools that
   need to be wielded. Returns a bitmask: the 1s bit is FALSE if the action must
   be aborted; the 2s bit is TRUE if the action took time; the 4s bit is TRUE if
   the item is still in the process of being wielded.

   If both the 2s and 1s bit of the result are set, this function sets the
   occupation itself, to save the caller the trouble. (The caller can still
   override it if, say, it wants to treat 7 and 3 differently message-wise.) */
int
wield_tool(struct obj *obj, const char *occ_txt, enum occupation occupation)
{
    int rv;

    if (obj == uwep)
        return 1;

    if (flags.pushweapon) {
        u.utracked[tos_first_equip + os_swapwep] = uwep;
        u.uoccupation_progress[tos_first_equip + os_swapwep] = 0;
    }

    rv = equip_in_slot(obj, os_wep, TRUE);

    if (obj == uwep && rv == 0) /* it took no time */
        return 1;

    if (rv == 0) /* impossible, and known impossible */
        return 0;

    if (flags.occupation == occ_equip) {
        action_incomplete(occ_txt, occupation);
        return 7;
    }

    if (obj == uwep) {
        action_incomplete(occ_txt, occupation);
        return 3;
    }

    /* Something went wrong equipping it if the desire to equip it disappeared
       but the tool isn't wielded. */
    if (u.utracked[tos_first_equip + os_wep] == NULL)
        return 2;

    impossible("desire to equip a tool, but no occupation");
    return 0;
}

int
can_twoweapon(void)
{
    struct obj *otmp;

#define NOT_WEAPON(obj) (!is_weptool(obj) && obj->oclass != WEAPON_CLASS)
    if (!could_twoweap(youmonst.data)) {
        if (cantwield(youmonst.data))
            pline(msgc_cancelled, "Don't be ridiculous!");
        else if (Upolyd)
            pline(msgc_cancelled,
                  "You can't use two weapons in your current form.");
        else
            pline(msgc_cancelled,
                  "%s aren't able to use two weapons at once.",
                  msgupcasefirst(makeplural(
                                     (u.ufemale && urole.name.f) ?
                                     urole.name.f : urole.name.m)));
    } else if (!uwep || !uswapwep)
        pline(msgc_cancelled,
              "Your %s%s%s empty.", uwep ? "left " : uswapwep ? "right " : "",
              body_part(HAND), (!uwep && !uswapwep) ? "s are" : " is");
    else if (NOT_WEAPON(uwep) || NOT_WEAPON(uswapwep)) {
        otmp = NOT_WEAPON(uwep) ? uwep : uswapwep;
        pline(msgc_cancelled, "%s %s.", Yname2(otmp),
              is_plural(otmp) ? "aren't weapons" : "isn't a weapon");
    } else if (bimanual(uwep) || bimanual(uswapwep)) {
        otmp = bimanual(uwep) ? uwep : uswapwep;
        pline(msgc_cancelled, "%s isn't one-handed.", Yname2(otmp));
    } else if (uarms)
        pline(msgc_cancelled,
              "You can't use two weapons while wearing a shield.");
    else if (uswapwep->oartifact)
        pline(msgc_cancelled, "%s %s being held second to another weapon!",
              Yname2(uswapwep), otense(uswapwep, "resist"));
    else if (!uarmg && !Stone_resistance &&
             (uswapwep->otyp == CORPSE &&
              touch_petrifies(&mons[uswapwep->corpsenm]))) {
        pline(msgc_fatal_predone, "You wield the %s corpse with your bare %s.",
              mons[uswapwep->corpsenm].mname, body_part(HAND));
        instapetrify(killer_msg(STONING,
            msgprintf("wielding %s corpse without gloves",
                      an(mons[uswapwep->corpsenm].mname))));
    } else if (Glib || uswapwep->cursed) {
        if (!Glib)
            uswapwep->bknown = TRUE;
        drop_uswapwep();
    } else
        return TRUE;
    return FALSE;
}

void
drop_uswapwep(void)
{
    struct obj *obj = uswapwep;

    pline(msgc_itemloss, "Your %s from your %s!",
          aobjnam(obj, "slip"), makeplural(body_part(HAND)));
    unwield_silently(obj);
    dropx(obj);
}

int
dotwoweapon(const struct nh_cmd_arg *arg)
{
    (void) arg;

    /* You can always toggle it off */
    if (u.twoweap) {
        pline(msgc_actionok, "You switch to your primary weapon.");
        u.twoweap = 0;
        update_inventory();
        return 0;
    }

    /* May we use two weapons? */
    if (can_twoweapon()) {
        /* Success! */
        pline(msgc_actionok, "You begin two-weapon combat.");
        u.twoweap = 1;
        update_inventory();
        return rnd(20) > ACURR(A_DEX);
    }
    return 0;
}

/*** Functions to empty a given slot ***/
/*
 * These should be used only when the item can't be put back in
 * the slot by life saving.  Proper usage includes:
 * 1.  The item has been eaten, stolen, burned away, or rotted away.
 * 2.  Making an item disappear for a bones pile.
 */
void
uwepgone(void)
{
    if (uwep) {
        if (artifact_light(uwep) && uwep->lamplit) {
            end_burn(uwep, FALSE);
            if (!Blind)
                pline(msgc_consequence, "%s glowing.", Tobjnam(uwep, "stop"));
        }
        setworn(NULL, W_MASK(os_wep));
        u.bashmsg = FALSE;
        update_inventory();
    }
}

void
uswapwepgone(void)
{
    if (uswapwep) {
        setworn(NULL, W_MASK(os_swapwep));
        update_inventory();
    }
}

void
uqwepgone(void)
{
    if (uquiver) {
        setworn(NULL, W_MASK(os_quiver));
        update_inventory();
    }
}

void
untwoweapon(void)
{
    if (u.twoweap) {
        pline(msgc_consequence, "You can no longer use two weapons at once.");
        u.twoweap = FALSE;
        update_inventory();
    }
    return;
}


int
chwepon(struct obj *otmp, int amount)
{
    const char *color = hcolor((amount < 0) ? "black" : "blue");
    const char *xtime;
    int otyp = STRANGE_OBJECT;

    if (!uwep || (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep))) {
        const char *buf = msgprintf(
            "Your %s %s.", makeplural(body_part(HAND)),
            (amount >= 0) ? "twitch" : "itch");
        strange_feeling(otmp, buf);
        exercise(A_DEX, (boolean) (amount >= 0));
        return 0;
    }

    if (otmp && otmp->oclass == SCROLL_CLASS)
        otyp = otmp->otyp;

    if (uwep->otyp == WORM_TOOTH && amount >= 0) {
        uwep->otyp = CRYSKNIFE;
        uwep->oerodeproof = 0;
        pline(msgc_itemrepair, "Your weapon seems sharper now.");
        uwep->cursed = 0;
        if (otyp != STRANGE_OBJECT)
            makeknown(otyp);
        goto weapon_unpaid_fixup;
    }

    if (uwep->otyp == CRYSKNIFE && amount < 0) {
        if (uwep->unpaid)
            costly_damage_obj(uwep);
        uwep->otyp = WORM_TOOTH;
        uwep->oerodeproof = 0;
        pline(msgc_itemloss, "Your weapon seems duller now.");
        if (otyp != STRANGE_OBJECT && otmp->bknown)
            makeknown(otyp);
        return 1;
    }

    if (amount < 0 && uwep->oartifact && restrict_name(uwep, ONAME(uwep))) {
        if (!Blind)
            pline(msgc_failcurse,
                  "Your %s %s.", aobjnam(uwep, "faintly glow"), color);
        return 1;
    }
    if (amount < 0 && uwep->unpaid)
        costly_damage_obj(uwep);
    /* there is a (soft) upper and lower limit to uwep->spe */
    if (((uwep->spe > 5 && amount >= 0) || (uwep->spe < -5 && amount < 0))
        && rn2(3)) {
        if (!Blind)
            pline(msgc_itemloss, "Your %s %s for a while and then %s.",
                  aobjnam(uwep, "violently glow"), color,
                  otense(uwep, "evaporate"));
        else
            pline(msgc_itemloss, "Your %s.", aobjnam(uwep, "evaporate"));

        useupall(uwep); /* let all of them disappear */
        return 1;
    }
    if (!Blind) {
        xtime = (amount * amount == 1) ? "moment" : "while";
        pline(Hallucination ? msgc_nospoil :
              amount == 0 ? msgc_failrandom :
              amount > 0 ? msgc_itemrepair : msgc_itemloss,
              "Your %s %s for a %s.",
              aobjnam(uwep, amount == 0 ? "violently glow" : "glow"), color,
              xtime);
        if (otyp != STRANGE_OBJECT && uwep->known &&
            (amount > 0 || (amount < 0 && otmp->bknown)))
            makeknown(otyp);
    }
    uwep->spe += amount;
    if (amount > 0)
        uwep->cursed = 0;

    /* 
     * Enchantment, which normally improves a weapon, has an
     * addition adverse reaction on Magicbane whose effects are
     * spe dependent.  Give an obscure clue here.
     */
    if (uwep->oartifact == ART_MAGICBANE && uwep->spe >= 0) {
        pline_implied(msgc_hint, "Your right %s %sches!", body_part(HAND),
                      (((amount > 1) && (uwep->spe > 1)) ? "flin" : "it"));
    }

    /* an elven magic clue, cookie@keebler */
    /* elven weapons vibrate warningly when enchanted beyond a limit */
    if ((uwep->spe > 5)
        && (is_elven_weapon(uwep) || uwep->oartifact || !rn2(7)))
        pline_implied(msgc_hint, "Your %s unexpectedly.",
                      aobjnam(uwep, "suddenly vibrate"));

weapon_unpaid_fixup:
    if (uwep->unpaid && (amount >= 0)) {
        adjust_bill_val(uwep);
    }

    return 1;
}

int
welded(struct obj *obj)
{
    if (obj && obj == uwep && will_weld(obj)) {
        obj->bknown = TRUE;
        return 1;
    }
    return 0;
}

void
weldmsg(enum msg_channel msgc, struct obj *obj)
{
    pline(msgc, "Your %s %s welded to your %s!", xname(obj),
          otense(obj, "are"), bimanual(obj) ?
          (const char *)makeplural(body_part(HAND)) : body_part(HAND));
}

/* Unwields all weapons silently. */
void
unwield_weapons_silently(void)
{
    setuwep(NULL);
    setuswapwep(NULL);
    setuqwep(NULL);
    u.twoweap = FALSE;
}

/* Unwields a given weapon silently. */
void
unwield_silently(struct obj *obj)
{
    if (obj == uwep)
        setuwep(NULL);
    if (obj == uswapwep)
        setuswapwep(NULL);
    if (obj == uquiver)
        setuqwep(NULL);
}

/*wield.c*/

