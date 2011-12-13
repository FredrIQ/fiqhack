/* Copyright (c) NetHack PC Development Team 1993, 1994.  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef NTCONF_H
#define NTCONF_H

#define EXEPATH			/* Allow .exe location to be used as HACKDIR */
#define TRADITIONAL_GLYPHMAP	/* Store glyph mappings at level change time */
#ifdef WIN32CON
#define LAN_FEATURES		/* Include code for lan-aware features. Untested in 3.4.0*/
#endif

#define PC_LOCKING		/* Prevent overwrites of aborted or in-progress games */
				/* without first receiving confirmation. */

/*
 * -----------------------------------------------------------------
 *  The remaining code shouldn't need modification.
 * -----------------------------------------------------------------
 */
/* #define SHORT_FILENAMES */	/* All NT filesystems support long names now */

#define NOCWD_ASSUMPTIONS	/* Always define this. There are assumptions that
                                   it is defined for WIN32.
				   Allow paths to be specified for HACKDIR,
				   LEVELDIR, SAVEDIR, BONESDIR, DATADIR,
				   SCOREDIR, LOCKDIR, CONFIGDIR, and TROUBLEDIR */
#define NO_TERMS

#ifdef OPTIONS_USED
#undef OPTIONS_USED
#endif
#define OPTIONS_USED	"ttyoptions"
#define OPTIONS_FILE OPTIONS_USED

/* Stuff to help the user with some common, yet significant errors */
#define INTERJECT_PANIC		0
#define INTERJECTION_TYPES	(INTERJECT_PANIC + 1)
extern void interject_assistance(int,int,void *,void *);
extern void interject(int);

/* The following is needed for prototypes of certain functions */
#if defined(_MSC_VER)
#include <process.h>	/* Provides prototypes of exit(), spawn()      */
#endif

#include <string.h>	/* Provides prototypes of strncmpi(), etc.     */
#ifdef STRNCMPI
#define strncmpi(a,b,c) strnicmp(a,b,c)
#endif

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

#define regularize	nt_regularize

#ifndef M
#define M(c)		((char) (0x80 | (c)))
/* #define M(c)		((c) - 128) */
#endif

#ifndef C
#define C(c)		(0x1f & (c))
#endif

#define FILENAME_CMP  stricmp		      /* case insensitive */

extern const char *alllevels, *allbones;
extern char hackdir[];
#define ABORT C('a')
#define getuid() 1
#define getlogin() (NULL)
extern void win32_abort(void);
#ifdef WIN32CON
extern void nttty_preference_update(const char *);
extern void toggle_mouse_support(void);
extern void map_subkeyvalue(char *);
extern void load_keyboard_handler(void);
#endif

#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <conio.h>
#undef kbhit		/* Use our special NT kbhit */
#define kbhit (*nt_kbhit)

#ifdef LAN_FEATURES
#define MAX_LAN_USERNAME 20
#endif

#ifndef alloca
#define ALLOCA_HACK	/* used in util/panic.c */
#endif

#ifdef _MSC_VER
#pragma warning(disable:4761)	/* integral size mismatch in arg; conv supp*/
#ifdef YYPREFIX
#pragma warning(disable:4102)	/* unreferenced label */
#endif
#endif

extern int set_win32_option(const char *, const char *);
#ifdef WIN32CON
#define LEFTBUTTON  FROM_LEFT_1ST_BUTTON_PRESSED
#define RIGHTBUTTON RIGHTMOST_BUTTON_PRESSED
#define MIDBUTTON   FROM_LEFT_2ND_BUTTON_PRESSED
#define MOUSEMASK (LEFTBUTTON | RIGHTBUTTON | MIDBUTTON)
#endif /* WIN32CON */

#endif /* NTCONF_H */
