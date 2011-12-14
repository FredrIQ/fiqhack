/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <setjmp.h>

#include "nethack_types.h"

/*
 * Files expected to exist in the playground directory.
 */

#define RECORD	      "record"	/* file containing list of topscorers */
#define RUMORFILE     "rumors"	/* file with fortune cookies */
#define ORACLEFILE    "oracles" /* file with oracular information */
#define DATAFILE      "data"	/* file giving the meaning of symbols used */
#define HISTORY       "history" /* file giving nethack's history */
#define LICENSE       "license" /* file with license information */

#define LEV_EXT ".lev"		/* extension for special level files */


/* Assorted definitions that may depend on selections in config.h. */
#include <stdint.h>

typedef signed char	schar;
typedef unsigned char	uchar;
typedef signed char	boolean;		/* 0 or 1 */

/*
 * type xchar: small integers in the range 0 - 127, usually coordinates
 * although they are nonnegative they must not be declared unsigned
 * since otherwise comparisons with signed quantities are done incorrectly
 */
typedef schar	xchar;

#ifndef STRNCMPI
#  define strcmpi(a,b) strncmpi((a),(b),-1)
#endif


#define SIZE(x) (int)(sizeof(x) / sizeof(x[0]))


/* A limit for some NetHack int variables.  It need not, and for comparable
 * scoring should not, depend on the actual limit on integers for a
 * particular machine, although it is set to the minimum required maximum
 * signed integer for C (2^15 -1).
 */
#define LARGEST_INT	32767


#include "coord.h"
/*
 * Automatic inclusions for the subsidiary files.
 * Please don't change the order.  It does matter.
 */

#ifdef UNIX
#include "unixconf.h"
#endif

#ifdef WIN32
#include "ntconf.h"
#endif

/* Displayable name of this port; don't redefine if defined in *conf.h */
#ifndef PORT_ID
# ifdef UNIX
#  define PORT_ID	"Unix"
# endif
# ifdef WIN32
#  define PORT_ID	"Windows"
#  ifndef PORT_SUB_ID
#   define PORT_SUB_ID	"tty"
#  endif
# endif
#endif

/* Used for consistency checks of various data files; declare it here so
   that utility programs which include config.h but not hack.h can see it. */
struct version_info {
	unsigned int	incarnation;	/* actual version number */
	unsigned int	feature_set;	/* bitmask of config settings */
	unsigned int	entity_count;	/* # of monsters and objects */
};


/*
 * Configurable internal parameters.
 *
 * Please be very careful if you are going to change one of these.  Any
 * changes in these parameters, unless properly done, can render the
 * executable inoperative.
 */


#define MAXNROFROOMS	40	/* max number of rooms per level */
#define MAX_SUBROOMS	24	/* max # of subrooms in a given room */
#define DOORMAX		120	/* max number of doors per level */

#define PL_CSIZ		32	/* sizeof pl_character */
#define PL_FSIZ		32	/* fruit name */
#define PL_PSIZ		63	/* player-given names for pets, other
				 * monsters, objects */

#define MAXDUNGEON	16	/* current maximum number of dungeons */
#define MAXLEVEL	32	/* max number of levels in one dungeon */
#define MAXSTAIRS	1	/* max # of special stairways in a dungeon */
#define ALIGNWEIGHT	4	/* generation weight of alignment */

#define MAXULEV		30	/* max character experience level */

#define MAXMONNO	120	/* extinct monst after this number created */
#define MHPMAX		500	/* maximum monster hp */

#endif /* GLOBAL_H */
