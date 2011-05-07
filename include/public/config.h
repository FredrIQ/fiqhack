/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef CONFIG_H /* make sure the compiler does not see the typedefs twice */
#define CONFIG_H


/*
 * Section 1:	Operating and window systems selection.
 *		Select the version of the OS you are using.
 */

#define UNIX		/* delete if no fork(), exec() available */


#ifdef WIN32
# undef UNIX
# define STRNCMPI
# define STRCMPI
#endif

#if defined(__linux__) && defined(__GNUC__) && !defined(_GNU_SOURCE)
/* ensure _GNU_SOURCE is defined before including any system headers */
# define _GNU_SOURCE
#endif


/*
 * Section 2:	Some global parameters and filenames.
 *		Commenting out WIZARD, LOGFILE, NEWS or PANICLOG removes that
 *		feature from the game; otherwise set the appropriate wizard
 *		name.  LOGFILE, NEWS and PANICLOG refer to files in the
 *		playground.
 */

#ifndef WIZARD		/* allow for compile-time or Makefile changes */
# define WIZARD  "daniel" /* the person allowed to use the -D option */
#endif

#define LOGFILE "logfile"	/* larger file for debugging purposes */
#define NEWS "news"		/* the file containing the latest hack news */
#define PANICLOG "paniclog"	/* log of panic and impossible events */

/*
 *	Data librarian.  Defining DLB places most of the support files into
 *	a tar-like file, thus making a neater installation.  See *conf.h
 *	for detailed configuration.
 */
/* #define DLB */	/* not supported on all platforms */

/*
 *	Defining INSURANCE slows down level changes, but allows games that
 *	died due to program or system crashes to be resumed from the point
 *	of the last level change, after running a utility program.
 */
#define INSURANCE	/* allow crashed game recovery */

#define CHDIR		/* delete if no chdir() available */

#ifdef CHDIR
/*
 * If you define HACKDIR, then this will be the default playground;
 * otherwise it will be the current directory.
 */
# ifndef HACKDIR
#  define HACKDIR "/usr/games/lib/nethackdir"
# endif

/*
 * Some system administrators are stupid enough to make Hack suid root
 * or suid daemon, where daemon has other powers besides that of reading or
 * writing Hack files.	In such cases one should be careful with chdir's
 * since the user might create files in a directory of his choice.
 * Of course SECURE is meaningful only if HACKDIR is defined.
 */
/* #define SECURE */	/* do setuid(getuid()) after chdir() */

/*
 * If it is desirable to limit the number of people that can play Hack
 * simultaneously, define HACKDIR, SECURE and MAX_NR_OF_PLAYERS.
 * #define MAX_NR_OF_PLAYERS 6
 */
#endif /* CHDIR */



/*
 * Section 3:	Definitions that may vary with system type.
 *		For example, both schar and uchar should be short ints on
 *		the AT&T 3B2/3B5/etc. family.
 */

#include <stdarg.h>

/* #define STRNCMPI */	/* compiler/library has the strncmpi function */

/*
 * Section 4:  THE FUN STUFF!!!
 *
 * Conditional compilation of special options are controlled here.
 * If you define the following flags, you will add not only to the
 * complexity of the game but also to the size of the load module.
 */

/* dungeon levels */
#define WALLIFIED_MAZE	/* Fancy mazes - Jean-Christophe Collet */
#define REINCARNATION	/* Special Rogue-like levels */
/* monsters & objects */
#define KOPS		/* Keystone Kops by Scott R. Turner */
#define SEDUCE		/* Succubi/incubi seduction, by KAA, suggested by IM */
#define TOURIST		/* Tourist players with cameras and Hawaiian shirts */
/* difficulty */
#define ELBERETH	/* Engraving the E-word repels monsters */

/* #define SCORE_ON_BOTL */	/* added by Gary Erickson (erickson@ucivax) */

/*
 * Section 5:  EXPERIMENTAL STUFF
 *
 * Conditional compilation of new or experimental options are controlled here.
 * Enable any of these at your own risk -- there are almost certainly
 * bugs left here.
 */

/*#define GOLDOBJ */	/* Gold is kept on obj chains - Helge Hafting */
/*#define AUTOPICKUP_EXCEPTIONS */ /* exceptions to autopickup */

/* End of Section 5 */

#include "global.h"	/* Define everything else according to choices above */

#endif /* CONFIG_H */
