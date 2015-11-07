/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef PROP_H
# define PROP_H

# include "compilers.h"
# include <limits.h>

/*** What the properties are ***/
/* Please do not change any of these numbers (although you can feel free to add
   new numbers at the end); this is needed to keep the xlogfile usable between
   versions. */
enum youprop {
    FIRE_RES                 = 1,
    COLD_RES                 = 2,
    SLEEP_RES                = 3,
    DISINT_RES               = 4,
    SHOCK_RES                = 5,
    POISON_RES               = 6,
    ACID_RES                 = 7,
    STONE_RES                = 8,
/* note: for the first eight properties, MR_xxx == (1 << (xxx_RES - 1)) */
    ADORNED                  = 9,
    REGENERATION             = 10,
    SEARCHING                = 11,
    SEE_INVIS                = 12,
    INVIS                    = 13,
    TELEPORT                 = 14,
    TELEPORT_CONTROL         = 15,
    POLYMORPH                = 16,
    POLYMORPH_CONTROL        = 17,
    LEVITATION               = 18,
    STEALTH                  = 19,
    AGGRAVATE_MONSTER        = 20,
    CONFLICT                 = 21,
    PROTECTION               = 22,
    PROT_FROM_SHAPE_CHANGERS = 23,
    WARNING                  = 24,
    TELEPAT                  = 25,
    FAST                     = 26,
    STUNNED                  = 27,
    CONFUSION                = 28,
    SICK                     = 29,
    BLINDED                  = 30,
    SLEEPING                 = 31,
    LWOUNDED_LEGS            = 32,
    RWOUNDED_LEGS            = 33,
    STONED                   = 34,
    STRANGLED                = 35,
    HALLUC                   = 36,
    HALLUC_RES               = 37,
    FUMBLING                 = 38,
    JUMPING                  = 39,
    WWALKING                 = 40,
    HUNGER                   = 41,
    GLIB                     = 42,
    REFLECTING               = 43,
    LIFESAVED                = 44,
    ANTIMAGIC                = 45,
    DISPLACED                = 46,
    CLAIRVOYANT              = 47,
    VOMITING                 = 48,
    ENERGY_REGENERATION      = 49,
    MAGICAL_BREATHING        = 50,
    HALF_SPDAM               = 51,
    HALF_PHDAM               = 52,
    SICK_RES                 = 53,
    DRAIN_RES                = 54,
    WARN_UNDEAD              = 55,
    /* 56 unused for numbering, was a broken duplicate of u.uinvulnerable */
    FREE_ACTION              = 57,
    SWIMMING                 = 58,
    SLIMED                   = 59,
    FIXED_ABIL               = 60,
    FLYING                   = 61,
    UNCHANGING               = 62,
    PASSES_WALLS             = 63,
    SLOW_DIGESTION           = 64,
    INFRAVISION              = 65,
    WARN_OF_MON              = 66,
    XRAY_VISION              = 67,
    DETECT_MONSTERS          = 68,
    LAST_PROP                = DETECT_MONSTERS,
};

/* This enum holds all the equipment that is tracked indirectly in struct you;
   that is, u.uequip is an array of pointers into other chains. These objects
   are recalculated from owornmask during game restore, rather than being saved
   in the binary save. */
enum objslot {
/* Armor */
    os_arm,       /* body armor */
    os_armc,      /* cloak */
    os_armh,      /* helmet/hat */
    os_arms,      /* shield */
    os_armg,      /* gloves/gauntlets */
    os_armf,      /* footwear */
    os_armu,      /* undershirt */

    os_last_armor = os_armu,

/* Other worn equipment */
    os_amul,      /* amulet */
    os_ringl,     /* left ring */
    os_ringr,     /* right ring */
    os_tool,      /* eyewear */

    os_last_worn = os_tool,

/* Other non-worn equipment */
    os_wep,       /* wielded weapon */
    os_quiver,    /* quiver for the "fire" command */
    os_swapwep,   /* readied or offhanded weapon */

    os_last_equip = os_swapwep,

/* Other object slot codes that appear in a wear mask */
    os_saddle,    /* for riding; also used for extrinsics from riding */

    os_last_maskable = os_saddle,

/* "slot" codes that are not part of u.uobjslot, but are used for extrinsics */
    os_carried,   /* extrinsic comes from a carried artifact */
    os_invoked,   /* extrinsic comes from an invoked artifact */

    os_last_slot = os_invoked,

/* "slot" codes that aren't extrinsics at all, but give reasons why you
   might have a property */
    os_timeout,        /* from potion, etc. */
    os_polyform,       /* does not appear in intrinsic or extrinsic field */
    os_birthopt,       /* from a birth option / difficulty level */
    os_circumstance,   /* currently only blindness from unconciousness */

/* these numbers are for numerical compatibility with 3.4.3, in order to keep
   caps the same as before; do not change them without also changing the code
   for intrinsics that time out */
    os_role = 24,      /* from role + level combination */
    os_race = 25,      /* from race + level combination */
    os_outside = 26,   /* conferred or granted (corpse, throne, etc.) */

    os_special = 28,   /* magic number used for controlled levitation */

    os_invalid = -1,
};

static_assert(os_circumstance < os_role, "Too many equipment slots!");
static_assert(os_special < sizeof (unsigned) * CHAR_BIT,
              "NetHack requires 32-bit integers.");

enum equipmsg {
    em_silent,    /* no messages; used for theft, etc. */
    em_voluntary, /* as if the player was doing the action intentionally */
    em_magicheal, /* item is removed via magic to heal a status */
};

