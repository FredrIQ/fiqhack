/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-08 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef YOUPROP_H
# define YOUPROP_H

# include "prop.h"
# include "permonst.h"
# include "mondata.h"

# define maybe_polyd(if_so,if_not)      (Upolyd ? (if_so) : (if_not))

/* The logic for properties has been moved to youprop.c; this file is now just
   compatibility macros. */

# define u_any_property(p)     (!!u_have_property((p), ANY_PROPERTY, FALSE))
# define worn_extrinsic(p)     mworn_extrinsic(&youmonst, p)
# define worn_blocked(p)       mworn_blocked(&youmonst, p)
# define worn_warntype()       mworn_warntype(&youmonst)

/*** Resistances to troubles ***/
/* With intrinsics and extrinsics */
# define HFire_resistance       u.uintrinsic[FIRE_RES]
# define Fire_resistance        u_any_property(FIRE_RES)
# define HCold_resistance       u.uintrinsic[COLD_RES]
# define Cold_resistance        u_any_property(COLD_RES)
# define HSleep_resistance      u.uintrinsic[SLEEP_RES]
# define Sleep_resistance       u_any_property(SLEEP_RES)
# define HDisint_resistance     u.uintrinsic[DISINT_RES]
# define Disint_resistance      u_any_property(DISINT_RES)
# define HShock_resistance      u.uintrinsic[SHOCK_RES]
# define Shock_resistance       u_any_property(SHOCK_RES)
# define HPoison_resistance     u.uintrinsic[POISON_RES]
# define Poison_resistance      u_any_property(POISON_RES)
# define HDrain_resistance      u.uintrinsic[DRAIN_RES]
# define Drain_resistance       u_any_property(DRAIN_RES)

/* Intrinsics only */
# define HSick_resistance       u.uintrinsic[SICK_RES]
# define Sick_resistance        u_any_property(SICK_RES)

/* Extrinsics and polyforms only */
# define EAntimagic             worn_extrinsic(ANTIMAGIC)
# define Antimagic              u_any_property(ANTIMAGIC)
# define EAcid_resistance       worn_extrinsic(ACID_RES)
# define Acid_resistance        u_any_property(ACID_RES)
# define EStone_resistance      worn_extrinsic(STONE_RES)
# define Stone_resistance       u_any_property(STONE_RES)


/*** Troubles ***/
/* Pseudo-property */
# define Punished               (uball)

/* These have silly names that don't follow the normal pattern :-( */
# define Blinded                u.uintrinsic[BLINDED]
# define Blindfolded            u_have_property(BLINDED, W_MASK(os_tool), FALSE)
# define Blind                  u_any_property(BLINDED)

/* Whereas these can't follow the normal pattern */
# define LWounded_legs          u.uintrinsic[LWOUNDED_LEGS]
# define RWounded_legs          u.uintrinsic[RWOUNDED_LEGS]
# define Wounded_legs           (LWounded_legs || RWounded_legs)
# define Wounded_leg_side       ((LWounded_legs ? LEFT_SIDE : 0) | \
                                 (RWounded_legs ? RIGHT_SIDE : 0))

# define HTelepat               u.uintrinsic[TELEPAT]
# define Blind_telepat          u_any_property(TELEPAT)
# define Unblind_telepat        u_have_property(TELEPAT,                \
                                                W_EQUIP|W_ARTIFACT, FALSE)

/* And those that work sensibly */
# define HStun                  u.uintrinsic[STUNNED]
# define Stunned                u_any_property(STUNNED)
# define HConfusion             u.uintrinsic[CONFUSION]
# define Confusion              u_any_property(CONFUSION)
# define Sick                   u.uintrinsic[SICK]
# define Stoned                 u.uintrinsic[STONED]
# define Strangled              u.uintrinsic[STRANGLED]
# define Vomiting               u.uintrinsic[VOMITING]
# define Glib                   u.uintrinsic[GLIB]
# define Slimed                 u.uintrinsic[SLIMED]      /* [Tom] */
# define HHallucination         u.uintrinsic[HALLUC]
# define Hallucination          u_any_property(HALLUC)
# define Halluc_resistance      u_any_property(HALLUC_RES)
# define HFumbling              u.uintrinsic[FUMBLING]
# define Fumbling               u_any_property(FUMBLING)
# define HSleeping              u.uintrinsic[SLEEPING]
# define Sleeping               u_any_property(SLEEPING)
# define HHunger                u.uintrinsic[HUNGER]
# define Hunger                 u_any_property(HUNGER)

