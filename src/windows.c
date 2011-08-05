/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

struct window_procs windowprocs;

/* delay 50 ms: check option before calling the window sys to do the job */
void nh_delay_output(void)
{
    if (flags.nap)
	(*windowprocs.win_delay_output)();
}


/*windows.c*/
