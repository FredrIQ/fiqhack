/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-16 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef YOUPROP_H
# define YOUPROP_H

# include "prop.h"
# include "permonst.h"
# include "mondata.h"
# include "pm.h"


/* KMH, intrinsics patch.
 * Reorganized and rewritten for >32-bit properties.
 * HXxx refers to intrinsic bitfields while in human form.
 * EXxx refers to extrinsic bitfields from worn objects.
 * BXxx refers to the cause of the property being blocked.
 * Xxx refers to any source, including polymorph forms.
 */


# define maybe_polyd(if_so,if_not)      (Upolyd ? (if_so) : (if_not))


/*** Resistances to troubles ***/
/* With intrinsics and extrinsics */
# define HFire_resistance       u.uintrinsic[FIRE_RES]
# define EFire_resistance       worn_extrinsic(FIRE_RES)
# define Fire_resistance        (HFire_resistance || EFire_resistance || \
                                 resists_fire(&youmonst))

# define HCold_resistance       u.uintrinsic[COLD_RES]
# define ECold_resistance       worn_extrinsic(COLD_RES)
# define Cold_resistance        (HCold_resistance || ECold_resistance || \
                                 resists_cold(&youmonst))

# define HSleep_resistance      u.uintrinsic[SLEEP_RES]
# define ESleep_resistance      worn_extrinsic(SLEEP_RES)
# define Sleep_resistance       (HSleep_resistance || ESleep_resistance || \
                                 resists_sleep(&youmonst))

# define HDisint_resistance     u.uintrinsic[DISINT_RES]
# define EDisint_resistance     worn_extrinsic(DISINT_RES)
# define Disint_resistance      (HDisint_resistance || EDisint_resistance || \
                                 resists_disint(&youmonst))

# define HShock_resistance      u.uintrinsic[SHOCK_RES]
# define EShock_resistance      worn_extrinsic(SHOCK_RES)
# define Shock_resistance       (HShock_resistance || EShock_resistance || \
                                 resists_elec(&youmonst))

# define HPoison_resistance     u.uintrinsic[POISON_RES]
# define EPoison_resistance     worn_extrinsic(POISON_RES)
# define Poison_resistance      (HPoison_resistance || EPoison_resistance || \
                                 resists_poison(&youmonst))

# define HDrain_resistance      u.uintrinsic[DRAIN_RES]
# define EDrain_resistance      worn_extrinsic(DRAIN_RES)
# define Drain_resistance       (HDrain_resistance || EDrain_resistance || \
                                 resists_drli(&youmonst))

/* Intrinsics only */
# define HSick_resistance       u.uintrinsic[SICK_RES]
# define Sick_resistance        (HSick_resistance || \
                                 youmonst.data->mlet == S_FUNGUS || \
                                 youmonst.data == &mons[PM_GHOUL] || \
                                 defends(AD_DISE,uwep))
# define Invulnerable           u.uintrinsic[INVULNERABLE]  /* [Tom] */

/* Extrinsics only */
# define EAntimagic             worn_extrinsic(ANTIMAGIC)
# define Antimagic              (EAntimagic || \
                                 (Upolyd && resists_magm(&youmonst)))

# define EAcid_resistance       worn_extrinsic(ACID_RES)
# define Acid_resistance        (EAcid_resistance || resists_acid(&youmonst))

# define EStone_resistance      worn_extrinsic(STONE_RES)
# define Stone_resistance       (EStone_resistance || resists_ston(&youmonst))


/*** Troubles ***/
/* Pseudo-property */
# define Punished               (uball)

/* Those implemented solely as timeouts (we use just intrinsic) */
# define HStun                  u.uintrinsic[STUNNED]
# define Stunned                (HStun || u.umonnum == PM_STALKER || \
                                 youmonst.data->mlet == S_BAT)
                /* Note: birds will also be stunned */

# define HConfusion             u.uintrinsic[CONFUSION]
# define Confusion              HConfusion

# define Blinded                u.uintrinsic[BLINDED]
# define Blindfolded            (ublindf && ublindf->otyp != LENSES)
                /* ...means blind because of a cover */
