/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-10 */
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

static void init_permonsts(const struct monst *, const struct permonst **,
                           const struct permonst **, const struct permonst **);
static boolean is_green(struct monst *);
static boolean slip_or_trip(struct monst *);

/* Messages on intrinsic gain/loss */
/* Order: prop, outside, role, lost outside, lost role */

struct propmsg {
    unsigned int prop;
    const char *gainoutside, *gainpm, *loseoutside, *losepm;
};

static const struct propmsg prop_msg[] = {
    {FIRE_RES, "You feel a momentary chill.", "cool",
     "You feel warmer.", "warmer"},
    {COLD_RES, "You feel full of hot air.", "warm",
     "You feel cooler.", "cooler"},
    {SLEEP_RES, "You feel wide awake.", "awake",
     "You feel tired!", "tired"},
    {DISINT_RES, "You feel very firm.", "firm",
     "You feel less firm.", "less firm"},
    {SHOCK_RES, "Your health currently feels amplified!", "insulated",
     "You feel conductive.", "conductive"},
    {POISON_RES, "You feel healthy.", "hardy",
     "You feel a little sick!", "sickly"},
    {ACID_RES, "Your skin feels leathery.", "thick-skinned",
     "Your skin feels less leathery.", "soft-skinned"},
    {STONE_RES, "You feel flexible.", "limber",
     "You feel stiff.", "stiff"},
    {REGENERATION, "You feel healthier than ever!", "regenerative",
     "You don't feel as healthy anymore.",  "less regenerative"},
    {SEARCHING, "You feel perceptive!", "perceptive",
     "You feel unfocused.", "unfocused"},
    {SEE_INVIS, "You feel aware.", "aware",
     "You thought you saw something!", "unaware"},
    {INVIS, "You feel hidden!", "hidden",
     "You feel paranoid.", "paranoid"},
    {TELEPORT, "You feel very jumpy.", "jumpy",
     "You feel less jumpy.", "less jumpy"},
    {TELEPORT_CONTROL, "You feel in control of yourself.", "controlled",
     "You feel less in control.", "uncontrolled"},
    {POLYMORPH, "Your body begins to shapeshift.", "shapeshifting",
     "You are no longer shapeshifting.", "less shapeshifting"},
    {POLYMORPH_CONTROL, "You feel in control of your shapeshifting", "shapeshift-controlled",
     "You feel no longer in control of your shapeshifting.", "less shapeshift-controlled"},
    {STEALTH, "", "stealthy", "", "noisy"},
    {AGGRAVATE_MONSTER, "You feel observed.", "observed",
     "You feel less attractive.", "conspicuous"},
    {CONFLICT, "The air around you turns hostile and conflicted.", "conflicted",
     "The air around you calms down.", "calmed"},
    {PROT_FROM_SHAPE_CHANGERS, "The air around you seems oddly fixed and static.",
     "shapeshift-protected",
     "The air around you briefly shifts in shape.", "less shapeshift-protected"},
    {WARNING, "You feel sensitive.", "sensitive", "You feel insensitive.", "insensitive"},
    {TELEPAT, "You feel a strange mental acuity.", "telepathic",
     "Your senses fail!", "untelepathic"},
    {FAST, "", "quick", "", "slow"},
    {SLEEPING, "You feel drowsy.", "drowsy", "You feel awake.", "awake"},
    {WWALKING, "You feel light on your feet.", "light",
     "You feel heavier.", "heavy"},
    {HUNGER, "You feel your metabolism speed up.", "hungry",
     "Your digestion calms down.", "full."},
    {REFLECTING, "Your body feels repulsive.", "repulsive",
     "You feel less repulsive.", "absorptive"},
    {LIFESAVED, "You feel a strange sense of immortality.", "immortal",
     "You lose your sense of immortality!", "mortal"},
    {ANTIMAGIC, "You feel resistant to magic.", "skeptical",
     "Your magic resistance fails!", "credulous"},
    {DISPLACED, "", "elusive", "", "exposed"},
    {CLAIRVOYANT, "You feel attuned to the dungeon.", "attuned to the dungeon",
     "You don't feel attuned to the dungeon after all", "less dungeon-attuned"},
    {ENERGY_REGENERATION, "You feel in touch with magic around you.", "magic-regenerative",
     "Your touch with magic fails!", "less magic-regenerative"},
    {MAGICAL_BREATHING, "Your breathing seem enhanced.", "breath-enhanced",
     "Your breathing seems normal.", "less breath-enhanced"},
    {SICK_RES, "You feel your immunity strengthen.", "immunized",
     "Your immune system fails!", "immunocompromised"},
    {DRAIN_RES, "You feel especially energetic.", "energic",
     "You feel less energic.", "less energic"},
    {CANCELLED, "You feel devoid of magic!", "magic-devoid",
     "Your magic returns.", "magical"},
    {FREE_ACTION, "You feel especially agile.", "agile",
     "You feel less agile.", "less agile"},
    {SWIMMING, "You feel more attuned to water.", "water-attuned",
     "You forget your swimming skills.", "less water-attuned"},
    {FIXED_ABIL, "You feel resistant to exercise", "ability-fixed",
     "You feel less resistant to exercise", "ability-fixed"},
    {FLYING, "You feel more buoyant.", "buoyant",
     "You feel less buoyant.", "less buoyant"},
    {UNCHANGING, "You feel resistant to change.", "unchanged",
     "You feel less resistant to change.", "changed"},
    {PASSES_WALLS, "Your body unsolidifies", "unsolid",
     "Your body solidifies.", "solid"},
    {INFRAVISION, "Your vision capabilities are enhanced.", "vision-enhanced",
     "You feel half blind!", "half blind"},
    {NO_PROP, "", "", "", ""}
};

static const struct propmsg prop_msg_hallu[] = {
    {FIRE_RES, "You be chillin'.", "cool",
     "You feel warmer.", "warmer"},
    {DISINT_RES, "You feel totally together, man.", "firm",
     "You feel split up.", "less firm"},
    {SHOCK_RES, "You feel grounded in reality.", "insulated",
     "You feel less grounded.", "conductive"},
    {SEE_INVIS, "", "attentive",
     "You tawt you taw a puttie tat!", "unattentive"},
    {TELEPORT, "You feel diffuse.", "jumpy",
     "You feel less jumpy.", "less jumpy"},
    {TELEPORT_CONTROL, "You feel centered in your personal space.", "controlled",
     "You feel less in control.", "uncontrolled"},
    {POLYMORPH, "You feel like a chameleon.", "shapeshifting",
     "You no longer feel like a chameleon.", "less shapeshifting"},
    {TELEPAT, "You feel in touch with the cosmos.", "telepathic",
     "Your cosmic connection is no more!", "untelepathic"},
    {WWALKING, "You feel like Jesus himself.", "light",
     "You realize that you aren't Jesus after all.", "heavy"},
    {MAGICAL_BREATHING, "You seem rather fishy...", "fishy",
     "You don't seem so fishy after all.", "less fishy"},
    {DRAIN_RES, "You are bouncing off the walls!", "energetic",
     "You feel less bouncy.", "less energetic"},
    {FLYING, "You feel like a super hero!", "buoyant",
     "You sadly lose your heroic abilities.", "less buoyant"},
    {NO_PROP, "", "", "", ""}
};


/* Intrinsics roles gain by level up
   Yes, this makes it theoretically possible to give level-based
   intrinsics for monsters. This technique is not currently used
   besides the fact that player monsters are granted relevant
   intrinsics.
   XL1 intrinsics are stored in permonst (Adding XL1 properties
   here *works*, but is not the right way to do it IMO -FIQ).
   TODO: make this part of permonst... somehow
   TODO: if this thing ever grants fly/lev/similar, monster XL
   change needs to call update_property properly. */
struct propxl {
    unsigned int mnum;
    unsigned int xl;
    unsigned int prop;
};

static const struct propxl prop_from_experience[] = {
    {PM_ARCHEOLOGIST, 10, SEARCHING},
    {PM_BARBARIAN, 7, FAST},
    {PM_BARBARIAN, 15, STEALTH},
    {PM_CAVEMAN, 15, WARNING},
    {PM_HEALER, 15, WARNING},
    {PM_KNIGHT, 7, FAST},
    {PM_MONK, 3, POISON_RES},
    {PM_MONK, 5, STEALTH},
    {PM_MONK, 7, WARNING},
    {PM_MONK, 9, SEARCHING},
    {PM_MONK, 11, FIRE_RES},
    {PM_MONK, 13, COLD_RES},
    {PM_MONK, 15, SHOCK_RES},
    {PM_MONK, 17, TELEPORT_CONTROL},
    {PM_PRIEST, 15, WARNING},
    {PM_PRIEST, 20, FIRE_RES},
    {PM_RANGER, 7, STEALTH},
    {PM_RANGER, 15, SEE_INVIS},
    {PM_ROGUE, 10, SEARCHING},
    {PM_SAMURAI, 15, STEALTH},
    {PM_TOURIST, 10, SEARCHING},
    {PM_TOURIST, 20, POISON_RES},
    {PM_VALKYRIE, 7, FAST},
    {PM_WIZARD, 15, WARNING},
    {PM_WIZARD, 17, TELEPORT_CONTROL},
    {PM_ELF, 4, SLEEP_RES},
    {PM_RED_DRAGON, 10, INFRAVISION},
    {PM_RED_DRAGON, 10, WARNING},
    {PM_RED_DRAGON, 10, SEE_INVIS},
    {PM_WHITE_DRAGON, 10, WATERPROOF},
    {PM_WHITE_DRAGON, 10, SEARCHING},
    {PM_ORANGE_DRAGON, 10, FREE_ACTION},
    {PM_BLACK_DRAGON, 10, DRAIN_RES},
    {PM_BLUE_DRAGON, 10, FAST},
    {PM_GREEN_DRAGON, 10, SICK_RES},
    /* Yellow dragons are acidic, so they have stone res already */
    {NON_PM, 0, 0}
};


/* Checks if a monster has any intrinsic at all in mintrinsic.
   Used to determine if a monster should be saved in the corpse data.
   TODO: probably hacklib material, since it's basically "is this array
   only consisting of zeroes" */
boolean
any_property(struct monst *mon)
{
    int i;
    for (i = 0; i <= LAST_PROP; i++)
        if (mon->mintrinsic[i])
            return TRUE;
    return FALSE;
}


/* Intrinsics for a certain monster form.
   Returns 1: yes, 0: no, -1: blocking. */
