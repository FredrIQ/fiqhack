/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* note for 3.1.0 and later: no longer manipulated by 'makedefs' */

#ifndef TRAP_H
# define TRAP_H

# include "global.h"
# include "dungeon.h"

union vlaunchinfo {
    short v_launch_otyp;        /* type of object to be triggered */
    coord v_launch2;    /* secondary launch point (for boulders) */
};

struct trap {
    struct trap *ntrap;
    unsigned ttyp:5;
    unsigned tseen:1;
    unsigned once:1;
    unsigned madeby_u:1;        /* So monsters may take offence when you trap
                                   them. Recognizing who made the trap isn't
                                   completely unreasonable, everybody has their 
                                   own style.  This flag is also needed when
                                   you untrap a monster.  It would be too easy
                                   to make a monster peaceful if you could set
                                   a trap for it and then untrap it. */
    xchar tx, ty;
    d_level dst;        /* destination for portals */
    coord launch;
    union vlaunchinfo vl;
# define launch_otyp      vl.v_launch_otyp
# define launch2          vl.v_launch2
};

# define newtrap()        malloc(sizeof(struct trap))
# define dealloc_trap(trap) free(trap)

/* reasons for statue animation */
# define ANIMATE_NORMAL   0
# define ANIMATE_SHATTER 1
# define ANIMATE_SPELL    2

/* reasons for animate_statue's failure */
# define AS_OK            0     /* didn't fail */
# define AS_NO_MON        1     /* makemon failed */
# define AS_MON_IS_UNIQUE 2     /* statue monster is unique */

/* Note: if adding/removing a trap, adjust trap_engravings[] in mklev.c */

/* unconditional traps */
# define NO_TRAP              0
# define ARROW_TRAP           1
# define DART_TRAP            2
# define ROCKTRAP             3
# define SQKY_BOARD           4
# define BEAR_TRAP            5
# define LANDMINE             6
# define ROLLING_BOULDER_TRAP 7
# define SLP_GAS_TRAP         8
# define RUST_TRAP            9
# define FIRE_TRAP            10
# define PIT                  11
# define SPIKED_PIT           12
# define HOLE                 13
# define TRAPDOOR             14
# define VIBRATING_SQUARE     15
# define TELEP_TRAP           16
# define LEVEL_TELEP          17
# define MAGIC_PORTAL         18
# define WEB                  19
# define STATUE_TRAP          20
# define MAGIC_TRAP           21
# define ANTI_MAGIC           22
# define POLY_TRAP            23
# define TRAPNUM 24

#endif /* TRAP_H */

