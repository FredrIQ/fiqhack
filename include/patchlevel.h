/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

/* NitroHack 4.0.0 */
#define VERSION_MAJOR	4
#define VERSION_MINOR	0
/*
 * PATCHLEVEL is updated for each release.
 */
#define PATCHLEVEL	0
/*
 * Incrementing EDITLEVEL can be used to force invalidation of old bones
 * and save files.
 */
#define EDITLEVEL	0

#define COPYRIGHT_BANNER_A \
"NitroHack, Copyright 1985-2011"

#define COPYRIGHT_BANNER_B \
"         By Stichting Mathematisch Centrum and M. Stephenson."

#define COPYRIGHT_BANNER_C \
"         See license for details."

/*
 * If two or more successive releases have compatible data files, define
 * this with the version number of the oldest such release so that the
 * new release will accept old save and bones files.  The format is
 *	0xMMmmPPeeL
 * 0x = literal prefix "0x", MM = major version, mm = minor version,
 * PP = patch level, ee = edit level, L = literal suffix "L",
 * with all four numbers specified as two hexadecimal digits.
 */
#define VERSION_COMPATIBILITY 0x04000000L	/* 4.0.0-0 */


/*patchlevel.h*/
