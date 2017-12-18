/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Mike Stephenson, Izchak Miller  1991.            */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef ALIGN_H
# define ALIGN_H

#include "global.h"
#include "quest.h"

typedef schar aligntyp; /* basic alignment type */

typedef struct align {  /* alignment & record */
    aligntyp type;
    int record;
} align;

/* bounds for "record" -- respect initial alignments of 10 */
# define ALIGNLIM       ((10L + (moves/200L)) > MIN_QUEST_ALIGN ? \
                         (10L + (moves/200L)) : MIN_QUEST_ALIGN)

# define A_NONE         (-128)  /* the value range of type */
/* A_{CHAOTIC,NEUTRAL,LAWFUL} moved to nethack_types.h */

# define A_COALIGNED    1
# define A_OPALIGNED    (-1)

# define AM_NONE        0
# define AM_CHAOTIC     1
# define AM_NEUTRAL     2
# define AM_LAWFUL      4

# define AM_MASK        7

# define AM_SPLEV_CO    3
# define AM_SPLEV_NONCO 7

# define AR_PIOUS       20
# define AR_DEVOUT      14
# define AR_FERVENT     9
# define AR_STRIDENT    4
# define AR_OK          3
# define AR_HALTING     1
# define AR_NOMINAL     0
# define AR_STRAYED     (-1)
# define AR_SINNED      (-4)
# define AR_TRANSGRESSED (-9)

# define Amask2align(x) ((aligntyp) ((!(x)) ? A_NONE \
             : ((x) == AM_LAWFUL) ? A_LAWFUL : ((int)x) - 2))
# define Align2amask(x) (((x) == A_NONE) ? AM_NONE \
             : ((x) == A_LAWFUL) ? AM_LAWFUL : (x) + 2)
# define Align2asave(x) ((x) == A_LAWFUL  ? 0 : \
                         (x) == A_NEUTRAL ? 1 : \
                         (x) == A_CHAOTIC ? 2 : \
                         3)
# define Asave2align(x) ((x) == 0 ? A_LAWFUL  : \
                         (x) == 1 ? A_NEUTRAL : \
                         (x) == 2 ? A_CHAOTIC : \
                         A_NONE)
# define Align2typ(x) ((x) > 0 ? A_LAWFUL :     \
                       (x) == -128 ? A_NONE :   \
                       (x) < 0 ? A_CHAOTIC :    \
                       A_NEUTRAL)

#endif /* ALIGN_H */
