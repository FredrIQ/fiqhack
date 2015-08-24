/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by FIQ, 2015-08-24 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MONUSE_H
# define MONUSE_H

/* Defines for various types of stuff.  The order in which monsters prefer
 * to use them is determined by the order of the code logic, not the
 * numerical order in which they are defined.
 */
enum monuse {
    MUSE_NONE = 0,

    MUSE_SCR_TELEPORTATION,
    MUSE_WAN_TELEPORTATION_SELF,
    MUSE_POT_HEALING,
    MUSE_POT_EXTRA_HEALING,
    MUSE_WAN_DIGGING,
    MUSE_TRAPDOOR,
    MUSE_TELEPORT_TRAP,
    MUSE_UPSTAIRS,
    MUSE_DOWNSTAIRS,
    MUSE_WAN_CREATE_MONSTER,
    MUSE_SCR_CREATE_MONSTER,
    MUSE_UP_LADDER,
    MUSE_DN_LADDER,
    MUSE_SSTAIRS,
    MUSE_WAN_TELEPORTATION,
    MUSE_BUGLE,
    MUSE_UNICORN_HORN,
    MUSE_POT_FULL_HEALING,
    MUSE_LIZARD_CORPSE,
/*  MUSE_INNATE_TPT 9999
 * We cannot use this.  Since monsters get unlimited teleportation, if they
 * were allowed to teleport at will you could never catch them.  Instead,
 * assume they only teleport at random times, despite the inconsistency that if
 * you polymorph into one you teleport at will.
 */

    /* offensive items */
    MUSE_WAN_DEATH,
    MUSE_WAN_SLEEP,
    MUSE_WAN_FIRE,
    MUSE_WAN_COLD,
    MUSE_WAN_LIGHTNING,
    MUSE_WAN_MAGIC_MISSILE,
    MUSE_WAN_STRIKING,
    MUSE_WAN_UNDEAD_TURNING,
    MUSE_WAN_SLOW_MONSTER,
    MUSE_SCR_FIRE,
    MUSE_POT_PARALYSIS,
    MUSE_POT_BLINDNESS,
    MUSE_POT_CONFUSION,
    MUSE_FROST_HORN,
    MUSE_FIRE_HORN,
    MUSE_POT_ACID,
/*  MUSE_WAN_TELEPORTATION,*/
    MUSE_POT_SLEEPING,
    MUSE_SCR_EARTH,

    /* misc items */
    MUSE_POT_GAIN_LEVEL,
    MUSE_WAN_MAKE_INVISIBLE,
    MUSE_WAN_MAKE_INVISIBLE_SELF,
    MUSE_POT_INVISIBILITY,
    MUSE_POLY_TRAP,
    MUSE_WAN_POLYMORPH,
    MUSE_WAN_POLYMORPH_SELF,
    MUSE_POT_SPEED,
    MUSE_WAN_SPEED_MONSTER,
    MUSE_WAN_SPEED_MONSTER_SELF,
    MUSE_BULLWHIP,
    MUSE_POT_POLYMORPH,
    MUSE_SCR_REMOVE_CURSE,
};


struct musable {
    struct obj *offensive;
    struct obj *defensive;
    struct obj *misc;
    enum monuse has_offense, has_defense, has_misc;
    /* =0, no capability; otherwise, different numbers. If it's an object, the
       object is also set (it's 0 otherwise). */
};

#endif

