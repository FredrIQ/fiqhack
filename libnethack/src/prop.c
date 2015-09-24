/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-09-25 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2014 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"

/* This file is responsible for determining whether the character has intrinsics
   and extrinsics, because it was previously done with a bunch of macros, which
   are rather hard to iterate over, and make it even harder to work with the
   game's logic. */

/* Returns an object slot mask giving all the reasons why the given
   player/monster might have the given property, limited by "reasons", an object
   slot mask (W_EQUIP, INTRINSIC, and ANY_PROPERTY are the most likely values
   here, but you can specify slots individually if you like). */
   
/* The "os_polyform" checks used to not be accurate at all. They are now,
   hopefully */

int
mon_prop2mt(enum youprop prop)
{
    switch (prop) {
    case SEE_INVIS:
        return mt_seeinvis;
    case INVIS:
        return mt_invis;
    case LEVITATION:
        return mt_levi;
    case FAST:
        return mt_fast;
    case STUNNED:
        return mt_stun;
    case CONFUSION:
        return mt_conf;
    case BLINDED:
        return mt_blind;
    case DETECT_MONSTERS:
        return mt_detectmon;
    case SLOW:
        return mt_slow;
    case PROTECTION:
        return mt_protection;
    case PASSES_WALLS:
        return mt_phasing;
    default:
        return -1;
    }
}

int
mon_mt2prop(enum mt_prop mt)
{
    switch (mt) {
    case mt_seeinvis:
        return SEE_INVIS;
    case mt_invis:
        return INVIS;
    case mt_levi:
        return LEVITATION;
    case mt_fast:
        return FAST;
    case mt_stun:
        return STUNNED;
    case mt_conf:
        return CONFUSION;
    case mt_blind:
        return BLINDED;
    case mt_detectmon:
        return DETECT_MONSTERS;
    case mt_slow:
        return SLOW;
    case mt_protection:
        return PROTECTION;
    case mt_phasing:
        return PASSES_WALLS;
    default:
        return -1;
    }
}        

unsigned
m_has_property(const struct monst *mon, enum youprop property,
               unsigned reasons, boolean even_if_blocked)
{
    unsigned rv = 0;
    struct obj *otmp;
    const struct permonst *mdat = mon->data;
    uchar mfromrace = mdat->mresists;
    uint64_t mfromoutside = mon->mintrinsics;

    /* The general case for equipment. Abort during binary load,
       since monster inventory chains might be missing*/ 
    if (!program_state.restoring_binary_save && m_minvent(mon))
        rv |= mworn_extrinsic(mon, property);

    if (mon == &youmonst) {
        /* Intrinsics */
        if (u.uintrinsic[property] & TIMEOUT)
            rv |= W_MASK(os_timeout);
        rv |= u.uintrinsic[property] & (INTRINSIC | I_SPECIAL);

        /* Birth options */
        if (property == BLINDED && flags.permablind)
            rv |= W_MASK(os_birthopt);
        if (property == HALLUC && flags.permahallu)
            rv |= W_MASK(os_birthopt);
        if (property == UNCHANGING && flags.polyinit_mnum != -1)
            rv |= W_MASK(os_birthopt);

    } else {
        enum mt_prop mt = mon_prop2mt(property);
        if (mt != mt_invalid && mon->mt_prop[mt])
            rv |= W_MASK(os_timeout);
        /* it is not possible to contain all possible properties in a 64bit
           but there is barely any missing ones... if those are later needed,
           the redundant ones can be special cased */
        if (property < 65 && (mfromoutside & (uint64_t)(1 << (property - 1))))
            rv |= W_MASK(os_outside);
    }


    /* Polyform / monster intrinsic */
    /* TODO: Change the monster data code into something that doesn't require a
       giant switch statement or ternary chain to get useful information from
       it. We use a ternary chain here because it cuts down on repetitive code
       and so is easier to read. */
    if (property == FIRE_RES     ? mfromrace & MR_FIRE                       :
        property == COLD_RES     ? mfromrace & MR_COLD                       :
        property == SLEEP_RES    ? mfromrace & MR_SLEEP                      :
        property == DISINT_RES   ? mfromrace & MR_DISINT                     :
        property == SHOCK_RES    ? mfromrace & MR_ELEC                       :
        property == POISON_RES   ? mfromrace & MR_POISON                     :
        property == DRAIN_RES    ? is_undead(mdat) || is_demon(mdat) ||
                                   is_were(mdat) || mdat == &mons[PM_DEATH] :
        property == SICK_RES     ? mdat->mlet == S_FUNGUS ||
                                   mdat == &mons[PM_GHOUL]                   :
        property == ANTIMAGIC    ? dmgtype(mdat, AD_MAGM) ||
                                   dmgtype(mdat, AD_RBRE) ||
                                   mdat == &mons[PM_BABY_GRAY_DRAGON]        :
        property == ACID_RES     ? mfromrace & MR_ACID                       :
        property == STONE_RES    ? mfromrace & MR_STONE                      :
        property == STUNNED      ? mdat->mflags2 & M2_STUNNED                :
        /* Note: haseyes() overrides blindness, making this check have no
           effect. See overrides below */
        property == BLINDED      ? !haseyes(mon->data)                       :
        property == HALLUC       ? Upolyd && dmgtype(mon->data, AD_HALU)     :
        property == SEE_INVIS    ? mdat->mflags1 & M1_SEE_INVIS              :
        property == TELEPAT      ? mdat->mflags2 & M2_TELEPATHIC             :
        property == INFRAVISION  ? pm_infravision(mdat)                      :
        /* Note: This one assumes that there's no way to permanently turn
           visible when you're in stalker form (i.e. mummy wrappings only). */
        property == INVIS        ? pm_invisible(mdat)                        :
        property == TELEPORT     ? mdat->mflags1 & M1_TPORT                  :
        property == LEVITATION   ? is_floater(mdat)                          :
        property == FLYING       ? mdat->mflags1 & M1_FLY                    :
        property == SWIMMING     ? mdat->mflags1 & M1_SWIM                   :
        property == PASSES_WALLS ? mdat->mflags1 & M1_WALLWALK               :
        property == REGENERATION ? mdat->mflags1 & M1_REGEN                  :
        property == REFLECTING   ? mon->data == &mons[PM_SILVER_DRAGON]      :
        property == TELEPORT_CONTROL  ? mdat->mflags1 & M1_TPORT_CNTRL     :
        property == MAGICAL_BREATHING ? amphibious(mon->data)                :
        0)
        rv |= W_MASK(os_polyform);

    if (mon == &youmonst) {
        /* External circumstances */
        if (property == BLINDED && u_helpless(hm_unconscious))
            rv |= W_MASK(os_circumstance);

        /* Riding */
        if (property == FLYING && u.usteed && is_flyer(u.usteed->data))
            rv |= W_MASK(os_saddle);
        if (property == SWIMMING && u.usteed && pm_swims(u.usteed->data))
            rv |= W_MASK(os_saddle);
    }

    /* Overrides

    TODO: Monsters with no eyes are not considered blind. This doesn't
    make much sense. However, changing it would be a major balance
    change (due to Elbereth), and so it has been left alone for now. */
    if (!even_if_blocked) {
        if (property == BLINDED) {
            for (otmp = m_minvent(mon); otmp; otmp = otmp->nobj)
                if (otmp->oartifact == ART_EYES_OF_THE_OVERWORLD &&
                    otmp->owornmask & W_MASK(os_tool))
                    rv &= (unsigned)(W_MASK(os_circumstance) |
                                     W_MASK(os_birthopt));
            if (!haseyes(mdat))
                rv &= (unsigned)(W_MASK(os_birthopt));
        }

        if (property == WWALKING && Is_waterlevel(m_mz(mon)))
            rv &= (unsigned)(W_MASK(os_birthopt));
        if (mworn_blocked(mon, property))
            rv &= (unsigned)(W_MASK(os_birthopt));
    }

    return rv & reasons;
}