/* This enum holds object pointers (other than equipped equipment) that are
   tracked indirectly in struct you (mostly state for handling occupations that
   might be interrupted, which is why it can't go in struct turnstate). These
   are saved and restored, and also updated when the object is reallocated and
   nulled out when the object is freed. There is also an associated integer with
   each that is designed to track progress on the occupation in question; not
   all of these are used, although many of them are. */
enum tracked_object_slots {
    tos_book,    /* book we were interrupted reading */
    tos_food,    /* food we were interrupted eating */
    tos_tin,     /* tin we were interrupted eating */
    tos_trap,    /* trap we were interrupted setting */
    tos_dig,     /* no associated item, used for progress only */
    tos_lock,    /* chest we were interrupted lockpicking or forcing */
    tos_ball,    /* iron ball we're punished with */
    tos_chain,   /* chain the ball is attached with */
    tos_limit,   /* occupation progress for wait/search/move; the tracked
                    object is not used, just the progress */

    tos_first_equip,   /* equipment we were interrupted equipping */
    tos_last_equip = tos_first_equip + os_last_maskable,

    tos_last_slot = tos_last_equip,
};
/* Ditto, for struct turnstate. These objects are not saved or restored, and
   have to be NULL between turns, just like with everything else in the
   turnstate struct. */
enum turntracked_object_slots {
    ttos_wand,    /* item that is currently being zapped */

    ttos_last_slot = ttos_wand,
};

/* Multi-turn commands. Many things that are on this list are also on the
   tracked_object_slots list, but there isn't a 1-to-1 correspondence.

   The main purpose of this is to know whether to interrupt an action when it no
   longer becomes applicable. For instance, without this, a door getting
   magically opened while you were lockpicking it with the "open" command would
   cause you to close the door on the next turn. The occupation being set allows
   the code to know whether to interrupt or not. */
enum occupation {
    occ_none = 0,
    occ_book,        /* reading a spellbook */
    occ_food,        /* eating */
    occ_tin,         /* opening a tin */
    occ_trap,        /* setting a trap */
    occ_dig,         /* digging */
    occ_lock,        /* picking or forcing a lock */
    occ_equip,       /* changing equipment */
    occ_move,        /* moving */
    occ_travel,      /* travelling */
    occ_autoexplore, /* autoexploring */
    occ_wipe,        /* wiping your face */
    occ_wait,        /* waiting */
    occ_search,      /* searching */
    occ_prepare,     /* e.g. removing a shield to be able to use a mattock */
};

/* Occupations generally track objects, but some track locations, or both. */
enum tracked_location {
    tl_trap,         /* location where a trap is set */
    tl_dig,          /* location which is being dug out */
    tl_lock,         /* location of a door that is having its lock picked */

    tl_last_slot = tl_lock,
};

# define EQUIP(oslot) which_armor(&youmonst, oslot)
# define W_MASK(oslot) (1 << (oslot))

/* W_ARMOR is the bitwise or of all W_MASKs up to and including os_last_armor,
   etc.. To generate this, we double and subtract 1. */
# define W_ARMOR     (W_MASK(os_last_armor) * 2 - 1)
# define W_WORN      (W_MASK(os_last_worn)  * 2 - 1)
# define W_EQUIP     (W_MASK(os_last_equip) * 2 - 1)
# define W_MASKABLE  (W_MASK(os_last_maskable) * 2 - 1)
# define W_RING      (W_MASK(os_ringl) | W_MASK(os_ringr))
# define W_ARTIFACT  (W_MASK(os_carried) | W_MASK(os_invoked))

/* Ring extrinsic values are also used for wounded legs. */
# define LEFT_SIDE   W_MASK(os_ringl)
# define RIGHT_SIDE  W_MASK(os_ringr)
# define BOTH_SIDES  (LEFT_SIDE | RIGHT_SIDE)

# define uarm     EQUIP(os_arm)
# define uarmc    EQUIP(os_armc)
# define uarmh    EQUIP(os_armh)
# define uarms    EQUIP(os_arms)
# define uarmg    EQUIP(os_armg)
# define uarmf    EQUIP(os_armf)
# define uarmu    EQUIP(os_armu)
# define uamul    EQUIP(os_amul)
# define uleft    EQUIP(os_ringl)
# define uright   EQUIP(os_ringr)
# define ublindf  EQUIP(os_tool)
# define uwep     EQUIP(os_wep)
# define uswapwep EQUIP(os_swapwep)
# define uquiver  EQUIP(os_quiver)
# define uball    (u.utracked[tos_ball])
# define uchain   (u.utracked[tos_chain])

/* Flags for intrinsics

   TODO: Give these better names. The current names are for 3.4.3
   compatibility. */

/* Timed properties */
# define TIMEOUT      0x00ffffffU      /* Up to 16 million turns */
/* Permanent properties */
# define FROMEXPER    ((unsigned)W_MASK(os_role))
# define FROMRACE     ((unsigned)W_MASK(os_race))
# define FROMOUTSIDE  ((unsigned)W_MASK(os_outside))
# define INTRINSIC    (FROMOUTSIDE|FROMRACE|FROMEXPER)
/* Control flags */
# define I_SPECIAL    ((unsigned)W_MASK(os_special))
# define ANY_PROPERTY ((unsigned)-1)

/* Just to make sure there are no arithmetic mistakes... */
static_assert(TIMEOUT == FROMEXPER-1, "Bad intrinsic timeout");

#endif /* PROP_H */

