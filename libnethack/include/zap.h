/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-27 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef ZAP_H
# define ZAP_H

# include "monattk.h"

/* Defines for zap.c */

# define ZT_MAGIC_MISSILE        (AD_MAGM-1)
# define ZT_FIRE                 (AD_FIRE-1)
# define ZT_COLD                 (AD_COLD-1)
# define ZT_SLEEP                (AD_SLEE-1)
# define ZT_DEATH                (AD_DISN-1) /* or disintegration */
# define ZT_LIGHTNING            (AD_ELEC-1)
# define ZT_POISON_GAS           (AD_DRST-1)
# define ZT_ACID                 (AD_ACID-1)
# define ZT_STUN                 (AD_STUN-1)
/* 9 is currently unassigned */

# define ZT_WAND(x)              (x)
# define ZT_SPELL(x)             (10+(x))
# define ZT_BREATH(x)            (20+(x))

# define BHIT_NONE     0x00
# define BHIT_MON      0x01
# define BHIT_DMON     0x02 /* displaced image was hit */
# define BHIT_OBJ      0x04
# define BHIT_OBSTRUCT 0x08
# define BHIT_SHOPDAM  0x10

# define is_hero_spell(type) ((type) >= 10 && (type) < 20)

#endif /* ZAP_H */