int
pm_has_property(const struct permonst *mdat, enum youprop property)
{
    uchar mfromrace = mdat->mresists;

    /* Blockers. Sickness resistance blocking sickness, etc, is handled
       elsewhere... */
    if (property == SLIMED            ? flaming(mdat) || unsolid(mdat) ||
                                        mdat == &mons[PM_GREEN_SLIME]        :
        property == STONED            ? poly_when_stoned(mdat)               :
        property == GLIB              ? nohands(mdat)                        :
        property == LIFESAVED         ? nonliving(mdat)                      :
        0)
        return -1;

    /* TODO: Change the monster data code into something that doesn't require a
       giant switch statement or ternary chain to get useful information from
       it. We use a ternary chain here because it cuts down on repetitive code
       and so is easier to read. */
    if (property == FIRE_RES          ? mfromrace & MR_FIRE                  :
        property == COLD_RES          ? mfromrace & MR_COLD                  :
        property == SLEEP_RES         ? mfromrace & MR_SLEEP                 :
        property == DISINT_RES        ? mfromrace & MR_DISINT                :
        property == SHOCK_RES         ? mfromrace & MR_ELEC                  :
        property == POISON_RES        ? mfromrace & MR_POISON                :
        property == ACID_RES          ? mfromrace & MR_ACID                  :
        property == STONE_RES         ? mfromrace & MR_STONE                 :
        property == DRAIN_RES         ? is_undead(mdat) || is_demon(mdat) ||
                                        is_were(mdat) ||
                                        mdat == &mons[PM_DEATH]              :
        property == SICK_RES          ? mdat->mlet == S_FUNGUS ||
                                        mdat == &mons[PM_GHOUL]              :
        property == ANTIMAGIC         ? dmgtype(mdat, AD_MAGM) ||
                                        dmgtype(mdat, AD_RBRE) ||
                                        mdat == &mons[PM_BABY_GRAY_DRAGON]   :
        property == STUNNED           ? mdat->mflags2 & M2_STUNNED           :
        property == BLINDED           ? !haseyes(mdat)                       :
        property == HALLUC            ? dmgtype(mdat, AD_HALU)               :
        property == SEE_INVIS         ? mdat->mflags1 & M1_SEE_INVIS         :
        property == TELEPAT           ? mdat->mflags2 & M2_TELEPATHIC        :
        property == INFRAVISION       ? pm_infravision(mdat)                 :
        property == INVIS             ? pm_invisible(mdat)                   :
        property == TELEPORT          ? mdat->mflags1 & M1_TPORT             :
        property == LEVITATION        ? is_floater(mdat)                     :
        property == FLYING            ? mdat->mflags1 & M1_FLY               :
        property == SWIMMING          ? pm_swims(mdat)                       :
        property == PASSES_WALLS      ? mdat->mflags1 & M1_WALLWALK          :
        property == REGENERATION      ? mdat->mflags1 & M1_REGEN             :
        property == REFLECTING        ? mdat == &mons[PM_SILVER_DRAGON]      :
        property == DISPLACED         ? mdat->mflags3 & M3_DISPLACED         :
        property == TELEPORT_CONTROL  ? mdat->mflags1 & M1_TPORT_CNTRL       :
        property == MAGICAL_BREATHING ? amphibious(mdat)                     :
        property == STEALTH           ? mdat->mflags3 & M3_STEALTHY          :
        property == FAST              ? mdat->mflags3 & M3_FAST              :
        property == SEARCHING         ? mdat->mflags3 & M3_SEARCH            :
        property == JUMPING           ? mdat->mflags3 & M3_JUMPS             :
        0)
        return 1;
    return 0;
}


/* Initialize 3 permonsts set to role, race and poly. Used to determine
   source of properties (os_role, os_race, os_polyform) */
static void
init_permonsts(const struct monst *mon, const struct permonst **role,
               const struct permonst **race, const struct permonst **poly)
{
    int racenum;
    if (mon == &youmonst) {
        *role = &mons[urole.malenum];
        *race = &mons[urace.malenum];
    } else {
        if (mon->orig_mnum)
            *role = &mons[mon->orig_mnum];
        else
            *role = mon->data;
        racenum = genus(monsndx(*role), 0);
        if (racenum)
            *race = &mons[racenum];
    }
    if (*role != mon->data &&
        (mon != &youmonst || Upolyd)) { /* polymorphed */
        *poly = mon->data;
        *race = NULL; /* polymorph grants the polyform's race */
        racenum = genus(monsndx(*poly), 0);
        if (racenum)
            *race = &mons[racenum];
    }
}


/* Returns an object slot mask giving all the reasons why the given
   player/monster might have the given property, limited by "reasons", an object
   slot mask (W_EQUIP, INTRINSIC, and ANY_PROPERTY are the most likely values
   here, but you can specify slots individually if you like). */
unsigned
m_has_property(const struct monst *mon, enum youprop property,
               unsigned reasons, boolean even_if_blocked)
{
    /* Monsters can't hallucinate at present */
    if (property == HALLUC && mon != &youmonst)
        return 0;

    /* Simplify property checking by allowing 0 but always returning 0 in this case */
    if (property == NO_PROP)
        return FALSE;

    if (property > LAST_PROP) {
        impossible("Invalid property: %d", property);
        return 0;
    }

    unsigned rv = 0;
    rv = mon->mintrinsic_cache[property];
    if (!(rv & W_MASK(os_cache))) {
        rv = 0;
        const struct permonst *mdat_role = NULL;
        const struct permonst *mdat_race = NULL;
        const struct permonst *mdat_poly = NULL;
        init_permonsts(mon, &mdat_role, &mdat_race, &mdat_poly);

        /* The general case for equipment */
        rv |= mworn_extrinsic(mon, property);

        /* Timed and corpse/etc-granted */
        if (mon->mintrinsic[property] & TIMEOUT_RAW)
            rv |= W_MASK(os_timeout);
        if (mon->mintrinsic[property] & FROMOUTSIDE_RAW)
            rv |= W_MASK(os_outside);

        /* Polyform / role / race properties */
        const struct propxl *pmprop;
        for (pmprop = prop_from_experience; pmprop->mnum != NON_PM;
             pmprop++) {
            if (pmprop->prop == property &&
                pmprop->xl <= (mon == &youmonst ? u.ulevel : mon->m_lev)) {
                if (pmprop->mnum == monsndx(mdat_role))
                    rv |= W_MASK(os_role);
                if (mdat_race && pmprop->mnum == monsndx(mdat_race))
                    rv |= W_MASK(os_race);
                if (mdat_poly && pmprop->mnum == monsndx(mdat_poly))
                    rv |= W_MASK(os_polyform);
            }
        }
        if (pm_has_property(mdat_role, property) > 0)
            rv |= W_MASK(os_role);
        if (mdat_race && pm_has_property(mdat_race, property) > 0)
            rv |= W_MASK(os_race);
        if (mdat_poly && pm_has_property(mdat_poly, property) > 0)
            rv |= W_MASK(os_polyform);

        /* External circumstances */
        /* Fumbling on ice */
        if (property == FUMBLING &&
            is_ice(m_dlevel(mon), m_mx(mon), m_my(mon)) &&
            (mon != &youmonst || !u.usteed) && !flying(mon) &&
            !levitates(mon) && !is_whirly(mon->data)) {
            struct obj *armf = which_armor(mon, os_armf);
            if (!armf || strcmp(OBJ_DESCR(objects[armf->otyp]), "snow boots"))
                rv |= W_MASK(os_circumstance);
        }

        /* Cases specific to the player */
        if (mon == &youmonst) {
            /* Birth options */
            if ((property == BLINDED && flags.permablind) ||
                (property == HALLUC && flags.permahallu) ||
                (property == UNCHANGING && flags.polyinit_mnum != -1))
                rv |= W_MASK(os_birthopt);

            /* External circumstances */
            if (property == BLINDED &&
                (u_helpless(hm_unconscious) || u.ucreamed))
                rv |= W_MASK(os_circumstance);

            /* Riding allows you to inherit a few properties from steeds */
            if ((property == FLYING || property == SWIMMING ||
                 property == WWALKING || property == LEVITATION) &&
                u.usteed && has_property(u.usteed, property))
                rv |= W_MASK(os_saddle);
        }

        /* Overrides

           TODO: Monsters with no eyes are not considered blind. This doesn't
           make much sense. However, changing it would be a major balance
           change (due to Elbereth), and so it has been left alone for now. */
        if ((property == BLINDED && !haseyes(mon->data)) ||
            (property == HALLUC && resists_hallu(mon)) ||
            (property == INVIS && aggravating(mon)) ||
            (property == WWALKING && m_dlevel(mon) && Is_waterlevel(m_mz(mon))) ||
            mworn_blocked(mon, property))
            rv |= (unsigned)(W_MASK(os_blocked));

        if (mon == &youmonst)
            youmonst.mintrinsic_cache[property] = (rv | W_MASK(os_cache));
    }
    rv &= ~W_MASK(os_cache);

    /* If a property is blocked, turn off all flags except circumstance/birthopt,
       unless even_if_blocked is TRUE. Yes, including os_blocked, because not
       doing that would interfere with macros and whatnot. */
    if ((rv & W_MASK(os_blocked)) && !even_if_blocked)
        rv &= (unsigned)(W_MASK(os_circumstance) |
                         W_MASK(os_birthopt));
    return rv & reasons;
}


/* Check if an object/spell/whatever would have any effect on a target */
boolean
obj_affects(const struct monst *user, struct monst *target, struct obj *obj)
{
    int wandlevel;
    switch (obj->otyp) {
    case WAN_FIRE:
    case SPE_FIREBALL:
    case SCR_FIRE:
        return (!prop_wary(user, target, FIRE_RES) ||
                prop_wary(user, target, SLIMED));
    case WAN_COLD:
    case SPE_CONE_OF_COLD:
        return !prop_wary(user, target, COLD_RES);
    case POT_SLEEPING:
        if (!prop_wary(user, target, FREE_ACTION))
            return TRUE;
        /* fallthrough */
    case WAN_SLEEP:
    case SPE_SLEEP:
        return !prop_wary(user, target, SLEEP_RES);
    case WAN_LIGHTNING:
        return !prop_wary(user, target, SHOCK_RES);
    case SCR_STINKING_CLOUD:
        /* technically also cause blindness, but for like 3 turns... */
    case POT_SICKNESS:
        return !prop_wary(user, target, POISON_RES);
    case POT_ACID:
        return !prop_wary(user, target, ACID_RES);
    case SPE_STONE_TO_FLESH:
        return (prop_wary(user, target, STONED) ||
                target->data == &mons[PM_STONE_GOLEM]);
    case EGG:
        /* trice eggs only */
        if (!touch_petrifies(&mons[obj->corpsenm]))
            return FALSE;
        return (!prop_wary(user, target, STONE_RES) ||
                target->data == &mons[PM_FLESH_GOLEM]);
    case WAN_MAKE_INVISIBLE:
        /* skilled users of /oInvis can uninvis */
        wandlevel = 0;
        if (obj->oclass == WAND_CLASS) {
            wandlevel = mprof(user, MP_WANDS);
            if (obj->mbknown)
                wandlevel = getwandlevel(user, obj);
            if (wandlevel >= P_SKILLED &&
                !binvisible(target))
                return TRUE;
        }
        return !prop_wary(user, target, INVIS);
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case POT_POLYMORPH:
        return !(prop_wary(user, target, UNCHANGING) ||
                 prop_wary(user, target, ANTIMAGIC));
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        return !prop_wary(user, target, ANTIMAGIC);
    case WAN_MAGIC_MISSILE:
    case SPE_MAGIC_MISSILE:
        if (!prop_wary(user, target, ANTIMAGIC))
            return TRUE;
        if (!user)
            return FALSE;
        if (obj->otyp == SPE_MAGIC_MISSILE) {
            wandlevel = mprof(user, MP_SATTK);
            if (wandlevel < P_SKILLED)
                return FALSE;
        } else {
            wandlevel = mprof(user, MP_WANDS);
            if (obj->mbknown)
                wandlevel = getwandlevel(user, obj);
        }

        if (wandlevel >= P_SKILLED)
            return TRUE;
        return FALSE;
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
        return !prop_wary(user, target, SLOW);
    case WAN_SPEED_MONSTER:
        /* a monster might not know if a target is fast, but
           if not, he'd find that out rather fast */
        return !very_fast(target);
    case WAN_UNDEAD_TURNING:
    case SPE_TURN_UNDEAD:
        return is_undead(target->data);
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
        return !prop_wary(user, target, CANCELLED);
    case WAN_DEATH:
    case SPE_FINGER_OF_DEATH:
        if (!(prop_wary(user, target, ANTIMAGIC) ||
              is_undead(target->data) ||
              is_demon(target->data)))
            return TRUE;
        if (!user || obj->oclass != WAND_CLASS)
            return FALSE;
        wandlevel = mprof(user, MP_WANDS);
        if (obj->mbknown)
            wandlevel = getwandlevel(user, obj);
        if (wandlevel >= P_EXPERT)
            return !prop_wary(user, target, DRAIN_RES);
        return FALSE;
    case POT_PARALYSIS:
        return !prop_wary(user, target, FREE_ACTION);
    case POT_CONFUSION:
        return !prop_wary(user, target, CONFUSION);
    case POT_BLINDNESS:
        return !prop_wary(user, target, BLINDED);
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
        /* healing/extra healing cures blindness unless selfzapped */
        if ((!user || user != target) &&
            prop_wary(user, target, BLINDED))
            return TRUE;
        if (target == &youmonst && Upolyd)
            return (u.mh < u.mhmax);
        return (m_mhp(target) < m_mhpmax(target));
    case SPE_DRAIN_LIFE:
        return !prop_wary(user, target, DRAIN_RES);
    }
    return TRUE;
}

