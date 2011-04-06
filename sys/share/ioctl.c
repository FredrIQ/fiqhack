/*	SCCS Id: @(#)ioctl.c	3.4	1990/22/02 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This cannot be part of hack.tty.c (as it was earlier) since on some
   systems (e.g. MUNIX) the include files <termio.h> and <sgtty.h>
   define the same constants, and the C preprocessor complains. */

#include "hack.h"

#include <termios.h>
struct termios termio;
#if defined(BSD)
#include <sys/ioctl.h>
#endif

#if defined(TIOCGWINSZ) && (defined(BSD) || defined(SVR4))
#define USE_WIN_IOCTL
#include "tcap.h"	/* for LI and CO */
#endif

void getwindowsz(void)
{
#ifdef USE_WIN_IOCTL
    /*
     * ttysize is found on Suns and BSD
     * winsize is found on Suns, BSD, and Ultrix
     */
    struct winsize ttsz;

    if (ioctl(fileno(stdin), (int)TIOCGWINSZ, (char *)&ttsz) != -1) {
	/*
	 * Use the kernel's values for lines and columns if it has
	 * any idea.
	 */
	if (ttsz.ws_row)
	    LI = ttsz.ws_row;
	if (ttsz.ws_col)
	    CO = ttsz.ws_col;
    }
#endif
}

void getioctls(void)
{
	(void) tcgetattr(fileno(stdin), &termio);
	getwindowsz();
}

void setioctls(void)
{
	(void) tcsetattr(fileno(stdin), TCSADRAIN, &termio);
}
