/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-16 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2014 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* This file is responsible for determining whether the character has intrinsics
   and extrinsics, because it was previously done with a bunch of macros, which
   are rather hard to iterate over, and make it even harder to work with the
   game's logic. */

/* Returns an object slot mask giving all the reasons why a player might have
   the given property, limited by "reasons", an object slot mask (W_EQUIP,
   INTRINSIC, and ANY_PROPERTY are the most likely values here, but you can
   specify slots individually if you like). */
unsigned
u_have_property(enum youprop property, unsigned reasons,
                boolean even_if_blocked)
{
    unsigned rv = 0;

    /* The general case for equipment */
    rv |= worn_extrinsic(property);

    /* Special cases from equipment (TODO: move these to the object description,
       or to worn_extrinsic) */
    if (property == SICK_RES && defends(AD_DISE,uwep))
        rv |= W_MASK(os_wep);
    if (property == BLINDED && ublindf && ublindf->otyp != LENSES)
        rv |= W_MASK(os_tool);

    /* Intrinsics */
    if (u.uintrinsic[property] & TIMEOUT)
        rv |= W_MASK(os_timeout);
    rv |= u.uintrinsic[property] & (INTRINSIC | I_SPECIAL);

    /* Birth options */
    if (property == BLINDED && flags.permablind)
        rv |= W_MASK(os_birthopt);
    if (property == HALLUC && flags.permahallu)
        rv |= W_MASK(os_birthopt);

    /* Polyform */
    /* TODO: Change the monster data code into something that doesn't require a
       giant switch statement or ternary chain to get useful information from
       it. We use a ternary chain here because it cuts down on repetitive code
       and so is easier to read. */
    if (property == FIRE_RES     ? resists_fire(&youmonst)                   :
        property == COLD_RES     ? resists_cold(&youmonst)                   :
        property == SLEEP_RES    ? resists_sleep(&youmonst)                  :
        property == DISINT_RES   ? resists_disint(&youmonst)                 :
        property == SHOCK_RES    ? resists_elec(&youmonst)                   :
        property == POISON_RES   ? resists_poison(&youmonst)                 :
        property == DRAIN_RES    ? resists_drli(&youmonst)                   :
        property == SICK_RES     ? youmonst.data->mlet == S_FUNGUS ||
                                   youmonst.data == &mons[PM_GHOUL]          :
        property == ANTIMAGIC    ? resists_magm(&youmonst)                   :
        property == ACID_RES     ? resists_acid(&youmonst)                   :
        property == STONE_RES    ? resists_ston(&youmonst)                   :
        property == STUNNED      ? u.umonnum == PM_STALKER ||
                                   youmonst.data->mlet == S_BAT              :
        property == BLINDED      ? !haseyes(youmonst.data)                   :
        property == HALLUC       ? Upolyd && dmgtype(youmonst.data, AD_HALU) :
        property == SEE_INVIS    ? perceives(youmonst.data)                  :
        property == TELEPAT      ? telepathic(youmonst.data)                 :
        property == INFRAVISION  ? infravision(youmonst.data)                :
        property == INVIS        ? pm_invisible(youmonst.data)               :
        property == TELEPORT     ? can_teleport(youmonst.data)               :
        property == LEVITATION   ? is_floater(youmonst.data)                 :
        property == FLYING       ? is_flyer(youmonst.data)                   :
        property == SWIMMING     ? is_swimmer(youmonst.data)                 :
        property == PASSES_WALLS ? passes_walls(youmonst.data)               :
        property == REGENERATION ? regenerates(youmonst.data)                :
        property == REFLECTING   ? youmonst.data == &mons[PM_SILVER_DRAGON]  :
        property == TELEPORT_CONTROL  ? control_teleport(youmonst.data)      :
        property == MAGICAL_BREATHING ? amphibious(youmonst.data)            :
        0)
        rv |= W_MASK(os_polyform);

    /* External circumstances */
    if (property == BLINDED && u_helpless(hm_unconscious))
        rv |= W_MASK(os_circumstance);

    /* Riding */
    if (property == FLYING && u.usteed && is_flyer(u.usteed->data))
        rv |= W_MASK(os_saddle);
    if (property == SWIMMING && u.usteed && is_swimmer(u.usteed->data))
        rv |= W_MASK(os_saddle);

    /* Overrides */
    if (!even_if_blocked) {
        if (property == BLINDED &&
            ublindf && ublindf->oartifact == ART_EYES_OF_THE_OVERWORLD)
            rv &= (unsigned)(W_MASK(os_circumstance) | W_MASK(os_birthopt));
        if (property == WWALKING && Is_waterlevel(&u.uz))
            rv &= (unsigned)(W_MASK(os_birthopt));
        if (worn_blocked(property))
            rv &= (unsigned)(W_MASK(os_birthopt));
    }

    return rv & reasons;
}