boolean
prop_wary(const struct monst *mon, struct monst *target, enum youprop prop)
{
    /* If !mon, or for some properties that is always announced,
       or for allies/peacefuls, or for WoY, always be accurate */
    if (!mon ||
        prop == INVIS || /* "the invisible X" */
        prop == AGGRAVATE_MONSTER || /* "seen: aggravate monster" */
        (target == &youmonst && mon->mpeaceful) ||
        (target != &youmonst && mon->mpeaceful == target->mpeaceful) ||
        mon->iswiz || mon == target || mon->mtame)
        return !!m_has_property(target, prop, ANY_PROPERTY, TRUE);
    /* Monsters always know properties gained from those */
    if (m_has_property(target, prop,
                       (W_MASK(os_polyform) | W_MASK(os_birthopt) |
                        W_MASK(os_role) | W_MASK(os_race)), TRUE))
        return TRUE;

    /* avoid monsters trying something futile */
    if (mworn_blocked(target, prop))
        return TRUE;

    /* TODO: make monsters learn properties properly */
    if (rn2(4))
        return !!has_property(target, prop);
    return FALSE;
}

int
property_timeout(struct monst *mon, enum youprop property)
{
    return mon->mintrinsic[property] & TIMEOUT_RAW;
}


void
decrease_property_timers(struct monst *mon)
{
    enum youprop prop;
    int skill = 0;
    skill = (mon == &youmonst ? P_SKILL(P_CLERIC_SPELL) :
             mprof(mon, MP_SCLRC));
    for (prop = 0; prop <= LAST_PROP; prop++) {
        if (mon->mintrinsic[prop] & TIMEOUT_RAW) {
            /* Decrease protection at half speed at Expert and not at all if maintained */
            if (prop == PROTECTION &&
                ((spell_maintained(mon, SPE_PROTECTION) &&
                  cast_protection(mon, FALSE, TRUE)) ||
                 (skill == P_EXPERT && (moves % 2))))
                continue;
            mon->mintrinsic[prop]--;
            update_property(mon, prop, os_dectimeout);
        }
    }
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
    /* FROMOUTSIDE isn't natural */
    if (teleportitis(mon) & (INTRINSIC & ~FROMOUTSIDE))
        return TRUE;
    if (m_mlev(mon) >= 12)
        return TRUE;
    if (m_mlev(mon) >= 8) {
        if (mon == &youmonst && Race_if(PM_WIZARD))
            return TRUE;
        if (mon != &youmonst && spellcaster(mon->data))
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
    unsigned lev_worn = (lev & W_MASKABLE);

    /* polyform */
    if (is_floater(mon->data))
        return (why ? W_MASK(os_polyform) : 0);

    /* uncontrolled intrinsic levitation */
    if ((lev & ~lev_worn) && !(lev & W_MASK(os_outside)))
        return (why ? (lev & ~lev_worn) : 0);

    /* has extrinsic */
    if ((lev & lev_worn) && !include_extrinsic)
        return (why ? lev_worn : 0);

    if (lev_worn) { /* armor/ring/slotless levitation active */
        struct obj *chain = mon->minvent;
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
            return DEADMONSTER(mon) ? 2 : 1;
    }

    /* monster levitation comes from an extrinsic */
    struct obj *chain = mon->minvent;
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
                pline(msgc_monneutral, "%s unsteadily for a moment.",
                      M_verbs(mon, "wobble"));
        }
        return dropped ? 1 : 0;
    }
    
    if (lev_source) {
        if (cansee(mon->mx, mon->my))
            pline(mon->mtame ? msgc_petwarning : msgc_monneutral,
                  "%s crashes to the floor!", Monnam(mon));

        mon->mhp -= rn1(8, 14); /* same as for player with 11 Con */
        if (mon->mhp <= 0) {
            if (cansee(mon->mx, mon->my))
                pline(mon->mtame ? msgc_petfatal : msgc_monneutral,
                      "%s dies!", Monnam(mon));
            else if (mon->mtame)
                pline(msgc_petfatal,
                      "You have a sad feeling for a moment, then it passes.");
            mondied(mon);
        }
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
        prop = rnd(LAST_PROP);
        if (m_has_property(mon, prop, W_MASK(os_outside), TRUE)) {
            set_property(mon, prop, -1, FALSE);
            return;
        }
    }
    if (mon == &youmonst || canseemon(mon))
        pline(combat_msgc(NULL, mon, cr_miss),
              "But nothing happens.");
    return;
}

/* Sets a property.
   type>0: Set a timeout
   type=0: Set os_outside
   type-1: Remove os_outside
   type-2: Remove os_outside and the timer
   forced will bypass update_property(). It is used
   when a special case is needed, and code will have
   to handle the work related to the property itself.
   Note that "set a timeout" literally sets whatever you specify.
   If you want to increase the timeout (potentially from 0),
   use inc_timeout(). */
boolean
set_property(struct monst *mon, enum youprop prop,
             int type, boolean forced)
{
    /* Invalidate property cache */
    mon->mintrinsic_cache[prop] = 0;

    boolean increased = FALSE;
    if (mon->mintrinsic[prop] & TIMEOUT_RAW && type > 0)
        increased = TRUE;

    /* check for redundant usage */
    if (type == 0 && m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -1 && !m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -2 &&
        !m_has_property(mon, prop, (W_MASK(os_outside) | W_MASK(os_timeout)), TRUE))
        return FALSE;

    if (type > 0) { /* set timeout */
        mon->mintrinsic[prop] &= ~TIMEOUT_RAW;
        mon->mintrinsic[prop] |= min(type, TIMEOUT_RAW);
    } else if (type == 0) /* set outside */
        mon->mintrinsic[prop] |= FROMOUTSIDE_RAW;
    else { /* unset outside */
        mon->mintrinsic[prop] &= ~FROMOUTSIDE_RAW;
        if (type == -2) /* ...and timeout */
            mon->mintrinsic[prop] &= ~TIMEOUT_RAW;
    }

    /* Invalidate property cache again (since it's polled in this function) */
    mon->mintrinsic_cache[prop] = 0;

    if (forced)
        return FALSE;

    if (type > 0 || type == -2) {
        if (increased)
            return update_property(mon, prop, os_inctimeout);
        else
            return update_property(mon, prop, os_timeout);
    } else
        return update_property(mon, prop, os_outside);
}

/* Increases a property timeout. Does nothing if the time is zero. */
boolean
inc_timeout(struct monst *mon, enum youprop prop,
            int time, boolean forced)
{
    if (!time)
        return FALSE;

    return set_property(mon, prop,
                        min(time + property_timeout(mon, prop),
                            TIMEOUT_RAW), forced);
}

/* Called on experience level changes */
void
update_xl_properties(struct monst *mon, int oldlevel)
{
    enum objslot slot;
    int newlevel = u.ulevel;
    if (mon != &youmonst)
        newlevel = mon->m_lev;

    const struct permonst *mdat_role = NULL;
    const struct permonst *mdat_race = NULL;
    const struct permonst *mdat_poly = NULL;
    init_permonsts(mon, &mdat_role, &mdat_race, &mdat_poly);

    const struct propxl *pmprop;
    for (pmprop = prop_from_experience; pmprop->mnum != NON_PM;
         pmprop++) {
        slot = os_invalid;
        /* Run update_property() for ones acquired/lost between the
           levels gained/lost. Skip the min level since the hero is
           sure to have that regardless */
        if (pmprop->xl <= max(oldlevel, newlevel) &&
            pmprop->xl > min(oldlevel, newlevel)) {
            if (pmprop->mnum == monsndx(mon->data))
                slot = os_role;
            if (mdat_race && pmprop->mnum == monsndx(mdat_race))
                slot = os_race;
            if (mdat_poly && pmprop->mnum == monsndx(mdat_poly))
                slot = os_polyform;
            if (slot != os_invalid)
                update_property(mon, pmprop->prop, slot);
        }
    }
}

/* Called on polyself to possibly do some extra work for some properties.
   Returns a monster index if that should override the current polymorph
   (used if you polymorph into a golem while petrifying). */
int
update_property_polymorph(struct monst *mon, int old_pm)
{
    int pmcur = monsndx(mon->data);
    enum youprop prop;
    int hasprop, hasprop_poly;
    boolean pm_hasprop, pm_blocks;
    for (prop = 0; prop <= LAST_PROP; prop++) {
        /* Permonst-specific blocks only happen for sliming/petrification, so
           bypassing update checks alltogether if a monster currently blocks is OK */
        if (m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked))
            continue;

        hasprop = has_property(mon, prop);
        hasprop_poly = hasprop & W_MASK(os_polyform);

        pm_hasprop = pm_has_property(&mons[old_pm], prop) > 0;
        pm_blocks = pm_has_property(&mons[old_pm], prop) < 0;
        if ((hasprop && pm_blocks) || /* has property, target blocks */
            (hasprop_poly && !pm_hasprop) || /* has property from polyself only, target lacks */
            (!hasprop && pm_hasprop)) /* lacks property, target has */
            update_property(mon, prop, os_polyform);

        /* polymorphed as a result, bail out since this might no longer be relevant
           (the polymorph, if any happened, will have run this again anyway) */
        if (pmcur != monsndx(mon->data))
            return monsndx(mon->data);
    }
    return 0;
}

/* Called to give any eventual messages and perform checks in case
   e.g. mon lost levitation (drowning), stone res (wielding trice).
   TODO: some of the status problem message logic is a mess, fix it */
boolean
update_property(struct monst *mon, enum youprop prop,
                enum objslot slot)
{
    /* Items call update_property() when lost, whether or not it had a property */
    if (prop == NO_PROP)
        return FALSE;

