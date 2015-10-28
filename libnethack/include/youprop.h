/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-10-28 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef YOUPROP_H
# define YOUPROP_H

# include "prop.h"
# include "permonst.h"
# include "mondata.h"

# define maybe_polyd(if_so,if_not)      (Upolyd ? (if_so) : (if_not))

/* The logic for properties has been moved to prop.c; this file is now just
   compatibility macros. */
/* Strongly consider not using these, but instead use the macros in mondata.h
   when checking properties in new code. It works on both you and monsters and
   generally follows a more consistent naming scheme. */

# define u_any_property(p)     (!!u_have_property((p), ANY_PROPERTY, FALSE))
# define worn_extrinsic(p)     mworn_extrinsic(&youmonst, p)
# define worn_blocked(p)       mworn_blocked(&youmonst, p)
# define worn_warntype()       mworn_warntype(&youmonst)

/*** Resistances to troubles ***/
/* With intrinsics and extrinsics */
# define Fire_resistance        u_any_property(FIRE_RES)
# define Cold_resistance        u_any_property(COLD_RES)
# define Sleep_resistance       u_any_property(SLEEP_RES)
# define Disint_resistance      u_any_property(DISINT_RES)
# define Shock_resistance       u_any_property(SHOCK_RES)
# define Poison_resistance      u_any_property(POISON_RES)
# define Drain_resistance       u_any_property(DRAIN_RES)

/* Intrinsics only */
# define Sick_resistance        u_any_property(SICK_RES)

/* Extrinsics and polyforms only */
# define Antimagic              u_any_property(ANTIMAGIC)
# define Acid_resistance        u_any_property(ACID_RES)
# define Stone_resistance       u_any_property(STONE_RES)


/*** Troubles ***/
/* Pseudo-property */
# define Punished               (uball)

/* These have silly names that don't follow the normal pattern :-( */
# define Blind                  u_any_property(BLINDED)

/* Whereas these can't follow the normal pattern */
# define Blind_telepat          u_any_property(TELEPAT)
# define Unblind_telepat        u_have_property(TELEPAT,                \
                                                W_EQUIP|W_ARTIFACT, FALSE)

/* And those that work sensibly */
# define Stunned                u_any_property(STUNNED)
# define Confusion              u_any_property(CONFUSION)
# define Hallucination          u_any_property(HALLUC)
# define Halluc_resistance      u_any_property(HALLUC_RES)
# define Fumbling               u_any_property(FUMBLING)

/*** Vision and senses ***/
# define See_invisible          u_any_property(SEE_INVIS)
# define Warning                u_any_property(WARNING)
# define Warn_of_mon            u_any_property(WARN_OF_MON)
# define Undead_warning         u_any_property(WARN_UNDEAD)
# define Searching              u_any_property(SEARCHING)
# define Clairvoyant            u_any_property(CLAIRVOYANT)
# define BClairvoyant           worn_blocked(CLAIRVOYANT)
# define Infravision            u_any_property(INFRAVISION)
# define Detect_monsters        u_any_property(DETECT_MONSTERS)

/*** Appearance and behavior ***/
# define Adornment              worn_extrinsic(ADORNED)

# define Invis                  u_any_property(INVIS)
# define BInvis                 worn_blocked(INVIS)
# define Invisible              (Invis && !See_invisible)
# define Displaced              worn_extrinsic(DISPLACED)
# define Stealth                u_any_property(STEALTH)
# define Aggravate_monster      u_any_property(AGGRAVATE_MONSTER)
# define Conflict               u_any_property(CONFLICT)

/*** Transportation ***/
# define Lev_at_will            levitates_at_will(&youmonst, FALSE, FALSE)
# define Jumping                u_any_property(JUMPING)
# define Teleportation          u_any_property(TELEPORT)
# define Teleport_control       u_any_property(TELEPORT_CONTROL)
# define Levitation             u_any_property(LEVITATION)
# define Flying                 u_any_property(FLYING)
# define Wwalking               u_any_property(WWALKING)
# define Swimming               u_any_property(SWIMMING)
# define Breathless             u_any_property(MAGICAL_BREATHING)
# define Passes_walls           u_any_property(PASSES_WALLS)

# define Ground_based           (!Levitation && !Flying &&      \
                                 !is_clinger(youmonst.data))
# define Engulfed               (u.uswallow)
# define Underwater             (u.uinwater)
/* Note that Underwater and u.uinwater are both used in code.
   The latter form is for later implementation of other in-water
   states, like swimming, wading, etc. */

/*** Physical attributes ***/
# define Slow_digestion         u_any_property(SLOW_DIGESTION)
# define Half_spell_damage      u_any_property(HALF_SPDAM)
# define Half_physical_damage   u_any_property(HALF_PHDAM)
# define Regeneration           u_any_property(REGENERATION)
# define Energy_regeneration    u_any_property(ENERGY_REGENERATION)
# define Protection             u_any_property(PROTECTION)
# define Protection_from_shape_changers \
                                u_any_property(PROT_FROM_SHAPE_CHANGERS)
# define Polymorph              u_any_property(POLYMORPH)
# define Polymorph_control      u_any_property(POLYMORPH_CONTROL)
# define Unchanging             u_any_property(UNCHANGING)
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
    hr_asleep,
    hr_fainted,
    hr_paralyzed,
    hr_afraid,
    hr_moving,
    hr_mimicking,
    hr_busy,
    hr_engraving,
    hr_praying,
    hr_first = hr_asleep,
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
    conduct_food,                        /* eaten any comestible */
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
    conduct_first = conduct_food,
};

#endif /* YOUPROP_H */

