/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-10 */
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
    NO_PROP                  = 0,
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
    CANCELLED                = 56,
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
    SLOW                     = 69,
    ZOMBIE                   = 70,
    WATERPROOF               = 71,
/*  DEATH_RES                = 72, TODO */
    LAST_PROP                = WATERPROOF,
    INVALID_PROP             = -1,
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

/* "slot" codes that aren't extrinsics at all, but intrinsics to give a reason
   as to why you have something */
    os_timeout,        /* from potion, etc. */
    os_polyform,       /* does not appear in intrinsic or extrinsic field */
    os_birthopt,       /* from a birth option / difficulty level */
    os_circumstance,   /* blindness from unconciousness, fumbling from ice */
    os_role,           /* from role + level combination */
    os_race,           /* from race + level combination */
    os_outside,        /* conferred or granted (corpse, throne, etc.) */
    os_special,        /* only for blindness by creaming for now */
    os_blocked,        /* property is blocked (only returned with allow_blocked) */

/* pseudo-slots */
    os_inctimeout,     /* increased timeout */
    os_dectimeout,     /* decreased timeout by decrease_property_timers() */
    os_cache,          /* whether the cache is valid or not */

    os_last = os_cache,

    os_invalid = -1,
};

static_assert(os_last < sizeof (unsigned) * CHAR_BIT,
              "Integer overflow in objslot.");

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
    occ_first = occ_book,
    occ_last = occ_prepare
};

#define OCM(reason) ocm_##reason = 1 << occ_##reason
enum FLAG_ENUM occupation_mask {
    ocm_none = 0,
    OCM(book),
    OCM(food),
    OCM(tin),
    ocm_edibility = ocm_food | ocm_tin,
    OCM(trap),
    OCM(dig),
    OCM(lock),
    OCM(equip),
    OCM(move),
    OCM(travel),
    OCM(autoexplore),
    ocm_farmove = ocm_move | ocm_travel | ocm_autoexplore,
    OCM(wipe),
    OCM(wait),
    OCM(search),
    ocm_rest = ocm_wait | ocm_search,
    OCM(prepare),
    ocm_all = -1,
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

/* mintrinsic is now a 16bit field. There is however 19 extrinsic slots,
   and thus TIMEOUT/FROMOUTSIDE can no longer point to the mintrinsic
   bitfield and the slot enum simultaneously. Thus,
   (TIMEOUT|FROMOUTSIDE)_RAW points to the mintrinsic while
   (TIMEOUT|FROMOUTSIDE) points to the slots (os_timeout/os_outside).
   Do not use the RAW macros unless you need to manipulate mintrinsic. */
# define TIMEOUT_RAW  0x7fffU      /* Up to 32767 turns */
# define FROMOUTSIDE_RAW 0x8000U
/* Permanent properties */
# define FROMROLE     ((unsigned)W_MASK(os_role))
# define FROMRACE     ((unsigned)W_MASK(os_race))
# define FROMPOLY     ((unsigned)W_MASK(os_polyform))
# define FROMOUTSIDE  ((unsigned)W_MASK(os_outside))
# define TIMEOUT      ((unsigned)W_MASK(os_timeout))
# define INTRINSIC    (FROMOUTSIDE|FROMRACE|FROMROLE|FROMPOLY)
# define EXTRINSIC    ~INTRINSIC
/* Control flags */
# define I_SPECIAL    ((unsigned)W_MASK(os_special))
# define ANY_PROPERTY ((unsigned)-1)

#endif /* PROP_H */