int
property_timeout(struct monst *mon, enum youprop property)
{
    if (!(has_property(mon, property) & W_MASK(os_timeout)))
        return 0;
    if (mon == &youmonst)
        return (u.uintrinsic[property] & TIMEOUT);
    else
        return mon->mt_prop[mon_prop2mt(property)];
}
/* Can this monster teleport at will?
   Any monster who has reached XL12 or more can teleport at will if they have teleportitis.
   If the monster has teleportitis in their natural form, they can always teleport at will.
   If the monster is a wizard, they can teleport at will from XL8 with teleportitis. */
boolean
teleport_at_will(const struct monst *mon)
{
    if (!teleportitis(mon))
        return FALSE;
    if (teleportitis(mon) & W_MASK(os_polyform))
        return TRUE;
    int level;
    if (mon == &youmonst)
        level = u.ulevel;
    else
        level = mon->m_lev;
    if (level >= 12)
        return TRUE;
    if (level >= 8) {
        if (mon == &youmonst && Race_if(PM_WIZARD))
            return TRUE;
        if (mon != &youmonst && attacktype(mon->data, AT_MAGC))
            return TRUE;
        return FALSE;
    }
    return FALSE;
}
    
/* Checks whether or not a monster has controlled levitation.
   "Controlled" levitation here means that the monster can
   end it on its' own accord. include_extrinsic also includes
   extrinsics. "why" makes this function return the reason
   for the uncontrolled levitation or 0 if it is, in fact,
   controlled (or non-existent). */
unsigned
levitates_at_will(const struct monst *mon, boolean include_extrinsic,
    boolean why)
{
    unsigned lev = levitates(mon);
    unsigned lev_worn = mworn_extrinsic(mon, LEVITATION);

    /* polyform */
    if (is_floater(mon->data))
        return (why ? W_MASK(os_polyform) : 0);

    /* uncontrolled intrinsic levitation */
    if ((lev & lev_worn) && !(lev & FROMOUTSIDE))
        return (why ? (lev & lev_worn) : 0);

    /* has extrinsic */
    if (!(lev & lev_worn) && !include_extrinsic)
        return (why ? lev_worn : 0);

    if (lev_worn) { /* armor/ring/slotless levitation active */
        struct obj *chain = m_minvent(mon);
        int warntype;
        long itemtype;
        
        while (chain) {
            /* worn item or slotless unremoveable item */
            itemtype = item_provides_extrinsic(chain, LEVITATION, &warntype);
            if (itemtype && chain->cursed && (chain->owornmask ||
                (itemtype == W_MASK(os_carried) && chain->otyp == LOADSTONE)))
                return (why ? itemtype : 0);
            chain = chain->nobj;
        }
    }
    
    return lev;
}

/* Used when monsters need to abort levitation for some reason.
   (0=no turn spent, 1=turn spent, 2=died) */
unsigned
mon_remove_levitation(struct monst *mon, boolean forced)
{
    unsigned lev_source = levitates_at_will(mon, TRUE, FALSE);
    if (!lev_source) {
        lev_source = levitates(mon);
        if (!forced)
            return 0;
    }

    /* equavilent to cancelling levi with > as player */
    if (lev_source & FROMOUTSIDE) {
        set_property(mon, LEVITATION, -2, forced);
        lev_source = levitates(mon);
        if (!forced)
            return (mon->mhp <= 0 ? 2 : 1);
    }

    /* monster levitation comes from an extrinsic */
    struct obj *chain = m_minvent(mon);
    int warntype;
    long itemtype;
    int slot;
    boolean dropped; /* Monsters can drop several items in a single turn,
                        but if it drops any items, it can't do stuff
                        beyond that */
    while (chain) {
        itemtype = item_provides_extrinsic(chain, LEVITATION, &warntype);
        if (itemtype) {
            if (chain->owornmask && (!dropped || forced)) {
                slot = chain->owornmask;
                if (forced) {
                    chain->owornmask = 0;
                    mon->misc_worn_check &= ~W_MASK(slot);
                } else
                    return equip(mon, chain, FALSE, TRUE);
            } if (itemtype == W_MASK(os_carried)) {
                if (forced)
                    mdrop_obj(mon, chain, FALSE);
                else if (chain->otyp == LOADSTONE && chain->cursed)
                    return 0;
                else {
                    mdrop_obj(mon, chain, TRUE);
                    dropped = TRUE;
                }
            }
        }
        chain = chain->nobj;
    }
    
    if (!forced || levitates(mon)) {
        /* at this point, only polyform levitation is left */
        if (forced) {
            if (cansee(mon->mx, mon->my))
                pline("%s wobbles unsteadily for a moment.", Monnam(mon));
        }
        return dropped ? 1 : 0;
    }
    
    if (cansee(mon->mx, mon->my))
        pline("%s crashes to the floor!", Monnam(mon));
    
    mon->mhp -= rn1(8, 14); /* same as for player with 11 Con */
    if (mon->mhp < 0) {
        if (cansee(mon->mx, mon->my))
            pline("%s dies!", Monnam(mon));
        else if (mon->mtame)
            pline("You have a sad feeling for a moment, then it passes.");
        mondied(mon);
    }
    return 0;
}

/* Gremlin attack. Removes a random intrinsic. */
void
gremlin_curse(struct monst *mon)
{
    int i;
    enum youprop prop;
    for (i = 0; i < 200; i++) {
        prop = rnd(64); /* Assumes only prop 1-64 can be os_outside (true for monsters) */
        if (m_has_property(mon, prop, W_MASK(os_outside), TRUE)) {
            set_property(mon, prop, -1, FALSE);
            return;
        }
    }
    if (mon == &youmonst || canseemon(mon))
        pline("But nothing happens.");
    return;
}

/* Sets a property.
   type>0: Set (or increase) a timeout
   type=0: Set os_outside
   type-1: Remove os_outside
   type-2: Remove os_outside and the timer
   forced will bypass update_property(). It is used
   when a special case is needed, and code will have
   to handle the work related to the property itself */
