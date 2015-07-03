/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-16 */
/* Copyright (c) Dean Luick, with acknowledgements to Kevin Darcy */
/* and Dave Cohrs, 1990.                                          */
/* Copyright (c) 2013 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

/* The main core of the program no longer orders all the glyphs in sequence,
   like NetHack 3.4.3 does. However, we need such a sequential numbering for
   the purpose of placing tiles into some order within a tileset. */

#ifndef TILESEQUENCE_H
# define TILESEQUENCE_H

# include "global.h"
# include "timeout.h"
# include "monst.h"
# include "monsym.h"
# include "dungeon.h"
# include "pm.h"
# include "onames.h"
# include "objclass.h"
# include "trap.h"
# include "permonst.h"
# include "decl.h"
# include "mkroom.h"
# include "rm.h"
# include "display.h"
# include "brandings.h"

/* In stacking order: */
# define TILESEQ_CMAP_SIZE       S_cmap_COUNT                /* backgrounds */
# define TILESEQ_GENBRAND_SIZE   NHCURSES_GENBRANDING_COUNT  /* gen brandings */
# define TILESEQ_TRAP_SIZE       (TRAPNUM-1)                 /* traps */
# define TILESEQ_OBJ_SIZE        NUM_OBJECTS                 /* objects */
# define TILESEQ_INVIS_SIZE      1                           /* mem invisible */
# define TILESEQ_MON_SIZE        NUMMONS                     /* monsters */
# define TILESEQ_MONBRAND_SIZE   NHCURSES_MONBRANDING_COUNT  /* mon brandings */
# define TILESEQ_WARN_SIZE       WARNCOUNT                   /* warnings */
# define TILESEQ_EXPLODE_SIZE    (NUMEXPCHARS * EXPL_MAX)    /* explosions */
# define TILESEQ_ZAP_SIZE        (NUMZAPCHARS * NUM_ZAP)     /* beams */
# define TILESEQ_SWALLOW_SIZE    NUMSWALLOWCHARS             /* engulfed */
# define TILESEQ_EFFECT_SIZE     E_COUNT                     /* effects */

# define TILESEQ_CMAP_OFF        0
# define TILESEQ_GENBRAND_OFF    (TILESEQ_CMAP_OFF + TILESEQ_CMAP_SIZE)
# define TILESEQ_TRAP_OFF        (TILESEQ_GENBRAND_OFF + TILESEQ_GENBRAND_SIZE)
# define TILESEQ_OBJ_OFF         (TILESEQ_TRAP_OFF + TILESEQ_TRAP_SIZE)
# define TILESEQ_INVIS_OFF       (TILESEQ_OBJ_OFF + TILESEQ_OBJ_SIZE)
# define TILESEQ_MON_OFF         (TILESEQ_INVIS_OFF + TILESEQ_INVIS_SIZE)
# define TILESEQ_MONBRAND_OFF    (TILESEQ_MON_OFF + TILESEQ_MON_SIZE)
# define TILESEQ_WARN_OFF        (TILESEQ_MONBRAND_OFF + TILESEQ_MONBRAND_SIZE)
# define TILESEQ_EXPLODE_OFF     (TILESEQ_WARN_OFF + TILESEQ_WARN_SIZE)
# define TILESEQ_ZAP_OFF         (TILESEQ_EXPLODE_OFF + TILESEQ_EXPLODE_SIZE)
# define TILESEQ_SWALLOW_OFF     (TILESEQ_ZAP_OFF + TILESEQ_ZAP_SIZE)
# define TILESEQ_EFFECT_OFF      (TILESEQ_SWALLOW_OFF + TILESEQ_SWALLOW_SIZE)

# define TILESEQ_COUNT           (TILESEQ_EFFECT_OFF + TILESEQ_EFFECT_SIZE)
# define TILESEQ_INVALID_OFF     (-1)

/* Tile sequencing functions */
extern unsigned long long substitution_from_name(const char **);
extern int tileno_from_name(const char *, int);
extern int tileno_from_api_name(const char *, const char *, int);
extern const char *name_from_tileno(int);
extern const char *name_from_substitution(unsigned long long);
extern unsigned long long sensible_substitutions(int tileno);
extern int mutually_exclusive_substitutions(unsigned long long);

/* Not technically involved with tile sequencing, but this is the most sensible
   place to put it, as this file is already doing the "combine drawing.c with
   tile numbers" job. The purpose of this function is to get the NetHack core's
   opinion on how a given cchar should be rendered. In some cases, it may not
   have an opinion, in which case it returns ULONG_MAX (which is not a valid
   cchar). */
extern unsigned long cchar_from_tileno(int);

#endif