    /* Invalidate property cache */
    mon->mintrinsic_cache[prop] &= ~W_MASK(os_cache);

    /* update_property() can run for monsters wearing armor during level creation,
       or potentially off-level, so level can be non-existent or outright wrong,
       take this into account when messing with this function */
    boolean offlevel = (!level || level != mon->dlevel);
    boolean vis = !offlevel && canseemon(mon);
    /* Used when the updating is related to monster invisibility
       since canseemon() wont work if the monster just turned
       itself invisible */
    boolean vis_invis = !offlevel && cansee(mon->mx, mon->my);
    /* if slot is inctimeout, point real_slot to timeout */
    int real_slot = (slot == os_inctimeout  ? os_timeout  :
                     slot == os_dectimeout  ? os_timeout  :
                     slot);
    boolean lost = !(has_property(mon, prop) & W_MASK(real_slot));
    boolean blocked;
    blocked = !!(m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked));
    /* Unless this was called as a result of block, check whether or not the
       property was actually lost, to avoid lost being set when gaining new properties
       if blocked. */
    if (lost && blocked && slot != os_blocked) {
        if (m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(real_slot))
            lost = FALSE;
        /* And if this is os_dectimeout, ensure that no pointless messages are printed */
        if (slot == os_dectimeout && !lost)
            return FALSE;
    }
    /* Whether or not a monster has it elsewhere */
    boolean redundant = !!(has_property(mon, prop) & ~W_MASK(real_slot));
    /* make a redundant flag accurate for speed changes... */
    boolean redundant_intrinsic = FALSE;
    if (((W_MASK(real_slot) & INTRINSIC) &&
         (has_property(mon, prop) & INTRINSIC & ~W_MASK(real_slot))) ||
        ((W_MASK(real_slot) & EXTRINSIC) &&
         (has_property(mon, prop) & ~INTRINSIC & ~W_MASK(real_slot))))
        redundant_intrinsic = TRUE;
    /* Special case: set redundant to whether or not the monster has the property
       if we're dealing with (inc|dec)timeout */
    if (slot == os_inctimeout || slot == os_dectimeout) {
        redundant = !!has_property(mon, prop);
        redundant_intrinsic = !!(has_property(mon, prop) & ~INTRINSIC);
    }

    /* Hallu checks *your* hallucination since it's used for special
       messages */
    boolean hallu = hallucinating(&youmonst);
    boolean you = (mon == &youmonst);
    /* if something was said about the situation */
    boolean effect = FALSE;
    boolean was_overprotected = !cast_protection(mon, FALSE, TRUE);
    int timer = property_timeout(mon, prop);
    struct obj *weapon;
    enum msg_channel msgc = msgc_monneutral;

    /* Messages when intrinsics are acquired/lost */
    if (mon == &youmonst &&
        (slot == os_role || slot == os_race ||
         slot == os_polyform || slot == os_outside)) {
        const struct propmsg *msg;
        if (hallu) {
            for (msg = prop_msg_hallu; msg->prop != NO_PROP; msg++) {
                if (msg->prop == prop) {
                    /* the XL-based properties always use "You feel ...!" */
                    if (slot != os_outside)
                        pline(lost ? msgc_intrloss : msgc_intrgain,
                              "You feel %s!",
                              lost ? msg->losepm : msg->gainpm);
                    else
                        pline(lost ? msgc_intrloss : msgc_intrgain, "%s", 
                              lost ? msg->loseoutside : msg->gainoutside);
                    effect = TRUE;
                    break;
                }
            }
        }
        if (!effect) { /* effect means a hallu msg was printed already */
            for (msg = prop_msg; msg->prop != NO_PROP; msg++) {
                if (msg->prop == prop) {
                    if (slot != os_outside)
                        pline(lost ? msgc_intrloss : msgc_intrgain, "You feel %s!",
                              lost ? msg->losepm : msg->gainpm);
                    else if (prop == POISON_RES && !lost && redundant) /* special msg */
                        pline(msgc_intrgain, "You feel especially healthy.");
                    else
                        pline(lost ? msgc_intrloss : msgc_intrgain, "%s", 
                              lost ? msg->loseoutside : msg->gainoutside);
                    effect = TRUE;
                    break;
                }
            }
        }
    }

    /* Additional work for gained/lost properties. Properties are in order, hence
       some redundant breaks. */
    /* TODO: This logic, especially considering os_(inc|dec)timeout could use some
       prettifying */
    switch (prop) {
    case FIRE_RES:
        /* BUG: shouldn't there be a check for lava here?
        if (lost && !redundant) {
        } */
    case COLD_RES:
    case SLEEP_RES:
    case DISINT_RES:
    case SHOCK_RES:
    case POISON_RES:
    case ACID_RES:
        break;
    case STONE_RES:
        weapon = m_mwep(mon);
        if (lost && !redundant && weapon &&
            weapon->otyp == CORPSE &&
            touch_petrifies(&mons[weapon->corpsenm])) {
            if (!you)
                mselftouch(mon, "No longer petrify-resistant, ",
                           find_mid(mon->dlevel, flags.mon_moving,
                                    FM_EVERYWHERE));
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
        } else if (!lost && !redundant)
            set_property(mon, STONED, -2, FALSE);
        break;
    case ADORNED:
        break;
    case REGENERATION:
        if (!redundant && you) {
            pline(lost ? msgc_intrloss : msgc_intrgain,
                  "You feel %sregenerative.", lost ? "less " : "");
            effect = TRUE;
        }
        break;
    case SEARCHING:
        break;
    case SEE_INVIS:
        if (you) {
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters(FALSE);        /* see invisible monsters */
            newsym(u.ux, u.uy);         /* see yourself! */
            if (!redundant && invisible(mon)) {
                pline(lost ? msgc_intrloss : msgc_intrgain,
                      lost ? "Your body seems to fade out." :
                      "You can see yourself, but remain transparent.");
                effect = TRUE;
            }
        }
        break;
    case INVIS:
        if (you) {
            if (!redundant) {
                if (lost)
                    pline(msgc_statusend,
                          "Your body seems to unfade...");
                else
                    pline(msgc_statusgood, "%s %s.",
                          hallu ? "Far out, man!  You" :
                          "Gee!  All of a sudden, you",
                          see_invisible(&youmonst) ?
                          "can see right through yourself" :
                          "can't see yourself");
                effect = TRUE;
            }
            newsym(u.ux, u.uy);
        } else if (!redundant && level && vis_invis) {
            if (see_invisible(&youmonst)) {
                pline(msgc_monneutral,
                      lost ? "%s body seems to unfade..." :
                      "%s body turns transparent!",
                      s_suffix(Monnam(mon)));
            } else {
                /* call x_monnam directly to get rid of "The invisible ..." */
                pline(msgc_monneutral,
                      lost ? "%s appears!" :
                      (msensem(&youmonst, mon) & MSENSE_ANYDETECT) ?
                      "%s disappears, but you can still sense it." :
                      "%s suddenly disappears!",
                      msgupcasefirst(x_monnam(mon, ARTICLE_THE, NULL,
                                              (mx_name(mon) ? SUPPRESS_SADDLE : 0) |
                                              SUPPRESS_IT |
                                              SUPPRESS_INVISIBLE |
                                              SUPPRESS_ENSLAVEMENT,
                                              FALSE)));
                set_mimic_blocking();       /* do special mimic handling */
                see_monsters(FALSE);        /* see invisible monsters */
            }
            effect = TRUE;
        }
        break;
    case TELEPORT:
        if (you)
            update_supernatural_abilities();
        break;
    case TELEPORT_CONTROL:
    case POLYMORPH:
    case POLYMORPH_CONTROL:
        break;
    case LEVITATION:
        if (lost && real_slot == os_timeout)
            set_property(mon, LEVITATION, -1, TRUE);

        /* this isn't really the right place, but there isn't any better place...
           reset levi_wary */
        if (slot != os_dectimeout && !you)
            mon->levi_wary = 0;

        if (!redundant) {
            if (!lost)
                float_up(mon);
            else
                float_down(mon);
            if (vis || you)
                effect = TRUE;
        }
        break;
    case STEALTH:
        if (slot == os_armf && !redundant &&
            !levitates(mon) && !flying(mon)) {
            if (you)
                pline(lost ? msgc_intrloss : msgc_intrgain,
                      lost ? "You sure are noisy." :
                      "You walk very quietly");
            else if (vis)
                pline(msgc_monneutral,
                      lost ? "%s sure is noisy." :
                      "%s walks very quietly.",
                      Monnam(mon));
            effect = TRUE;
        }
        break;
    case AGGRAVATE_MONSTER:
        /* avoid running vision code offlevel */
        if (!you && !redundant && level) {
            you_aggravate(mon);
            see_monsters(FALSE);
        }
        break;
    case CONFLICT:
        /* Monsters should not be causing conflict. Just
           in case it happens anyway, alert the player. */
        if (!you) {
            pline(msgc_levelwarning,
                  lost ? "You feel as if a conflict disappeared." :
                  "You feel as if someone is causing conflict.");
            effect = TRUE;
        }
        break;
    case PROTECTION:
        while (slot == os_dectimeout && !cast_protection(mon, FALSE, TRUE)) {
            if ((mon->mintrinsic[PROTECTION] & TIMEOUT_RAW) > 10)
                mon->mintrinsic[PROTECTION] -= 10;
            else {
                mon->mintrinsic[PROTECTION] &= ~TIMEOUT_RAW;
                break;
            }
        }

        if (slot == os_dectimeout && was_overprotected && (you || vis)) {
            pline(you ? msgc_statusbad : msgc_monneutral,
                  "The %s haze around %s fades rapidly.", hcolor("golden"),
                  mon_nam(mon));
            effect = TRUE;
        }

        if (you && slot == os_armc && !lost) {
            pline(msgc_intrgain,
                  "Your cloak feels unusually protective.");
            effect = TRUE;
        } else if (slot == os_dectimeout && !(timer % 10) &&
                   (you || vis)) {
            pline(you ? msgc_statusend : msgc_monneutral,
                  "The %s haze around %s %s.", hcolor("golden"),
                  you ? "you" : mon_nam(mon),
                  m_mspellprot(mon) ? "becomes less dense" : "disappears");
            effect = TRUE;
        } else if (slot == os_outside && lost) {
            if (you)
                u.ublessed = 0;
        }
        break;
    case PROT_FROM_SHAPE_CHANGERS:
        if (!redundant && lost)
            restartcham();
        else
            resistcham();
        break;
    case WARNING:
    case TELEPAT:
        if (you)
            see_monsters(FALSE);
        break;
    case FAST:
        /* No redundant messages */
        if (mon == &youmonst &&
            (slot == os_role || slot == os_race ||
             slot == os_polyform || slot == os_outside))
            break;

        /* only give the "new energy" message if the monster has redundant speed */
        if (redundant_intrinsic) {
            if (slot == os_inctimeout && you) {
                pline(msgc_hint, "Your %s get new energy.",
                      makeplural(body_part(LEG)));
                effect = TRUE;
            }
            break;
        }
        /* if "redundant" is set at this point, it is pointing
           at speed of the "other" kind (very fast if intrinsic, fast if extrinsic) */

        /* speed boots/objects of speed */
        if (W_MASK(slot) & W_EQUIP) {
            if (you || vis) {
                pline(!you ? msgc_monneutral :
                      lost ? msgc_statusend :
                      msgc_statusgood, "%s %s %s%s.",
                      you ? "You" : Monnam(mon),
                      you ? "feel yourself" : "seems to",
                      lost ? "slow down" : "speed up",
                      redundant && lost ? " slightly" :
                      redundant ? " a bit more" : "");
                effect = TRUE;
            }
            break;
        }

        /* general (non-speed-boots) speed loss */
        if (lost) {
            if (slot == os_outside && redundant) {
                if (you) {
                    pline(msgc_intrloss,
                          "Your quickness feels less natural.");
                    effect = TRUE;
                }
                break;
            }

            if (you || vis) {
                pline(you ? msgc_intrloss : msgc_monneutral,
                      "%s down%s.", M_verbs(mon, "slow"),
                      redundant && (W_MASK(real_slot) & ~INTRINSIC) ? " slightly" : "");
                effect = TRUE;
            }
            break;
        }

        /* intrinsic acquirement */
        if (slot == os_outside) {
            if (!redundant && (you || vis)) {
                pline(msgc_intrgain, "%s up.", M_verbs(mon, "speed"));
                effect = TRUE;
            } else if (you) {
                pline(msgc_intrgain, "Your quickness feels more natural.");
                effect = TRUE;
            }
            break;
        }

        if (real_slot & ~INTRINSIC) {
            if (slot != os_inctimeout && !redundant_intrinsic) {
                if (you || vis) {
                    pline(you ? msgc_statusgood : msgc_monneutral,
                          "%s %s moving %sfaster.",
                          you ? "You" : Monnam(mon),
                          you ? "are suddenly" : "seems to be",
                          redundant ? "" : "much ");
                    effect = TRUE;
                }
            }
        }
        break;
    case STUNNED:
        if (you || vis) {
            if (redundant) {
                if (slot == os_inctimeout) {
                    if (you)
                        pline(msgc_statusbad,
                              hallu ? "You feel like wobbling some more." :
                              "You struggle to keep your balance.");
                    else
                        pline(msgc_monneutral,
                              "%s struggles to keep %s balance.",
                              Monnam(mon), mhis(mon));
                    effect = TRUE;
                }
            } else {
                if (lost)
                    pline(you ? msgc_statusheal : msgc_monneutral,
                          "%s %s %s now.",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks",
                          you && hallu ? "less wobbly" : "a bit steadier");
                else
                    pline(you ? msgc_statusbad : msgc_monneutral,
                          "%s %s%s...",
                          you ? "You" : Monnam(mon),
                          you && hallu ? "wobble" : stagger(mon->data, "stagger"),
                          you ? "" : "s");
                effect = TRUE;
            }
        }
        break;
    case CONFUSION:
        if (you) {
            if (lost && !redundant) {
                pline(msgc_statusheal, hallu ? "You feel less trippy." :
                      "You are no longer confused.");
                effect = TRUE;
            } else if (redundant) {
                if (slot == os_inctimeout) {
                    pline(msgc_statusbad, "You are even more %s",
                          hallu ? "trippy!" : "confused...");
                    effect = TRUE;
                }
            } else {
                pline(msgc_statusbad, hallu ? "What a trippy feeling!" :
                      "Huh, What?  Where am I?");
                effect = TRUE;
            }
        } else if (vis && (!redundant || slot == os_inctimeout)) {
            pline(redundant ? msgc_actionok :
                  msgc_monneutral,
                  redundant ? "%s looks even more confused..." :
                  lost ? "%s looks less confused now." :
                  "%s looks rather confused.", Monnam(mon));
            effect = TRUE;
        }
        break;
    case SICK:
        msgc = msgc_monneutral;

        if (you || mon->mtame) {
            msgc = msgc_statusheal;
            if (!lost)
                msgc = (mon->mtame ? msgc_petfatal :
                        timer ? msgc_fatal : msgc_fatal_predone);
        }
        if (lost && slot == os_dectimeout) {
            if (you || vis) {
                pline(msgc, "%s die%s from %s illness.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      you ? "your" : mhis(mon));
                effect = TRUE;
            }
            if (you)
                done(POISONING, delayed_killer(POISONING));
            else {
                monkilled(mon->usicked ? &youmonst : NULL, mon, "", AD_DISE);
                mon->usicked = 0; /* in case monster lifesaved */
            }
            break;
        }

        if (slot == os_dectimeout)
            break;

        if (you) {
            pline(msgc,
                  redundant ? "You feel much worse." :
                  lost ? "What a relief!" :
                  "You feel deathly sick.");
            effect = TRUE;
        } else if (vis) {
            pline(msgc,
                  redundant ? "%s looks much worse." :
                  lost ? "%s looks relieved." :
                  "%s looks deathly sick.",
                  Monnam(mon));
            effect = TRUE;
        }
        if (lost && !redundant && you) {
            set_delayed_killer(POISONING, NULL);
            u.usick_type = 0;
        }

        if (!you && !redundant)
            mon->usicked = lost || !flags.mon_moving ? 1 : 0;
        break;
    case BLINDED:
        if (slot == os_tool) {
            if (you) {
                if (lost && blocked) {
                    pline(msgc_statusheal, "You can see!");
                    effect = TRUE;
                } else if (!lost && !redundant) {
                    pline(msgc_statusbad,
                          "You can't see any more.");
                    effect = TRUE;
                } else if (lost) {
                    pline(redundant ? msgc_yafm :
                          msgc_statusheal,
                          redundant ? "You still can't see..." :
                          "You can see again.");
                    effect = TRUE;
                }
            }
        } else if (you || vis) {
            if (blocked) {
                if (you) {
                    pline(msgc_playerimmune,
                          "Your vision seems to %s for a moment but is %s now",
                          lost ? "brighten" : "dim",
                          hallu ?
                          (lost ? "sadder" : "happier") :
                          "normal");
                    effect = TRUE;
                } else if (!lost) {
                    pline(combat_msgc(NULL, mon, cr_immune),
                          "%s is briefly blinded.", Monnam(mon));
                    effect = TRUE;
                }
            } else if (redundant) {
                if (you && (slot != os_dectimeout ||
                            lost)) {
                    eyepline(lost ? "twitches" : "itches",
                             lost ? "twitch" : "itch");
                    effect = TRUE;
                }
            } else {
                if (you)
                    pline(lost ? msgc_statusheal : msgc_statusbad,
                          lost && hallu ? "Far out!  A light show!" :
                          lost ? "You can see again." :
                          hallu ? "Oh, bummer!  Everything is dark! Help!" :
                          "A cloud of darkness falls upon you.");
                else
                    pline(msgc_monneutral, "%s %s.", Monnam(mon),
                          lost ? "can see again" : "is blinded");
                effect = TRUE;
            }
        }
        turnstate.vision_full_recalc = TRUE;
        see_monsters(FALSE);
        break;
    case SLEEPING: /* actually restful sleep */
        if (!lost && !redundant && real_slot != os_timeout) {
            set_property(mon, prop, rnd(100), TRUE);
            break;
        }

        /* Kill the timer if the property was fully lost */
        if (lost && !(has_property(mon, prop) & ~TIMEOUT))
            set_property(mon, prop, -2, TRUE);

        if (lost && slot == os_dectimeout) {
            int sleeptime = 0;
            if (!resists_sleep(mon) &&
                ((you && !u_helpless(hm_unconscious)) ||
                 (!you && mon->mcanmove)))
                sleeptime = rnd(20);
            if (sleeptime) {
                if (you || vis) {
                    pline(you ? msgc_statusbad :
                          mon->mtame ? msgc_petwarning :
                          msgc_monneutral, "%s asleep.", M_verbs(mon, "fall"));
                    effect = TRUE;
                }
                sleep_monst(NULL, mon, sleeptime, -1);
            }

            if (restful_sleep(mon))
                set_property(mon, prop, sleeptime + rnd(100), TRUE);
        }
        break;
    case LWOUNDED_LEGS:
        if (lost && !redundant) {
            /* if this is from dec_timeout and RWOUNDED_LEGS timer is 1, heal
               both legs with a single message */
            if (slot == os_dectimeout &&
                property_timeout(mon, RWOUNDED_LEGS) == 1 &&
                !(leg_hurtr(mon) & ~TIMEOUT))
                heal_legs(mon, BOTH_SIDES);
            else
                heal_legs(mon, LEFT_SIDE);
        }
        break;
    case RWOUNDED_LEGS:
        if (lost && !redundant)
            heal_legs(mon, RIGHT_SIDE);
        break;
    case STONED:
        if (you)
            msgc = timer ? msgc_fatal : msgc_fatal_predone;
        else if (mon->mtame)
            msgc = msgc_petfatal;
        if (lost && slot != os_dectimeout) {
            /* Check for golem change first */
            if (blocked && poly_when_stoned(mon->data)) {
                if (you)
                    polymon(PM_STONE_GOLEM, TRUE);
                else
                    newcham(mon, &mons[PM_STONE_GOLEM], FALSE, FALSE);
                set_property(mon, prop, -2, TRUE);
                if (!you)
                    mon->ustoned = 0;
                else
                    set_delayed_killer(STONING, NULL);
            }
            if (you || vis) {
                if (hallu)
                    pline(you ? msgc_fatalavoid : msgc_monneutral,
                          "What a pity - %s just ruined a piece of %sart!",
                          you ? "you" : mon_nam(mon),
                          acurr(mon, A_CHA) > 15 ? "fine " : "");
                else
                    pline(you ? msgc_fatalavoid : msgc_monneutral,
                          "%s %s more limber!",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (!you)
                    mon->ustoned = 0;
                else
                    set_delayed_killer(STONING, NULL);
                effect = TRUE;
            }
        } else {
            if (you || vis) {
                switch (timer) {
                case 4:
                    pline(msgc, "%s slowing down.",
                          M_verbs(mon, "are"));
                    break;
                case 3:
                    pline(msgc, "%s limbs are stiffening.",
                          s_suffix(Monnam(mon)));
                    break;
                case 2:
                    pline(msgc, "%s limbs have turned to stone.",
                          s_suffix(Monnam(mon)));
                    break;
                case 1:
                    pline(msgc, "%s turned to stone.",
                          M_verbs(mon, "have"));
                    break;
                case 0:
                default:
                    pline(msgc, "%s a statue.", M_verbs(mon, "are"));
                    break;
                }
                effect = TRUE;
            }
            /* remove intrinsic speed, even if mon re-acquired it */
            set_property(mon, FAST, -1, TRUE);
            if (you)
                exercise(A_DEX, FALSE);

            if (lost) { /* petrified */
                if (you)
                    done(STONING, delayed_killer(STONING));
                else
                    monstone(mon);
            } else if (timer == 5) {
                if (!you && !flags.mon_moving)
                    mon->ustoned = 1;
            } else if (timer <= 2) {
                if (you)
                    helpless(3, hr_paralyzed, "unable to move due to turning to stone",
                             NULL);
                else {
                    mon->mcanmove = 0;
                    mon->mfrozen = timer + 1;
                }
            }
        }
        break;
    case STRANGLED:
        if (you)
            msgc = (lost && slot == os_dectimeout ?
                    msgc_fatal_predone : msgc_fatal);
        else if (mon->mtame)
            msgc = msgc_petfatal;
        if (lost && slot != os_dectimeout) {
            if (you || vis) { /* TODO: give a suitable message if unbreathing */
                pline(you ? msgc_fatalavoid : msgc_monneutral,
                      "%s can breathe more easily!",
                      you ? "You" : Monnam(mon));
                effect = TRUE;
            }
            /* unset the timer, in case the loss was from removing the amulet */
            set_property(mon, prop, -2, TRUE);
            break;
        }

        if (!lost && !redundant && slot != os_dectimeout) {
            if (you || vis)
                pline(msgc, "It constricts %s throat!",
                      you ? "your" : s_suffix(mon_nam(mon)));
            set_property(&youmonst, STRANGLED, 5, TRUE);
        }

        if (you)
            exercise(A_STR, FALSE);

        if (lost) {
            if (you || vis)
                pline(msgc, "%s suffocate%s.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
            effect = TRUE;
            if (you)
                done(SUFFOCATION, killer_msg(SUFFOCATION,
                                             u.uburied ? "suffocation" : "strangulation"));
            else
                mondied(mon);
            break;
        }

        if (you || vis) {
            if (unbreathing(mon) || !rn2(50)) {
                if (timer == 4)
                    pline(msgc, "%s %s is becoming constricted.",
                          you ? "Your" : s_suffix(Monnam(mon)),
                          mbodypart(mon, NECK));
                else if (timer == 3)
                    pline(msgc, "%s blood is having trouble reaching %s brain.",
                          you ? "Your" : s_suffix(Monnam(mon)),
                          you ? "your" : s_suffix(mon_nam(mon)));
                else if (timer == 2)
                    pline(msgc, "The pressure on %s %s increases.",
                          you ? "your" : s_suffix(mon_nam(mon)),
                          mbodypart(mon, NECK));
                else if (timer == 1)
                    pline(msgc, "%s consciousness is fading.",
                          you ? "Your" : s_suffix(Monnam(mon)));
            } else {
                if (timer == 4)
                    pline(msgc, "%s it hard to breathe.", M_verbs(mon, "find"));
                else if (timer == 3)
                    pline(msgc, "%s gasping for air.", M_verbs(mon, "are"));
                else if (timer == 2)
                    pline(msgc, "%s can no longer breathe.",
                          you ? "You" : Monnam(mon));
                else if (timer == 1)
                    pline(msgc, "%s turning %s.", M_verbs(mon, "are"),
                          hcolor("blue"));
            }
            effect = TRUE;
        }
        break;
    case HALLUC:
        if (you) {
            if (lost && blocked)
                pline(msgc_statusheal,
                      "Your vision seems to %s for a moment but is %s now",
                      "flatten", "normal");
            else if (lost && blind(mon))
                eyepline("itches", "itch");
            else if (lost)
                pline(msgc_statusheal,
                      "Everything looks SO boring now.");
            else if (!redundant)
                pline(msgc_statusbad,
                      "Oh wow!  Everything %s so cosmic!",
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
        /* If we gained the property and it wasn't due to a timer,
           set fumbling timeout to rnd(20) */
        if (!lost && !redundant && real_slot != os_timeout) {
            set_property(mon, prop, rnd(20), TRUE);
            break;
        }

        /* Kill the timer if the property was fully lost
           (This is redundant, but fine, if slot was os_dectimeout) */
        if (lost && !(has_property(mon, prop) & ~TIMEOUT))
            set_property(mon, prop, -2, TRUE);

        if (lost && slot == os_dectimeout) {
            /* canmove+not eating is close enough to umoved */
            if ((((you || mon == u.usteed) && u.umoved) ||
                 (!you && mon != u.usteed && mon->mcanmove &&
                  !mon->meating)) &&
                !levitates(mon)) {
                effect = slip_or_trip(mon);
                if (you)
                    helpless(2, hr_moving, "fumbling", "");
                else {
                    mon->mcanmove = 0;
                    mon->mfrozen = 2;
                }
            }

            /* os_circumstance is ice, don't restart fumble timer */
            if (fumbling(mon) & ~W_MASK(os_circumstance))
                set_property(mon, FUMBLING, rnd(20), FALSE);
        }
        break;
    case JUMPING:
        if (you && slot == os_armf) {
            pline(lost ? msgc_intrloss : msgc_intrgain,
                  "Your %s feel %s.", makeplural(body_part(LEG)),
                  lost ? "shorter" : "longer");
            effect = TRUE;
        }
        break;
    case WWALKING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        else
            minliquid(mon);
        break;
    case HUNGER:
        break;
    case GLIB:
        if (blocked) /* no message, just remove timers */
            set_property(mon, prop, -2, TRUE);
        else if (you && lost && !redundant) {
            pline(msgc_statusheal, "Your %s feels less slippery",
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
            pline(lost ? msgc_intrloss : msgc_intrgain,
                  lost ? "You stop shimmering" :
                  "Your outline shimmers and shifts");
            effect = TRUE;
        }
        break;
    case CLAIRVOYANT:
        if (slot == os_dectimeout && !(timer % 15)) {
            if (you && !blocked) {
                do_vicinity_map();
                effect = TRUE;
            }
        }

        /* If we still have clairvoyance, increase the timer
           if timer is 0. */
        if (!(timer % 15)) {
            if (m_has_property(mon, prop, ANY_PROPERTY, TRUE)) {
                set_property(mon, prop, max(timer, 15), TRUE);
            }
        }
        break;
    case VOMITING:
        if (you)
            msgc = msgc_statusbad;
        if (lost && slot != os_dectimeout) {
            if (you || vis) {
                pline(you ? msgc_statusheal : msgc_monneutral,
                      "%s %s much less nauseated now.",
                      you ? "You" : Monnam(mon),
                      you ? "feel" : "looks");
                effect = TRUE;
            }
        } else if (slot == os_dectimeout) {
            if (you || vis) {
                if (timer == 14)
                    pline(msgc, "%s %s mildly nauseated.",
                          you ? "You" : Monnam(mon),
                          you ? "are feeling" : "looks");
                if (timer == 11)
                    pline(msgc, "%s %s slightly confused.",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (timer == 8)
                    pline(msgc, "%s can't seem to think straight.",
                          you ? "You" : Monnam(mon));
                if (timer == 5)
                    pline(msgc, "%s %s incredibly sick.",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (timer == 2)
                    pline(msgc, "%s suddenly vomit%s!",
                          you ? "You" : Monnam(mon),
                          you ? "" : "s");
                if ((timer % 3) == 2)
                    effect = TRUE;
            }

            if (you)
                exercise(A_CON, FALSE);

            if (timer == 11)
                inc_timeout(mon, CONFUSION, dice(2, 4), TRUE);
            if (timer == 8)
                inc_timeout(mon, STUNNED, dice(2, 4), TRUE);
            if (timer == 2)
                vomit(mon);
        }
        break;
    case ENERGY_REGENERATION:
    case MAGICAL_BREATHING:
    case HALF_SPDAM:
    case HALF_PHDAM:
        break;
    case SICK_RES:
        set_property(mon, SICK, -2, FALSE);
        set_property(mon, ZOMBIE, -2, FALSE);
        break;
    case DRAIN_RES:
        break;
    case WARN_UNDEAD:
        if (you)
            see_monsters(FALSE);
        break;
    case CANCELLED:
    case FREE_ACTION:
        break;
    case SWIMMING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case SLIMED:
        if (you)
            msgc = timer ? msgc_fatal : msgc_fatal_predone;
        else if (mon->mtame)
            msgc = msgc_petfatal;
        if (lost && slot != os_dectimeout) {
            if (blocked) { /* lost by poly */
                if (flaming(mon->data))
                    burn_away_slime(mon);
            } else if (monsndx(mon->data) != PM_GREEN_SLIME &&
                       (you || vis)) {
                pline(you ? msgc_fatalavoid : msgc_monneutral,
                      "The slime on %s disappears!",
                      you ? "you" : mon_nam(mon));
                effect = TRUE;
            }
            /* remove stray timers */
            set_property(mon, SLIMED, -2, TRUE);
            if (!you)
                mon->uslimed = 0;
            else
                set_delayed_killer(TURNED_SLIME, NULL);
        } else {
            /* if slimes are genocided at any point during this process, immediately remove sliming */
            if (mvitals[PM_GREEN_SLIME].mvflags & G_GENOD)
                return set_property(mon, SLIMED, -2, FALSE);

            int idx = rndmonidx();
            const char *turninto = (hallu ? (monnam_is_pname(idx)
                                             ? monnam_for_index(idx)
                                             : (idx < SPECIAL_PM &&
                                                (mons[idx].geno & G_UNIQ))
                                             ? the(monnam_for_index(idx))
                                             : an(monnam_for_index(idx)))
                                    : "a green slime");

            if (you)
                exercise(A_DEX, FALSE);

            if (you || vis) {
                if (timer == 9)
                    pline(msgc, "%s %s %s very well.",
                          you ? "You" : Monnam(mon),
                          you ? "don't" : "doesn't",
                          you ? "feel" : "look");
                else if (timer == 8)
                    pline(msgc, "%s %s turning a %s %s.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is",
                          is_green(mon) && !hallu ? "more vivid shade of" : "little",
                          hcolor("green"));
                else if (timer == 6)
                    pline(msgc, "%s limbs are getting oozy.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 4)
                    pline(msgc, "%s skin begins to peel away.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 2)
                    pline(msgc, "%s %s turning into %s.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is",
                          turninto);
                else if (timer == 0)
                    pline(msgc, "%s become %s.", M_verbs(mon, "have"),
                          turninto);
                effect = TRUE;
            }

            /* remove intrinsic speed at "oozy", even if mon re-acquired it */
            if (timer <= 6)
                set_property(mon, FAST, -1, TRUE);

            if (timer == 10) {
                if (!you && !flags.mon_moving)
                    mon->uslimed = 1;
            } else if (!timer) {
                if (you)
                    done(TURNED_SLIME, delayed_killer(TURNED_SLIME));
                else {
                    newcham(mon, &mons[PM_GREEN_SLIME], FALSE, FALSE);
                    set_property(mon, SLIMED, -2, TRUE);
                }
            }
        }
        break;
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
        if (!redundant) {
            if (you && u.utraptype == TT_PIT) {
                u.utraptype = 0;
                u.utrap = 0;
                turnstate.vision_full_recalc = TRUE;
            }

            if (you && slot == os_timeout) {
                pline(lost ? msgc_statusbad : msgc_statusgood, "You feel %s.",
                      lost ? "more solid" : "etheral");
                effect = TRUE;
            }
        }
        break;
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
                    pline(msgc_failcurse, "You feel lonely");
                effect = TRUE; /* either lonely or detected stuff */
            }
        }
        break;
    case SLOW:
        if (you && !redundant) {
            pline(msgc_statusheal, lost ? "Your speed returns." :
                  "You feel abnormally slow.");
            effect = TRUE;
        } else if (vis && !redundant) {
            pline(msgc_monneutral, lost ? "%s speeds up." :
                  "%s slows down abnormally.",
                  Monnam(mon));
            effect = TRUE;
        }
        break;
    case ZOMBIE:
        if (slot == os_timeout) {
            if (nonliving(mon->data) || izombie(mon)) {
                /* random polymorphs, etc */
                if (izombie(mon)) {
                    set_property(mon, ZOMBIE, -2, TRUE);
                    set_property(mon, ZOMBIE, 0, TRUE);
                } else
                    set_property(mon, ZOMBIE, -2, TRUE);
                mon->uzombied = FALSE;
                if (mon == &youmonst)
                    set_delayed_killer(TURNED_ZOMBIE, NULL);
                break;
            }
            if (lost) {
                if (you || vis) {
                    pline(you ? msgc_statusheal : msgc_monneutral,
                          "%s zombifying disease wears off.",
                          s_suffix(Monnam(mon)));
                    effect = TRUE;
                }
                break;
            }
            if (you || vis) {
                pline(you && timer > 40 ? msgc_statusbad :
                      you ? msgc_fatal :
                      mon->mtame ? msgc_petfatal :
                      msgc_monneutral,
                      "%s a %s zombification disease%s",
                      M_verbs(mon, "attain"), timer > 40 ? "minor" : "major",
                      timer > 40 ? "." : "!");
                effect = TRUE;
            }
            break;
        }
        if (slot == os_inctimeout) { /* actually a decrease from being hit */
            if (you || vis) {
                pline(you && timer > 40 ? msgc_statusbad :
                      you ? msgc_fatal :
                      mon->mtame ? msgc_petfatal :
                      msgc_monneutral,
                      "%s zombification disease builds up%s",
                      s_suffix(Monnam(mon)),
                      timer > 40 ? "." : " critically!");
                effect = TRUE;
            }
            break;
        }
        if (slot == os_dectimeout) {
            if (timer == 40) {
                if (you || vis)
                    pline(you ? msgc_fatal :
                          mon->mtame ? msgc_petfatal :
                          msgc_monneutral,
                          "%s zombification disease turns terminal!",
                          s_suffix(Monnam(mon)));
                effect = TRUE;
                break;
            }
            if (timer > 40 && !rn2(10)) {
                timer += 100;
                if (timer > 200)
                    set_property(mon, prop, -2, FALSE);
                else
                    inc_timeout(mon, prop, 100, TRUE);
                break;
            }
            if (lost) { /* turned */
                int mndx = NON_PM;
                if (is_human(mon->data))
                    mndx = PM_HUMAN_ZOMBIE;
                if (is_elf(mon->data))
                    mndx = PM_ELF_ZOMBIE;
                if (is_orc(mon->data))
                    mndx = PM_ORC_ZOMBIE;
                if (is_gnome(mon->data))
                    mndx = PM_GNOME_ZOMBIE;
                if (is_dwarf(mon->data))
                    mndx = PM_DWARF_ZOMBIE;
                if (is_giant(mon->data))
                    mndx = PM_GIANT_ZOMBIE;
                if (mon->data == &mons[PM_ETTIN])
                    mndx = PM_ETTIN_ZOMBIE;
                if (mon->data->mlet == S_KOBOLD)
                    mndx = PM_KOBOLD_ZOMBIE;
                if (!you && mndx != NON_PM && mon->data != &mons[mndx] &&
                    !newcham(mon, &mons[mndx], FALSE, FALSE)) {
                    if (vis) {
                        pline(mon->mtame ? msgc_petfatal : msgc_monneutral,
                              "%s from the zombification and is destroyed!",
                              M_verbs(mon, "shudder"));
                        effect = TRUE;
                    }
                    monkilled(NULL, mon, "", -AD_ZOMB);
                    break;
                }
                if (you || (vis && mndx != NON_PM)) {
                    pline(you ? msgc_fatal_predone :
                          mon->mtame ? msgc_petfatal : msgc_monneutral,
                          "%s into a zombie...", M_verbs(mon, "turn"));
                    effect = TRUE;
                } else if (vis) {
                    pline(mon->mtame ? msgc_petfatal :
                          msgc_monneutral,
                          "%s into an instrument of undead...", M_verbs(mon, "turn"));
                    effect = TRUE;
                }
                if (you)
                    done(TURNED_ZOMBIE, delayed_killer(TURNED_ZOMBIE));
                else {
                    mon->mtame = mon->mpeaceful = 0;
                    set_property(mon, ZOMBIE, 0, TRUE);
                }
            }
        }
        break;
    case WATERPROOF:
        break;
    default:
        impossible("Unknown property: %u", prop);
        break;
    }

    /* If a property timed out, interrupt the player. */
    if (you && lost && slot == os_dectimeout && effect)
        action_interrupted();

    return effect;
}

static boolean
is_green(struct monst *mon)
{
    if (mon->data == &mons[PM_GREMLIN] || mon->data == &mons[PM_LEPRECHAUN] ||
        /* Are wood nymphs green? */
        mon->data == &mons[PM_BABY_GREEN_DRAGON] ||
        mon->data == &mons[PM_GREEN_DRAGON] ||
        /* Are NetHack's lichens green?  Some real lichens are, some not.
           What about guardian nagas and their hatchlings?  Their default
           representation is green, but that's also true of hobbits, among
           other things... */
        mon->data == &mons[PM_GARTER_SNAKE] || /* usually green and black */
        mon->data == &mons[PM_GREEN_MOLD] ||
        /* Green elves are not green; etymology is they live in forests. */
        mon->data == &mons[PM_GECKO] || mon->data == &mons[PM_IGUANA] ||
        /* Lizards come in a variety of colors.  What about crocodiles?
           I grew up thinking crocodiles and aligators are green, but
           Google Images seems to suggest they're more often gray/brown. */
        mon->data == &mons[PM_MEDUSA] || mon->data == &mons[PM_JUIBLEX] ||
        mon->data == &mons[PM_NEFERET_THE_GREEN] ||
        mon->data == &mons[PM_GREEN_SLIME]) {
        return TRUE;
    }
    return FALSE;
}

/* give a fumble message */
static boolean
slip_or_trip(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct obj *otmp = vobj_at(m_mx(mon), m_my(mon));
    const char *what, *pronoun;
    boolean on_foot = TRUE;
    int pctload = 0;
    if (!you)
        pctload = (curr_mon_load(mon) * 100) / max_mon_load(mon);
    else
        pctload = (inv_weight_total() * 100) / weight_cap();

    if (!you && !vis) {
        if (pctload > 50 && canhear())
            pline(msgc_levelwarning, "You hear fumbling %s.",
                  dist2(u.ux, u.uy, mon->mx, mon->my) > BOLT_LIM * BOLT_LIM ?
                  "in the distance" : "nearby");
        mwake_nearby(mon, FALSE);
        return FALSE; /* can't see the target anyway */
    }

    enum msg_channel msgc = you ? msgc_statusbad : msgc_monneutral;
    if ((you && u.usteed) || flying(mon) || levitates(mon))
        on_foot = FALSE;

    if (otmp && on_foot &&
        ((you && !u.uinwater) || (!you && waterwalks(mon))) &&
        is_pool(level, m_mx(mon), m_my(mon)))
        otmp = 0;

    if (otmp && on_foot) {      /* trip over something in particular */
        /* If there is only one item, it will have just been named during the
           move, so refer to by via pronoun; otherwise, if the top item has
           been or can be seen, refer to it by name; if not, look for rocks to
           trip over; trip over anonymous "something" if there aren't any
           rocks. */
        pronoun = otmp->quan == 1L ? "it" : Hallucination ? "they" : "them";
        what = !otmp->nexthere ? pronoun :
            (otmp->dknown || !Blind) ? doname(otmp) :
            ((otmp = sobj_at(ROCK, level, m_mx(mon), m_my(mon))) == 0 ?
             "something" : otmp-> quan == 1L ? "a rock" : "some rocks");
        if (Hallucination)
            pline(msgc, "Egads!  %s bite%s %s %s!", msgupcasefirst(what),
                  (!otmp || otmp->quan == 1L) ? "s" : "",
                  you ? "your" : s_suffix(mon_nam(mon)),
                  body_part(FOOT));
        else
            pline(msgc, "%s trip%s over %s.", you ? "You" : Monnam(mon),
                  you ? "" : "s", what);
    } else if (rn2(3) && is_ice(level, m_mx(mon), m_my(mon)) &&
               on_foot)
        pline(msgc, "%s %s%s on the ice.",
              you ? "You" : Monnam(mon),
              rn2(2) ? "slip" : "slide",
              you ? "" : "s");
    else {
        if (on_foot) {
            switch (rn2(4)) {
            case 1:
                pline(msgc, "%s trip%s over %s own %s.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s", you ? "your" : mhis(mon),
                      Hallucination ? "elbow" : makeplural(body_part(FOOT)));
                break;
            case 2:
                pline(msgc, "%s slip%s %s.", you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      Hallucination ? "on a banana peel" :
                      you ? "and nearly fall" :
                      "and nearly falls");
                break;
            case 3:
                pline(msgc, "%s flounder%s.", you ? "You" : Monnam(mon),
                      you ? "" : "s");
                break;
            default:
                pline(msgc, "%s stumble%s.", you ? "You" : Monnam(mon),
                      you ? "" : "s");
                break;
            }
        } else if (you && u.usteed) {
            switch (rn2(4)) {
            case 1:
                pline(msgc, "Your %s slip out of the stirrups.",
                      makeplural(body_part(FOOT)));
                break;
            case 2:
                pline(msgc, "You let go of the reins.");
                break;
            case 3:
                pline(msgc, "You bang into the saddle-horn.");
                break;
            default:
                pline(msgc, "You slide to one side of the saddle.");
                break;
            }
            dismount_steed(DISMOUNT_FELL);
        } else {
            /* tripping in the air */
            switch (rn2(3)) {
            case 1:
                pline(msgc, "%s in place.", M_verbs(mon, "tumble"));
                break;
            case 2:
                pline(msgc, "%s %s balance!", M_verbs(mon, "lose"),
                      you ? "your" : mhis(mon));
                break;
            default:
                pline(msgc, "%s a hard time controlling %s movement.",
                      M_verbs(mon, "have"), you ? "your" : mhis(mon));
                break;
            }
        }
    }
    if (pctload > 50) {
        pline(msgc_levelwarning, "%s make%s a lot of noise!",
              you ? "You" : Monnam(mon), you ? "" : "s");
        mwake_nearby(mon, FALSE);
    }
    return TRUE;
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
    /* TODO: checking deadmonster() breaks death messages and some other
       related things, but simply removing the checks probably breaks
       other things, figure out how to deal with this */
    if (viewer != &youmonst)
        if (!onmap(viewer) || DEADMONSTER(viewer))
            return 0;
    if (viewee != &youmonst)
        if (!onmap(viewee) || DEADMONSTER(viewee))
            return 0;
    if (!level) {
        impossible("vision calculations during level creation: %s->%s",
                   k_monnam(viewer), k_monnam(viewee));
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
        tx = m_mx(viewee), ty = m_my(viewee),
        dx = viewee->dx, dy = viewee->dy;

    int distance = dist2(sx, sy, tx, ty);
    int distance_displaced = (displaced(viewee) ? dist2(sx, sy, dx, dy) : 500);

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
    /* Equavilent, but for a monster's displaced image if relevant */
    boolean loe_displaced = (displaced(viewee) &&
                             clear_path(sx, sy, dx, dy, msensem_vizarray));

    /* A special case for many vision methods: water or the ground blocking
       vision. A hiding target is also included in these, because hiding is
       often a case of using an object, part of the floor, a cranny in the
       ceiling, etc., to block vision (and even when it isn't, it should block
       vision in the same cases). */
    boolean vertical_loe =
        !(m_mburied(viewer) || m_mburied(viewee) ||
          ((!!m_underwater(viewee)) ^ (!!m_underwater(viewer))) ||
          m_mundetected(viewee));

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
    boolean target_lit_displaced = FALSE;
    if (displaced(viewee))
        target_lit_displaced = (distance_displaced <= 2 ||
                                (lev == level && templit(dx, dy)) ||
                                lev->locations[dx][dy].lit);

    /* TODO: Maybe infravisibility should be a property? */
    boolean infravision_ok = infravision(viewer) &&
        pm_infravisible(viewee->data);

    boolean blinded = !!blind(viewer);
    boolean see_invisible = !!see_invisible(viewer);

    if (loe && vertical_loe && !blinded) {
        if (!invisible && target_lit) {
            sensemethod |= MSENSE_VISION;
            if (loe_displaced && target_lit_displaced)
                sensemethod |= MSENSE_DISPLACED;
        }
        if (!invisible && infravision_ok) {
            sensemethod |= MSENSE_INFRAVISION;
            if (loe_displaced)
                sensemethod |= MSENSE_DISPLACED;
        }
        if (invisible && (target_lit || infravision_ok) && see_invisible) {
            sensemethod |= MSENSE_SEEINVIS | MSENSEF_KNOWNINVIS;
            if (loe_displaced && (target_lit_displaced || infravision_ok))
                sensemethod |= MSENSE_DISPLACED;
        }
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
    if (vertical_loe && distance <= XRAY_RANGE * XRAY_RANGE && xray) {
        sensemethod |= MSENSE_XRAY;
        if (distance_displaced <= XRAY_RANGE * XRAY_RANGE && xray)
            sensemethod |= MSENSE_DISPLACED;
        if (invisible && see_invisible)
            sensemethod |= MSENSEF_KNOWNINVIS;
    }

    /* Ideally scent should work around corners, but not through walls. That's
       awkward to write, though, because it'd require pathfinding. */
    if (vertical_loe && loe && distance <= 5 && has_scent(viewer->data))
        sensemethod |= MSENSE_SCENT;

    /* Monster detection. All that is needed (apart from same-level, which was
       checked earlier) is the property itself. */
    if (detects_monsters(viewer) || (viewer->mtame && viewee == &youmonst))
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
    if (viewer->data == &mons[PM_XORN] && money_cnt(viewee->minvent))
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
        mm_aggression(viewee, viewer) & ALLOW_M) {
        sensemethod |= MSENSE_WARNING;
    }

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

    /* Displacement is set unconditionally where relevant for normal vision methods
       earlier. However, if the viewer sense the method through other vision methods,
       we need to disable the displacement flag since it only affects monsters who
       sense another with only normal/invis/infra/xray and where the displaced image
       isn't on the monster itself */
    if ((sensemethod & MSENSE_DISPLACED) &&
        ((sensemethod & ~(MSENSE_ANYVISION | MSENSE_DISPLACED |
                          MSENSEF_KNOWNINVIS)) ||
         (dx == m_mx(viewer) && dy == m_my(viewer)))) {
        sensemethod &= ~MSENSE_DISPLACED;
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
        if (u.ualign.record >= AR_PIOUS)
            mon_is(&menu, mon, "piously aligned");
        else if (u.ualign.record >= AR_DEVOUT)
            mon_is(&menu, mon, "devoutly aligned");
        else if (u.ualign.record >= AR_FERVENT)
            mon_is(&menu, mon, "fervently aligned");
        else if (u.ualign.record >= AR_STRIDENT)
            mon_is(&menu, mon, "stridently aligned");
        else if (u.ualign.record == AR_OK)
            mon_is(&menu, mon, "aligned");
        else if (u.ualign.record >= AR_HALTING)
            mon_is(&menu, mon, "haltingly aligned");
        else if (u.ualign.record == AR_NOMINAL)
            mon_is(&menu, mon, "nominally aligned");
        else if (u.ualign.record <= AR_TRANSGRESSED)
            mon_has(&menu, mon, "transgressed");
        else if (u.ualign.record <= AR_SINNED)
            mon_has(&menu, mon, "sinned");
        else if (u.ualign.record <= AR_STRAYED)
            mon_has(&menu, mon, "strayed");
        else
            impossible("Unknown alignment threshould?");
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
    if (waterproof(mon))
        mon_is(&menu, mon, "protected from water");
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
    if (cancelled(mon))
        mon_is(&menu, mon, "cancelled");
    if (slippery_fingers(mon))
        mon_has(&menu, mon, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (fumbling(mon))
        mon_x(&menu, mon, "fumble");
    if (leg_hurt(mon))
        mon_has(&menu, mon, msgcat("wounded", makeplural(body_part(LEG))));;
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
    else if (binvisible(mon))
        mon_is(&menu, mon, "visible");
    if (displacement(mon))
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
    if (hitbon(mon))
        mon_has(&menu, mon, enlght_combatinc("to hit", hitbon(mon), final));
    if (dambon(mon))
        mon_has(&menu, mon, enlght_combatinc("damage", dambon(mon), final));
    if (slow_digestion(mon))
        mon_has(&menu, mon, "slower digestion");
    if (regenerates(mon))
        mon_x(&menu, mon, "regenerate");
    if (protected(mon) || protbon(mon)) {
        int prot = protbon(mon);
        if (mon == &youmonst)
            prot += u.ublessed;
        prot += m_mspellprot(mon);

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
    const char *buf;
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
        buf = msgprintf(" %d", u.uhunger);
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
    if (waterproof(&youmonst))
        you_are(&menu, "protected from water");
    if (u.uinvulnerable)
        you_are(&menu, "invulnerable");
    if (u.uedibility)
        you_can(&menu, "recognize detrimental food");

        /*** Troubles ***/
    if (Halluc_resistance)
        enl_msg(&menu, "You resist", "", "ed", " hallucinations");
    if (final) {
        if (hallucinating(&youmonst))
            you_are(&menu, "hallucinating");
        if (stunned(&youmonst))
            you_are(&menu, "stunned");
        if (confused(&youmonst))
            you_are(&menu, "confused");
        if (blind(&youmonst))
            you_are(&menu, "blinded");
        if (sick(&youmonst)) {
            if (u.usick_type & SICK_VOMITABLE)
                you_are(&menu, "sick from food poisoning");
            if (u.usick_type & SICK_NONVOMITABLE)
                you_are(&menu, "sick from illness");
        }
    }
    if (petrifying(&youmonst))
        you_are(&menu, "turning to stone");
    if (sliming(&youmonst))
        you_are(&menu, "turning into slime");
    if (strangled(&youmonst))
        you_are(&menu, (u.uburied) ? "buried" : "being strangled");
    if (cancelled(&youmonst))
        you_are(&menu, "cancelled");
    if (slippery_fingers(&youmonst))
        you_have(&menu, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (fumbling(&youmonst))
        enl_msg(&menu, "You fumble", "", "d", "");
    if (leg_hurt(&youmonst))
        you_have(&menu, msgcat("wounded ", makeplural(body_part(LEG))));;
    if (restful_sleep(&youmonst))
        enl_msg(&menu, "You ", "fall", "fell", " asleep");
    if (hunger(&youmonst))
        enl_msg(&menu, "You hunger", "", "ed", " rapidly");

        /*** Vision and senses ***/
    if (see_invisible(&youmonst))
        enl_msg(&menu, You_, "see", "saw", " invisible");
    if (telepathic(&youmonst))
        you_are(&menu, "telepathic");
    if (warned(&youmonst))
        you_are(&menu, "warned");
    if (warned_of_mon(&youmonst)) {
        int warntype = worn_warntype();
        buf = msgcat("aware of the presence of ",
                     (warntype & M2_ORC) ? "orcs" :
                     (warntype & M2_DEMON) ? "demons" : "something");
        you_are(&menu, buf);
    }
    if (warned_of_undead(&youmonst))
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
    else if (binvisible(&youmonst))
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
    if (Passes_walls)
        you_can(&menu, "walk through walls");

    /* FIXME: This is printed even if you die in a riding accident. */
    if (u.usteed)
        you_are(&menu, msgcat("riding ", y_monnam(u.usteed)));
    if (Engulfed)
        you_are(&menu, msgcat("swallowed by ", a_monnam(u.ustuck)));
    else if (u.ustuck) {
        buf = msgprintf(
            "%s %s", (Upolyd && sticks(youmonst.data)) ? "holding" : "held by",
            a_monnam(u.ustuck));
        you_are(&menu, buf);
    }

        /*** Physical attributes ***/
    if (hitbon(&youmonst))
        you_have(&menu, enlght_combatinc("to hit", hitbon(&youmonst), final));
    if (dambon(&youmonst))
        you_have(&menu, enlght_combatinc("damage", dambon(&youmonst), final));
    if (Slow_digestion)
        you_have(&menu, "slower digestion");
    if (Regeneration)
        enl_msg(&menu, "You regenerate", "", "d", "");
    if (protected(&youmonst) || protbon(&youmonst)) {
        int prot = protbon(&youmonst);
        prot += m_mspellprot(&youmonst);
        prot += u.ublessed;

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
    if (fast(&youmonst))
        you_are(&menu, very_fast(&youmonst) ? "very fast" : "fast");
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

#define addmenu(x,y) if (ihas_property(&youmonst, x))   \
                          add_menutext(&menu, y);

    /* Resistances */
    addmenu(FIRE_RES, "You are fire resistant.");
    addmenu(COLD_RES, "You are cold resistant.");
    addmenu(SLEEP_RES, "You are sleep resistant.");
    addmenu(DISINT_RES, "You are disintegration resistant.");
    addmenu(SHOCK_RES, "You are shock resistant.");
    addmenu(POISON_RES, "You are poison resistant.");
    addmenu(ACID_RES, "You are acid resistant.");
    addmenu(DRAIN_RES, "You are level-drain resistant.");
    addmenu(SICK_RES, "You are immune to sickness.");
    addmenu(SEE_INVIS, "You see invisible.");
    addmenu(TELEPAT, "You are telepathic.");
    addmenu(WARNING, "You are warned.");
    addmenu(SEARCHING, "You have automatic searching.");
    addmenu(INFRAVISION, "You have infravision.");

    /* Appearance, behaviour */
    if (invisible(&youmonst) & INTRINSIC) {
        if (see_invisible(&youmonst))
            add_menutext(&menu, "You are invisible to others.");
        else
            add_menutext(&menu, "You are invisible.");
    }

    addmenu(STEALTH, "You are stealthy.");
    addmenu(AGGRAVATE_MONSTER, "You aggravate monsters.");
    addmenu(CONFLICT, "You cause conflict.");
    addmenu(JUMPING, "You can jump.");
    addmenu(TELEPORT, "You can teleport.");
    addmenu(TELEPORT_CONTROL, "You have teleport control.");
    addmenu(SWIMMING, "You can swim.");
    addmenu(MAGICAL_BREATHING, "You can survive without air.");
    addmenu(PROTECTION, "You are protected.");
    addmenu(POLYMORPH, "You are polymorphing.");
    addmenu(POLYMORPH_CONTROL, "You have polymorph control.");
    addmenu(FAST, "You are fast.");

#undef addmenu

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
        enl_msg(&menu, You_, "", "", buf);
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
