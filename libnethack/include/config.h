/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-29 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef CONFIG_H        /* make sure the compiler does not see the typedefs
                           twice */
# define CONFIG_H

/* swap byte order while reading and writing saves and bones files. This should
 * enable portability between architectures.
 * If you do not intend to import or export saves or bones files, this option
 * doesn't matter.
 * Endianness can be autodetected on some systems. */

/* #define IS_BIG_ENDIAN */

# define PANICLOG "paniclog"    /* log of panic and impossible events */

/* include bones ghost kills in livelog; almost certainly breaks save compat. */
/* # define LIVELOG_BONES_KILLER */


# include "global.h"    /* Define everything else according to choices above */

#endif /* CONFIG_H */