/* Enlightenment and conduct */
static const char
     You_[] = "You ", are[] = "are ", were[] = "were ", have[] =
    "have ", had[] = "had ", can[] = "can ", could[] = "could ";
static const char
     have_been[] = "have been ", have_never[] = "have never ", never[] =
    "never ";

#define enl_msg(menu,prefix,present,past,suffix) \
            enlght_line(menu,prefix, final ? past : present, suffix)
#define you_are(menu,attr)            enl_msg(menu,You_,are,were,attr)
#define you_have(menu,attr)           enl_msg(menu,You_,have,had,attr)
#define you_can(menu,attr)            enl_msg(menu,You_,can,could,attr)
#define you_have_been(menu,goodthing) enl_msg(menu,You_,have_been,were, \
                                              goodthing)
#define you_have_never(menu,badthing) \
            enl_msg(menu,You_,have_never,never,badthing)
#define you_have_X(menu,something) \
            enl_msg(menu,You_,have,(const char *)"", something)

static void
enlght_line(struct nh_menulist *menu, const char *start, const char *middle,
            const char *end)
{
    const char *buf;

    buf = msgprintf("%s%s%s.", start, middle, end);
    add_menutext(menu, buf);
}

/* format increased damage or chance to hit */
static const char *
enlght_combatinc(const char *inctyp, int incamt, int final)
{
    const char *modif, *bonus;

    if (final || wizard) {
        modif = msgprintf("%+d", incamt);
    } else {
        int absamt = abs(incamt);

        if (absamt <= 3)
            modif = "small";
        else if (absamt <= 6)
            modif = "moderate";
        else if (absamt <= 12)
            modif = "large";
        else
            modif = "huge";
    }
    bonus = (incamt > 0) ? "bonus" : "penalty";
    /* "bonus to hit" vs "damage bonus" */
    if (!strcmp(inctyp, "damage")) {
        const char *ctmp = inctyp;

        inctyp = bonus;
        bonus = ctmp;
    }
    return msgprintf("%s %s %s", an(modif), bonus, inctyp);
}


