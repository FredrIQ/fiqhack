/*	SCCS Id: @(#)pcsys.c	3.4	2002/01/22		  */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *  System related functions for OS/2 and Windows NT
 */

#include "hack.h"
#include "wintty.h"

#include <ctype.h>
#include <fcntl.h>
#include <process.h>

#if defined(WIN32)
void nethack_exit(int);
#else
#define nethack_exit exit
#endif
static void msexit(void);


#ifdef WIN32CON
extern int GUILaunched;    /* from nttty.c */
#endif

#if defined(WIN32)

void
flushout()
{
	(void) fflush(stdout);
	return;
}

static const char *COMSPEC = "COMSPEC";

#define getcomspec() nh_getenv(COMSPEC)

#endif /* WIN32 */

/*
 * Add a backslash to any name not ending in /, \ or :	 There must
 * be room for the \
 */
void
append_slash(name)
char *name;
{
	char *ptr;

	if (!*name)
		return;
	ptr = name + (strlen(name) - 1);
	if (*ptr != '\\' && *ptr != '/' && *ptr != ':') {
		*++ptr = '\\';
		*++ptr = '\0';
	}
	return;
}

#ifdef WIN32
boolean getreturn_enabled;
#endif

void
getreturn(str)
const char *str;
{
#ifdef WIN32
	if (!getreturn_enabled) return;
#endif
	msmsg("Hit <Enter> %s.", str);
	while (Getchar() != '\n') ;
	return;
}

#ifndef WIN32CON
void
msmsg (const char * fmt, ...)
{
	va_list the_args;
	va_start(the_args, fmt);
	Vprintf(fmt, the_args);
	flushout();
	va_end(the_args);
	return;
}
#endif

/*
 * Follow the PATH, trying to fopen the file.
 */
#define PATHSEP ';'

FILE *
fopenp(name, mode)
const char *name, *mode;
{
	char buf[BUFSIZ], *bp, *pp, lastch = 0;
	FILE *fp;

	/* Try the default directory first.  Then look along PATH.
	 */
	(void) strncpy(buf, name, BUFSIZ - 1);
	buf[BUFSIZ-1] = '\0';
	if ((fp = fopen(buf, mode)))
		return fp;
	else {
		int ccnt = 0;
		pp = getenv("PATH");
		while (pp && *pp) {
			bp = buf;
			while (*pp && *pp != PATHSEP) {
				lastch = *bp++ = *pp++;
				ccnt++;
			}
			if (lastch != '\\' && lastch != '/') {
				*bp++ = '\\';
				ccnt++;
			}
			(void) strncpy(bp, name, (BUFSIZ - ccnt) - 2);
			bp[BUFSIZ - ccnt - 1] = '\0';
			if ((fp = fopen(buf, mode)))
				return fp;
			if (*pp)
				pp++;
		}
	}
	return (FILE *)0;
}

#if defined(WIN32)
void nethack_exit(code)
int code;
{
	msexit();
	exit(code);
}

/* Chdir back to original directory
 */

static void msexit()
{
#ifdef CHDIR
	extern char orgdir[];
#endif

	flushout();
#ifndef WIN32
	enable_ctrlP(); 	/* in case this wasn't done */
#endif
#if defined(CHDIR) && !defined(NOCWD_ASSUMPTIONS)
	chdir(orgdir);		/* chdir, not chdirx */
	chdrive(orgdir);
#endif
#ifdef WIN32CON
	/* Only if we started from the GUI, not the command prompt,
	 * we need to get one last return, so the score board does
	 * not vanish instantly after being created.
	 * GUILaunched is defined and set in nttty.c.
	 */
	synch_cursor();
	if (GUILaunched) getreturn("to end");
	synch_cursor();
#endif
	return;
}
#endif /* WIN32 */