boolean
set_property(struct monst *mon, enum youprop prop,
             int type, boolean forced)
{
    boolean increased = FALSE;

    /* check for redundant usage */
    if (type == 0 && m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -1 && !m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -2 &&
        !m_has_property(mon, prop, (W_MASK(os_outside) | W_MASK(os_timeout)), TRUE))
        return FALSE;

    if (type > 0) {
        if (mon != &youmonst) {
            enum mt_prop mt = mon_prop2mt(prop);
            if (mt == mt_invalid) {
                impossible("Trying to set invalid timed property!");
                return FALSE;
            }
            if (mon->mt_prop[mt] > 0)
                increased = TRUE;
            mon->mt_prop[mt] = min(255, mon->mt_prop[mt] + type);
            /* reset "levitation wary" flag for monsters */
            if (prop == LEVITATION)
                mon->levi_wary = 0;
        } else {
            if ((u.uintrinsic[prop] & TIMEOUT) > 0)
                increased = TRUE;
            long val = (long)type;
            long old = (u.uintrinsic[prop] & TIMEOUT);
            val += old;
            if (val > TIMEOUT)
                val = TIMEOUT;
            u.uintrinsic[prop] &= TIMEOUT;
            u.uintrinsic[prop] += val;
        }
    } else if (type == 0) {
        if (mon != &youmonst && prop < 65)
            mon->mintrinsics |= (uint64_t)(1 << (prop - 1));
        else
            u.uintrinsic[prop] |= FROMOUTSIDE;
    } else {
        if (mon != &youmonst && prop < 65)
            mon->mintrinsics &= (uint64_t)~(1 << (prop - 1));
        else if (mon == &youmonst)
            u.uintrinsic[prop] &= FROMOUTSIDE;
        if (type == -2) {
            if (mon != &youmonst) {
                enum mt_prop mt = mon_prop2mt(prop);
                if (mt == mt_invalid) {
                    impossible("Trying to set invalid timed property!");
                    return FALSE;
                }
                mon->mt_prop[mt] = 0;
            } else
                u.uintrinsic[prop] &= TIMEOUT;
        }
    }
    if (!forced) {
        if (type > 0 || type == -2) {
            if (increased)
                return update_property(mon, prop, os_newtimeout);
            else
                return update_property(mon, prop, os_timeout);
        } else
            return update_property(mon, prop, os_outside);
    }
    return FALSE;
}

/* Called to give any eventual messages and perform checks in case
   e.g. mon lost levitation (drowning), stone res (wielding trice).
   Currently only in use for monsters.
   TODO: some of the status problem message logic is a mess, fix it */
boolean
update_property(struct monst *mon, enum youprop prop,
                enum objslot slot)
{
    /* Items call update_property() when lost, whether or not it had a property */
    if (prop == NO_PROP)
        return FALSE;
    boolean vis = canseemon(mon);
    /* Used when the updating is related to monster invisibility
       since canseemon() wont work if the monster just turned
       itself invisible */
    boolean vis_invis = cansee(mon->mx, mon->my);
    boolean extrinsic = (slot <= os_last_slot);
    boolean lost = !(has_property(mon, prop) & W_MASK(slot));
    if (slot == os_newtimeout)
        lost = FALSE;
    /* Whether or not a monster has it elsewhere */
    boolean redundant = !!(has_property(mon, prop) & ~W_MASK(slot));
    /* Hallu checks *your* hallucination since it's used for special
       messages */
    boolean hallu = hallucinating(&youmonst);
    boolean you = (mon == &youmonst);
    /* if something was said about the situation */
    boolean effect = FALSE;
    struct obj *weapon;

