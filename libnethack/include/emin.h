/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) David Cohrs, 1990.                               */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef EMIN_H
# define EMIN_H

# include "align.h"

struct emin {
    aligntyp min_align; /* alignment of minion */
};

# define EMIN(mon)       ((struct emin *)&(mon)->mextra[0])
# define CONST_EMIN(mon) ((const struct emin *)&(mon)->mextra[0])

#endif /* EMIN_H */

