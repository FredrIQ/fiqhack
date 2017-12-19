/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */
/* track.c - version 1.0.2 */

#include "hack.h"

#define UTSZ    50

static int utcnt, utpnt;
static coord utrack[UTSZ];


void
initrack(void)
{
    utcnt = utpnt = 0;
    memset(utrack, 0, sizeof utrack);
}


/* add to track */
void
settrack(void)
{
    if (utcnt < UTSZ)
        utcnt++;
    if (utpnt == UTSZ)
        utpnt = 0;
    utrack[utpnt].x = u.ux;
    utrack[utpnt].y = u.uy;
    utpnt++;
}


coord *
gettrack(int x, int y)
{
    int cnt, ndist;
    coord *tc;

    cnt = utcnt;
    for (tc = &utrack[utpnt]; cnt--;) {
        if (tc == utrack)
            tc = &utrack[UTSZ - 1];
        else
            tc--;
        ndist = distmin(x, y, tc->x, tc->y);

        if (ndist <= 1)
            return ndist ? tc : 0;
    }
    return NULL;
}


void
save_track(struct memfile *mf)
{
    int i;

    mtag(mf, 0, MTAG_TRACK);
    for (i = 0; i < UTSZ; i++) {  /* savemap: ignore */
        mwrite8(mf, utrack[i].x);
        mwrite8(mf, utrack[i].y);
    } /* savemap: 784 bits (more) */
    mwrite32(mf, utcnt);
    mwrite32(mf, utpnt);
}


void
restore_track(struct memfile *mf)
{
    int i;

    for (i = 0; i < UTSZ; i++) {
        utrack[i].x = mread8(mf);
        utrack[i].y = mread8(mf);
    }
    utcnt = mread32(mf);
    utpnt = mread32(mf);
}

/*track.c*/
