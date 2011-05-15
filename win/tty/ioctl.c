/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This cannot be part of hack.tty.c (as it was earlier) since on some
   systems (e.g. MUNIX) the include files <termio.h> and <sgtty.h>
   define the same constants, and the C preprocessor complains. */

#include <stdio.h>

#include "nethack.h"

#include <termios.h>
#include <sys/ioctl.h>

struct termios termio;

#include "tcap.h"	/* for LI and CO */

void getwindowsz(void)
{
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
}

void getioctls(void)
{
	tcgetattr(fileno(stdin), &termio);
	getwindowsz();
}

void setioctls(void)
{
	tcsetattr(fileno(stdin), TCSADRAIN, &termio);
}
