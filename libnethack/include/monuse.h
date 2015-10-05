/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-10-05 */
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

    /* dungeon interaction */
    MUSE_TRAPDOOR,
    MUSE_TELEPORT_TRAP,
    MUSE_UPSTAIRS,
    MUSE_DOWNSTAIRS,
    MUSE_UP_LADDER,
    MUSE_DN_LADDER,
    MUSE_SSTAIRS,
    MUSE_POLY_TRAP,

    /* general items */
    MUSE_SCR,
    MUSE_POT,
    MUSE_THROW,
    MUSE_WAN,
    MUSE_SPE,
    MUSE_CONTAINER,
    MUSE_EAT,

    /* misc (TODO: merge tools into a single function) */
    MUSE_UNICORN_HORN,
    MUSE_DIRHORN,
    MUSE_BUGLE,
    MUSE_BAG_OF_TRICKS,
    MUSE_KEY,
    MUSE_INNATE_TPT,
    MUSE_BULLWHIP,
};


struct musable {
    struct obj *obj;
    int spell; /* for spells */
    int x;
    int y;
    int z;
    enum monuse use;
    /* =0, no capability; otherwise, different numbers. If it's an object, the
       object is also set (it's 0 otherwise). */
};

#endif

