/* Copyright (c) NitroHack PC Development Team 1993, 1994.  */
/* NitroHack may be freely redistributed.  See license for details. */

#ifndef NTCONF_H
#define NTCONF_H

/*
 * -----------------------------------------------------------------
 *  The remaining code shouldn't need modification.
 * -----------------------------------------------------------------
 */
/* The following is needed for prototypes of certain functions */
#if defined(_MSC_VER)
#include <process.h>	/* Provides prototypes of exit(), spawn()      */
#endif

#include <string.h>	/* Provides prototypes of strncmpi(), etc.     */
#define strncmpi(a,b,c) strnicmp(a,b,c)

#include <sys/types.h>
#include <stdlib.h>

#define PATHLEN		BUFSZ /* maximum pathlength */
#define FILENAME	BUFSZ /* maximum filename length (conservative) */

#if defined(_MAX_PATH) && defined(_MAX_FNAME)
# if (_MAX_PATH < BUFSZ) && (_MAX_FNAME < BUFSZ)
#undef PATHLEN
#undef FILENAME
#define PATHLEN		_MAX_PATH
#define FILENAME	_MAX_FNAME
# endif
#endif

#include <time.h>

#define FILENAME_CMP  stricmp		      /* case insensitive */
#define ftruncate _chsize

#define getuid() 1
#define getlogin() (NULL)

#include <fcntl.h>
#include <io.h>

#ifdef _MSC_VER
# pragma warning(disable:4761)	/* integral size mismatch in arg; conv supp*/
# pragma warning(disable:4996)	/* POSIX name is deprecated*/
# define inline __inline
# ifdef YYPREFIX
#  pragma warning(disable:4102)	/* unreferenced label */
# endif
# define YY_NO_UNISTD_H
# define snprintf(buf, len, fmt, ...) _snprintf_s(buf, len, len-1, fmt, __VA_ARGS__)
#endif

#endif /* NTCONF_H */
