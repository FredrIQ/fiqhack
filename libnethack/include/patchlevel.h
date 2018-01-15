/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-16 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Incrementing EDITLEVEL can be used to force invalidation of old bones
 * and save files.
 */
#define EDITLEVEL 0

#define COPYRIGHT_BANNER_A \
"FIQHack, Copyright (C) 2017; based on NetHack, Copyright (C) 1985-2006"

#define COPYRIGHT_BANNER_B \
"  FIQHack is based on NetHack4 by Alex Smith and many others."

#define COPYRIGHT_BANNER_C \
"  NetHack by Stichting Mathematisch Centrum and M. Stephenson. See " \
    "dat/licence."

/*
 * If two or more successive releases have compatible data files, define
 * this with the version number of the oldest such release so that the
 * new release will accept old save and bones files.  The format is
 *      0xMMmmPPeeL
 * 0x = literal prefix "0x", MM = major version, mm = minor version,
 * PP = patch level, ee = edit level, L = literal suffix "L",
 * with all four numbers specified as two hexadecimal digits.
 * Keep this consistent with nethack.h.
 */
#define VERSION_COMPATIBILITY 0x04030000L       /* 4.3.0-0 */


/*patchlevel.h*/
