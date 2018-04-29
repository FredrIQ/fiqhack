/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-04-29 */
/* Copyright (c) 1990 by Jean-Christophe Collet                   */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef RECT_H
# define RECT_H

# include "global.h"

struct nhrect {
    xchar lx, ly;
    xchar hx, hy;
};

/* According to 1.3d and 3.0.0 source, these represent the minimum amount of
   required space around a room in any direction, counting the walls as part of
   the room.
   Rooms' XLIM and YLIM buffers can overlap with other rooms. */
#define XLIM    4
#define YLIM    3

#endif /* RECT_H */