/*** Vision and senses ***/
# define HSee_invisible         u.uintrinsic[SEE_INVIS]
# define See_invisible          u_any_property(SEE_INVIS)
# define HWarning               u.uintrinsic[WARNING]
# define Warning                u_any_property(WARNING)
# define HWarn_of_mon           u.uintrinsic[WARN_OF_MON]
# define Warn_of_mon            u_any_property(WARN_OF_MON)
# define HUndead_warning        u.uintrinsic[WARN_UNDEAD]
# define Undead_warning         u_any_property(WARN_UNDEAD)
# define HSearching             u.uintrinsic[SEARCHING]
# define Searching              u_any_property(SEARCHING)
# define HClairvoyant           u.uintrinsic[CLAIRVOYANT]
# define Clairvoyant            u_any_property(CLAIRVOYANT)
# define BClairvoyant           worn_blocked(CLAIRVOYANT)
# define HInfravision           u.uintrinsic[INFRAVISION]
# define Infravision            u_any_property(INFRAVISION)
# define HDetect_monsters       u.uintrinsic[DETECT_MONSTERS]
# define Detect_monsters        u_any_property(DETECT_MONSTERS)

/*** Appearance and behavior ***/
# define Adornment              worn_extrinsic(ADORNED)

# define HInvis                 u.uintrinsic[INVIS]
# define Invis                  u_any_property(INVIS)
# define BInvis                 worn_blocked(INVIS)
# define Invisible              (Invis && !See_invisible)
# define Displaced              worn_extrinsic(DISPLACED)
# define HStealth               u.uintrinsic[STEALTH]
# define Stealth                u_any_property(STEALTH)
# define HAggravate_monster     u.uintrinsic[AGGRAVATE_MONSTER]
# define Aggravate_monster      u_any_property(AGGRAVATE_MONSTER)
# define HConflict              u.uintrinsic[CONFLICT]
# define Conflict               u_any_property(CONFLICT)

/*** Transportation ***/
# define Lev_at_will                                                    \
    (u_have_property(LEVITATION, I_SPECIAL|W_ARTIFACT, FALSE) &&        \
     !u_have_property(LEVITATION, ~(I_SPECIAL|W_ARTIFACT|W_MASK(os_timeout)), \
                      FALSE))

# define HJumping               u.uintrinsic[JUMPING]
# define Jumping                u_any_property(JUMPING)
# define HTeleportation         u.uintrinsic[TELEPORT]
# define Teleportation          u_any_property(TELEPORT)
# define HTeleport_control      u.uintrinsic[TELEPORT_CONTROL]
# define Teleport_control       u_any_property(TELEPORT_CONTROL)
# define HLevitation            u.uintrinsic[LEVITATION]
# define Levitation             u_any_property(LEVITATION)
# define Flying                 u_any_property(FLYING)
# define Wwalking               u_any_property(WWALKING)
# define HSwimming              u.uintrinsic[SWIMMING]
# define Swimming               u_any_property(SWIMMING)
# define HMagical_breathing     u.uintrinsic[MAGICAL_BREATHING]
# define Amphibious             u_any_property(MAGICAL_BREATHING)
# define HPasses_walls          u.uintrinsic[PASSES_WALLS]
# define Passes_walls           u_any_property(PASSES_WALLS)

# define Ground_based           (!Levitation && !Flying &&      \
                                 !is_clinger(youmonst.data))
# define Breathless             (HMagical_breathing ||                  \
                                 worn_extrinsic(MAGICAL_BREATHING) ||   \
                                 breathless(youmonst.data))
# define Engulfed               (u.uswallow)
# define Underwater             (u.uinwater)
/* Note that Underwater and u.uinwater are both used in code.
   The latter form is for later implementation of other in-water
   states, like swimming, wading, etc. */