    switch (prop) {
    case FIRE_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel warmer." :
                  hallu ? "You be chillin'." :
                  "You feel a momentary chill.");
            effect = TRUE;
        }
        /* BUG: shouldn't there be a check for lava here?
        if (lost && !redundant) {
        } */
        break;
    case COLD_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel cooler." :
                  "You feel full of hot air.");
            effect = TRUE;
        }
        break;
    case SLEEP_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel tired!" :
                  "You feel wide awake.");
            effect = TRUE;
        }
        break;
    case DISINT_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel less firm." :
                  hallu ? "You feel totally together, man." :
                  "You feel very firm.");
            effect = TRUE;
        }
        break;
    case SHOCK_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel conductive." :
                  hallu ? "You feel grounded in reality." :
                  "Your health currently feels amplified!");
            effect = TRUE;
        }
        break;
    case POISON_RES:
        if (!extrinsic && you) {
            pline(lost ? "You feel a little sick!" :
                  mworn_extrinsic(mon, prop) ?
                  "You feel especially healthy." :
                  "You feel healthy.");
            effect = TRUE;
        }
        break;
    case ACID_RES:
        break;
    case STONE_RES:
        weapon = m_mwep(mon);
        if (lost && !redundant && weapon &&
            weapon->otyp == CORPSE &&
            touch_petrifies(&mons[weapon->corpsenm])) {
            if (!you)
                mselftouch(mon, "No longer petrify-resistant, ",
                           !flags.mon_moving);
            else {
                const char *kbuf;
                kbuf = msgprintf("losing stone resistance while wielding %s",
                                 urace.adj);
                selftouch("No longer petrify-resistant, you", kbuf);
            }
            if (!resists_ston(mon)) { /* lifesaved */
                if (!you) {
                    setmnotwielded(mon, mon->mw);
                    MON_NOWEP(mon);
                } else
                    uwepgone();
            }
            if (you || vis)
                effect = TRUE;
        }
        break;
    case ADORNED:
    case REGENERATION:
    case SEARCHING:
        break;
    case SEE_INVIS:
        if (you) {
            if (!extrinsic && lost) {
                pline(hallu ? "You tawt you taw a puttie tat!" :
                      "You thought you saw something!");
                effect = TRUE;
            }
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters(FALSE);        /* see invisible monsters */
            newsym(u.ux, u.uy);         /* see yourself! */
            if (!redundant && invisible(mon)) {
                pline(lost ? "Your body seems to fade out." :
                      "You can see yourself, but remain transparent");
                effect = TRUE;
            }
        }
        break;
    case INVIS:
        if (you) {
            if (slot == os_outside) {
                pline(lost ? "You feel paranoid." :
                      "You feel hidden!");
                effect = TRUE;
            } else if (!redundant) {
                if (lost)
                    pline("Your body seems to unfade...");
                else
                    self_invis_message();
                effect = TRUE;
            }
            newsym(u.ux, u.uy);
        } else if (!redundant && vis_invis) {
            if (see_invisible(&youmonst)) {
                pline(lost ? "%s body seems to unfade..." :
                      "%s body turns transparent!",
                      s_suffix(Monnam(mon)));
            } else {
                pline(lost ? "%s appears!" :
                      "%s suddenly disappears!",
                      Monnam(mon));
                set_mimic_blocking();       /* do special mimic handling */
                see_monsters(FALSE);        /* see invisible monsters */
            }
            effect = TRUE;
        }
        break;
    case TELEPORT:
        if (you)
            if (!extrinsic) {
                pline(lost ? "You feel less jumpy." :
                      hallu ? "You feel diffuse." :
                      "You feel very jumpy.");
                effect = TRUE;
            }
            update_supernatural_abilities();
        break;
    case TELEPORT_CONTROL:
        if (!extrinsic && you) {
            pline(hallu ? "You feel centered in your personal space." :
                  "You feel in control of yourself.");
            effect = TRUE;
        }
        break;
    case POLYMORPH:
    case POLYMORPH_CONTROL:
        break;
    case LEVITATION:
        if (!redundant) {
            if (!lost && slot != os_newtimeout)
                float_up(mon);
            else
                float_down(mon);
            if (vis || you)
                effect = TRUE;
        }
        break;
    case STEALTH:
        if (you && !extrinsic && lost) {
            pline("You feel clumsy.");
            effect = TRUE;
        }
        if (slot == os_armf && !redundant &&
            !levitates(mon) && !flying(mon)) {
            if (you)
                pline(lost ? "You sure are noisy." :
                      "You walk very quietly");
            else if (vis)
                pline(lost ? "%s sure is noisy." :
                      "%s walks very quietly.",
                      Monnam(mon));
            effect = TRUE;
        }
        break;
    case AGGRAVATE_MONSTER:
        if (you && !extrinsic && lost) {
            pline("You feel less attractive.");
            effect = TRUE;
        }
        if (!you && !redundant) {
            you_aggravate(mon);
            see_monsters(FALSE);
        }
        break;
    case CONFLICT:
        /* Monsters should not be causing conflict. Just
           in case it happens anyway, alert the player. */
        if (!you) {
            pline(lost ? "You feel as if a conflict disappeared." :
                  "You feel as if someone is causing conflict.");
            effect = TRUE;
        }
        break;
    case PROTECTION:
        if (you && slot == os_armc && !lost)
            /* kludge to auto-ID CoP */
            effect = TRUE;
        break;
    case PROT_FROM_SHAPE_CHANGERS:
        if (!redundant && lost)
            restartcham();
        else
            resistcham();
        break;
    case WARNING:
        if (you)
            see_monsters(FALSE);
        break;
    case TELEPAT:
        if (!extrinsic && you) {
            pline(lost ? "Your senses fail!" :
                  hallu ? "You feel in touch with the cosmos." :
                  "You feel a strange mental acuity.");
            effect = TRUE;
        }
        /* no harm in calling this unconditionally, and it is needed
           in a lot of cases */
        see_monsters(FALSE);
        break;
    case FAST:
        if (slot == os_outside) {
            if (you) {
                if (!redundant)
                    pline(lost ? "You slow down." :
                          "You speed up");
                else
                    pline(lost ? "Your quickness feels less natural." :
                          "Your quickness feels more natural.");
                effect = TRUE;
            } else if (!redundant && vis) {
                pline(lost ? "%s slows down." :
                      "%s speeds up",
                      Monnam(mon));
                effect = TRUE;
            }
        } else {
            /* Special case: we need to check if this speed increase is
               redundant, but "redundant" wont be enough, because if
               the only redundancy is from os_outside, there is still a
               partial speed up going on. */
            if (slot != os_newtimeout && !(has_property(mon, FAST) &
                ~(W_MASK(os_outside) | W_MASK(slot)))) {
                /* if "redundant" is set at this point, it is pointing
                   at intrinsic speed only */
                if (you) {
                    pline("You are suddenly moving %s%s.",
                          (redundant ? "" : "much "),
                          (lost ? "slower" : "faster"));
                    effect = TRUE;
                } else if (vis) {
                    pline("%s seems to be moving %s%s.",
                          Monnam(mon),
                          (redundant ? "" : "much "),
                          (lost ? "slower" : "faster"));
                    effect = TRUE;
                }
            } else if (slot == os_newtimeout && you) {
                pline("Your %s get new energy.",
                      makeplural(body_part(LEG)));
                effect = TRUE;
            }
        }
        break;
    case STUNNED:
        if (you) {
            if (redundant)
                pline(hallu ? "You feel like wobbling some more." :
                      "You struggle to keep your balance.");
            else if (!lost) {
                pline("You %s...",
                      hallu ? "wobble" : stagger(mon->data, "stagger"));
            } else
                pline("You feel %s now.",
                      hallu ? "less wobbly" :
                      "a bit steadier");
            effect = TRUE;
        } else if (vis) {
            if (redundant)
                pline("%s struggles to keep %s balance.",
                      Monnam(mon), mhis(mon));
            else if (!lost) {
                pline("%s %ss...",
                      Monnam(mon), stagger(mon->data, "stagger"));
            } else
                pline("%s looks a bit steadier now.",
                      Monnam(mon));
            effect = TRUE;
        }
        break;
    case CONFUSION:
        if (you) {
            if (!lost)
                pline(hallu ? "What a trippy feeling!" :
                      redundant ? "You are even more confused..." :
                      "Huh, What?  Where am I?");
            else
                pline(hallu ? "You feel less trippy." :
                      "You are no longer confused.");
            effect = TRUE;
        } else if (vis) {
            pline(lost ? "%s looks less confused now." :
                  "%s looks rather confused...", Monnam(mon));
            effect = TRUE;
        }
    case SICK:
        if (you) {
            pline(redundant ? "You feel even worse." :
                  lost ? "What a relief!" :
                  "You feel deathly sick.");
            effect = TRUE;
        } else if (vis) {
            pline(redundant ? "%s looks even worse." :
                  lost ? "%s looks relieved." :
                  "%s looks deathly sick.",
                  Monnam(mon));
            effect = TRUE;
        }
        break;
    case BLINDED:
        if (you && slot == os_tool) {
            if (lost &&
                m_has_property(mon, BLINDED, ANY_PROPERTY, TRUE)) {
                pline("You can see!");
                effect = TRUE;
            } else if (!lost && !redundant) {
                pline("You can't see any more.");
                effect = TRUE;
            } else if (lost) {
                pline(redundant ? "You still can't see..." :
                      "You can see again.");
                effect = TRUE;
            }
            turnstate.vision_full_recalc = TRUE;
            see_monsters(FALSE);
        } else if (you) {
            boolean eoto;
            if (!redundant &&
                m_has_property(mon, BLINDED, ANY_PROPERTY, TRUE))
                eoto = TRUE;
            if (redundant && !eoto) {
                if (!lost)
                    eyepline("itches", "itch");
                else
                    eyepline("twitches", "twitch");
            } else if (eoto)
                pline("Your vision seems to %s for a moment but is %s now",
                      lost ? "dim" : "brighten",
                      hallu ?
                      (lost ? "happier" : "sadder") :
                      "normal");
            else {
                if (lost)
                    pline(hallu ?
                          "Oh, bummer!  Everything is dark!  Help!" :
                          "A cloud of darkness falls upon you.");
                else
                    pline(hallu ? "Far out!  A light show!" :
                          "You can see again.");
            }
            effect = TRUE;
            turnstate.vision_full_recalc = TRUE;
            see_monsters(FALSE);
        }
        break;
    case SLEEPING: /* actually restful sleep */
    case LWOUNDED_LEGS:
    case RWOUNDED_LEGS:
    case STONED:
    case STRANGLED:
        break;
    case HALLUC:
        if (you) {
            boolean grays;
            if (!redundant &&
                m_has_property(mon, HALLUC, ANY_PROPERTY, TRUE))
                grays = TRUE;
            if (lost && grays)
                pline("Your vision seems to %s for a moment but is %s now",
                      "flatten", "normal");
            else if (lost && blind(mon))
                eyepline("itches", "itch");
            else if (lost)
                pline("Everything looks SO boring now.");
            else /* BUG[?]: no grayswandir msg for getting hallu? */
                pline("Oh wow!  Everything %s so cosmic!",
                      blind(mon) ? "feels" : "looks");
            effect = TRUE;
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        } else
            impossible("Monster got hallucination?");
        break;
    case HALLUC_RES:
        if (you) {
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        }
        break;
    case FUMBLING:
        break;
    case JUMPING:
        if (you && slot == os_armf) {
            pline("Your %s feel %s.", makeplural(body_part(LEG)),
                  lost ? "shorter" : "longer");
            effect = TRUE;
        }
        break;
    case WWALKING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case HUNGER:
        break;
    case GLIB:
        if (you && lost && !redundant) {
            pline("Your %s feels less slippery",
                  makeplural(body_part(FINGER)));
            effect = TRUE;
        }
        break;
    case REFLECTING:
    case LIFESAVED:
    case ANTIMAGIC:
        break;
    case DISPLACED:
        if (you && !redundant) {
            pline(lost ? "You stop shimmering" :
                  "Your outline shimmers and shifts");
            effect = TRUE;
        }
        break;
    case CLAIRVOYANT:
    case VOMITING:
    case ENERGY_REGENERATION:
    case MAGICAL_BREATHING:
    case HALF_SPDAM:
    case HALF_PHDAM:
    case SICK_RES:
    case DRAIN_RES:
        break;
    case WARN_UNDEAD:
        if (you)
            see_monsters(FALSE);
        break;
    case CANCELLED:
        if (you && !redundant) {
            pline(lost ? "Your magic returns." :
                  "You feel devoid of magic!");
            effect = TRUE;
        }
        break;
    case FREE_ACTION:
        break;
    case SWIMMING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case SLIMED:
    case FIXED_ABIL:
        break;
    case FLYING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case UNCHANGING:
        set_property(mon, SLIMED, -2, TRUE);
        break;
    case PASSES_WALLS:
    case SLOW_DIGESTION:
    case INFRAVISION:
        break;
    case WARN_OF_MON:
        if (you)
            see_monsters(FALSE);
        break;
    case XRAY_VISION:
        if (you) {
            turnstate.vision_full_recalc = TRUE;
            see_monsters(FALSE);
        }
        break;
    case DETECT_MONSTERS:
        if (you && !redundant) {
            see_monsters(FALSE);
            /* did we just get the property */
            if (!lost) {
                int x, y;
                int found_monsters = 0;
                for (x = 0; x < COLNO; x++) {
                    for (y = 0; y < ROWNO; y++) {
                        if (level->locations[x][y].mem_invis) {
                            /* don't clear object memory from below monsters */
                            level->locations[x][y].mem_invis = FALSE;
                            newsym(x, y);
                        }
                        if (MON_AT(level, x, y))
                            found_monsters++;
                    }
                }
                if (!found_monsters)
                    pline("You feel lonely");
                effect = TRUE; /* either lonely or detected stuff */
            }
        }
        break;
    case SLOW:
        if (you && !redundant) {
            pline(lost ? "Your speed returns." :
                  "You feel abnormally slow.");
            effect = TRUE;
        } else if (vis && !redundant) {
            pline(lost ? "%s speeds up." :
                  "%s slows down abnormally.",
                  Monnam(mon));
            effect = TRUE;
        }
        break;
    default:
        impossible("Unknown property: %u", prop);
        break;
    }
    if (effect)
        return TRUE;
    return FALSE;
}
unsigned
u_have_property(enum youprop property, unsigned reasons,
                boolean even_if_blocked)
{
    return m_has_property(&youmonst, property, reasons, even_if_blocked);
}


