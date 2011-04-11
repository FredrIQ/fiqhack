/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

struct window_procs windowprocs;

/*
 * tty_message_menu() provides a means to get feedback from the
 * --More-- prompt; other interfaces generally don't need that.
 */
/*ARGSUSED*/
char genl_message_menu(char let, int how, const char *mesg)
{
    pline("%s", mesg);
    return 0;
}

/*ARGSUSED*/
void genl_preference_update(const char *pref)
{
	/* window ports are expected to provide
	   their own preference update routine
	   for the preference capabilities that
	   they support.
	   Just return in this genl one. */
}

/* delay 50 ms: check option before calling the window sys to do the job */
void nh_delay_output(void)
{
    if (flags.nap)
	(*windowprocs.win_delay_output)();
}


/*windows.c*/
