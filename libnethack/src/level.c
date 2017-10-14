/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-14 */
/* Copyright (c) Sean Hunt, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

boolean has_sanctum(struct level *lev, xchar alignmask) {
    int sx, sy;
    for (sx = 0; sx < COLNO; ++sx)
        for (sy = 0; sy < ROWNO; ++sy) {
            xchar locmask = lev->locations[sx][sy].flags;
            if (IS_ALTAR(lev->locations[sx][sy].typ) &&
                (locmask & AM_SANCTUM) &&
                (locmask & alignmask))
                return TRUE;
        }

    return FALSE;
}

boolean has_terrain(struct level *lev, schar typ) {
    int sx, sy;
    for (sx = 0; sx < COLNO; ++sx)
        for (sy = 0; sy < ROWNO; ++sy)
            if (lev->locations[sx][sy].typ == typ)
                return TRUE;

    return FALSE;
}