/* Player and monster helplessness. This is currently separate from properties,
   because for player helplessness, we record a reason to place in the death
   messages. */
boolean
u_helpless(enum helpless_mask mask)
{
    int i;

    /* A lack of a cause canonically indicates that we weren't actually helpless
       for this reason. We may not have an endmsg, and the timer may already
       have expired but the helplessness not yet been canceled, so we can't use
       these as indications. */
    for (i = hr_first; i <= hr_last; ++i)
        if ((mask & (1 << i)) && *turnstate.helpless_causes[i])
            return TRUE;

    return FALSE;
}

boolean
m_helpless(const struct monst *mon, enum helpless_mask mask)
{
    if (mon == &youmonst)
        return u_helpless(mask);
    if (mon->msleeping && (mask & (1 << hr_asleep)))
        return TRUE;
    if (!mon->mcanmove && (mask & (1 << hr_paralyzed)))
        return TRUE;
    if (mon->meating && (mask & (1 << hr_busy)))
        return TRUE;
    if ((mon->m_ap_type == M_AP_OBJECT || mon->m_ap_type == M_AP_FURNITURE) &&
        (mask & (1 << hr_mimicking)))
        return TRUE;

    return FALSE;
}

/* Hack: check if a monster could sense someone else at specific X/Y coords.
   This is implemented by temporary changing mx/my to the destination, call msensem,
   and then revert mx/my to its' old values */
unsigned
msensem_xy(struct monst *viewer, struct monst *viewee,
           xchar tx, xchar ty)
{
    xchar ox = viewer->mx;
    xchar oy = viewer->my;
    viewer->mx = tx;
    viewer->my = ty;
    unsigned sensed = msensem(viewer, viewee);
    viewer->mx = ox;
    viewer->my = oy;
    return sensed;
}

/* Returns the bitwise OR of all MSENSE_ values that explain how "viewer" can
   see "viewee". &youmonst is accepted as either argument. If both arguments
   are the same, this tests if/how a monster/player can detect itself. */
