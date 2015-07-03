/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef VAULT_H
# define VAULT_H

# include "global.h"
# include "dungeon.h"

# define FCSIZ (ROWNO+COLNO)
struct fakecorridor {
    xchar fx, fy, ftyp;
};

struct egd {
    int fcbeg, fcend;   /* fcend: first unused pos */
    int vroom;  /* room number of the vault */
    xchar gdx, gdy;     /* goal of guard's walk */
    xchar ogx, ogy;     /* guard's last position */
    d_level gdlevel;    /* level (& dungeon) guard was created in */
    xchar warncnt;      /* number of warnings to follow */
    unsigned gddone:1;  /* true iff guard has released player */
    struct fakecorridor fakecorr[FCSIZ];
};

# define EGD(mon)       ((struct egd *)&(mon)->mextra[0])
# define CONST_EGD(mon) ((const struct egd *)&(mon)->mextra[0])

#endif /* VAULT_H */

