/*	SCCS Id: @(#)unixconf.h 3.4	1999/07/02	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef UNIX
#ifndef UNIXCONF_H
#define UNIXCONF_H

/*
 * Some include files are in a different place under SYSV
 *	BSD		   SYSV
 * <sys/time.h>		<time.h>
 * <sgtty.h>		<termio.h>
 *
 * Some routines are called differently
 * index		strchr
 * rindex		strrchr
 *
 */

/* define exactly one of the following two choices */
/* #define BSD 1 */	/* define for 4.n/Free/Open/Net BSD  */
			/* also for relatives like SunOS 4.x, DG/UX, and */
			/* older versions of Linux */
#define SYSV		/* define for System V, Solaris 2.x, newer versions */
			/* of Linux */
			

/* define any of the following that are appropriate */
/* #define SVR4 */	/* use in addition to SYSV for System V Release 4 */
			/* including Solaris 2+ */
#define NETWORK		/* if running on a networked system */
			/* e.g. Suns sharing a playground through NFS */
#define LINUX	/* Another Unix clone */
/* #define CYGWIN32 */	/* Unix on Win32 -- use with case sensitive defines */

#define TERMINFO	/* uses terminfo rather than termcap */
			/* Should be defined for most SYSV, SVR4 (including
			 * Solaris 2+), and Linux systems. */
#define TEXTCOLOR	/* Use System V r3.2 terminfo color support */
			/* and/or ANSI color support on termcap systems */
			/* and/or X11 color */

/* #define RANDOM */		/* if neither random/srandom nor lrand48/srand48
				   is available from your system */


/*
 * The next two defines are intended mainly for the Andrew File System,
 * which does not allow hard links.  Lock files
 * will be created in LOCKDIR using open() instead of in the playground using
 * link().
 *		Ralf Brown, 7/26/89 (from v2.3 hack of 10/10/88)
 */

/* #define LOCKDIR "/usr/games/lib/nethackdir" */	/* where to put locks */

/*
 * If you want the static parts of your playground on a read-only file
 * system, define VAR_PLAYGROUND to be where the variable parts are kept.
 */
/* #define VAR_PLAYGROUND "/var/lib/games/nethack" */


/*
 * Define PORT_HELP to be the name of the port-specfic help file.
 * This file is found in HACKDIR.
 * Normally, you shouldn't need to change this.
 * There is currently no port-specific help for Unix systems.
 */
/* #define PORT_HELP "Unixhelp" */

#ifdef TTY_GRAPHICS
/*
 * To enable the `timed_delay' option for using a timer rather than extra
 * screen output when pausing for display effect.  Requires that `msleep'
 * function be available (with time argument specified in milliseconds).
 * Various output devices can produce wildly varying delays when the
 * "extra output" method is used, but not all systems provide access to
 * a fine-grained timer.
 */
/* #define TIMED_DELAY */	/* usleep() */
#endif


#define FCMASK	0660	/* file creation mask */


/*
 * The remainder of the file should not need to be changed.
 */

#if defined(BSD)
#include <sys/time.h>
#else
#include <time.h>
#endif

#define HLOCK	"perm"	/* an empty file used for locking purposes */

#ifndef REDO
#define Getchar nhgetch
#endif
#define tgetch getchar

#include "system.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#if defined(BSD)
#define memcpy(d, s, n)		bcopy(s, d, n)
#define memcmp(s1, s2, n)	bcmp(s2, s1, n)
#else	/* therefore SYSV */
# ifndef index	/* some systems seem to do this for you */
#define index	strchr
# endif
# ifndef rindex
#define rindex	strrchr
# endif
#endif

/* Use the high quality random number routines. */
#if defined(BSD) || defined(LINUX) || defined(CYGWIN32) || defined(RANDOM) || defined(__APPLE__)
#define Rand()	random()
#else
#define Rand()	lrand48()
#endif

#ifdef TIMED_DELAY
# if defined(LINUX) || defined(BSD)
# define msleep(k) usleep((k)*1000)
# endif
#endif

#endif /* UNIXCONF_H */
#endif /* UNIX */