unsigned
msensem(const struct monst *viewer, const struct monst *viewee)
{
    unsigned sensemethod = 0;

    /* sanity checks, so the caller doesn't have to */
    if (viewer != &youmonst)
        if (!onmap(viewer) || DEADMONSTER(viewer))
            return 0;
    if (viewee != &youmonst)
        if (!onmap(viewee) || DEADMONSTER(viewee))
            return 0;
    if (!level) {
        impossible("vision calculations during level creation");
        return 0;
    }

    /* TODO: once levels rewrite is done, this code can be simplified (and won't
       work in its present form). */
    d_level *sz = m_mz(viewer), *tz = m_mz(viewee);
    if (sz->dnum != tz->dnum || sz->dlevel != tz->dlevel)
        return 0;
    struct level *lev = level;
    if (viewer != &youmonst)
        lev = viewer->dlevel;

    int sx = m_mx(viewer), sy = m_my(viewer),
        tx = m_mx(viewee), ty = m_my(viewee);

    int distance = dist2(sx, sy, tx, ty);

    /* Special case: if either endpoint is an engulfing monster, then we want
       LOE to the square specifically, ignoring players on that square (because
       the edge of an engulfing monster blocks LOE to the player). */
    char **msensem_vizarray =
        (Engulfed && (viewer == u.ustuck || viewee == u.ustuck)) ?
        NULL : viz_array;

    /* Line of effect. clear_path is like couldsee(), but doesn't require the
       player to be at either endpoint. (If the player happens to be at one of
       the endpoints, it just calls couldsee() directly.) */
    boolean loe = clear_path(sx, sy, tx, ty, msensem_vizarray);

    /* A special case for many vision methods: water or the ground blocking
       vision. A hiding target is also included in these, because hiding is
       often a case of using an object, part of the floor, a cranny in the
       ceiling, etc., to block vision (and even when it isn't, it should block
       vision in the same cases). */
    boolean vertical_loe =
        !(m_mburied(viewer) || m_mburied(viewee) ||
          ((!!m_underwater(viewee)) ^ (!!m_underwater(viewer))) ||
          m_mhiding(viewee));

    boolean invisible = !!invisible(viewee);

    /* For normal vision, one necessary condition is that the target must be
       adjacent or on a lit square (we assume there's just enough light in the
       dungeon that even in dark areas, adjacent squares are visible to normal
       vision). We test "lit" by testing that the square is either temporarily
       lit, or permanently lit. (We can't use the normal cansee() check because
       that doesn't work for squares outside the player's LOE, and it's possible
       that neither the viewer nor the viewee is the player.)

       TODO: templit off-level. That's very hard to implement because we don't
       run lighting calculations off-level. */
    boolean target_lit = distance <= 2 || (lev == level && templit(tx, ty)) ||
        lev->locations[tx][ty].lit;

    /* TODO: Maybe infravision (and perhaps even infravisibility) should be
       properties? */
    boolean infravision_ok = pm_infravision(viewer->data) &&
        pm_infravisible(viewee->data);

    boolean blinded = !!blind(viewer);
    boolean see_invisible = !!see_invisible(viewer);

    if (loe && vertical_loe && !blinded) {
        if (!invisible && target_lit)
            sensemethod |= MSENSE_VISION;
        if (!invisible && infravision_ok)
            sensemethod |= MSENSE_INFRAVISION;
        if (invisible && (target_lit || infravision_ok) && see_invisible)
            sensemethod |= MSENSE_SEEINVIS | MSENSEF_KNOWNINVIS;
    }

    /* Telepathy. The viewee needs a mind; the viewer needs either to be blind,
       or for the telepathy to be extrinsic and the viewer within BOLT_LIM. */
    if (!mindless(viewee->data) && !m_helpless(viewer, hm_unconscious)) {
        unsigned telepathy_reason = telepathic(viewer);
        if ((telepathy_reason && blinded) ||
            (telepathy_reason & (W_EQUIP | W_ARTIFACT) &&
             distance <= BOLT_LIM * BOLT_LIM))
            sensemethod |= MSENSE_TELEPATHY;
    }

    /* Astral vision. Like regular vision, but has a distance check rather than
       an LOE check. It's unclear whether this pierces blindness, because the
       only item that gives astral vision also gives blindness immunity; this
       code assumes not. */
    boolean xray = astral_vision(viewer) && (!invisible || see_invisible);
    if (vertical_loe && distance <= XRAY_RANGE * XRAY_RANGE && xray &&
        (target_lit || infravision_ok)) {
        sensemethod |= MSENSE_XRAY;
        if (invisible && see_invisible)
            sensemethod |= MSENSEF_KNOWNINVIS;
    }

    /* Ideally scent should work around corners, but not through walls. That's
       awkward to write, though, because it'd require pathfinding. */
    if (vertical_loe && loe && distance <= 5 && has_scent(viewer->data))
        sensemethod |= MSENSE_SCENT;

    /* Monster detection. All that is needed (apart from same-level, which was
       checked earlier) is the property itself. */
    if (detects_monsters(viewer))
        sensemethod |= MSENSE_MONDETECT;

    /* Warning versus monster class. (Actually implemented as monster
       /race/.) */
    if (mworn_warntype(viewer) & viewee->data->mflags2)
        sensemethod |= MSENSE_WARNOFMON;

    /* Covetous sense. Note that the player can benefit from this too, e.g. a
       player in master lich form will be able to detect the Wizard of Yendor
       holding the Book of the Dead. */
    if (covetous_sense(viewer, viewee))
        sensemethod |= MSENSE_COVETOUS;

    /* Smell of gold, approximating 3.4.3 behaviour (which was previously in
       set_apparxy in monmove.c). Xorns can sense any monster with gold in their
       inventory. */
    if (viewer->data == &mons[PM_XORN] && money_cnt(m_minvent(viewee)))
        sensemethod |= MSENSE_GOLDSMELL;

    /* Cooperative telepathy. Friendly monsters reveal themselves to each other
       with telepathy. If one has telepathy, that one's telepathy determines how
       easily they sense each other. If both has, they can be seen everywhere */
    if (!mindless(viewer->data) && !m_helpless(viewer, hm_unconscious)) {
        unsigned telepathy_reason = telepathic(viewee);
        if ((telepathy_reason && blinded) ||
            (telepathy_reason & (W_EQUIP | W_ARTIFACT) &&
             distance <= BOLT_LIM * BOLT_LIM))
            sensemethod |= MSENSE_TEAMTELEPATHY;
        if (telepathic(viewer) && telepathic(viewee))
            sensemethod |= MSENSE_TEAMTELEPATHY;
    }

    /* Aggravate monster. If a monster has the aggravate monster property,
       every monster on the level can sense it everywhere */
    if (aggravating(viewee))
        sensemethod |= MSENSE_AGGRAVATE;

    /* Warning. This partial-senses monsters that are hostile to the viewer, and
       have a level of 4 or greater, and a distance of 100 or less. */
    if (distance <= 100 && m_mlev(viewee) >= 4 &&
        warned(viewer) &&
        mm_aggression(viewee, viewer) & ALLOW_M)
        sensemethod |= MSENSE_WARNING;

    /* Deducing the existence of a long worm via seeing a segment.

       Based on the code that was formerly worm_known in worm.c, but expanded to
       handle monster viewing.

       Note: assumes that normal vision, possibly modified by astral vision and
       see invisible, is the only way to see a long worm tail. Infravision
       doesn't work (they're cold-blooded), and currently no other types of
       vision are implemented. Detection would find the head. */
    if (viewee->wormno && (!invisible || see_invisible) &&
        vertical_loe && !blinded) {
        struct wseg *curr = viewee->dlevel->wtails[viewee->wormno];

        while (curr) {
            boolean seg_dist = dist2(sx, sy, curr->wx, curr->wy);
            boolean seg_loe =
                clear_path(sx, sy, curr->wx, curr->wy, msensem_vizarray) ||
                (xray && seg_dist <= XRAY_RANGE * XRAY_RANGE);
            boolean seg_lit = seg_dist <= 2 ||
                (lev == level && templit(curr->wx, curr->wy)) ||
                lev->locations[curr->wx][curr->wy].lit;

            if (seg_loe && seg_lit)
                sensemethod |= MSENSE_WORM;

            curr = curr->nseg;
        }
    }

    /* Calculate known invisibility, because we have all the information to
       hand, and it's a complex calculation without it. We need to be able to
       see the monster's location with normal vision, but not the monster
       itself. Also don't include warning in this (because then, we can't match
       the monster to the message). */
    if (loe && vertical_loe && !blinded && sensemethod && target_lit &&
        !(sensemethod & (MSENSE_ANYVISION | MSENSE_WARNING)))
        sensemethod |= MSENSEF_KNOWNINVIS;

    /* If the target is in item form, it's not being seen properly. Any
       vision-style detection of the target is going to not see it as a
       monster. */
    if (m_helpless(viewee, 1 << hr_mimicking) &&
        (lev != level || !Protection_from_shape_changers) &&
        (sensemethod & MSENSE_ANYVISION)) {
        sensemethod &= ~MSENSE_ANYVISION;
        sensemethod |= MSENSE_ITEMMIMIC;
    }

    return sensemethod;
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
enlighten_mon(struct monst *mon, int final)
    /* final: 0 => still in progress; 1 => over, survived; 2 => dead */
{
    int ltmp;
    int n;
    const char *title;
    const char *buf;
    struct nh_menulist menu;
    
    init_menulist(&menu);
    title = final ? "Final Attributes:" : "Current Attributes:";
    
    const char *monname = (mon == &youmonst ? "You" : Monnam(mon));
    const char *is = (mon == &youmonst ? " are " : " is ");
    const char *was = (mon == &youmonst ? " were " : " was ");
    const char *has = (mon == &youmonst ? " have " : " has ");
    const char *had = " had ";
    const char *can = " can ";
    const char *could = " could ";
    const char *see = (mon == &youmonst ? " see " : " sees ");
    const char *saw = " saw";
    n = menu.icount;

#define mon_is(menu,mon,attr)         enl_msg(menu,monname,is,was,attr)
#define mon_has(menu,mon,attr)        enl_msg(menu,monname,has,had,attr)
#define mon_can(menu,mon,attr)        enl_msg(menu,monname,can,could,attr)
#define mon_sees(menu,mon,attr)       enl_msg(menu,monname,see,saw,attr)
#define mon_x(menu,mon,attr)          enl_msg(menu,monname,"","d",attr)
    
    if (mon == &youmonst && flags.elbereth_enabled &&
        u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
            "the Hand of Elbereth",
            "the Envoy of Balance",
            "the Glory of Arioch"
        };
        mon_is(&menu, mon, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
        if (u.ualign.record >= 20)
            mon_is(&menu, mon, "piously aligned");
        else if (u.ualign.record > 13)
            mon_is(&menu, mon, "devoutly aligned");
        else if (u.ualign.record > 8)
            mon_is(&menu, mon, "fervently aligned");
        else if (u.ualign.record > 3)
            mon_is(&menu, mon, "stridently aligned");
        else if (u.ualign.record == 3)
            mon_is(&menu, mon, "aligned");
        else if (u.ualign.record > 0)
            mon_is(&menu, mon, "haltingly aligned");
        else if (u.ualign.record == 0)
            mon_is(&menu, mon, "nominally aligned");
        else if (u.ualign.record >= -3)
            mon_is(&menu, mon, "strayed");
        else if (u.ualign.record >= -8)
            mon_is(&menu, mon, "sinned");
        else
            mon_is(&menu, mon, "transgressed");
        if (wizard) {
            buf = msgprintf(" %d", u.uhunger);
            enl_msg(&menu, "Hunger level ", "is", "was", buf);

            buf = msgprintf(" %d / %ld", u.ualign.record, ALIGNLIM);
            enl_msg(&menu, "Your alignment ", "is", "was", buf);
        }
    }


        /*** Resistances to troubles ***/
    if (resists_fire(mon))
        mon_is(&menu, mon, "fire resistant");
    if (resists_cold(mon))
        mon_is(&menu, mon, "cold resistant");
    if (resists_sleep(mon))
        mon_is(&menu, mon, "sleep resistant");
    if (resists_disint(mon))
        mon_is(&menu, mon, "disintegration-resistant");
    if (resists_elec(mon))
        mon_is(&menu, mon, "shock resistant");
    if (resists_poison(mon))
        mon_is(&menu, mon, "poison resistant");
    if (resists_magm(mon))
        mon_is(&menu, mon, "magic resistant");
    if (resists_drli(mon))
        mon_is(&menu, mon, "level-drain resistant");
    if (resists_sick(mon))
        mon_is(&menu, mon, "immune to sickness");
    if (resists_acid(mon))
        mon_is(&menu, mon, "acid resistant");
    if (resists_ston(mon))
        mon_is(&menu, mon, "petrification resistant");
    if (resists_hallu(mon))
        mon_is(&menu, mon, "hallucination resistant");
    if (mon == &youmonst && u.uinvulnerable)
        mon_is(&menu, mon, "invulnerable");
    if ((mon == &youmonst && u.uedibility) ||
        (mon != &youmonst && mon->mtame))
        mon_can(&menu, mon, "recognize detrimental food");

    /*** Troubles ***/
    if (hallucinating(mon))
        mon_is(&menu, mon, "hallucinating");
    if (stunned(mon))
        mon_is(&menu, mon, "stunned");
    if (confused(mon))
        mon_is(&menu, mon, "confused");
    if (blind(mon))
        mon_is(&menu, mon, "blinded");
    if (sick(mon)) {
        if (mon == &youmonst && (u.usick_type & SICK_VOMITABLE))
            mon_is(&menu, mon, "sick from food poisoning");
        else
            mon_is(&menu, mon, "sick from illness");
    }
    if (petrifying(mon))
        mon_is(&menu, mon, "turning to stone");
    if (sliming(mon))
        mon_is(&menu, mon, "turning into slime");
    if (strangled(mon))
        mon_is(&menu, mon, (u.uburied) ? "buried" : "being strangled");
    if (slippery_fingers(mon))
        mon_has(&menu, mon, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (fumbling(mon))
        mon_x(&menu, mon, "fumble");
    if (mon == &youmonst && Wounded_legs && !u.usteed)
        mon_has(&menu, mon, msgcat("wounded", makeplural(body_part(LEG))));;
    if (mon == &youmonst && Wounded_legs && u.usteed && wizard) {
        buf = x_monnam(u.usteed, ARTICLE_YOUR, NULL, SUPPRESS_SADDLE |
                       SUPPRESS_HALLUCINATION, FALSE);
        enl_msg(&menu, msgupcasefirst(buf), " has", " had", " wounded legs");
    }

    if (restful_sleep(mon))
        mon_has(&menu, mon, "restful sleep");
    if (hunger(mon))
        mon_has(&menu, mon, "fast metabolism");

        /*** Vision and senses ***/
    if (see_invisible(mon))
        mon_sees(&menu, mon, "invisible");
    if (telepathic(mon))
        mon_is(&menu, mon, "telepathic");
    if (warned(mon))
        mon_is(&menu, mon, "warned");
    if (warned_of_mon(mon)) {
        int warntype = mworn_warntype(mon);
        buf = msgcat("aware of the presence of ",
                    (warntype & M2_ORC) ? "orcs" :
                    (warntype & M2_DEMON) ? "demons" :
                    "something");
        mon_is(&menu, mon, buf);
    }
    if (warned_of_undead(mon))
        mon_is(&menu, mon, "warned of undead");
    if (searching(mon))
        mon_has(&menu, mon, "automatic searching");
    if (clairvoyant(mon))
        mon_is(&menu, mon, "clairvoyant");
    if (infravision(mon))
        mon_has(&menu, mon, "infravision");
    if (detects_monsters(mon))
        mon_is(&menu, mon, "sensing the presence of monsters");
    if (mon == &youmonst && u.umconf)
        mon_is(&menu, mon, "going to confuse monsters");

        /*** Appearance and behavior ***/
    if (adorned(mon)) {
        int adorn = 0;

        /* BUG: this does the wrong thing for monsters */
        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        if (adorn < 0)
            mon_is(&menu, mon, "poorly adorned");
        else
            mon_is(&menu, mon, "adorned");
    }
    if (invisible(mon))
        mon_is(&menu, mon, "invisible");
    else if (invisible(mon) && see_invisible(mon))
        mon_is(&menu, mon, "invisible to others");
    /* ordinarily "visible" is redundant; this is a special case for the
       situation when invisibility would be an expected attribute */
    else if (m_has_property(mon, INVIS, ANY_PROPERTY, TRUE) &&
             !invisible(mon))
        mon_is(&menu, mon, "visible");
    if (displaced(mon))
        mon_is(&menu, mon, "displaced");
    if (stealthy(mon))
        mon_is(&menu, mon, "stealthy");
    if (aggravating(mon))
        mon_x(&menu, mon, "aggravate");
    if (conflicting(mon))
        mon_is(&menu, mon, "conflicting");

        /*** Transportation ***/
    if (jumps(mon))
        mon_can(&menu, mon, "jump");
    if (teleportitis(mon)) {
        if (mon == &youmonst &&
            supernatural_ability_available(SPID_RLOC))
            mon_can(&menu, mon, "teleport at will");
        else if (mon != &youmonst && (mon->m_lev == 12 ||
                (mon->data->mflags1 & M1_TPORT)))
            mon_can(&menu, mon, "teleport at will");
        else
            mon_can(&menu, mon, "teleport");
    }
    if (teleport_control(mon))
        mon_has(&menu, mon, "teleport control");
    if (levitates_at_will(mon, FALSE, FALSE))
        mon_is(&menu, mon, "levitating, at will");
    else if (levitates(mon))
        mon_is(&menu, mon, "levitating");   /* without control */
    else if (flying(mon))
        mon_can(&menu, mon, "fly");
    if (waterwalks(mon))
        mon_can(&menu, mon, "walk on water");
    if (swims(mon))
        mon_can(&menu, mon, "swim");
    if (mon->data->mflags1 & M1_AMPHIBIOUS)
        mon_can(&menu, mon, "breathe water");
    else if (unbreathing(mon))
        mon_can(&menu, mon, "survive without air");
    if (phasing(mon))
        mon_can(&menu, mon, "walk through walls");

    /* FIXME: This is printed even if you die in a riding accident. */
    if (mon == &youmonst && u.usteed)
        mon_is(&menu, mon, msgcat("riding ", y_monnam(u.usteed)));
    if (mon == &youmonst && Engulfed)
        mon_is(&menu, mon, msgcat("swallowed by ", a_monnam(u.ustuck)));
    else if (u.ustuck && (mon == u.ustuck || mon == &youmonst)) {
        if (mon == &youmonst)
            buf = msgprintf("%s %s",
                  (Upolyd && sticks(youmonst.data)) ?
                  "holding" : "held by", a_monnam(u.ustuck));
        else
            buf = msgprintf("%s %s",
                  (Upolyd && sticks(youmonst.data)) ?
                  "held by" : "holding", "you");
        mon_is(&menu, mon, buf);
    }

    /*** Physical attributes ***/
    if (mon == &youmonst) {
        if (u.uhitinc)
            mon_has(&menu, mon, enlght_combatinc("to hit", u.uhitinc, final));
        if (u.udaminc)
            mon_has(&menu, mon, enlght_combatinc("damage", u.udaminc, final));
    }
    if (slow_digestion(mon))
        mon_has(&menu, mon, "slower digestion");
    if (regenerates(mon))
        mon_x(&menu, mon, "regenerate");
    if (mon == &youmonst && (u.uspellprot || Protection)) {
        int prot = 0;

        if (uleft && uleft->otyp == RIN_PROTECTION)
            prot += uleft->spe;
        if (uright && uright->otyp == RIN_PROTECTION)
            prot += uright->spe;
        if (HProtection & INTRINSIC)
            prot += u.ublessed;
        prot += u.uspellprot;

        if (prot < 0)
            mon_is(&menu, mon, "ineffectively protected");
        else
            mon_is(&menu, mon, "protected");
    }
    if (shapeshift_prot(mon))
        mon_is(&menu, mon, "protected from shape changers");
    if (polymorphitis(mon))
        mon_is(&menu, mon, "polymorphing");
    if (polymorph_control(mon))
        mon_has(&menu, mon, "polymorph control");
    if ((mon == &youmonst && u.ulycn >= LOW_PM) || is_were(mon->data))
        mon_is(&menu, mon, an(mons[u.ulycn].mname));
    if (mon == &youmonst && Upolyd) {
        const char *buf;
        if (u.umonnum == u.ulycn)
            buf = "in beast form";
        else
            buf = msgprintf("%spolymorphed into %s",
                            flags.polyinit_mnum == -1 ? "" : "permanently ",
                            an(youmonst.data->mname));
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.mtimedone);
        mon_is(&menu, mon, buf);
    }
    if (unchanging(mon))
        mon_can(&menu, mon, "not change form");
    if (very_fast(mon))
        mon_is(&menu, mon, "very fast");
    else if (fast(mon))
        mon_is(&menu, mon, "fast");
    if (slow(mon))
        mon_is(&menu, mon, "slow");
    if (reflecting(mon))
        mon_has(&menu, mon, "reflection");
    if (free_action(mon))
        mon_has(&menu, mon, "free action");
    if (fixed_abilities(mon))
        mon_has(&menu, mon, "fixed abilities");
    if (will_be_lifesaved(mon))
        mon_is(&menu, mon, "life saving");
    if (mon == &youmonst && u.twoweap)
        mon_is(&menu, mon, "wielding two weapons at once");

        /*** Miscellany ***/
    if (mon == &youmonst) {
        if (Luck) {
            ltmp = abs((int)Luck);
            const char *buf = msgprintf(
                "%s%slucky",
                ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
                Luck < 0 ? "un" : "");
            if (wizard)
                buf = msgprintf("%s (%d)", buf, Luck);
            you_are(&menu, buf);
        } else if (mon == &youmonst && wizard)
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
        } else if (!final) {
            /*
            * We need to suppress this when the game is over, because death
            * can change the value calculated by can_pray(), potentially
            * resulting in a false claim that you could have prayed safely.
            */
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
    if (n == menu.icount)
        mon_has(&menu, mon, "no special properties");

    display_menu(&menu, title, PICK_NONE, PLHINT_ANYWHERE,
                NULL);
    return;
}

/* TODO: replace with enlighten_mon() */
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
            buf = msgprintf("%spolymorphed into %s",
                            flags.polyinit_mnum == -1 ? "" : "permanently ",
                            an(youmonst.data->mname));
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
        buf = msgprintf(" your moral code, losing %u point%s of alignment, "
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
