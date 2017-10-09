/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MFNDPOS_H
# define MFNDPOS_H

# define ALLOW_PEACEFUL 0x00010000L     /* can displace peacefuls */
# define ALLOW_TRAPS    0x00020000L     /* can enter traps */
# define ALLOW_MUXY     0x00040000L     /* can attack your believed location */
# define ALLOW_M        0x00080000L     /* can attack other monsters */
# define ALLOW_TM       0x00100000L     /* can attack tame monsters */
# define ALLOW_ALL      (ALLOW_MUXY | ALLOW_M | ALLOW_TM | ALLOW_TRAPS)
# define NOTONL         0x00200000L     /* avoids direct line to player */
# define OPENDOOR       0x00400000L     /* opens closed doors */
# define UNLOCKDOOR     0x00800000L     /* unlocks locked doors */
# define BUSTDOOR       0x01000000L     /* breaks any doors */
# define ALLOW_ROCK     0x02000000L     /* pushes rocks */
# define ALLOW_WALL     0x04000000L     /* walks thru walls */
# define ALLOW_DIG      0x08000000L     /* digs */
# define ALLOW_BARS     0x10000000L     /* may pass thru iron bars */
# define ALLOW_SANCT    0x20000000L     /* enters temples */
# define ALLOW_SSM      0x40000000L     /* ignores scare monster */
# define NOGARLIC       0x80000000UL    /* hates garlic */

#endif /* MFNDPOS_H */

