/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef UNIX
#ifndef UNIXCONF_H
#define UNIXCONF_H

/* define any of the following that are appropriate */
/* #define RANDOM */		/* if neither random/srandom nor lrand48/srand48
				   is available from your system */

/*
 * Define PORT_HELP to be the name of the port-specfic help file.
 * This file is found in HACKDIR.
 * Normally, you shouldn't need to change this.
 * There is currently no port-specific help for Unix systems.
 */
/* #define PORT_HELP "Unixhelp" */

/*
 * To enable the `timed_delay' option for using a timer rather than extra
 * screen output when pausing for display effect.  Requires that `msleep'
 * function be available (with time argument specified in milliseconds).
 * Various output devices can produce wildly varying delays when the
 * "extra output" method is used, but not all systems provide access to
 * a fine-grained timer.
 */
#define TIMED_DELAY	/* usleep() */


#define FCMASK	0660	/* file creation mask */


/*
 * The remainder of the file should not need to be changed.
 */
#include <time.h>

#define HLOCK	"perm"	/* an empty file used for locking purposes */

#include "system.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#ifndef index	/* some systems seem to do this for you */
#define index	strchr
#endif

#ifndef rindex
#define rindex	strrchr
#endif

/* Use the high quality random number routines. */
#define Rand()	random()

#ifdef TIMED_DELAY
# define msleep(k) usleep((k)*1000)
#endif

#endif /* UNIXCONF_H */
#endif /* UNIX */