# define Blind  (((Blinded || Blindfolded || !haseyes(youmonst.data)) && \
                 !(ublindf && ublindf->oartifact == ART_EYES_OF_THE_OVERWORLD)) || \
                 flags.permablind || unconscious())
                /* ...the Eyes operate even when you really are blind or don't
                   have any eyes, but get beaten by game options or
                   unconsciousness */

# define Sick                   u.uintrinsic[SICK]
# define Stoned                 u.uintrinsic[STONED]
# define Strangled              u.uintrinsic[STRANGLED]
# define Vomiting               u.uintrinsic[VOMITING]
# define Glib                   u.uintrinsic[GLIB]
# define Slimed                 u.uintrinsic[SLIMED]      /* [Tom] */

/* Hallucination is solely a timeout; its resistance is extrinsic */
# define HHallucination         u.uintrinsic[HALLUC]
# define EHalluc_resistance     worn_extrinsic(HALLUC_RES)
# define Halluc_resistance      (EHalluc_resistance || \
                                 (Upolyd && dmgtype(youmonst.data, AD_HALU)))
# define Hallucination          ((HHallucination && !Halluc_resistance) || \
                                 flags.permahallu)

/* Timeout, plus a worn mask */
# define HFumbling              u.uintrinsic[FUMBLING]
# define EFumbling              worn_extrinsic(FUMBLING)
# define Fumbling               (HFumbling || EFumbling)

# define LWounded_legs          u.uintrinsic[LWOUNDED_LEGS]
# define RWounded_legs          u.uintrinsic[RWOUNDED_LEGS]
# define Wounded_legs           (LWounded_legs || RWounded_legs)

# define HSleeping              u.uintrinsic[SLEEPING]
# define ESleeping              worn_extrinsic(SLEEPING)
# define Sleeping               (HSleeping || ESleeping)

# define HHunger                u.uintrinsic[HUNGER]
# define EHunger                worn_extrinsic(HUNGER)
# define Hunger                 (HHunger || EHunger)


/*** Vision and senses ***/
# define HSee_invisible         u.uintrinsic[SEE_INVIS]
# define ESee_invisible         worn_extrinsic(SEE_INVIS)
# define See_invisible          (HSee_invisible || ESee_invisible || \
                                 perceives(youmonst.data))

# define HTelepat               u.uintrinsic[TELEPAT]
# define ETelepat               worn_extrinsic(TELEPAT)
# define Blind_telepat          (HTelepat || ETelepat || \
                                 telepathic(youmonst.data))
# define Unblind_telepat        (ETelepat)

# define HWarning               u.uintrinsic[WARNING]
# define EWarning               worn_extrinsic(WARNING)
# define Warning                (HWarning || EWarning)

/* Warning for a specific type of monster */
# define HWarn_of_mon           u.uintrinsic[WARN_OF_MON]
# define EWarn_of_mon           worn_extrinsic(WARN_OF_MON)
# define Warn_of_mon            (HWarn_of_mon || EWarn_of_mon)

# define HUndead_warning        u.uintrinsic[WARN_UNDEAD]
# define Undead_warning         (HUndead_warning)

# define HSearching             u.uintrinsic[SEARCHING]
# define ESearching             worn_extrinsic(SEARCHING)
# define Searching              (HSearching || ESearching)

# define HClairvoyant           u.uintrinsic[CLAIRVOYANT]
# define EClairvoyant           worn_extrinsic(CLAIRVOYANT)
# define BClairvoyant           worn_blocked(CLAIRVOYANT)
# define Clairvoyant            ((HClairvoyant || EClairvoyant) &&\
                                 !BClairvoyant)

# define HInfravision           u.uintrinsic[INFRAVISION]
# define EInfravision           worn_extrinsic(INFRAVISION)
# define Infravision            (HInfravision || EInfravision || \
                                  infravision(youmonst.data))

# define HDetect_monsters       u.uintrinsic[DETECT_MONSTERS]
# define EDetect_monsters       worn_extrinsic(DETECT_MONSTERS)
# define Detect_monsters        (HDetect_monsters || EDetect_monsters)


