/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef ARTIFACT_H
# define ARTIFACT_H

# include "global.h"
# include "align.h"
# include "permonst.h"

# define SPFX_NONE    0x0000000L /* no special effects, just a bonus */
# define SPFX_NOGEN   0x0000001L /* item is special, bequeathed by gods */
# define SPFX_RESTR   0x0000002L /* item is restricted - can't be named */
# define SPFX_INTEL   0x0000004L /* item is self-willed - intelligent */
# define SPFX_SPEAK   0x0000008L /* item can speak */
# define SPFX_WARN    0x0000010L /* item warns you of danger */
# define SPFX_BEHEAD  0x0000020L /* beheads monsters */
# define SPFX_HALRES  0x0000040L /* blocks hallucinations */
# define SPFX_ESP     0x0000080L /* ESP (like amulet of ESP) */
# define SPFX_STLTH   0x0000100L /* Stealth */
# define SPFX_REGEN   0x0000200L /* Regeneration */
# define SPFX_EREGEN  0x0000400L /* Energy Regeneration */
# define SPFX_HSPDAM  0x0000800L /* 1/2 spell damage (on player) in combat */
# define SPFX_HPHDAM  0x0001000L /* 1/2 physical damage (on player) in combat */
# define SPFX_TCTRL   0x0002000L /* Teleportation Control */
# define SPFX_LUCK    0x0004000L /* Increase Luck (like Luckstone) */
# define SPFX_WARNMON 0x0008000L /* warning against monster type, or monster detection */
# define SPFX_XRAY    0x0010000L /* gives X-RAY vision to player */
# define SPFX_REFLECT 0x0020000L /* Reflection */

/* What "match" refers to: a permonst, "letter", alignment, flag... */
enum mon_matchtyp {
    MTYP_ALL,
    MTYP_PM,
    MTYP_S,
    MTYP_M1,
    MTYP_M2,
    MTYP_M3,
    MTYP_ALIGN,
};

/* Contains data about matching a specific monster class.
   If matchtyp is MTYP_ALL, match everything. */
struct monclass {
    enum mon_matchtyp matchtyp;
    int match;
};

struct artifact {
    const char *name;
    long cost;  /* price when sold to hero (default 100 x base cost) */
    unsigned long spfx; /* special effect from wielding/wearing */
    unsigned long cspfx;        /* special effect just from carrying obj */
    struct monclass mtype;      /* monster type, symbol, or flag */
    struct attack attk, defn, cary;
    aligntyp alignment; /* alignment of bequeathing gods */
    uchar inv_prop;     /* property obtained by invoking artifact */
    short role; /* character role associated with */
    short race; /* character race associated with */
    short otyp;
};

enum artigen_type {
    ag_none,
    ag_other, /* This is the value of old existing artifacts before this was implemented */
    ag_gift,
    ag_wish,
    ag_monwish,
    ag_named,
    ag_bones,
};

# define nartifact_gifted() nartifact_value(ag_gift)
# define nartifact_wished() (nartifact_value(ag_wish) + nartifact_value(ag_monwish))

/* invoked properties with special powers */
# define TAMING         (LAST_PROP+1)
# define HEALING        (LAST_PROP+2)
# define ENERGY_BOOST   (LAST_PROP+3)
# define UNTRAP         (LAST_PROP+4)
# define CHARGE_OBJ     (LAST_PROP+5)
# define LEV_TELE       (LAST_PROP+6)
# define CREATE_PORTAL  (LAST_PROP+7)
# define ENLIGHTENING   (LAST_PROP+8)
# define CREATE_AMMO    (LAST_PROP+9)
# define SELF_UNCURSE   (LAST_PROP+10)

#endif /* ARTIFACT_H */