void
enlightenment(int final)
    /* final: 0 => still in progress; 1 => over, survived; 2 => dead */
{
    int ltmp;
    const char *title;
    struct nh_menulist menu;

    init_menulist(&menu);
    title = final ? "Final Attributes:" : "Current Attributes:";

    if (flags.elbereth_enabled && u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
            "the Hand of Elbereth",
            "the Envoy of Balance",
            "the Glory of Arioch"
        };
        you_are(&menu, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
    }

    /* note: piousness 20 matches MIN_QUEST_ALIGN (quest.h) */
    if (u.ualign.record >= 20)
        you_are(&menu, "piously aligned");
    else if (u.ualign.record > 13)
        you_are(&menu, "devoutly aligned");
    else if (u.ualign.record > 8)
        you_are(&menu, "fervently aligned");
    else if (u.ualign.record > 3)
        you_are(&menu, "stridently aligned");
    else if (u.ualign.record == 3)
        you_are(&menu, "aligned");
    else if (u.ualign.record > 0)
        you_are(&menu, "haltingly aligned");
    else if (u.ualign.record == 0)
        you_are(&menu, "nominally aligned");
    else if (u.ualign.record >= -3)
        you_have(&menu, "strayed");
    else if (u.ualign.record >= -8)
        you_have(&menu, "sinned");
    else
        you_have(&menu, "transgressed");
    if (wizard) {
        const char *buf = msgprintf(" %d", u.uhunger);
        enl_msg(&menu, "Hunger level ", "is", "was", buf);

        buf = msgprintf(" %d / %ld", u.ualign.record, ALIGNLIM);
        enl_msg(&menu, "Your alignment ", "is", "was", buf);
    }

        /*** Resistances to troubles ***/
    if (Fire_resistance)
        you_are(&menu, "fire resistant");
    if (Cold_resistance)
        you_are(&menu, "cold resistant");
    if (Sleep_resistance)
        you_are(&menu, "sleep resistant");
    if (Disint_resistance)
        you_are(&menu, "disintegration-resistant");
    if (Shock_resistance)
        you_are(&menu, "shock resistant");
    if (Poison_resistance)
        you_are(&menu, "poison resistant");
    if (Drain_resistance)
        you_are(&menu, "level-drain resistant");
    if (Sick_resistance)
        you_are(&menu, "immune to sickness");
    if (Antimagic)
        you_are(&menu, "magic-protected");
    if (Acid_resistance)
        you_are(&menu, "acid resistant");
    if (Stone_resistance)
        you_are(&menu, "petrification resistant");
    if (u.uinvulnerable)
        you_are(&menu, "invulnerable");
    if (u.uedibility)
        you_can(&menu, "recognize detrimental food");

        /*** Troubles ***/
    if (Halluc_resistance)
        enl_msg(&menu, "You resist", "", "ed", " hallucinations");
    if (final) {
        if (Hallucination)
            you_are(&menu, "hallucinating");
        if (Stunned)
            you_are(&menu, "stunned");
        if (Confusion)
            you_are(&menu, "confused");
        if (Blinded)
            you_are(&menu, "blinded");
        if (Sick) {
            if (u.usick_type & SICK_VOMITABLE)
                you_are(&menu, "sick from food poisoning");
            if (u.usick_type & SICK_NONVOMITABLE)
                you_are(&menu, "sick from illness");
        }
    }
    if (Stoned)
        you_are(&menu, "turning to stone");
    if (Slimed)
        you_are(&menu, "turning into slime");
    if (Strangled)
        you_are(&menu, (u.uburied) ? "buried" : "being strangled");
    if (Glib)
        you_have(&menu, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (Fumbling)
        enl_msg(&menu, "You fumble", "", "d", "");
    if (Wounded_legs && !u.usteed)
        you_have(&menu, msgcat("wounded", makeplural(body_part(LEG))));;
    if (Wounded_legs && u.usteed && wizard) {
        const char *buf =
            x_monnam(u.usteed, ARTICLE_YOUR, NULL,
                     SUPPRESS_SADDLE | SUPPRESS_HALLUCINATION, FALSE);
        enl_msg(&menu, msgupcasefirst(buf), " has", " had", " wounded legs");
    }

    if (Sleeping)
        enl_msg(&menu, "You ", "fall", "fell", " asleep");
    if (Hunger)
        enl_msg(&menu, "You hunger", "", "ed", " rapidly");

        /*** Vision and senses ***/
    if (See_invisible)
        enl_msg(&menu, You_, "see", "saw", " invisible");
    if (Blind_telepat)
        you_are(&menu, "telepathic");
    if (Warning)
        you_are(&menu, "warned");
    if (Warn_of_mon) {
        int warntype = worn_warntype();
        const char *buf = msgcat(
            "aware of the presence of ",
            (warntype & M2_ORC) ? "orcs" :
            (warntype & M2_DEMON) ? "demons" : "something");
        you_are(&menu, buf);
    }
    if (Undead_warning)
        you_are(&menu, "warned of undead");
    if (Searching)
        you_have(&menu, "automatic searching");
    if (Clairvoyant)
        you_are(&menu, "clairvoyant");
    if (Infravision)
        you_have(&menu, "infravision");
    if (Detect_monsters)
        you_are(&menu, "sensing the presence of monsters");
    if (u.umconf)
        you_are(&menu, "going to confuse monsters");

        /*** Appearance and behavior ***/
    if (Adornment) {
        int adorn = 0;

        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        if (adorn < 0)
            you_are(&menu, "poorly adorned");
        else
            you_are(&menu, "adorned");
    }
    if (Invisible)
        you_are(&menu, "invisible");
    else if (Invis)
        you_are(&menu, "invisible to others");
    /* ordinarily "visible" is redundant; this is a special case for the
       situation when invisibility would be an expected attribute */
    else if (u_have_property(INVIS, ANY_PROPERTY, TRUE) && BInvis)
        you_are(&menu, "visible");
    if (Displaced)
        you_are(&menu, "displaced");
    if (Stealth)
        you_are(&menu, "stealthy");
    if (Aggravate_monster)
        enl_msg(&menu, "You aggravate", "", "d", " monsters");
    if (Conflict)
        enl_msg(&menu, "You cause", "", "d", " conflict");

        /*** Transportation ***/
    if (Jumping)
        you_can(&menu, "jump");
    if (Teleportation)
        you_can(&menu, "teleport");
    if (Teleport_control)
        you_have(&menu, "teleport control");
    if (Lev_at_will)
        you_are(&menu, "levitating, at will");
    else if (Levitation)
        you_are(&menu, "levitating");   /* without control */
    else if (Flying)
        you_can(&menu, "fly");
    if (Wwalking)
        you_can(&menu, "walk on water");
    if (Swimming)
        you_can(&menu, "swim");
    if (Breathless)
        you_can(&menu, "survive without air");
    else if (Amphibious)
        you_can(&menu, "breathe water");
    if (Passes_walls)
        you_can(&menu, "walk through walls");

    /* FIXME: This is printed even if you die in a riding accident. */
    if (u.usteed)
        you_are(&menu, msgcat("riding ", y_monnam(u.usteed)));
    if (Engulfed)
        you_are(&menu, msgcat("swallowed by ", a_monnam(u.ustuck)));
    else if (u.ustuck) {
        const char *buf = msgprintf(
            "%s %s", (Upolyd && sticks(youmonst.data)) ? "holding" : "held by",
            a_monnam(u.ustuck));
        you_are(&menu, buf);
    }

        /*** Physical attributes ***/
    if (u.uhitinc)
        you_have(&menu, enlght_combatinc("to hit", u.uhitinc, final));
    if (u.udaminc)
        you_have(&menu, enlght_combatinc("damage", u.udaminc, final));
    if (Slow_digestion)
        you_have(&menu, "slower digestion");
    if (Regeneration)
        enl_msg(&menu, "You regenerate", "", "d", "");
    if (u.uspellprot || Protection) {
        int prot = 0;

        if (uleft && uleft->otyp == RIN_PROTECTION)
            prot += uleft->spe;
        if (uright && uright->otyp == RIN_PROTECTION)
            prot += uright->spe;
        if (HProtection & INTRINSIC)
            prot += u.ublessed;
        prot += u.uspellprot;

        if (prot < 0)
            you_are(&menu, "ineffectively protected");
        else
            you_are(&menu, "protected");
    }
    if (Protection_from_shape_changers)
        you_are(&menu, "protected from shape changers");
    if (Polymorph)
        you_are(&menu, "polymorphing");
    if (Polymorph_control)
        you_have(&menu, "polymorph control");
    if (u.ulycn >= LOW_PM)
        you_are(&menu, an(mons[u.ulycn].mname));
    if (Upolyd) {
        const char *buf;
        if (u.umonnum == u.ulycn)
            buf = "in beast form";
        else
            buf = msgprintf("polymorphed into %s", an(youmonst.data->mname));
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.mtimedone);
        you_are(&menu, buf);
    }
    if (Unchanging)
        you_can(&menu, "not change from your current form");
    if (Fast)
        you_are(&menu, Very_fast ? "very fast" : "fast");
    if (Reflecting)
        you_have(&menu, "reflection");
    if (Free_action)
        you_have(&menu, "free action");
    if (Fixed_abil)
        you_have(&menu, "fixed abilities");
    if (Lifesaved)
        enl_msg(&menu, "Your life ", "will be", "would have been", " saved");
    if (u.twoweap)
        you_are(&menu, "wielding two weapons at once");

        /*** Miscellany ***/
    if (Luck) {
        ltmp = abs((int)Luck);
        const char *buf = msgprintf(
            "%s%slucky",
            ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
            Luck < 0 ? "un" : "");
        if (wizard)
            buf = msgprintf("%s (%d)", buf, Luck);
        you_are(&menu, buf);
    } else if (wizard)
        enl_msg(&menu, "Your luck ", "is", "was", " zero");
    if (u.moreluck > 0)
        you_have(&menu, "extra luck");
    else if (u.moreluck < 0)
        you_have(&menu, "reduced luck");
    if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
        ltmp = stone_luck(FALSE);
        if (ltmp <= 0)
            enl_msg(&menu, "Bad luck ", "does", "did", " not time out for you");
        if (ltmp >= 0)
            enl_msg(&menu, "Good luck ", "does", "did",
                    " not time out for you");
    }

    if (u.ugangr) {
        const char *buf = msgprintf(
            " %sangry with you",
            u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.ugangr);
        enl_msg(&menu, u_gname(), " is", " was", buf);
    } else
        /*
         * We need to suppress this when the game is over, because death
         * can change the value calculated by can_pray(), potentially
         * resulting in a false claim that you could have prayed safely.
         */
    if (!final) {
        const char *buf = msgprintf(
            "%ssafely pray", can_pray(FALSE) ? "" : "not ");
        /* can_pray sets some turnstate that needs to be reset. */
        turnstate.pray.align = A_NONE;
        turnstate.pray.type = pty_invalid;
        turnstate.pray.trouble = ptr_invalid;
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.ublesscnt);
        you_can(&menu, buf);
    }

    {
        const char *p, *buf = "";

        if (final < 2) {       /* still in progress, or quit/escaped/ascended */
            p = "survived after being killed ";
            switch (u.umortality) {
            case 0:
                p = !final ? NULL : "survived";
                break;
            case 1:
                buf = "once";
                break;
            case 2:
                buf = "twice";
                break;
            case 3:
                buf = "thrice";
                break;
            default:
                buf = msgprintf("%d times", u.umortality);
                break;
            }
        } else {        /* game ended in character's death */
            p = "are dead";
            switch (u.umortality) {
            case 0:
                impossible("dead without dying?");
            case 1:
                break;  /* just "are dead" */
            default:
                buf = msgprintf(" (%d%s time!)", u.umortality,
                                ordin(u.umortality));
                break;
            }
        }
        if (p)
            enl_msg(&menu, You_, "have been killed ", p, buf);
    }

    display_menu(&menu, title, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return;
}

