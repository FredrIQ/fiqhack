/*	SCCS Id: @(#)pcconf.h	3.4	1995/10/11	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef PCCONF_H
#define PCCONF_H

#define MICRO		/* always define this! */

#define PATHLEN		64	/* maximum pathlength */
#define FILENAME	80	/* maximum filename length (conservative) */
#ifndef MICRO_H
#include "micro.h"		/* contains necessary externs for [os_name].c */
#endif


/* ===================================================
 *  The remaining code shouldn't need modification.
 */

#ifndef SYSTEM_H
#include "system.h"
#endif

/* Borland Stuff */
# if defined(__BORLANDC__)
#  if defined(__OVERLAY__) && !defined(VROOMM)
/* __OVERLAY__ is automatically defined by Borland C if overlay option is on */
#define VROOMM		/* Borland's VROOMM overlay system */
#  endif
#  if !defined(STKSIZ)
#define STKSIZ	5*1024	/* Use a default of 5K stack for Borland C	*/
			/* This macro is used in any file that contains */
			/* a main() function.				*/
#  endif
#define PC_LOCKING
# endif

#ifdef PC_LOCKING
#define HLOCK "NHPERM"
#endif

#ifndef index
# define index	strchr
#endif
#ifndef rindex
# define rindex strrchr
#endif

#ifndef AMIGA
#include <time.h>
#endif

#ifdef RANDOM
/* Use the high quality random number routines. */
# define Rand() random()
#else
# define Rand() rand()
#endif

#ifndef TOS
# define FCMASK 0660	/* file creation mask */
#endif

#include <fcntl.h>

#ifndef REDO
# undef Getchar
# define Getchar nhgetch
#endif


/* OVERLAY must be defined with VROOMM */
#if defined(VROOMM)
# ifndef OVERLAY
#  define OVERLAY
# endif
#endif

#if defined(FUNCTION_LEVEL_LINKING)
#define OVERLAY
#define OVL0
#define OVL1
#define OVL2
#define OVL3
#define OVLB
#endif

#if defined(OVERLAY) && !defined(VROOMM) && !defined(FUNCTION_LEVEL_LINKING)
#define USE_TRAMPOLI
#endif

#ifdef TIMED_DELAY
# ifdef __BORLANDC__
# define msleep(k) delay(k)
# endif
# ifdef __SC__
# define msleep(k) (void) usleep((long)((k)*1000))
# endif
#endif

#endif /* PCCONF_H */
