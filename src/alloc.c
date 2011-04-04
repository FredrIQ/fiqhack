/*	SCCS Id: @(#)alloc.c	3.4	1995/10/04	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */


/* since this file is also used in auxiliary programs, don't include all the
 * function declarations for all of nethack
 */
#define EXTERN_H	/* comment line for pre-compiled headers */
#include "config.h"

char *fmt_ptr(const void *,char *);
long *alloc(unsigned int);
extern void panic(const char *,...) PRINTF_F(1,2);


long *
alloc(lth)
unsigned int lth;
{
	void * ptr;

	ptr = malloc(lth);
	if (!ptr) panic("Memory allocation failure; cannot get %u bytes", lth);
	return (long *) ptr;
}


/* format a pointer for display purposes; caller supplies the result buffer */
char *
fmt_ptr(ptr, buf)
const void * ptr;
char *buf;
{
	sprintf(buf, "%p", (void *)ptr);
	return buf;
}


/*alloc.c*/
