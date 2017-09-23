/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-07-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef PERMONST_H
# define PERMONST_H

# include "global.h"

/* SAVEBREAK: Cruft from old mextra system. In older save files, the very first thing to be
   saved was name length (16bit, but interestingly the name was capped at 63) and type of
   mon->mextra (16bit). The type was compared with the enum below (which didn't have
   "legacy" all over it) and the game was saved/restored based on it. To account for this,
   until a savebreak is performed, monst has 16 dummy bits on start, plus 16 bits of
   "legacy" extype. This is *always* saved as MX_(NO|YES), and if a restore finds anything
   else, the save file is assumed to follow the old system and is read accordingly. For
   the next savebreak, extypes_legacy will be removed entirely and a "has mextra" flag will
   be saved into the mflags part instead.
   (MX_(NO|YES) determines whether or not a monster has a mextra) */
enum mon_extypes {
    MX_NONE_LEGACY = 0,
    MX_EPRI_LEGACY,
    MX_EMIN_LEGACY,
    MX_EDOG_LEGACY,
    MX_ESHK_LEGACY,
    MX_EGD_LEGACY,
    MX_NO_LEGACY,
    MX_YES_LEGACY,
    MX_NO,
    MX_YES,
    MX_LAST_OLDLEGACY = MX_EGD_LEGACY,
    MX_LAST_LEGACY = MX_YES_LEGACY,
};

/* new mxflags. EMIN is no more. */
#define MX_NONE 0x00
#define MX_EDOG 0x01
#define MX_EPRI 0x02
#define MX_ESHK 0x04
#define MX_EGD  0x08
#define MX_NAME 0x10

/* This structure covers all attack forms.
 * aatyp is the gross attack type (eg. claw, bite, breath, ...)
 * adtyp is the damage type (eg. physical, fire, cold, spell, ...)
 * damn is the number of hit dice of damage from the attack.
 * damd is the number of sides on each die.
 *
 * Some attacks can do no points of damage.  Additionally, some can
 * have special effects *and* do damage as well.  If damn and damd
 * are set, they may have a special meaning.  For example, if set
 * for a blinding attack, they determine the amount of time blinded.
 */

struct attack {
    uchar aatyp;
    uchar adtyp, damn, damd;
};

/* Max # of attacks for any given monster.
 */

# define NATTK    6

/* Weight of a human body
 */

# define WT_HUMAN 1450

# ifndef ALIGN_H
#  include "align.h"
# endif
# include "monattk.h"
# include "monflag.h"

struct permonst {
    const char *mname;  /* full name */
    char mlet;  /* symbol */
    schar mlevel,       /* base monster level */
          mmove,        /* move speed */
          ac,   /* (base) armor class */
          mr;   /* (base) magic resistance */
    aligntyp maligntyp; /* basic monster alignment */
    unsigned short geno;        /* creation/geno mask value */
    struct attack mattk[NATTK]; /* attacks matrix */
    unsigned short cwt, /* weight of corpse */
          cnutrit;      /* its nutritional value */
    short pxtyp;        /* type of extension */
    uchar msound;       /* noise it makes (6 bits) */
    uchar msize;        /* physical size (3 bits) */
    unsigned int mflags1,       /* boolean bitflags */
        mflags2, mflags3;       /* more boolean bitflags */
    unsigned int mskill;        /* proficiency bitflags */
    uchar mresists;     /* resistances */
    uchar mconveys;     /* conveyed by eating */
    uchar mcolor;       /* color to use */
};

extern const struct permonst mons[];    /* the master list of monster types */

# define VERY_SLOW 3
# define SLOW_SPEED 9
# define NORMAL_SPEED 12/* movement rates */
# define FAST_SPEED 15
# define VERY_FAST 24

# define NON_PM     (-1)              /* "not a monster" */
# define LOW_PM     (NON_PM+1)        /* first monster in mons[] */
# define SPECIAL_PM PM_LONG_WORM_TAIL /* [normal] < ~ < [special] */
        /* mons[SPECIAL_PM] through mons[NUMMONS-1], inclusive, are never
           generated randomly and cannot be polymorphed into */

#endif /* PERMONST_H */
