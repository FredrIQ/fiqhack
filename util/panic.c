/*	SCCS Id: @(#)panic.c	3.4	1994/03/02	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *	This code was adapted from the code in end.c to run in a standalone
 *	mode for the makedefs / drg code.
 */

#include "config.h"

/*VARARGS1*/
boolean panicking;
void panic(char *,...);

void
panic (char *str, ...)
{
	va_list the_args;
	va_start(the_args, str);
	if(panicking++)
#ifdef SYSV
	    (void)
#endif
		abort();    /* avoid loops - this should never happen*/

	(void) fputs(" ERROR:  ", stderr);
	Vfprintf(stderr, str, the_args);
	(void) fflush(stderr);
#if defined(UNIX)
# ifdef SYSV
		(void)
# endif
		    abort();	/* generate core dump */
#endif
	va_end(the_args);
	exit(EXIT_FAILURE);		/* redundant */
	return;
}

#ifdef ALLOCA_HACK
/*
 * In case bison-generated foo_yacc.c tries to use alloca(); if we don't
 * have it then just use malloc() instead.  This may not work on some
 * systems, but they should either use yacc or get a real alloca routine.
 */
long *alloca(cnt)
unsigned cnt;
{
	return cnt ? alloc(cnt) : (long *)0;
}
#endif

/*panic.c*/