/*** Appearance and behavior ***/
# define Adornment              worn_extrinsic(ADORNED)

# define HInvis                 u.uintrinsic[INVIS]
# define EInvis                 worn_extrinsic(INVIS)
# define BInvis                 worn_blocked(INVIS)
# define Invis                  ((HInvis || EInvis || \
                                 pm_invisible(youmonst.data)) && !BInvis)
# define Invisible              (Invis && !See_invisible)
                /* Note: invisibility also hides inventory and steed */

# define EDisplaced             worn_extrinsic(DISPLACED)
# define Displaced              EDisplaced

# define HStealth               u.uintrinsic[STEALTH]
# define EStealth               worn_extrinsic(STEALTH)
# define BStealth               worn_blocked(STEALTH)
# define Stealth                ((HStealth || EStealth) && !BStealth)

# define HAggravate_monster     u.uintrinsic[AGGRAVATE_MONSTER]
# define EAggravate_monster     worn_extrinsic(AGGRAVATE_MONSTER)
# define Aggravate_monster      (HAggravate_monster || EAggravate_monster)

# define HConflict              u.uintrinsic[CONFLICT]
# define EConflict              worn_extrinsic(CONFLICT)
# define Conflict               (HConflict || EConflict)


/*** Transportation ***/
# define HJumping               u.uintrinsic[JUMPING]
# define EJumping               worn_extrinsic(JUMPING)
# define Jumping                (HJumping || EJumping)

# define HTeleportation         u.uintrinsic[TELEPORT]
# define ETeleportation         worn_extrinsic(TELEPORT)
# define Teleportation          (HTeleportation || ETeleportation || \
                                 can_teleport(youmonst.data))

# define HTeleport_control      u.uintrinsic[TELEPORT_CONTROL]
# define ETeleport_control      worn_extrinsic(TELEPORT_CONTROL)
# define Teleport_control       (HTeleport_control || ETeleport_control || \
                                 control_teleport(youmonst.data))

# define HLevitation            u.uintrinsic[LEVITATION]
# define ELevitation            worn_extrinsic(LEVITATION)
# define Levitation             (HLevitation || ELevitation || \
                                 is_floater(youmonst.data))
        /* Can't touch surface, can't go under water; overrides all others */
# define Lev_at_will            (((HLevitation & I_SPECIAL) != 0L || \
                                 (ELevitation & W_ARTIFACT) != 0L) && \
                                 (HLevitation & ~(I_SPECIAL|TIMEOUT)) == 0L && \
                                 (ELevitation & ~W_ARTIFACT) == 0L && \
                                 !is_floater(youmonst.data))

# define EFlying                worn_extrinsic(FLYING)
# define Flying                 (EFlying || is_flyer(youmonst.data) || \
                                 (u.usteed && is_flyer(u.usteed->data)))
        /* May touch surface; does not override any others */

# define Wwalking               (worn_extrinsic(WWALKING) && \
                                 !Is_waterlevel(&u.uz))
        /* Don't get wet, can't go under water; overrides others except
           levitation */
        /* Wwalking is meaningless on water level */

# define HSwimming              u.uintrinsic[SWIMMING]
# define ESwimming              worn_extrinsic(SWIMMING)    /* [Tom] */
# define Swimming               (HSwimming || ESwimming || \
                                 is_swimmer(youmonst.data) || \
                                 (u.usteed && is_swimmer(u.usteed->data)))
        /* Get wet, don't go under water unless if amphibious */

# define HMagical_breathing     u.uintrinsic[MAGICAL_BREATHING]
# define EMagical_breathing     worn_extrinsic(MAGICAL_BREATHING)
# define Amphibious             (HMagical_breathing || EMagical_breathing || \
                                 amphibious(youmonst.data))
        /* Get wet, may go under surface */

# define Breathless             (HMagical_breathing || EMagical_breathing || \
                                 breathless(youmonst.data))

# define Engulfed               (u.uswallow)
# define Underwater             (u.uinwater)
/* Note that Underwater and u.uinwater are both used in code.
   The latter form is for later implementation of other in-water
   states, like swimming, wading, etc. */

