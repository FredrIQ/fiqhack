/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-10-20 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef PROP_H
# define PROP_H

/*** What the properties are ***/
# define FIRE_RES                 1
# define COLD_RES                 2
# define SLEEP_RES                3
# define DISINT_RES               4
# define SHOCK_RES                5
# define POISON_RES               6
# define ACID_RES                 7
# define STONE_RES                8
/* note: for the first eight properties, MR_xxx == (1 << (xxx_RES - 1)) */
# define ADORNED                  9
# define REGENERATION             10
# define SEARCHING                11
# define SEE_INVIS                12
# define INVIS                    13
# define TELEPORT                 14
# define TELEPORT_CONTROL         15
# define POLYMORPH                16
# define POLYMORPH_CONTROL        17
# define LEVITATION               18
# define STEALTH                  19
# define AGGRAVATE_MONSTER        20
# define CONFLICT                 21
# define PROTECTION               22
# define PROT_FROM_SHAPE_CHANGERS 23
# define WARNING                  24
# define TELEPAT                  25
# define FAST                     26
# define STUNNED                  27
# define CONFUSION                28
# define SICK                     29
# define BLINDED                  30
# define SLEEPING                 31
# define LWOUNDED_LEGS            32
# define RWOUNDED_LEGS            33
# define STONED                   34
# define STRANGLED                35
# define HALLUC                   36
# define HALLUC_RES               37
# define FUMBLING                 38
# define JUMPING                  39
# define WWALKING                 40
# define HUNGER                   41
# define GLIB                     42
# define REFLECTING               43
# define LIFESAVED                44
# define ANTIMAGIC                45
# define DISPLACED                46
# define CLAIRVOYANT              47
# define VOMITING                 48
# define ENERGY_REGENERATION      49
# define MAGICAL_BREATHING        50
# define HALF_SPDAM               51
# define HALF_PHDAM               52
# define SICK_RES                 53
# define DRAIN_RES                54
# define WARN_UNDEAD              55
# define INVULNERABLE             56
# define FREE_ACTION              57
# define SWIMMING                 58
# define SLIMED                   59
# define FIXED_ABIL               60
# define FLYING                   61
# define UNCHANGING               62
# define PASSES_WALLS             63
# define SLOW_DIGESTION           64
# define INFRAVISION              65
# define WARN_OF_MON              66
# define XRAY_VISION              67
# define DETECT_MONSTERS          68
# define LAST_PROP                (DETECT_MONSTERS)

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
    os_ball,      /* punishment ball */
    os_chain,     /* punishment chain */

    os_saddle,    /* for riding */

    os_last_maskable = os_saddle,

/* "slot" codes that are not part of u.uobjslot, but are used for extrinsics */
    os_carried,   /* extrinsic comes from a carried artifact */
    os_invoked,   /* extrinsic comes from an invoked artifact */

    os_last_slot = os_invoked,

    os_invalid = -1,
};

enum equipmsg {
    em_silent,    /* no messages; used for theft, etc. */
    em_voluntary, /* as if the player was doing the action intentionally */
    em_magical,   /* for magical forcible removal: "<the ring> falls off!" */
};

/* This enum holds non-equipment object pointers that are tracked indirectly in
   struct you (mostly state for handling occupations that might be interrupted,
   which is why it can't go in struct turnstate). These are saved and restored,
   and also updated when the object is reallocated and nulled out when the
   object is freed. */
enum tracked_object_slots {
    tos_book,    /* book we were interrupted reading */
    tos_food,    /* food we were interrupted eating */
    tos_tin,     /* tin we were interrupted eating */

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
# define uball    EQUIP(os_ball)
# define uchain   EQUIP(os_chain)

/* Flags for intrinsics */

/* Timed properties */
# define TIMEOUT      0x00ffffffU       /* Up to 16 million turns */
/* Permanent properties */
# define FROMEXPER    0x01000000U       /* Gain/lose with experience, for role */
# define FROMRACE     0x02000000U       /* Gain/lose with experience, for race */
# define FROMOUTSIDE  0x04000000U       /* By corpses, prayer, thrones, etc. */
# define INTRINSIC    (FROMOUTSIDE|FROMRACE|FROMEXPER)
/* Control flags */
# define I_SPECIAL    0x10000000U       /* Property is controllable */

#endif /* PROP_H */
