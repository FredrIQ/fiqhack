/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-23 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2014 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artilist.h"
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
    {STONE_RES, "You feel extraordinarily limber.", "limber",
     "You feel stiff.", "stiff"},
    {SEARCHING, "", "perceptive", "", "unfocused"},
    {SEE_INVIS, "", "aware",
     "You thought you saw something!", "unaware"},
    {INVIS, "You feel hidden!", "hidden",
     "You feel paranoid.", "paranoid"},
    {TELEPORT, "You feel very jumpy.", "jumpy",
     "You feel less jumpy.", "less jumpy"},
    {TELEPORT_CONTROL, "You feel in control of yourself.", "controlled",
     "You feel less in control.", "uncontrolled"},
    {POLYMORPH, "Your body begins to shapeshift.", "shapeshifting",
     "You are no longer shapeshifting.", "less shapeshifting"},
    {POLYMORPH_CONTROL, "You feel in control of your shapeshifting",
     "shapeshift-controlled", "You feel no lnger in control of your shapeshifting.",
     "less shapeshift-controlled"},
    {STEALTH, "", "stealthy", "", "noisy"},
    {AGGRAVATE_MONSTER, "You feel attractive!", "attractive",
     "You feel less attractive.", "less attractive"},
    {WARNING, "", "sensitive", "", "insensitive"},
    {TELEPAT, "You feel a strange mental acuity.", "telepathic",
     "Your senses fail!", "untelepathic"},
    {FAST, "", "quick", "", "slow"},
    {SLEEPING, "You feel drowsy.", "drowsy", "You feel awake.", "awake"},
    {WWALKING, "You feel light on your feet.", "light",
     "You feel heavier.", "heavy"},
    {HUNGER, "You feel your metabolism speed up.", "hungry",
     "Your metabolism slows down.", "full."},
    {REFLECTING, "Your body feels repulsive.", "repulsive",
     "You feel less repulsive.", "absorptive"},
    {LIFESAVED, "You feel a strange sense of immortality.", "immortal",
     "You lose your sense of immortality!", "mortal"},
    {ANTIMAGIC, "You feel resistant to magic.", "skeptical",
     "Your magic resistance fails!", "credulous"},
    {DISPLACED, "Your outline shimmers and shifts.", "elusive",
     "You stop shimmering.", "exposed"},
    {SICK_RES, "You feel your immunity strengthen.", "immunized",
     "Your immunity system fails!", "immunocompromised"},
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
    {INFRAVISION, "", "vision-enhanced", "", "half blind"},
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
    if (property == BLINDED           ? !haseyes(mdat)                       :
        property == SLIMED            ? flaming(mdat) || unsolid(mdat) ||
                                        mdat == &mons[PM_GREEN_SLIME]        :
        property == STONED            ? poly_when_stoned(mdat)               :
        property == GLIB              ? nohands(mdat)                        :
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

    unsigned rv = 0;
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
            pmprop->xl <= (mon == &youmonst ? youmonst.m_lev : mon->m_lev)) {
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
        if (property == BLINDED && u_helpless(hm_unconscious))
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
    if ((property == HALLUC && resists_hallu(mon)) ||
        (property == WWALKING && Is_waterlevel(m_mz(mon))) ||
        mworn_blocked(mon, property))
        rv |= (unsigned)(W_MASK(os_blocked));

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
        prop == DISPLACED || /* seen: displacement */
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
            /* Decrease protection less at Expert and not at all if maintained,
               unless we are currently overprotected as a result */
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
   Any monster who has reached XL12 or more can teleport at will if they have
   teleportitis. If the monster has teleportitis in their natural form, they can always
   teleport at will. If the monster is a wizard, they can teleport at will from XL8 with
   teleportitis. */
boolean
teleport_at_will(const struct monst *mon)
{
    if (!teleportitis(mon))
        return FALSE;
    /* FROMOUTSIDE isn't natural */
    if (teleportitis(mon) & (INTRINSIC & ~FROMOUTSIDE))
        return TRUE;
    if (mon->m_lev >= 12)
        return TRUE;
    if (mon->m_lev >= 8) {
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
        struct obj *chain = m_minvent(mon);
        long itemtype;
        
        while (chain) {
            /* worn item or slotless unremoveable item */
            itemtype = item_provides_extrinsic(chain, LEVITATION);
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
    struct obj *chain = m_minvent(mon);
    long itemtype;
    int slot;
    boolean dropped; /* Monsters can drop several items in a single turn,
                        but if it drops any items, it can't do stuff
                        beyond that */
    while (chain) {
        itemtype = item_provides_extrinsic(chain, LEVITATION);
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
    int newlevel = youmonst.m_lev;
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
update_property_polymorph(struct monst *mon, int pm)
{
    int pmcur = monsndx(mon->data);
    enum youprop prop;
    boolean hasprop, hasprop_poly, pm_hasprop, pm_blocks;
    for (prop = 0; prop <= LAST_PROP; prop++) {
        /* Permonst-specific blocks only happen for sliming/petrification, so
           bypassing update checks alltogether if a monster currently blocks is OK */
        if (m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked))
            continue;

        hasprop = !!has_property(mon, prop);
        hasprop_poly = hasprop && !(has_property(mon, prop) & ~W_MASK(os_polyform));
        pm_hasprop = pm_has_property(&mons[pm], prop) > 0;
        pm_blocks = pm_has_property(&mons[pm], prop) < 0;
        if ((hasprop && pm_blocks) || /* has property, target blocks */
            (hasprop_poly && !pm_hasprop) || /* has property @polyself, target lacks */
            (!hasprop && pm_hasprop)) /* lacks property, target has */
            update_property(mon, prop, os_newpolyform);

        /* polymorphed as a result, bail out since this might no longer be relevant
           (the polymorph, if any happened, will have run this again anyway) */
        if (pmcur != monsndx(mon->data))
            return monsndx(mon->data);
    }
    return 0;
}

/* Called to give any eventual messages and perform checks in case
   e.g. mon lost levitation (drowning), stone res (wielding trice).
   TODO: this function is rather fragile */
boolean
update_property(struct monst *mon, enum youprop prop,
                enum objslot slot)
{
    /* Items call update_property() when lost, whether or not it had a property */
    if (prop == NO_PROP)
        return FALSE;

    /* update_property() can run for monsters wearing armor during level creation,
       or potentially off-level, so level can be non-existent or outright wrong,
       take this into account when messing with this function */
    boolean offlevel = (!level || level != mon->dlevel);
    boolean vis = !offlevel && canseemon(mon);
    /* Used when the updating is related to monster invisibility
       since canseemon() wont work if the monster just turned
       itself invisible */
    boolean vis_invis = !offlevel && cansee(mon->mx, mon->my);
    /* if slot is inctimeout or newpolyform, point real_slot to
       timeout or polyform respectively -- new* is to give proper messages */
    int real_slot = (slot == os_inctimeout  ? os_timeout  :
                     slot == os_dectimeout  ? os_timeout  :
                     slot == os_newpolyform ? os_polyform :
                     slot);
    boolean lost = !(has_property(mon, prop) & W_MASK(real_slot));
    boolean blocked;
    blocked = !!(m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked));
    /* Unless this was called as a result of newpolyform/block, check whether or not the
       property was actually lost, to avoid lost being set when gaining new properties
       if blocked. */
    if (lost && blocked && slot != os_newpolyform && slot != os_blocked) {
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
        if (mon == &youmonst)
            spoteffects(FALSE);
        else
            minliquid(mon);
        break;
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
                           !flags.mon_moving ? &youmonst : NULL);
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
    case REGENERATION:
    case SEARCHING:
        break;
    case SEE_INVIS:
        if (you) {
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters(FALSE);        /* see invisible monsters */
            newsym(youmonst.mx, youmonst.my);         /* see yourself! */
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
            newsym(youmonst.mx, youmonst.my);
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
        if (you && slot == os_armc && !lost) {
            pline(msgc_intrgain,
                  "Your cloak feels unusually protective.");
            effect = TRUE;
        } else if (slot == os_dectimeout && !(timer % 10) &&
                   (you || vis)) {
            pline(you ? msgc_statusend : msgc_monneutral,
                  "The %s haze around %s %s.", hcolor("golden"),
                  you ? "you" : mon_nam(mon),
                  spellprot(mon) ? "becomes less dense" : "disappears");
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
            mon->usicked = lost || !flags.mon_moving ? 0 : 1;
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
                if (timer == 4)
                    pline(msgc, "%s slowing down.",
                          M_verbs(mon, "are"));
                else if (timer == 3)
                    pline(msgc, "%s limbs are stiffening.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 2)
                    pline(msgc, "%s limbs have turned to stone.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 1)
                    pline(msgc, "%s turned to stone.",
                          M_verbs(mon, "are"));
                else if (timer == 0)
                    pline(msgc, "%s a statue.", M_verbs(mon, "are"));
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
                done(SUFFOCATION, killer_msg(SUFFOCATION, u.uburied ? "suffocation" :
                                             "strangulation"));
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
            spoteffects(FALSE);
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
            spoteffects(FALSE);
        else
            minliquid(mon);
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
            /* if slimes are genocided at any point during this process, immediately
               remove sliming */
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
            spoteffects(FALSE);
        else
            minliquid(mon);
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
            pline(lost ? msgc_statusheal : msgc_statusbad,
                  lost ? "Your speed returns." : "You feel abnormally slow.");
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
    case CREAMED:
        if ((!you && !vis) || (slot == os_dectimeout))
            break;

        if (lost && !redundant) {
            pline(msgc_statusheal, "%s %s %s cleaner.", s_suffix(Monnam(mon)),
                  mbodypart(mon, FACE), you ? "feels" : "looks");

            /* Hack: blindness to give proper blindness messages */
            if (!property_timeout(mon, BLINDED)) {
                set_property(mon, BLINDED, 1, TRUE);
                set_property(mon, BLINDED, -2, FALSE);
            }
            break;
        }

        if (!lost)
            pline(msgc_statusbad, "Yeech!  %s been creamed.", M_verbs(mon, "have"));
    default:
        impossible("Unknown property: %u", prop);
        break;
    }
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
        pctload = (inv_weight() * 100) / weight_cap();

    if (!you && !vis) {
        if (pctload > 50 && canhear())
            pline(msgc_levelwarning, "You hear fumbling %s.",
                  dist2(youmonst.mx, youmonst.my, mon->mx, mon->my) >
                  BOLT_LIM * BOLT_LIM ? "in the distance" : "nearby");
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
    if ((viewer != &youmonst && !viewer->dlevel) ||
        (viewee != &youmonst && !viewee->dlevel))
        panic("msensem: viewer or target has no dlevel, migrating? %s->%s",
              viewer->data ? k_monnam(viewer) : "<zeromonst?>",
              viewee->data ? k_monnam(viewee) : "<zeromonst?>");

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
    /* 500 is arbitrary, it prevents oddities in xray vision */
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
          viewee->mundetected);

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
    if (vertical_loe && distance <= XRAY_RANGE * XRAY_RANGE && xray &&
        (target_lit || infravision_ok)) {
        sensemethod |= MSENSE_XRAY;
        if (distance_displaced <= XRAY_RANGE * XRAY_RANGE && xray &&
            (target_lit_displaced || infravision_ok))
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
    if (detects_monsters(viewer))
        sensemethod |= MSENSE_MONDETECT;

    /* Warning versus monster class. (Actually implemented as monster
       /race/.) */
    if (monwarn_affects(viewer, viewee))
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
    if (distance <= 100 && viewee->m_lev >= 4 &&
        warned(viewer) &&
        mm_aggression(viewee, viewer, Conflict) & ALLOW_M) {
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


/* Struct to handle warning strings */
struct mon_warn_info {
    enum mon_matchtyp matchtyp;
    unsigned match;
    const char *matchstr;
};

static const struct mon_warn_info monwarn_str[] = {
    {MTYP_ALIGN, AM_LAWFUL, "lawful monsters"},
    {MTYP_ALIGN, AM_NEUTRAL, "neutral monsters"},
    {MTYP_ALIGN, AM_CHAOTIC, "chaotic monsters"},
    {MTYP_ALIGN, AM_UNALIGNED, "unaligned monsters"},
    {MTYP_S, S_ANT, "insects"},
    {MTYP_S, S_BLOB, "blobs"},
    {MTYP_S, S_COCKATRICE, "trices"},
    {MTYP_S, S_DOG, "canines"},
    {MTYP_S, S_EYE, "eyes, spheres"},
    {MTYP_S, S_FELINE, "felines"},
    {MTYP_S, S_GREMLIN, "gremlins, gargoyles"},
    {MTYP_S, S_HUMANOID, "humanoids"},
    {MTYP_S, S_IMP, "minor demons"},
    {MTYP_S, S_JELLY, "jellies"},
    {MTYP_S, S_KOBOLD, "kobolds"},
    {MTYP_S, S_LEPRECHAUN, "leprechauns"},
    {MTYP_S, S_MIMIC, "mimics"},
    {MTYP_S, S_NYMPH, "nymphs"},
    {MTYP_S, S_ORC, "orcs"},
    {MTYP_S, S_PIERCER, "piercers"},
    {MTYP_S, S_QUADRUPED, "quadrupeds"},
    {MTYP_S, S_RODENT, "rodents"},
    {MTYP_S, S_SPIDER, "spiders"},
    {MTYP_S, S_TRAPPER, "trappers, lurker aboves"},
    {MTYP_S, S_UNICORN, "horses, unicorns"},
    {MTYP_S, S_VORTEX, "vortices"},
    {MTYP_S, S_WORM, "worms"},
    {MTYP_S, S_XAN, "fantastical insects"},
    {MTYP_S, S_LIGHT, "lights"},
    {MTYP_S, S_ZRUTY, "zruties"},
    {MTYP_S, S_ANGEL, "angelic beings"},
    {MTYP_S, S_BAT, "bats"},
    {MTYP_S, S_CENTAUR, "centaurs"},
    {MTYP_S, S_DRAGON, "dragons"},
    {MTYP_S, S_ELEMENTAL, "stalkers, elementals"},
    {MTYP_S, S_FUNGUS, "fungi"},
    {MTYP_S, S_GNOME, "gnomes"},
    {MTYP_S, S_GIANT, "giant humanoids"},
    {MTYP_S, S_JABBERWOCK, "jabberwocks"},
    {MTYP_S, S_KOP, "Keystone Kops"},
    {MTYP_S, S_LICH, "liches"},
    {MTYP_S, S_MUMMY, "mummies"},
    {MTYP_S, S_NAGA, "nagas"},
    {MTYP_S, S_OGRE, "ogres"},
    {MTYP_S, S_PUDDING, "puddings, green slimes"},
    {MTYP_S, S_QUANTMECH, "quantum mechanics"},
    {MTYP_S, S_RUSTMONST, "erosive monsters"},
    {MTYP_S, S_SNAKE, "snakes"},
    {MTYP_S, S_TROLL, "trolls"},
    {MTYP_S, S_UMBER, "umber hulks"},
    {MTYP_S, S_VAMPIRE, "vampires"},
    {MTYP_S, S_WRAITH, "ghosts, wraiths"},
    {MTYP_S, S_XORN, "xorns"},
    {MTYP_S, S_YETI, "yetis"},
    {MTYP_S, S_ZOMBIE, "zombies"},
    {MTYP_S, S_HUMAN, "elves, humans"},
    {MTYP_S, S_GOLEM, "golems"},
    {MTYP_S, S_DEMON, "major demons"},
    {MTYP_S, S_EEL, "water dwellers"},
    {MTYP_S, S_LIZARD, "lizards"},
    {MTYP_S, S_WORM_TAIL, "worm tails"},
    {MTYP_S, S_MIMIC_DEF, "mimic symbols"},
    {MTYP_M1, M1_FLY, "flying monsters"},
    {MTYP_M1, M1_SWIM, "swimmers"},
    {MTYP_M1, M1_AMORPHOUS, "amorphous monsters"},
    {MTYP_M1, M1_WALLWALK, "phasers"},
    {MTYP_M1, M1_CLING, "clingers"},
    {MTYP_M1, M1_TUNNEL, "tunnelers"},
    {MTYP_M1, M1_NEEDPICK, "diggers"},
    {MTYP_M1, M1_CONCEAL, "item hiders"},
    {MTYP_M1, M1_HIDE, "dungeon hiders"},
    {MTYP_M1, M1_AMPHIBIOUS, "amphibious monsters"},
    {MTYP_M1, M1_BREATHLESS, "unbreathing monsters"},
    {MTYP_M1, M1_NOTAKE, "monsters that don't pickup items"},
    {MTYP_M1, M1_NOEYES, "monsters who lack eyes"},
    {MTYP_M1, M1_NOHANDS, "monsters without hands"},
    {MTYP_M1, M1_NOLIMBS, "monsters without limbs"},
    {MTYP_M1, M1_NOHEAD, "monsters without heads"},
    {MTYP_M1, M1_MINDLESS, "mindless monsters"},
    {MTYP_M1, M1_HUMANOID, "humanoids"},
    {MTYP_M1, M1_ANIMAL, "animals"},
    {MTYP_M1, M1_SLITHY, "slithy monsters"},
    {MTYP_M1, M1_UNSOLID, "incorporeal monsters"},
    {MTYP_M1, M1_THICK_HIDE, "thick-skinned monsters"},
    {MTYP_M1, M1_OVIPAROUS, "oviparous monsters"},
    {MTYP_M1, M1_REGEN, "monsters that regenerate"},
    {MTYP_M1, M1_SEE_INVIS, "monsters that see invisible"},
    {MTYP_M1, M1_TPORT, "teleporting monsters"},
    {MTYP_M1, M1_TPORT_CNTRL, "monsters with teleport control"},
    {MTYP_M1, M1_ACID, "acidic monsters"},
    {MTYP_M1, M1_POIS, "poisonous monsters"},
    {MTYP_M1, M1_CARNIVORE, "carnivores"},
    {MTYP_M1, M1_HERBIVORE, "herbivores"},
    {MTYP_M1, M1_METALLIVORE, "metallivores"},
    {MTYP_M2, M2_NOPOLY, "monsters that isn't a valid polymorph form"},
    {MTYP_M2, M2_UNDEAD, "undead"},
    {MTYP_M2, M2_WERE, "werecreatures"},
    {MTYP_M2, M2_HUMAN, "humans"},
    {MTYP_M2, M2_ELF, "elves"},
    {MTYP_M2, M2_DWARF, "dwarves"},
    {MTYP_M2, M2_GNOME, "gnomes"},
    {MTYP_M2, M2_ORC, "orcs"},
    {MTYP_M2, M2_DEMON, "demons"},
    {MTYP_M2, M2_MERC, "mercenaries"},
    {MTYP_M2, M2_LORD, "lords"},
    {MTYP_M2, M2_PRINCE, "princes"},
    {MTYP_M2, M2_MINION, "minions"},
    {MTYP_M2, M2_GIANT, "giants"},
    {MTYP_M2, M2_TELEPATHIC, "telepathic monsters"},
    {MTYP_M2, M2_STUNNED, "permanently stunned monsters"},
    {MTYP_M2, M2_MALE, "always-males"},
    {MTYP_M2, M2_FEMALE, "always-females"},
    {MTYP_M2, M2_NEUTER, "genderless monsters"},
    {MTYP_M2, M2_PNAME, "monsters whose name is proper"}, /* ??? */
    {MTYP_M2, M2_HOSTILE, "always-hostiles"},
    {MTYP_M2, M2_PEACEFUL, "always-peacefuls"},
    {MTYP_M2, M2_DOMESTIC, "domestic animals"},
    {MTYP_M2, M2_WANDER, "wanderers"},
    {MTYP_M2, M2_STALK, "followers"},
    {MTYP_M2, M2_NASTY, "extra nasty monsters"},
    {MTYP_M2, M2_STRONG, "strong monsters"},
    {MTYP_M2, M2_ROCKTHROW, "boulder throwers"},
    {MTYP_M2, M2_GREEDY, "greedy monsters"},
    {MTYP_M2, M2_JEWELS, "gem-lovers"},
    {MTYP_M2, M2_COLLECT, "monsters that pickup equipment and food"},
    {MTYP_M2, M2_MAGIC, "monsters that pickup magical items"},
    {MTYP_M3, M3_WANTSAMUL, "monsters who covet the Amulet of Yendor"},
    {MTYP_M3, M3_WANTSBELL, "monsters who covet the Bell of Opening"},
    {MTYP_M3, M3_WANTSBOOK, "monsters who covet the Book of the Dead"},
    {MTYP_M3, M3_WANTSCAND, "monsters who covet the Candelabrum of Invocation"},
    {MTYP_M3, M3_WANTSARTI, "monsters who covet your quest artifact"},
    {MTYP_M3, M3_COVETOUS, "covetous monsters"},
    {MTYP_M3, M3_WAITFORU, "monsters that idle until you are in sight"},
    {MTYP_M3, M3_CLOSE, "monsters that idle until you are next to them"},
    {MTYP_M3, M3_WAITMASK, "meditating monsters"},
    {MTYP_M3, M3_INFRAVISION, "monsters with infravision"},
    {MTYP_M3, M3_INFRAVISIBLE, "infravisible monsters"},
    {MTYP_M3, M3_SCENT, "monsters with extraordinary smell"},
    {MTYP_M3, M3_SPELLCASTER, "spellcasters"},
    {MTYP_M3, M3_DISPLACED, "permanently displaced monsters"},
    {MTYP_M3, M3_JUMPS, "monsters that can jump"},
    {MTYP_M3, M3_STEALTHY, "stealthy monsters"},
    {MTYP_M3, M3_FAST, "permanently fast monsters"},
    {MTYP_M3, M3_SEARCH, "monsters with automatic searching"},
    {MTYP_ALL, 1, "everything"},
    {MTYP_ALL, 0, "nothing"},
    {MTYP_ALL, -1, "terminator"},
};

static boolean
set_monwarn_vars(const struct monst *mon, const struct monst *target,
                 int *pm, int *mlet,
                 unsigned *mflags1, unsigned *mflags2, unsigned *mflags3,
                 unsigned *malignmask, int *globals)
{
    if (program_state.restoring_binary_save)
        return FALSE; /* chain might not be linked yet */

    struct obj *obj;
    for (obj = m_minvent(mon); obj; obj = obj->nobj) {
        if (!obj->oartifact || !item_provides_extrinsic(obj, WARN_OF_MON))
            continue;

        const struct artifact *oart = &artilist[(int)obj->oartifact];
        switch (oart->mtype.matchtyp) {
        case MTYP_ALL:
            if (oart->mtype.match == 1)
                *globals = 1;
            break;
        case MTYP_PM:
            pm[oart->mtype.match] = 1;
            break;
        case MTYP_S:
            mlet[oart->mtype.match] = 1;
            break;
        case MTYP_M1:
            *mflags1 |= oart->mtype.match;
            break;
        case MTYP_M2:
            *mflags2 |= oart->mtype.match;
            break;
        case MTYP_M3:
            *mflags3 |= oart->mtype.match;
            break;
        case MTYP_ALIGN:
            *malignmask |= oart->mtype.match;
            break;
        default:
            break;
        }
    }
    if (target && target->data) {
        const struct permonst *tdat = target->data;
        int has_pm_target = pm[monsndx(tdat)];
        memset(pm, 0, NUMMONS);
        pm[monsndx(tdat)] = has_pm_target;
        int has_mlet_target = mlet[tdat->mlet];
        memset(mlet, 0, MAXMCLASSES);
        mlet[tdat->mlet] = has_mlet_target;
        *mflags1 &= ~(tdat->mflags1);
        *mflags2 &= ~(tdat->mflags2);
        *mflags3 &= ~(tdat->mflags3);
        *malignmask &= ~Align2amask(malign(target));
    }
    return TRUE;
}

boolean
monwarn_affects(const struct monst *viewer, const struct monst *viewee)
{
    int pm[NUMMONS];
    int mlet[MAXMCLASSES];
    memset(&pm, 0, sizeof (pm));
    memset(&mlet, 0, sizeof (mlet));

    unsigned mflags1 = 0;
    unsigned mflags2 = 0;
    unsigned mflags3 = 0;
    unsigned malignmask = 0;
    int globals = 0;
    if (!set_monwarn_vars(viewer, viewee, pm, mlet, &mflags1, &mflags2, &mflags3,
                          &malignmask, &globals))
        panic("warning check attempted during binary save restore?");

    if (globals == 1 ||
        pm[monsndx(viewee->data)] ||
        mlet[viewee->data->mlet] ||
        (mflags1 & viewee->data->mflags1) ||
        (mflags2 & viewee->data->mflags2) ||
        (mflags3 & viewee->data->mflags3) ||
        (malignmask & Align2amask(malign(viewee))))
        return TRUE;
    return FALSE;
}

/* Returns a warning string. If target is non-NULL, filters the warnings to ones that
   affect target. */
const char *
get_monwarnstr(const struct monst *mon, const struct monst *target)
{
    int pm[NUMMONS];
    int mlet[MAXMCLASSES];
    memset(&pm, 0, sizeof (pm));
    memset(&mlet, 0, sizeof (mlet));

    unsigned mflags1 = 0;
    unsigned mflags2 = 0;
    unsigned mflags3 = 0;
    unsigned malignmask = 0;
    int globals = 0;
    if (!set_monwarn_vars(mon, target, pm, mlet, &mflags1, &mflags2, &mflags3,
                          &malignmask, &globals))
        panic("warning string attempted during binary save restore?");

    char outbuf[BUFSZ];
    char *outbufp = outbuf;

    outbuf[0] = '\0';

    /* General warning targets */
    const struct mon_warn_info *warnstr;
    for (warnstr = monwarn_str; warnstr->match != -1; warnstr++) {
        switch (warnstr->matchtyp) {
        case MTYP_ALL:
            if (globals && globals == warnstr->match)
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
            break;
        case MTYP_ALIGN:
            if (malignmask & warnstr->match)
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
            break;
        case MTYP_S:
            if (mlet[warnstr->match])
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
            break;
        case MTYP_M1:
            if (mflags1 & warnstr->match)
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
            break;
        case MTYP_M2:
            /* Some S_ types makes certain flags here redundant, so check for those */
            if ((mflags2 & warnstr->match) &&
                (warnstr->match != M2_HUMAN || !mlet[S_HUMAN]) &&
                (warnstr->match != M2_ELF || !mlet[S_HUMAN]) &&
                (warnstr->match != M2_DWARF || !mlet[S_HUMANOID]) &&
                (warnstr->match != M2_GNOME || !mlet[S_GNOME]) &&
                (warnstr->match != M2_ORC || !mlet[S_ORC]) &&
                (warnstr->match != M2_GIANT || !mlet[S_GIANT]))
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
            break;
        case MTYP_M3:
            /* Covetous flags and meditating flags might be redundant */
            if ((mflags3 & warnstr->match) &&
                ((warnstr->match != M3_WANTSAMUL && warnstr->match != M3_WANTSBELL &&
                  warnstr->match != M3_WANTSBOOK && warnstr->match != M3_WANTSCAND &&
                  warnstr->match != M3_WANTSARTI) ||
                 (mflags3 & M3_COVETOUS) != M3_COVETOUS) &&
                ((warnstr->match != M3_WAITFORU && warnstr->match != M3_CLOSE) ||
                 (mflags3 & M3_WAITMASK) != M3_WAITMASK))
                append_str_comma(outbuf, &outbufp, warnstr->matchstr);
        default:
            break;
        }
    }

    /* Specific warning targets (single permonsts) */
    int i;
    for (i = 0; i < NUMMONS; i++) {
        if (pm[i] && !mlet[mons[i].mlet])
            append_str_comma(outbuf, &outbufp, warnstr->matchstr);
    }

    return msg_from_string(outbuf);
}

/* Enlightenment and conduct */

static void
eline(struct nh_menulist *menu, const char *format, ...)
{
    va_list args;
    const char *buf;

    va_start(args, format);
    buf = msgvprintf(format, args, TRUE);
    va_end(args);
    add_menutext(menu, buf);
}

/* format increased damage or chance to hit */
static const char *
enl_combatinc(const char *inctyp, int incamt, int final)
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

/* Adds a menu entry for the given property if the monster has it.
   If show_source is nonzero, also tell the source of the property.
   If the property given is negative, give the message if the monster
   would have the property, but it is blocked in some way. */
static void
add_property_eline(struct nh_menulist *menu, int show_source, const struct monst *mon,
                   int proparg, const char *who, const char *what)
{
    boolean blocked = 0;
    int prop = proparg;
    if (prop < 0) {
        blocked = 1;
        prop = -prop;
    }

    /* The monster doesn't actually have the property, or blocked is checked and the
       property is either not blocked, or the block is dormant */
    if ((!blocked && !has_property(mon, prop)) ||
        (blocked &&
         (!bhas_property(mon, prop) ||
          !(m_has_property(mon, prop, ANY_PROPERTY, TRUE) & ~W_MASK(os_blocked)))))
        return;

    eline(menu, "%s %s", who, what);
}

#define add_eline(mon, prop, who, what)                                 \
    add_property_eline(&menu, show_source, (mon), (prop), (who), (what))

/* final: 0 => still in progress; 1 => over, survived; 2 => dead */
void
enlighten_mon(struct monst *mon, int final, int show_source)
{
    int ltmp;
    int n;
    const char *title;
    const char *buf;
    struct nh_menulist menu;
    
    init_menulist(&menu);
    title = final ? "Final Attributes:" : "Current Attributes:";

    boolean you = (mon == &youmonst);
    const char *name = Monnam(mon);
    const char *names = s_suffix(name);
    const char *s = you ? "" : "s"; /* verb suffix */

    const char *monhas = msgcat(name, you ? " have" : " has");
    const char *monis = msgcat(name, you ? " are" : " is");
    const char *moncan = msgcat(name, " can");
    const char *monsee = msgcat(name, you ? " see" : " sees");
    if (final) {
        monhas = msgcat(name, " had");
        monis = msgcat(name, you ? " were" : " was");
        moncan = msgcat(name, " could");
        monsee = msgcat(name, " saw");
    }

    n = menu.icount;

    if (mon == &youmonst && flags.elbereth_enabled &&
        u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
            "the Hand of Elbereth",
            "the Envoy of Balance",
            "the Glory of Arioch"
        };
        eline(&menu, "%s %s", monis, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
        if (u.ualign.record >= 20)
            eline(&menu, "%s piously aligned", monis);
        else if (u.ualign.record > 13)
            eline(&menu, "%s devoutly aligned", monis);
        else if (u.ualign.record > 8)
            eline(&menu, "%s fervently aligned", monis);
        else if (u.ualign.record > 3)
            eline(&menu, "%s stridently aligned", monis);
        else if (u.ualign.record == 3)
            eline(&menu, "%s aligned", monis);
        else if (u.ualign.record > 0)
            eline(&menu, "%s haltingly aligned", monis);
        else if (u.ualign.record == 0)
            eline(&menu, "%s nominally aligned", monis);
        else if (u.ualign.record >= -3)
            eline(&menu, "%s strayed", monhas);
        else if (u.ualign.record >= -8)
            eline(&menu, "%s sinned", monhas);
        else
            eline(&menu, "%s devoutly aligned", monis);
        if (wizard) {
            buf = msgprintf(" %d", u.uhunger);
            eline(&menu, "Hunger level %s %s", final ? "was" : "is", buf);

            buf = msgprintf(" %d / %ld", u.ualign.record, ALIGNLIM);
            eline(&menu, "Your alignment %s %s", final ? "was" : "is", buf);
        }
    }


    /*** Resistances to troubles ***/
    add_eline(mon, FIRE_RES, monis, "fire resistant");
    add_eline(mon, COLD_RES, monis, "cold resistant");
    add_eline(mon, SLEEP_RES, monis, "sleep resistant");
    add_eline(mon, DISINT_RES, monis, "disintegration-resistant");
    add_eline(mon, SHOCK_RES, monis, "shock resistant");
    add_eline(mon, POISON_RES, monis, "poison resistant");
    add_eline(mon, ACID_RES, monis, "acid resistant");
    add_eline(mon, STONE_RES, monis, "petrification resistant");
    add_eline(mon, DRAIN_RES, monis, "drain resistant");
    add_eline(mon, ANTIMAGIC, monis, "magic resistant");
    add_eline(mon, HALLUC_RES, monis, "hallucination resistant");
    add_eline(mon, SICK_RES, monis, "immune to sickness");
    add_eline(mon, REFLECTING, monhas, "reflection");
    add_eline(mon, FREE_ACTION, monhas, "free action");
    add_eline(mon, HALF_PHDAM, monis, "taking half physical damage");
    add_eline(mon, HALF_SPDAM, monis, "taking half spell damage");
    add_eline(mon, WATERPROOF, monis, "protected from water");
    if (!blind(mon) && haseyes(mon->data) && resists_blnd(mon))
        eline(&menu, "%s resist%s light-induced blindness", name, s);
    if (mon == &youmonst && u.uinvulnerable)
        eline(&menu, "%s invulnerable", monis);
    if ((mon == &youmonst && u.uedibility) ||
        (mon != &youmonst && mon->mtame))
        eline(&menu, "%s recognize detrimental food", moncan);


    /*** Troubles ***/
    add_eline(mon, STONED, monis, "turning to stone");
    add_eline(mon, STRANGLED, monis, "being strangled");
    add_eline(mon, SLIMED, monis, "turning into slime");
    add_eline(mon, SICK, monis, "sick");
    add_eline(mon, CANCELLED, monis, "cancelled");
    add_eline(mon, HUNGER, monis, "hungering rapidly");
    add_eline(mon, SLEEPING, monis, "falling asleep sporadically");
    add_eline(mon, HALLUC, monis, "hallucinating");
    add_eline(mon, BLINDED, monis, "blind");
    add_eline(mon, -BLINDED, monis, "not blind");
    add_eline(mon, CONFUSION, monis, "confused");
    add_eline(mon, STUNNED, monis, "stunned");
    add_eline(mon, SLOW, monis, "slowed");
    add_eline(mon, FUMBLING, monis, "fumbling");
    add_eline(mon, GLIB, monhas,
              msgcat("slippery ", makeplural(mbodypart(mon, FINGER))));

    boolean bothlegs = FALSE;
    if (leg_hurtl(mon) && leg_hurtr(mon))
        bothlegs = TRUE;
    if (leg_hurt(mon))
        eline(&menu, "%s %swounded %s%s", monhas, bothlegs ? "" : "a ", bothlegs ? "" :
              leg_hurtl(mon) ? "left " : "right ", bothlegs ?
              makeplural(mbodypart(mon, LEG)) : mbodypart(mon, LEG));


    /*** Vision and senses ***/
    add_eline(mon, SEE_INVIS, monsee, "invisible");
    add_eline(mon, TELEPAT, monhas, "telepathy");
    add_eline(mon, WARNING, monhas, "warning");
    add_eline(mon, SEARCHING, monhas, "automatic searching");
    add_eline(mon, CLAIRVOYANT, monis, "clairvoyant");
    add_eline(mon, -CLAIRVOYANT, monis, "not clairvoyant");
    add_eline(mon, INFRAVISION, monhas, "infravision");
    add_eline(mon, DETECT_MONSTERS, monis, "sensing the presence of monsters");
    add_eline(mon, WARN_OF_MON, monis,
              msgcat("aware of the presence of ", get_monwarnstr(mon, NULL)));


    /*** Appearance and behavior ***/
    add_eline(mon, INVIS, monis, see_invisible(mon) ? "invisible to others" :
              "invisible");
    add_eline(mon, -INVIS, monis, "visible");
    add_eline(mon, DISPLACED, monis, "displaced");
    add_eline(mon, STEALTH, monis, "stealthy");
    add_eline(mon, AGGRAVATE_MONSTER, name,
              msgcat(final ? "aggravated" : you ? "aggravate" : "aggravates",
                     " monsters"));
    add_eline(mon, CONFLICT, name,
              msgcat(final ? "caused" : you ? "cause" : "causes", " conflict"));
    add_eline(mon, PROT_FROM_SHAPE_CHANGERS, name,
              msgcat(final ? "disrupted" : you ? "disrupt" : "disrupts",
                     " shape changers"));
    if (mon->confhits)
        eline(&menu, "%s going to confuse monsters", monis);


    /*** Transportation ***/
    add_eline(mon, TELEPORT, moncan,
              ((mon == &youmonst && supernatural_ability_available(SPID_RLOC)) ||
               (mon != &youmonst && (mon->m_lev == 12 ||
                                     (mon->data->mflags1 & M1_TPORT)))) ?
              "teleport at will" : "teleport");
    add_eline(mon, TELEPORT_CONTROL, monhas, "teleport control");
    add_eline(mon, JUMPING, moncan, "jump");
    add_eline(mon, LEVITATION, monis, levitates_at_will(mon, FALSE, FALSE) ?
              "levitating at will" : "levitating");
    if (!levitates(mon)) /* levitation overrides flying */
        add_eline(mon, FLYING, moncan, "fly");
    add_eline(mon, PASSES_WALLS, moncan, "walk through walls");
    add_eline(mon, WWALKING, moncan, "walk on water");
    add_eline(mon, SWIMMING, moncan, "swim");
    if (mon->data->mflags1 & M1_AMPHIBIOUS)
        eline(&menu, "%s breathe water", moncan);
    else
        add_eline(mon, MAGICAL_BREATHING, moncan, "survive without air");


    /*** Monster-to-monster connections ***/
    if (you && u.usteed)
        eline(&menu, "%s riding %s", monis, a_monnam(u.usteed));
    if (u.ustuck && (you || mon == u.ustuck)) {
        if (Engulfed)
            eline(&menu, "%s %s %s", monis, you ? "swallowed by" : "engulfing",
                  you ? a_monnam(u.ustuck) : "you");
        else
            eline(&menu, "%s %s %s", monis,
                  ((you && sticks(youmonst.data)) || (!you && sticks(mon->data))) ?
                  "holding" : "held by", you ? a_monnam(u.ustuck) : "you");
    }


    /*** Physical attributes ***/
    add_eline(mon, UNCHANGING, moncan, "not change form");
    if (!unchanging(mon)) {
        /* Only mention lycantropy if not currently in effect, to avoid redundancy */
        if (!shapeshift_prot(&youmonst) &&
            ((you && u.ulycn >= LOW_PM && u.umonnum != u.ulycn) ||
             (is_were(mon->data) && is_human(mon->data)))) {
            int pm = NON_PM;
            if (you)
                pm = u.ulycn;
            else
                pm = counter_were(monsndx(mon->data));
            eline(&menu, "%s %s", monis, an(mons[pm].mname));
        } else if (!shapeshift_prot(&youmonst) && mon->cham)
            eline(&menu, "%s a shapeshifter", monis);
        else
            add_eline(mon, POLYMORPH, monis, "polymorphing");
    }
    add_eline(mon, POLYMORPH_CONTROL, monhas, "polymorph control");
    if (Upolyd || (is_were(mon->data) && !is_human(mon->data))) {
        const char *buf;
        if (!you || u.umonnum == u.ulycn)
            buf = "in beast form";
        else
            buf = msgprintf("%spolymorphed into %s",
                            flags.polyinit_mnum == NON_PM ? "" : "permanently ",
                            an(youmonst.data->mname));
        if (you && flags.polyinit_mnum == NON_PM && wizard)
            buf = msgprintf("%s (%d)", buf, u.mtimedone);
        eline(&menu, "%s %s", monis, buf);
    }
    add_eline(mon, REGENERATION, name,
              final ? "regenerated" : you ? "regenerate" : "regenerates");
    add_eline(mon, SLOW_DIGESTION, monhas, "slower digestion");
    add_eline(mon, FAST, monis, very_fast(mon) ? "very fast" : "fast");
    add_eline(mon, FIXED_ABIL, monhas, "fixed abilities");
    if (adorned(mon)) {
        int adorn = 0;
        struct obj *ringl = which_armor(mon, os_ringl);
        struct obj *ringr = which_armor(mon, os_ringr);
        if (ringl && ringl->otyp == RIN_ADORNMENT)
            adorn += ringl->spe;
        if (ringr && ringr->otyp == RIN_ADORNMENT)
            adorn += ringr->spe;
        buf = msgprintf("%sadorned", adorn < 0 ? "poorly " : "");
        add_eline(mon, ADORNED, monis, buf);
    }
    if (hitbon(mon))
        eline(&menu, "%s %s", monhas, enl_combatinc("to hit", hitbon(mon), final));
    if (dambon(mon))
        eline(&menu, "%s %s", monhas, enl_combatinc("damage", dambon(mon), final));
    if (protected(mon) || protbon(mon)) {
        int prot = protbon(mon);
        if (you)
            prot += u.ublessed;
        prot += spellprot(mon);

        add_eline(mon, PROTECTION, monis, prot < 0 ? "ineffectively protected" :
                  "protected");
    }


    /*** Miscellany (player only) ***/
    if (you) {
        if (u.twoweap)
            eline(&menu, "%s wielding two weapons at once", monis);
        if (Luck) {
            ltmp = abs((int)Luck);
            const char *buf = msgprintf(
                "%s%slucky",
                ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
                Luck < 0 ? "un" : "");
            if (wizard)
                buf = msgprintf("%s (%d)", buf, Luck);
            eline(&menu, "%s %s", monis, buf);
        } else if (wizard)
            eline(&menu, "%s luck %s zero", names, final ? "was" : "is");
        if (u.moreluck)
            eline(&menu, "%s %s luck", monhas, u.moreluck > 0 ? "extra" : "reduced");
        if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
            ltmp = stone_luck(FALSE);
            if (ltmp <= 0)
                eline(&menu, "Bad luck %s not time out for %s", final ? "did" : "does",
                      mon_nam(mon));
            if (ltmp >= 0)
                eline(&menu, "Good luck %s not time out for %s", final ? "did" : "does",
                      mon_nam(mon));
        }

        if (u.ugangr) {
            const char *buf = msgprintf(
                " %sangry with you",
                u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
            if (wizard)
                buf = msgprintf("%s (%d)", buf, u.ugangr);
            eline(&menu, "%s %s %sangry with %s", u_gname(), final ? "was" : "is", buf,
                  mon_nam(mon));
        } else if (!final) {
            /* We need to suppress this when the game is over, because death
               can change the value calculated by can_pray(), potentially
               resulting in a false claim that you could have prayed safely. */
            const char *buf = msgprintf(
                "%ssafely pray", can_pray(FALSE) ? "" : "not ");
            /* can_pray sets some turnstate that needs to be reset. */
            turnstate.pray.align = A_NONE;
            turnstate.pray.type = pty_invalid;
            turnstate.pray.trouble = ptr_invalid;
            if (wizard)
                buf = msgprintf("%s (%d)", buf, u.ublesscnt);
            eline(&menu, "%s %s", moncan, buf);
        }
    }

    int mortality = 0;
    if (you)
        mortality = u.umortality;
    else if (mx_edog(mon))
        mortality = mon->mextra->edog->revivals;

    if (final || mortality) {
        const char *buf = "";
        if (final == 1) /* final was due to death */ {
            if (mortality)
                buf = msgprintf(" (%d%s time!)", mortality, ordin(mortality));
            /* Not monis, since that would be in past tense, which is wrong here */
            eline(&menu, "%s dead%s", M_verbs(mon, "are"), buf);
        } else {
            buf = (mortality == 1 ? "once" :
                   mortality == 2 ? "twice" :
                   mortality == 3 ? "thrice" :
                   msgprintf("%d times", mortality));
            if (final)
                eline(&menu, "%s survived after being killed %s", name, buf);
            else
                eline(&menu, "%s been killed %s", monhas, buf);
        }
    }
    if (n == menu.icount)
        eline(&menu, monhas, "no special properties");

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

static void
conductline(struct nh_menulist *menu, boolean final, enum player_conduct conduct,
      boolean show_amount, const char *broken, const char *unbroken,
      const char *unbroken_grammar)
{
    /* If not showing amount, hide the conduct alltogether if broken before turn 1800 */
    if (!show_amount && u.uconduct_time[conduct] < 1800)
        return;

    /* Don't show redundant conducts */
    if ((conduct == conduct_artiwish && !u.uconduct[conduct_wish]) ||
        (conduct == conduct_vegetarian && !u.uconduct[conduct_vegan]) ||
        (conduct == conduct_vegan && !u.uconduct[conduct_food]))
        return;

    if (!u.uconduct[conduct])
        eline(menu, "You %s%s", unbroken_grammar, unbroken);
    else {
        if (!show_amount)
            eline(menu, "You %s until turn %d", broken, u.uconduct_time[conduct]);
        else
            eline(menu, "You %s%s %u time%s, starting on turn %d", final ? "have " : "",
                  broken, u.uconduct[conduct], plur(u.uconduct[conduct]),
                  u.uconduct_time[conduct]);
    }
}

#define cline(c, t, b, ub, ubg)                                 \
    conductline(&menu, final, (c), (t), (b), (ub), (ubg))

void
show_conduct(int final)
{
    int ngenocided;
    struct nh_menulist menu;
    const char *buf;

    const char *havebeen = final ? "were" : "have been";
    const char *have = final ? "" : "have ";
    /* Create the conduct window */
    init_menulist(&menu);

    cline(conduct_food, FALSE,
          "did not eat", "without food", final ? "went" : "have gone");
    cline(conduct_vegan, FALSE,
          "followed a strict vegan diet", "followed a strict vegan diet", have);
    cline(conduct_vegetarian, FALSE, "were vegetarian", "vegetarian", havebeen);
    cline(conduct_gnostic, FALSE, "were an atheist", "an atheist", havebeen);
    cline(conduct_killer, FALSE, "were a pacifist", "a pacifist", havebeen);
    cline(conduct_weaphit, TRUE,
          "used a wielded weapon", "never hit with a wielded weapon", have);
    cline(conduct_illiterate, TRUE, "read items or engraved", "illiterate", havebeen);
    cline(conduct_polypile, TRUE,
          "polymorphed items", "never polymorphed an object", have);
    cline(conduct_polyself, TRUE, "changed form", "never changed form", have);
    cline(conduct_wish, TRUE, "wished for items", "never wished for anything", have);
    cline(conduct_artiwish, TRUE,
          "wished for artifacts", "never wished for artifacts", have);
    cline(conduct_puddingsplit, TRUE, "split puddings", "never split a pudding", have);
    cline(conduct_lostalign, TRUE,
          "violated your moral code", "never violated your moral code", have);

    ngenocided = num_genocides();
    if (!u.uconduct[conduct_genocide])
        eline(&menu, "You %snever genocided any monsters%s", have,
              ngenocided ? ", but other monsters did" : "");
    else {
        buf = msgprintf("%d", u.uconduct[conduct_genocide]);
        if (ngenocided != u.uconduct[conduct_genocide]) /* other monster genocides */
            buf = msgprintf("%d (out of %d)", u.uconduct[conduct_genocide], ngenocided);
        eline(&menu, "You %sgenocided %s type%s of monster%s, starting on turn %d", have,
              buf, plur(u.uconduct[conduct_genocide]),
              plur(u.uconduct[conduct_genocide]), u.uconduct_time[conduct_genocide]);
    }

    /* birth options */
    if (!flags.bones_enabled)
        eline(&menu, "You %sdisabled bones files", have);
    if (!flags.elbereth_enabled) /* not the same as not /writing/ E */
        eline(&menu, "You %sabstained from Elbereth's help", have);
    if (flags.permahallu)
        eline(&menu, "You %s permanently hallucinating", final ? "were" : "are");
    if (flags.permablind)
        eline(&menu, "You %s blind from birth", final ? "were" : "are");

    /* Pop up the window and wait for a key */
    display_menu(&menu, "Voluntary challenges:", PICK_NONE,
                 PLHINT_ANYWHERE, NULL);
}