# define HPasses_walls          u.uintrinsic[PASSES_WALLS]
# define EPasses_walls          worn_extrinsic(PASSES_WALLS)
# define Passes_walls           (HPasses_walls || EPasses_walls || \
                                 passes_walls(youmonst.data))


/*** Physical attributes ***/
# define HSlow_digestion        u.uintrinsic[SLOW_DIGESTION]
# define ESlow_digestion        worn_extrinsic(SLOW_DIGESTION)
# define Slow_digestion         (HSlow_digestion || ESlow_digestion)    /* KMH */

# define HHalf_spell_damage     u.uintrinsic[HALF_SPDAM]
# define EHalf_spell_damage     worn_extrinsic(HALF_SPDAM)
# define Half_spell_damage      (HHalf_spell_damage || EHalf_spell_damage)

# define HHalf_physical_damage  u.uintrinsic[HALF_PHDAM]
# define EHalf_physical_damage  worn_extrinsic(HALF_PHDAM)
# define Half_physical_damage   (HHalf_physical_damage || EHalf_physical_damage)

# define HRegeneration          u.uintrinsic[REGENERATION]
# define ERegeneration          worn_extrinsic(REGENERATION)
# define Regeneration           (HRegeneration || ERegeneration || \
                                 regenerates(youmonst.data))

# define HEnergy_regeneration   u.uintrinsic[ENERGY_REGENERATION]
# define EEnergy_regeneration   worn_extrinsic(ENERGY_REGENERATION)
# define Energy_regeneration    (HEnergy_regeneration || EEnergy_regeneration)

# define HProtection            u.uintrinsic[PROTECTION]
# define EProtection            worn_extrinsic(PROTECTION)
# define Protection             (HProtection || EProtection)

# define HProtection_from_shape_changers \
                                u.uintrinsic[PROT_FROM_SHAPE_CHANGERS]
# define EProtection_from_shape_changers \
                                worn_extrinsic(PROT_FROM_SHAPE_CHANGERS)
# define Protection_from_shape_changers \
                                (HProtection_from_shape_changers || \
                                 EProtection_from_shape_changers)

# define HPolymorph             u.uintrinsic[POLYMORPH]
# define EPolymorph             worn_extrinsic(POLYMORPH)
# define Polymorph              (HPolymorph || EPolymorph)

# define HPolymorph_control     u.uintrinsic[POLYMORPH_CONTROL]
# define EPolymorph_control     worn_extrinsic(POLYMORPH_CONTROL)
# define Polymorph_control      (HPolymorph_control || EPolymorph_control)

# define HUnchanging            u.uintrinsic[UNCHANGING]
# define EUnchanging            worn_extrinsic(UNCHANGING)
# define Unchanging             (HUnchanging || EUnchanging)    /* KMH */

# define HFast                  u.uintrinsic[FAST]
# define EFast                  worn_extrinsic(FAST)
# define Fast                   (HFast || EFast)
# define Very_fast              ((HFast & ~INTRINSIC) || EFast)

# define EReflecting            worn_extrinsic(REFLECTING)
# define Reflecting             (EReflecting || \
                                 (youmonst.data == &mons[PM_SILVER_DRAGON]))

# define Free_action            worn_extrinsic(FREE_ACTION) /* [Tom] */

# define Fixed_abil             worn_extrinsic(FIXED_ABIL)  /* KMH */

# define Lifesaved              worn_extrinsic(LIFESAVED)

# define Xray_vision            worn_extrinsic(XRAY_VISION)
# define XRAY_RANGE             3

/*** Possessions ***/
#define Uhave_amulet            carrying(AMULET_OF_YENDOR)
#define Uhave_bell              carrying(BELL_OF_OPENING)
#define Uhave_book              carrying(SPE_BOOK_OF_THE_DEAD)
#define Uhave_menorah           carrying(CANDELABRUM_OF_INVOCATION)
#define Uhave_questart          carrying_questart()

#endif /* YOUPROP_H */
