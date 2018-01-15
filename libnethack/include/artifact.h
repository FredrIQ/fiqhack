/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef ARTIFACT_H
# define ARTIFACT_H

# include "global.h"
# include "align.h"
# include "permonst.h"

# define SPFX_NONE     0x0000000L /* no special effects, just a bonus */
# define SPFX_NOGEN    0x0000001L /* item is special, bequeathed by gods */
# define SPFX_RESTR    0x0000002L /* item is restricted - can't be named */
# define SPFX_INTEL    0x0000004L /* item is self-willed - intelligent */
# define SPFX_SPEAK    0x0000008L /* item can speak (not implemented) */
# define SPFX_SEEK     0x0000010L /* item helps you search for things */
# define SPFX_WARN     0x0000020L /* item warns you of danger */
# define SPFX_ATTK     0x0000040L /* item has a special attack (attk) */
# define SPFX_DEFN     0x0000080L /* item has a special defence (defn) */
# define SPFX_DRLI     0x0000100L /* drains a level from monsters */
# define SPFX_SEARCH   0x0000200L /* helps searching */
# define SPFX_BEHEAD   0x0000400L /* beheads monsters */
# define SPFX_HALRES   0x0000800L /* blocks hallucinations */
# define SPFX_ESP      0x0001000L /* ESP (like amulet of ESP) */
# define SPFX_STLTH    0x0002000L /* Stealth */
# define SPFX_REGEN    0x0004000L /* Regeneration */
# define SPFX_EREGEN   0x0008000L /* Energy Regeneration */
# define SPFX_HSPDAM   0x0010000L /* 1/2 spell damage (on player) in combat */
# define SPFX_HPHDAM   0x0020000L /* 1/2 physical damage (on player) in combat */
# define SPFX_TCTRL    0x0040000L /* Teleportation Control */
# define SPFX_LUCK     0x0080000L /* Increase Luck (like Luckstone) */
# define SPFX_DMONS    0x0100000L /* attack bonus on one monster type */
# define SPFX_DCLAS    0x0200000L /* attack bonus on monsters w/ symbol mtype */
# define SPFX_DFLAG1   0x0400000L /* attack bonus on monsters w/ mflags1 flag */
# define SPFX_DFLAG2   0x0800000L /* attack bonus on monsters w/ mflags2 flag */
# define SPFX_DALIGN   0x1000000L /* attack bonus on non-aligned monsters */
# define SPFX_DBONUS   0x1F00000L /* attack bonus mask */
# define SPFX_XRAY     0x2000000L /* gives X-RAY vision to player */
# define SPFX_REFLECT  0x4000000L/* Reflection */
# define SPFX_WTREDUC  0x8000000L/* Reduced weight (halved) */
# define SPFX_FREEACT 0x10000000L


struct artifact {
    const char *name;
    long cost;  /* price when sold to hero (default 100 x base cost) */
    unsigned long spfx; /* special effect from wielding/wearing */
    unsigned long cspfx;        /* special effect just from carrying obj */
    unsigned long mtype;        /* monster type, symbol, or flag */
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
