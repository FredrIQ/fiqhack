/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#ifdef TTY_GRAPHICS
#include "wintty.h"
#endif

static void def_raw_print(const char *s);

struct window_procs windowprocs;

static void def_raw_print(const char *s)
{
    puts(s);
}


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
/*windows.c*/
