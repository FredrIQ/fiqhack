/*	SCCS Id: @(#)tradstdc.h 3.4	1993/05/30	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef TRADSTDC_H
#define TRADSTDC_H

#include <stdarg.h>

/*
 * Used for robust ANSI parameter forward declarations:
 * int VDECL(sprintf, (char *, const char *, ...));
 *
 * NDECL() is used for functions with zero arguments;
 * FDECL() is used for functions with a fixed number of arguments;
 * VDECL() is used for functions with a variable number of arguments.
 */

#define NDECL(f)	f(void)
#define FDECL(f,p)	f p
#define VDECL(f,p)	f p

/*
 * Allow gcc2 to check parameters of printf-like calls with -Wformat;
 * append this to a prototype declaration (see pline() in extern.h).
 */
#ifdef __GNUC__
#define PRINTF_F(f,v) __attribute__ ((format (printf, f, v)))
#endif
#ifndef PRINTF_F
#define PRINTF_F(f,v)
#endif

#endif /* TRADSTDC_H */