/*** Physical attributes ***/
# define HSlow_digestion        u.uintrinsic[SLOW_DIGESTION]
# define Slow_digestion         u_any_property(SLOW_DIGESTION)
# define HHalf_spell_damage     u.uintrinsic[HALF_SPDAM]
# define Half_spell_damage      u_any_property(HALF_SPDAM)
# define HHalf_physical_damage  u.uintrinsic[HALF_PHDAM]
# define Half_physical_damage   u_any_property(HALF_PHDAM)
# define HRegeneration          u.uintrinsic[REGENERATION]
# define Regeneration           u_any_property(REGENERATION)
# define HEnergy_regeneration   u.uintrinsic[ENERGY_REGENERATION]
# define Energy_regeneration    u_any_property(ENERGY_REGENERATION)
# define HProtection            u.uintrinsic[PROTECTION]
# define Protection             u_any_property(PROTECTION)
# define HProtection_from_shape_changers \
                                u.uintrinsic[PROT_FROM_SHAPE_CHANGERS]
# define Protection_from_shape_changers \
                                u_any_property(PROT_FROM_SHAPE_CHANGERS)
# define HPolymorph             u.uintrinsic[POLYMORPH]
# define Polymorph              u_any_property(POLYMORPH)
# define HPolymorph_control     u.uintrinsic[POLYMORPH_CONTROL]
# define Polymorph_control      u_any_property(POLYMORPH_CONTROL)
# define HUnchanging            u.uintrinsic[UNCHANGING]
# define Unchanging             u_any_property(UNCHANGING)
# define HFast                  u.uintrinsic[FAST]
# define Fast                   u_any_property(FAST)
# define Very_fast              u_have_property(FAST, ~INTRINSIC, FALSE)
# define Reflecting             u_any_property(REFLECTING)
# define Free_action            u_any_property(FREE_ACTION)
# define Fixed_abil             u_any_property(FIXED_ABIL)
# define Lifesaved              u_any_property(LIFESAVED)
# define Xray_vision            u_any_property(XRAY_VISION)

# define XRAY_RANGE             3

/*** Possessions ***/
#define Uhave_amulet            carrying(AMULET_OF_YENDOR)
#define Uhave_bell              carrying(BELL_OF_OPENING)
#define Uhave_book              carrying(SPE_BOOK_OF_THE_DEAD)
#define Uhave_menorah           carrying(CANDELABRUM_OF_INVOCATION)
#define Uhave_questart          carrying_questart()

/* Reasons to be helpless */
enum helpless_reason {
    hr_first = 0,
    hr_asleep = hr_first,
    hr_fainted,
    hr_paralyzed,
    hr_afraid,
    hr_moving,
    hr_mimicking,
    hr_busy,
    hr_engraving,
    hr_praying,
    hr_last = hr_praying
};

/* Masks for clearing helplessness */
/* This must match helpless_reason exactly */
#define HM(reason) hm_##reason = 1 << hr_##reason
enum FLAG_ENUM helpless_mask {
    hm_none = 0,
    HM(asleep),
    HM(fainted),
    hm_unconscious = hm_asleep | hm_fainted,
    HM(paralyzed),
    HM(afraid),
    HM(moving),
    HM(mimicking),
    HM(busy),
    HM(engraving),
    HM(praying),
    hm_all = (1 << (hr_last + 1)) - 1,
};

/* The game's player conducts.
 * Editing this enum will break save compatibility. */
enum player_conduct {
    conduct_first = 0,
    conduct_food = conduct_first,        /* eaten any comestible */
    conduct_vegan,                       /* ... or any animal byproduct */
    conduct_vegetarian,                  /* ... or any meat */
    conduct_gnostic,                     /* used prayer, priest, or altar */
    conduct_weaphit,                     /* hit a monster with a weapon */
    conduct_killer,                      /* killed a monster yourself */
    conduct_illiterate,                  /* read something (other than BotD) */
    conduct_polypile,                    /* polymorphed an object */
    conduct_polyself,                    /* transformed yourself */
    conduct_wish,                        /* used a wish */
    conduct_artiwish,                    /* wished for an artifact */
    conduct_genocide,                    /* committed genocide */
    conduct_elbereth,                    /* wrote an elbereth */
    conduct_puddingsplit,                /* split a pudding */
    conduct_lostalign,                   /* lost alignment record points */
    conduct_unused1,                     /* unused, might not be 0 in -beta1 */
    num_conducts,
};

#endif /* YOUPROP_H */

