/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Izchak Miller, 1989.                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef EPRI_H
# define EPRI_H

# include "global.h"
# include "align.h"
# include "dungeon.h"

struct epri {
    aligntyp shralign;  /* alignment of priest's shrine */
    /* leave as first field to match emin */
    schar shroom;       /* index in rooms */
    coord shrpos;       /* position of shrine */
    d_level shrlevel;   /* level (& dungeon) of shrine */
};

# define EPRI(mon)       ((struct epri *)&(mon)->mextra[0])
# define CONST_EPRI(mon) ((const struct epri *)&(mon)->mextra[0])

/* A priest without ispriest is a roaming priest without a shrine, so
 * the fields (except shralign, which becomes only the priest alignment)
 * are available for reuse.
 */
# define renegade shroom

#endif /* EPRI_H */