void
unspoilered_intrinsics(void)
{
    int n;
    struct nh_menulist menu;

    init_menulist(&menu);

    /* Intrinsic list

       This lists only intrinsics that produce messages when gained and/or lost,
       to avoid giving away information that the player might not know. */
    n = menu.icount;

    /* Resistances */
    if (HFire_resistance)
        add_menutext(&menu, "You are fire resistant.");
    if (HCold_resistance)
        add_menutext(&menu, "You are cold resistant.");
    if (HSleep_resistance)
        add_menutext(&menu, "You are sleep resistant.");
    if (HDisint_resistance)
        add_menutext(&menu, "You are disintegration-resistant.");
    if (HShock_resistance)
        add_menutext(&menu, "You are shock resistant.");
    if (HPoison_resistance)
        add_menutext(&menu, "You are poison resistant.");
    if (HDrain_resistance)
        add_menutext(&menu, "You are level-drain resistant.");
    if (HSick_resistance)
        add_menutext(&menu, "You are immune to sickness.");
    /* Senses */
    if (HSee_invisible)
        add_menutext(&menu, "You see invisible.");
    if (HTelepat)
        add_menutext(&menu, "You are telepathic.");
    if (HWarning)
        add_menutext(&menu, "You are warned.");
    if (HSearching)
        add_menutext(&menu, "You have automatic searching.");
    if (HInfravision)
        add_menutext(&menu, "You have infravision.");
    /* Appearance, behaviour */
    if (HInvis && Invisible)
        add_menutext(&menu, "You are invisible.");
    if (HInvis && !Invisible)
        add_menutext(&menu, "You are invisible to others.");
    if (HStealth)
        add_menutext(&menu, "You are stealthy.");
    if (HAggravate_monster)
        add_menutext(&menu, "You aggravte monsters.");
    if (HConflict)
        add_menutext(&menu, "You cause conflict.");
    /* Movement */
    if (HJumping)
        add_menutext(&menu, "You can jump.");
    if (HTeleportation)
        add_menutext(&menu, "You can teleport.");
    if (HTeleport_control)
        add_menutext(&menu, "You have teleport control.");
    if (HSwimming)
        add_menutext(&menu, "You can swim.");
    if (HMagical_breathing)
        add_menutext(&menu, "You can survive without air.");
    if (HProtection)
        add_menutext(&menu, "You are protected.");
    if (HPolymorph)
        add_menutext(&menu, "You are polymorhing.");
    if (HPolymorph_control)
        add_menutext(&menu, "You have polymorph control.");
    if (HFast)
        add_menutext(&menu, "You are fast.");
    if (n == menu.icount)
        add_menutext(&menu, "You have no intrinsic abilities.");

    display_menu(&menu, "Your Intrinsic Statistics",
                 PICK_NONE, PLHINT_ANYWHERE, NULL);
}


