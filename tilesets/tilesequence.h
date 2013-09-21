/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
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
# include "permonst.h"
# include "decl.h"
# include "mkroom.h"
# include "rm.h"
# include "display.h"

# define NUM_ZAP 8                   /* number of zap beam types */
# define MAXEXPCHARS E_explodecount  /* nuumber of characters in an explosion */

# define GLYPH_MON_OFF           0
# define GLYPH_PET_OFF           (NUMMONS        + GLYPH_MON_OFF)
# define GLYPH_INVIS_OFF         (NUMMONS        + GLYPH_PET_OFF)
# define GLYPH_DETECT_OFF        (1              + GLYPH_INVIS_OFF)
# define GLYPH_BODY_OFF          (NUMMONS        + GLYPH_DETECT_OFF)
# define GLYPH_RIDDEN_OFF        (NUMMONS        + GLYPH_BODY_OFF)
# define GLYPH_OBJ_OFF           (NUMMONS        + GLYPH_RIDDEN_OFF)
# define GLYPH_CMAP_OFF          (NUM_OBJECTS    + GLYPH_OBJ_OFF)
# define GLYPH_EXPLODE_OFF       ((MAXPCHARS - MAXEXPCHARS) + GLYPH_CMAP_OFF)
# define GLYPH_ZAP_OFF           ((MAXEXPCHARS * EXPL_MAX) + GLYPH_EXPLODE_OFF)
# define GLYPH_SWALLOW_OFF       ((NUM_ZAP << 2) + GLYPH_ZAP_OFF)
# define GLYPH_WARNING_OFF       ((NUMMONS << 3) + GLYPH_SWALLOW_OFF)
# define MAX_GLYPH               (WARNCOUNT      + GLYPH_WARNING_OFF)

#endif
