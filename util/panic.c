/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

/*
 *	This code was adapted from the code in end.c to run in a standalone
 *	mode for the makedefs / drg code.
 */

#include "config.h"

/*VARARGS1*/
boolean panicking;
void panic(char *,...);

void panic (char *str, ...)
{
	va_list the_args;
	va_start(the_args, str);
	if (panicking++)
	    abort();    /* avoid loops - this should never happen*/

	fputs(" ERROR:  ", stderr);
	vfprintf(stderr, str, the_args);
	fflush(stderr);

	va_end(the_args);
	exit(EXIT_FAILURE);		/* redundant */
	return;
}

/*panic.c*/