void
show_conduct(int final)
{
    int ngenocided;
    struct nh_menulist menu;
    const char *buf;

    /* Create the conduct window */
    init_menulist(&menu);

    if (!u.uconduct[conduct_food])
        enl_msg(&menu, You_, "have gone", "went", " without food");
    /* But beverages are okay */
    else if (!u.uconduct[conduct_vegan])
        you_have_X(&menu, "followed a strict vegan diet");
    else if (!u.uconduct[conduct_vegetarian])
        you_have_been(&menu, "vegetarian");
    if (u.uconduct_time[conduct_food] > 1800) {
        buf = msgprintf("did not eat until turn %d",
                        u.uconduct_time[conduct_food]);
        enl_msg(&menu, You_, "", "had ", buf);
    }
    if (u.uconduct_time[conduct_vegan] > 1800) {
        buf = msgprintf("followed a strict vegan diet until turn %d",
                        u.uconduct_time[conduct_vegan]);
        enl_msg(&menu, You_, "", "had ", buf);
    }
    if (u.uconduct_time[conduct_vegetarian] > 1800) {
        buf = msgprintf("followed a strict vegetarian diet until turn %d",
                        u.uconduct_time[conduct_vegetarian]);
        enl_msg(&menu, You_, "", "had ", buf);
    }

    if (!u.uconduct[conduct_gnostic])
        you_have_been(&menu, "an atheist");
    if (u.uconduct_time[conduct_gnostic] > 1800) {
        buf = msgprintf("an atheist until turn %d",
                        u.uconduct_time[conduct_gnostic]);
        enl_msg(&menu, You_, "were ", "had been ", buf);
    }

    if (!u.uconduct[conduct_weaphit])
        you_have_never(&menu, "hit with a wielded weapon");
    else {
        buf = msgprintf("used a wielded weapon %d time%s, starting on turn %d",
                        u.uconduct[conduct_weaphit],
                        plur(u.uconduct[conduct_weaphit]),
                        u.uconduct_time[conduct_weaphit]);
        you_have_X(&menu, buf);
    }
    if (!u.uconduct[conduct_killer])
        you_have_been(&menu, "a pacifist");
    if (u.uconduct_time[conduct_killer] > 1800) {
        buf = msgprintf("a pacifist until turn %d",
                        u.uconduct_time[conduct_killer]);
        enl_msg(&menu, You_, "were ", "had been ", buf);
    }

    if (!u.uconduct[conduct_illiterate])
        you_have_been(&menu, "illiterate");
    else {
        buf = msgprintf("read items or engraved %d time%s, starting on turn %d",
                        u.uconduct[conduct_illiterate],
                        plur(u.uconduct[conduct_illiterate]),
                        u.uconduct_time[conduct_illiterate]);
        you_have_X(&menu, buf);
    }

    ngenocided = num_genocides();
    if (ngenocided == 0) {
        you_have_never(&menu, "genocided any monsters");
    } else {
        buf = msgprintf("genocided %d type%s of monster%s, starting on turn %d",
                        ngenocided, plur(ngenocided), plur(ngenocided),
                        u.uconduct_time[conduct_genocide]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_polypile])
        you_have_never(&menu, "polymorphed an object");
    else {
        buf = msgprintf("polymorphed %d item%s, starting on turn %d",
                        u.uconduct[conduct_polypile],
                        plur(u.uconduct[conduct_polypile]),
                        u.uconduct_time[conduct_polypile]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_polyself])
        you_have_never(&menu, "changed form");
    else {
        buf = msgprintf("changed form %d time%s, starting on turn %d",
                        u.uconduct[conduct_polyself],
                        plur(u.uconduct[conduct_polyself]),
                        u.uconduct_time[conduct_polyself]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_wish])
        you_have_X(&menu, "used no wishes");
    else {
        buf = msgprintf("used %u wish%s, starting on turn %d",
                        u.uconduct[conduct_wish],
                        (u.uconduct[conduct_wish] > 1) ? "es" : "",
                        u.uconduct_time[conduct_wish]);
        you_have_X(&menu, buf);

        if (!u.uconduct[conduct_artiwish])
            enl_msg(&menu, You_, "have not wished", "did not wish",
                    " for any artifacts");
        else {
            buf = msgprintf("wished for your your first artifact on turn %d",
                            u.uconduct_time[conduct_artiwish]);
            you_have_X(&menu, buf);
        }
    }

    if (!u.uconduct[conduct_puddingsplit])
        you_have_never(&menu, "split a pudding");
    else {
        buf = msgprintf("split %u pudding%s, starting on turn %d",
                        u.uconduct[conduct_puddingsplit],
                        plur(u.uconduct[conduct_puddingsplit]),
                        u.uconduct_time[conduct_puddingsplit]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_elbereth])
        enl_msg(&menu, You_, "have never written", "never wrote",
                " Elbereth's name");
    else {
        buf = msgprintf(" Elbereth's name %u time%s, starting on turn %d",
                        u.uconduct[conduct_elbereth],
                        plur(u.uconduct[conduct_elbereth]),
                        u.uconduct_time[conduct_elbereth]);
        enl_msg(&menu, You_, "have written", "wrote", buf);
    }

    if (!u.uconduct[conduct_lostalign])
        enl_msg(&menu, You_, "have never violated", "never violated",
                " your personal moral code");
    else {
        sprintf(buf, " your moral code, losing %u point%s of alignment, "
                "starting on turn %d",
                u.uconduct[conduct_lostalign],
                plur(u.uconduct[conduct_lostalign]),
                u.uconduct_time[conduct_lostalign]);
        enl_msg(&menu, You_, "have violated", "violated", buf);
    }

    /* birth options */
    if (!flags.bones_enabled)
        you_have_X(&menu, "disabled loading bones files");
    if (!flags.elbereth_enabled)        /* not the same as not /writing/ E */
        you_have_X(&menu, "abstained from Elbereth's help");
    if (flags.permahallu)
        enl_msg(&menu, You_, "are ", "were", "permanently hallucinating");
    if (flags.permablind)
        enl_msg(&menu, You_, "are ", "were ", "permanently blind");

    /* Pop up the window and wait for a key */
    display_menu(&menu, "Voluntary challenges:", PICK_NONE,
                 PLHINT_ANYWHERE, NULL);
}
